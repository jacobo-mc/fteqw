#include "quakedef.h"

#ifdef D3D11QUAKE
#include "shader.h"
#include "winquake.h"
#define COBJMACROS
#include <d3d11.h>
extern ID3D11Device *pD3DDev11;


#ifndef IID_ID3DBlob
	//microsoft can be such a pain sometimes.
	typedef struct _D3D_SHADER_MACRO
	{
		LPCSTR Name;
		LPCSTR Definition;

	} D3D_SHADER_MACRO, *LPD3D_SHADER_MACRO;

	typedef enum _D3D_INCLUDE_TYPE { 
	  D3D_INCLUDE_LOCAL         = 0,
	  D3D_INCLUDE_SYSTEM        = ( D3D_INCLUDE_LOCAL + 1 ),
	  D3D_INCLUDE_FORCE_DWORD   = 0x7fffffff 
	} D3D_INCLUDE_TYPE;

	#undef INTERFACE
	#define INTERFACE ID3DInclude
	DECLARE_INTERFACE_(INTERFACE, IUnknown)
	{
		STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) PURE;
		STDMETHOD(Close)(THIS_ LPCVOID pData) PURE;
	};

	#undef INTERFACE
	#define INTERFACE ID3DBlob
	DECLARE_INTERFACE_(INTERFACE, IUnknown)
	{
		STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
		STDMETHOD_(ULONG, AddRef)(THIS) PURE;
		STDMETHOD_(ULONG, Release)(THIS) PURE;
		STDMETHOD_(LPVOID, GetBufferPointer)(THIS) PURE;
		STDMETHOD_(SIZE_T, GetBufferSize)(THIS) PURE;
	};
	#undef INTERFACE
#endif
#define ID3DBlob_GetBufferPointer(b) b->lpVtbl->GetBufferPointer(b)
#define ID3DBlob_Release(b) b->lpVtbl->Release(b)
#define ID3DBlob_GetBufferSize(b) b->lpVtbl->GetBufferSize(b)

HRESULT (WINAPI *pD3DCompile) (
	LPCVOID pSrcData,
	SIZE_T SrcDataSize,
	LPCSTR pSourceName,
	const D3D_SHADER_MACRO *pDefines,
	ID3DInclude *pInclude,
	LPCSTR pEntrypoint,
	LPCSTR pTarget,
	UINT Flags1,
	UINT Flags2,
	ID3DBlob **ppCode,
	ID3DBlob **ppErrorMsgs
);
static dllhandle_t *shaderlib;


D3D_FEATURE_LEVEL d3dfeaturelevel;


HRESULT STDMETHODCALLTYPE d3dinclude_Close(ID3DInclude *this, LPCVOID pData)
{
	free((void*)pData);
	return S_OK;
}
HRESULT STDMETHODCALLTYPE d3dinclude_Open(ID3DInclude *this, D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
{
	if (IncludeType == D3D_INCLUDE_SYSTEM)
	{
		if (!strcmp(pFileName, "ftedefs.h"))
		{
			static const char *defstruct =
				"cbuffer ftemodeldefs : register(b0)\n"
				"{\n"
					"matrix m_model;\n"
					"float3 e_eyepos; float e_time;\n"
					"float3 e_light_ambient; float pad1;\n"
					"float3 e_light_dir; float pad2;\n"
					"float3 e_light_mul; float pad3;\n"
					"float4 e_lmscale[4];\n"
				"};\n"
				"cbuffer fteviewdefs : register(b1)\n"
				"{\n"
					"matrix m_view;\n"
					"matrix m_projection;\n"
					"float3 v_eyepos; float v_time;\n"
				"};\n"
				"cbuffer ftelightdefs : register(b2)\n"
				"{\n"
					"matrix l_cubematrix;\n"
					"float3 l_lightposition; float padl1;\n"
					"float3 l_colour; float padl2;\n"
					"float3 l_lightcolourscale;float l_lightradius;\n"
					"float4 l_shadowmapproj;\n"
					"float2 l_shadowmapscale; float2 padl3;\n"
				"};\n"
				;
			*ppData = strdup(defstruct);
			*pBytes = strlen(*ppData);
			return S_OK;
		}
		if (!strcmp(pFileName, "sys/skeletal.h"))
		{
			static const char *defstruct =
				""
				;
			*ppData = strdup(defstruct);
			*pBytes = strlen(*ppData);
			return S_OK;
		}
	}
	else
	{
		
	}
	return E_FAIL;
}
ID3DIncludeVtbl myd3dincludetab =
{
	d3dinclude_Open,
	d3dinclude_Close
};
ID3DInclude myd3dinclude =
{
	&myd3dincludetab
};

typedef struct
{
	vecV_t coord;
	vec2_t tex;
	vec2_t lm;
	vec3_t ndir;
	vec3_t sdir;
	vec3_t tdir;
	byte_vec4_t colorsb;
} vbovdata_t;

void D3D11Shader_DeleteProg(program_t *prog, unsigned int permu)
{
	ID3D11InputLayout *layout;
	ID3D11PixelShader *frag;
	ID3D11VertexShader *vert;
	vert = prog->permu[permu].handle.hlsl.vert;
	frag = prog->permu[permu].handle.hlsl.frag;
	layout = prog->permu[permu].handle.hlsl.layout;
	if (vert)
		ID3D11VertexShader_Release(vert);
	if (frag)
		ID3D11PixelShader_Release(frag);
	if (layout)
		ID3D11InputLayout_Release(layout);
}

//create a program from two blobs.
static qboolean D3D11Shader_CreateShaders(program_t *prog, const char *name, int permu, 
										  void *vblob, size_t vsize,
										  void *hblob, size_t hsize,
										  void *dblob, size_t dsize,
										  void *fblob, size_t fsize)
{
	qboolean success = true;

	if (FAILED(ID3D11Device_CreateVertexShader(pD3DDev11, vblob, vsize, NULL, (ID3D11VertexShader**)&prog->permu[permu].handle.hlsl.vert)))
		success = false;

	if (hblob || dblob)
	{
		prog->permu[permu].handle.hlsl.topology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
		if (FAILED(ID3D11Device_CreateHullShader(pD3DDev11, hblob, hsize, NULL, (ID3D11HullShader**)&prog->permu[permu].handle.hlsl.hull)))
			success = false;

		if (FAILED(ID3D11Device_CreateDomainShader(pD3DDev11, dblob, dsize, NULL, (ID3D11DomainShader**)&prog->permu[permu].handle.hlsl.domain)))
			success = false;
	}
	else
		prog->permu[permu].handle.hlsl.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	if (FAILED(ID3D11Device_CreatePixelShader(pD3DDev11, fblob, fsize, NULL, (ID3D11PixelShader**)&prog->permu[permu].handle.hlsl.frag)))
		success = false;

	if (success)
	{
		D3D11_INPUT_ELEMENT_DESC decl[13];
		int elements = 0;
		vbovdata_t *foo = NULL;

		decl[elements].SemanticName = "POSITION";
		decl[elements].SemanticIndex = 0;
		decl[elements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		decl[elements].InputSlot = 0;
		decl[elements].AlignedByteOffset = (char*)&foo->coord[0] - (char*)NULL;
		decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		decl[elements].InstanceDataStepRate = 0;
		elements++;

		decl[elements].SemanticName = "TEXCOORD";
		decl[elements].SemanticIndex = 0;
		decl[elements].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		decl[elements].InputSlot = 0;
		decl[elements].AlignedByteOffset = (char*)&foo->tex[0] - (char*)NULL;
		decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		decl[elements].InstanceDataStepRate = 0;
		elements++;
		/*
		decl[elements].SemanticName = "TEXCOORD";
		decl[elements].SemanticIndex = 1;
		decl[elements].Format = DXGI_FORMAT_R32G32_FLOAT;
		decl[elements].InputSlot = 1;
		decl[elements].AlignedByteOffset = (char*)&foo->lm[0] - (char*)NULL;
		decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		decl[elements].InstanceDataStepRate = 0;
		elements++;
		*/
		decl[elements].SemanticName = "COLOR";
		decl[elements].SemanticIndex = 0;
		decl[elements].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		decl[elements].InputSlot = 0;
		decl[elements].AlignedByteOffset = (char*)&foo->colorsb[0] - (char*)NULL;
		decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		decl[elements].InstanceDataStepRate = 0;
		elements++;

		decl[elements].SemanticName = "NORMAL";
		decl[elements].SemanticIndex = 0;
		decl[elements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		decl[elements].InputSlot = 0;
		decl[elements].AlignedByteOffset = (char*)&foo->ndir[0] - (char*)NULL;
		decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		decl[elements].InstanceDataStepRate = 0;
		elements++;

		decl[elements].SemanticName = "TANGENT";
		decl[elements].SemanticIndex = 0;
		decl[elements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		decl[elements].InputSlot = 0;
		decl[elements].AlignedByteOffset = (char*)&foo->sdir[0] - (char*)NULL;
		decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		decl[elements].InstanceDataStepRate = 0;
		elements++;

		decl[elements].SemanticName = "BINORMAL";
		decl[elements].SemanticIndex = 0;
		decl[elements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		decl[elements].InputSlot = 0;
		decl[elements].AlignedByteOffset = (char*)&foo->tdir[0] - (char*)NULL;
		decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		decl[elements].InstanceDataStepRate = 0;
		elements++;

/*
		decl[elements].SemanticName = "BLENDWEIGHT";
		decl[elements].SemanticIndex = 0;
		decl[elements].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		decl[elements].InputSlot = 0;
		decl[elements].AlignedByteOffset = 0;
		decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		decl[elements].InstanceDataStepRate = 0;
		elements++;

		decl[elements].SemanticName = "BLENDINDICIES";
		decl[elements].SemanticIndex = 0;
		decl[elements].Format = DXGI_FORMAT_R8G8B8A8_UINT;
		decl[elements].InputSlot = 0;
		decl[elements].AlignedByteOffset = 0;
		decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		decl[elements].InstanceDataStepRate = 0;
		elements++;
*/
		if (FAILED(ID3D11Device_CreateInputLayout(pD3DDev11, decl, elements, vblob, vsize, (ID3D11InputLayout**)&prog->permu[permu].handle.hlsl.layout)))
		{
			Con_Printf("HLSL Shader %s requires unsupported inputs\n", name);
			success = false;
		}
	}
	return success;
}

static qboolean D3D11Shader_LoadBlob(program_t *prog, const char *name, unsigned int permu, vfsfile_t *blobfile)
{
	qboolean success;
	char *vblob, *hblob, *dblob, *fblob;
	unsigned int vsz, hsz, dsz, fsz;

	VFS_READ(blobfile, &vsz, sizeof(vsz));
	vblob = Z_Malloc(vsz);
	VFS_READ(blobfile, vblob, vsz);

	VFS_READ(blobfile, &hsz, sizeof(hsz));
	if (hsz != ~0u)
	{
		hblob = Z_Malloc(hsz);
		VFS_READ(blobfile, hblob, hsz);
	}
	else
		hblob = NULL;

	VFS_READ(blobfile, &dsz, sizeof(dsz));
	if (dsz != ~0u)
	{
		dblob = Z_Malloc(dsz);
		VFS_READ(blobfile, dblob, dsz);
	}
	else
		dblob = NULL;

	VFS_READ(blobfile, &fsz, sizeof(fsz));
	fblob = Z_Malloc(fsz);
	VFS_READ(blobfile, fblob, fsz);


	success = D3D11Shader_CreateShaders(prog, name, permu, vblob, vsz, hblob, hsz, dblob, dsz, fblob, fsz);
	Z_Free(vblob);
	Z_Free(hblob);
	Z_Free(dblob);
	Z_Free(fblob);
	return success;
}

qboolean D3D11Shader_CreateProgram (program_t *prog, const char *name, unsigned int permu, int ver, const char **precompilerconstants, const char *vert, const char *hull, const char *domain, const char *frag, qboolean silenterrors, vfsfile_t *blobfile)
{
	char *vsformat;
	char *hsformat = NULL;
	char *dsformat = NULL;
	char *fsformat;
	D3D_SHADER_MACRO defines[64];
	ID3DBlob *vcode = NULL, *hcode = NULL, *dcode = NULL, *fcode = NULL, *errors = NULL;
	qboolean success = false;

	if (d3dfeaturelevel >= D3D_FEATURE_LEVEL_11_0)	//and 11.1
	{
		vsformat = "vs_5_0";
		hsformat = "hs_5_0";
		dsformat = "ds_5_0";
		fsformat = "ps_5_0";
	}
	else if (d3dfeaturelevel >= D3D_FEATURE_LEVEL_10_1)
	{
		vsformat = "vs_4_1";
		fsformat = "ps_4_1";
	}
	else if (d3dfeaturelevel >= D3D_FEATURE_LEVEL_10_0)
	{
		vsformat = "vs_4_0";
		fsformat = "ps_4_0";
	}
	else if (d3dfeaturelevel >= D3D_FEATURE_LEVEL_9_3)
	{	//dx10-compatible output for 9.3 hardware
		vsformat = "vs_4_0_level_9_3";
		fsformat = "ps_4_0_level_9_3";
	}
	else
	{	//dx10-compatible output for 9.1|9.2 hardware
		vsformat = "vs_4_0_level_9_1";
		fsformat = "ps_4_0_level_9_1";
	}

	prog->permu[permu].handle.hlsl.vert = NULL;
	prog->permu[permu].handle.hlsl.frag = NULL;
	prog->permu[permu].handle.hlsl.layout = NULL;

	if (pD3DCompile)
	{
		int consts;
		for (consts = 0; precompilerconstants[consts]; consts++)
			;
		if (consts+3 >= sizeof(defines) / sizeof(defines[0]))
			return success;

		consts = 0;
		defines[consts].Name = NULL; /*shader type*/
		defines[consts].Definition = "1";
		consts++;

		defines[consts].Name = "ENGINE_"DISTRIBUTION;
		defines[consts].Definition = __DATE__;
		consts++;

		defines[consts].Name = Z_StrDup("LEVEL");
		defines[consts].Definition = Z_StrDup(va("0x%x", d3dfeaturelevel));
		consts++;

		for (; *precompilerconstants; precompilerconstants++)
		{
			const char *t = *precompilerconstants;
			t = COM_Parse(t);
			t = COM_Parse(t);
			defines[consts].Name = Z_StrDup(com_token);
			defines[consts].Definition = t?Z_StrDup(t):NULL;
			consts++;
		}

		defines[consts].Name = NULL;
		defines[consts].Definition = NULL;

		success = true;

		defines[0].Name = "VERTEX_SHADER";
		if (FAILED(pD3DCompile(vert, strlen(vert), name, defines, &myd3dinclude, "main", vsformat, 0, 0, &vcode, &errors)))
			success = false;
		if (errors && !silenterrors)
		{
			char *messages = ID3DBlob_GetBufferPointer(errors);
			Con_Printf("vertex shader %s:\n%s", name, messages);
			ID3DBlob_Release(errors);
		}

		if (hull)
		{
			if (!hsformat)
				success = false;
			else
			{
				defines[0].Name = "HULL_SHADER";
				if (FAILED(pD3DCompile(hull, strlen(hull), name, defines, &myd3dinclude, "main", hsformat, 0, 0, &hcode, &errors)))
					success = false;
				if (errors && !silenterrors)
				{
					char *messages = ID3DBlob_GetBufferPointer(errors);
					Con_Printf("hull shader %s:\n%s", name, messages);
					ID3DBlob_Release(errors);
				}
			}
		}

		if (domain)
		{
			if (!dsformat)
				success = false;
			else
			{
				defines[0].Name = "DOMAIN_SHADER";
				if (FAILED(pD3DCompile(domain, strlen(domain), name, defines, &myd3dinclude, "main", dsformat, 0, 0, &dcode, &errors)))
					success = false;
				if (errors && !silenterrors)
				{
					char *messages = ID3DBlob_GetBufferPointer(errors);
					Con_Printf("domain shader %s:\n%s", name, messages);
					ID3DBlob_Release(errors);
				}
			}
		}

		defines[0].Name = "FRAGMENT_SHADER";
		if (FAILED(pD3DCompile(frag, strlen(frag), name, defines, &myd3dinclude, "main", fsformat, 0, 0, &fcode, &errors)))
			success = false;
		if (errors && !silenterrors)
		{
			char *messages = ID3DBlob_GetBufferPointer(errors);
			Con_Printf("fragment shader %s:\n%s", name, messages);
			ID3DBlob_Release(errors);
		}

		while(consts-->2)
		{
			Z_Free((void*)defines[consts].Name);
			Z_Free((void*)defines[consts].Definition);
		}

		if (success)
			success = D3D11Shader_CreateShaders(prog, name, permu, 
				ID3DBlob_GetBufferPointer(vcode), ID3DBlob_GetBufferSize(vcode),
				hcode?ID3DBlob_GetBufferPointer(hcode):NULL, hcode?ID3DBlob_GetBufferSize(hcode):0,
				dcode?ID3DBlob_GetBufferPointer(dcode):NULL, dcode?ID3DBlob_GetBufferSize(dcode):0,
				ID3DBlob_GetBufferPointer(fcode), ID3DBlob_GetBufferSize(fcode));

		if (success && blobfile)
		{
			unsigned int sz;
			sz = ID3DBlob_GetBufferSize(vcode);
			VFS_WRITE(blobfile, &sz, sizeof(sz));
			VFS_WRITE(blobfile, ID3DBlob_GetBufferPointer(vcode), sz);

			if (!hcode)
			{
				sz = ~0u;
				VFS_WRITE(blobfile, &sz, sizeof(sz));
			}
			else
			{
				sz = ID3DBlob_GetBufferSize(hcode);
				VFS_WRITE(blobfile, &sz, sizeof(sz));
				VFS_WRITE(blobfile, ID3DBlob_GetBufferPointer(hcode), sz);
			}

			if (!dcode)
			{
				sz = ~0u;
				VFS_WRITE(blobfile, &sz, sizeof(sz));
			}
			else
			{
				sz = ID3DBlob_GetBufferSize(dcode);
				VFS_WRITE(blobfile, &sz, sizeof(sz));
				VFS_WRITE(blobfile, ID3DBlob_GetBufferPointer(dcode), sz);
			}

			sz = ID3DBlob_GetBufferSize(fcode);
			VFS_WRITE(blobfile, &sz, sizeof(sz));
			VFS_WRITE(blobfile, ID3DBlob_GetBufferPointer(fcode), sz);
		}

		if (vcode)
			ID3DBlob_Release(vcode);
		if (hcode)
			ID3DBlob_Release(hcode);
		if (dcode)
			ID3DBlob_Release(dcode);
		if (fcode)
			ID3DBlob_Release(fcode);
	}
	return success;
}

qboolean D3D11Shader_Init(unsigned int flevel)
{
	//FIXME: if the feature level is below 10, make sure the compiler supports all the right targets etc
	int ver;
	dllfunction_t funcsold[] =
	{
		{(void**)&pD3DCompile, "D3DCompileFromMemory"},
		{NULL,NULL}
	};
	dllfunction_t funcsnew[] =
	{
		{(void**)&pD3DCompile, "D3DCompile"},
		{NULL,NULL}
	};

	for (ver = 47; ver >= 33; ver--)
	{
		shaderlib = Sys_LoadLibrary(va("D3dcompiler_%i.dll", ver), (ver>=40)?funcsnew:funcsold);
		if (shaderlib)
			break;
	}

	if (!shaderlib)
	{
		//no shader library available. at least make sure that there's a 2d blob that we can use.
		if (!COM_FCheckExists("hlsl11/default2d.blob"))
			return false;
	}

	sh_config.minver = 11;
	sh_config.maxver = 11;
	sh_config.blobpath = "hlsl11/%s.blob";
	sh_config.progpath = shaderlib?"hlsl11/%s.hlsl":NULL;
	sh_config.shadernamefmt = "%s_hlsl11";

	sh_config.progs_supported	= true;
	sh_config.progs_required	= true;

	sh_config.pDeleteProg		= D3D11Shader_DeleteProg;
	sh_config.pLoadBlob			= D3D11Shader_LoadBlob;
	sh_config.pCreateProgram	= D3D11Shader_CreateProgram;
	sh_config.pProgAutoFields	= NULL;

	sh_config.tex_env_combine		= 1;
	sh_config.nv_tex_env_combine4	= 1;
	sh_config.env_add				= 1;

	d3dfeaturelevel = flevel;
	return true;
}

#endif
