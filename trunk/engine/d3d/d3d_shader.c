#include "quakedef.h"

#ifdef D3D9QUAKE
#include "shader.h"
#include "winquake.h"
#if !defined(HMONITOR_DECLARED) && (WINVER < 0x0500)
    #define HMONITOR_DECLARED
    DECLARE_HANDLE(HMONITOR);
#endif
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 pD3DDev9;

typedef struct {
  LPCSTR Name;
  LPCSTR Definition;
} D3DXMACRO;

#define D3DXHANDLE void *
#define LPD3DXINCLUDE void *

#undef INTERFACE
#define INTERFACE d3dxbuffer
DECLARE_INTERFACE_(d3dxbuffer,IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	STDMETHOD_(LPVOID,GetBufferPointer)(THIS) PURE;
	STDMETHOD_(SIZE_T,GetBufferSize)(THIS) PURE;
};
typedef struct d3dxbuffer *LPD3DXBUFFER;

typedef enum _D3DXREGISTER_SET 
{ 
    D3DXRS_BOOL, 
    D3DXRS_INT4, 
    D3DXRS_FLOAT4, 
    D3DXRS_SAMPLER, 
    D3DXRS_FORCE_DWORD = 0x7fffffff 
} D3DXREGISTER_SET, *LPD3DXREGISTER_SET; 
typedef enum _D3DXPARAMETER_CLASS 
{ 
    D3DXPC_SCALAR, 
    D3DXPC_VECTOR, 
    D3DXPC_MATRIX_ROWS, 
    D3DXPC_MATRIX_COLUMNS, 
    D3DXPC_OBJECT, 
    D3DXPC_STRUCT, 
    D3DXPC_FORCE_DWORD = 0x7fffffff 
} D3DXPARAMETER_CLASS, *LPD3DXPARAMETER_CLASS;
typedef enum _D3DXPARAMETER_TYPE 
{ 
    D3DXPT_VOID, 
    D3DXPT_BOOL, 
    D3DXPT_INT, 
    D3DXPT_FLOAT, 
    D3DXPT_STRING, 
    D3DXPT_TEXTURE, 
    D3DXPT_TEXTURE1D, 
    D3DXPT_TEXTURE2D, 
    D3DXPT_TEXTURE3D, 
    D3DXPT_TEXTURECUBE, 
    D3DXPT_SAMPLER, 
    D3DXPT_SAMPLER1D, 
    D3DXPT_SAMPLER2D, 
    D3DXPT_SAMPLER3D, 
    D3DXPT_SAMPLERCUBE, 
    D3DXPT_PIXELSHADER, 
    D3DXPT_VERTEXSHADER, 
    D3DXPT_PIXELFRAGMENT, 
    D3DXPT_VERTEXFRAGMENT,
} D3DXPARAMETER_TYPE, *LPD3DXPARAMETER_TYPE;
typedef struct _D3DXCONSTANT_DESC 
{ 
    LPCSTR Name;                        // Constant name 

    D3DXREGISTER_SET RegisterSet;       // Register set 
    UINT RegisterIndex;                 // Register index 
    UINT RegisterCount;                 // Number of registers occupied 
 
    D3DXPARAMETER_CLASS Class;          // Class 
    D3DXPARAMETER_TYPE Type;            // Component type 
 
    UINT Rows;                          // Number of rows 
    UINT Columns;                       // Number of columns 
    UINT Elements;                      // Number of array elements 
    UINT StructMembers;                 // Number of structure member sub-parameters 
 
    UINT Bytes;                         // Data size, in bytes 
    LPCVOID DefaultValue;               // Pointer to default value 
 
} D3DXCONSTANT_DESC, *LPD3DXCONSTANT_DESC; 
typedef struct _D3DXCONSTANTTABLE_DESC 
{ 
    LPCSTR Creator;                     // Creator string 
    DWORD Version;                      // Shader version 
    UINT Constants;                     // Number of constants 
 
} D3DXCONSTANTTABLE_DESC, *LPD3DXCONSTANTTABLE_DESC;
 
#undef INTERFACE
#define INTERFACE d3dxconstanttable
DECLARE_INTERFACE_(d3dxconstanttable,IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	STDMETHOD_(LPVOID,GetBufferPointer)(THIS) PURE;
	STDMETHOD_(SIZE_T,GetBufferSize)(THIS) PURE;

    STDMETHOD(GetDesc)(THIS_ D3DXCONSTANTTABLE_DESC *pDesc) PURE; 
    STDMETHOD(GetConstantDesc)(THIS_ D3DXHANDLE hConstant, D3DXCONSTANT_DESC *pConstantDesc, UINT *pCount) PURE; 
 
    STDMETHOD_(D3DXHANDLE, GetConstant)(THIS_ D3DXHANDLE hConstant, UINT Index) PURE; 
    STDMETHOD_(D3DXHANDLE, GetConstantByName)(THIS_ D3DXHANDLE hConstant, LPCSTR pName) PURE; 
	STDMETHOD_(D3DXHANDLE, GetConstantElement)(THIS_ D3DXHANDLE hConstant, UINT Index) PURE;

	/*more stuff not included here cos I don't need it*/
};
typedef struct d3dxconstanttable *LPD3DXCONSTANTTABLE;


HRESULT (WINAPI *pD3DXCompileShader) (
	LPCSTR pSrcData,
	UINT SrcDataLen,
	const D3DXMACRO *pDefines,
	LPD3DXINCLUDE pInclude,
	LPCSTR pEntrypoint,
	LPCSTR pTarget,
	UINT Flags,
	LPD3DXBUFFER *ppCode,
	LPD3DXBUFFER *ppErrorMsgs,
	LPD3DXCONSTANTTABLE *constants
);
static dllhandle_t *shaderlib;

#ifndef IUnknown_Release
#define IUnknown_Release(This)	\
    (This)->lpVtbl -> Release(This)
#endif

static qboolean D3D9Shader_CreateProgram (program_t *prog, const char *sname, unsigned int permu, int ver, const char **precompilerconstants, const char *vert, const char *tcs, const char *tes, const char *frag, qboolean silent, vfsfile_t *blobfile)
{
	D3DXMACRO defines[64];
	LPD3DXBUFFER code = NULL, errors = NULL;
	qboolean success = false;

	prog->permu[permu].handle.hlsl.vert = NULL;
	prog->permu[permu].handle.hlsl.frag = NULL;

	if (pD3DXCompileShader)
	{
		int consts;
		for (consts = 0; precompilerconstants[consts]; consts++)
			;
		consts+=2;
		if (consts >= sizeof(defines) / sizeof(defines[0]))
			return success;

		consts = 0;
		defines[consts].Name = NULL; /*shader type*/
		defines[consts].Definition = "1";
		consts++;

		defines[consts].Name = "ENGINE_"DISTRIBUTION;
		defines[consts].Definition = __DATE__;
		consts++;

		for (; *precompilerconstants; precompilerconstants++)
		{
			defines[consts].Name = NULL;
			defines[consts].Definition = NULL;
			consts++;
		}

		defines[consts].Name = NULL;
		defines[consts].Definition = NULL;

		success = true;

		defines[0].Name = "VERTEX_SHADER";
		if (FAILED(pD3DXCompileShader(vert, strlen(vert), defines, NULL, "main", "vs_2_0", 0, &code, &errors, (LPD3DXCONSTANTTABLE*)&prog->permu[permu].handle.hlsl.ctabv)))
			success = false;
		else
		{
			IDirect3DDevice9_CreateVertexShader(pD3DDev9, code->lpVtbl->GetBufferPointer(code), (IDirect3DVertexShader9**)&prog->permu[permu].handle.hlsl.vert);
			code->lpVtbl->Release(code);
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("Error compiling vertex shader %s:\n%s", sname, messages);
			errors->lpVtbl->Release(errors);
		}

		defines[0].Name = "FRAGMENT_SHADER";
		if (FAILED(pD3DXCompileShader(frag, strlen(frag), defines, NULL, "main", "ps_2_0", 0, &code, &errors, (LPD3DXCONSTANTTABLE*)&prog->permu[permu].handle.hlsl.ctabf)))
			success = false;
		else
		{
			IDirect3DDevice9_CreatePixelShader(pD3DDev9, code->lpVtbl->GetBufferPointer(code), (IDirect3DPixelShader9**)&prog->permu[permu].handle.hlsl.frag);
			code->lpVtbl->Release(code);
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("Error compiling pixel shader %s:\n%s", sname, messages);
			errors->lpVtbl->Release(errors);
		}
	}
	return success;
}

static int D3D9Shader_FindUniform_(LPD3DXCONSTANTTABLE ct, char *name)
{
	if (ct)
	{
		UINT dc = 1;
		D3DXCONSTANT_DESC d;
		if (!FAILED(ct->lpVtbl->GetConstantDesc(ct, name, &d, &dc)))
			return d.RegisterIndex;
	}
	return -1;
}

static int D3D9Shader_FindUniform(union programhandle_u *h, int type, char *name)
{
	int offs;

	if (!type || type == 1)
	{
		offs = D3D9Shader_FindUniform_(h->hlsl.ctabv, name);
		if (offs >= 0)
			return offs;
	}
	if (!type || type == 2)
	{
		offs = D3D9Shader_FindUniform_(h->hlsl.ctabf, name);
		if (offs >= 0)
			return offs;
	}

	return -1;
}

static void D3D9Shader_ProgAutoFields(program_t *prog, char **cvarnames, int *cvartypes)
{
	unsigned int i, p;
	qboolean found;
	int uniformloc;
	char tmpname[128];
	cvar_t *cvar;

	prog->numparams = 0;

	prog->nofixedcompat = true;

	/*set cvar uniforms*/
	for (i = 0; cvarnames[i]; i++)
	{
		for (p = 0; cvarnames[i][p] && (unsigned char)cvarnames[i][p] > 32 && p < sizeof(tmpname)-1; p++)
			tmpname[p] = cvarnames[i][p];
		tmpname[p] = 0;
		cvar = Cvar_FindVar(tmpname);
		if (!cvar)
			continue;
		cvar->flags |= CVAR_SHADERSYSTEM;
		for (p = 0; p < PERMUTATIONS; p++)
		{
			if (!prog->permu[p].handle.hlsl.vert || !prog->permu[p].handle.hlsl.frag)
				continue;
			uniformloc = D3D9Shader_FindUniform(&prog->permu[p].handle, 1, va("cvar_%s", tmpname));
			if (uniformloc != -1)
			{
				vec4_t v = {cvar->value, 0, 0, 0};
				IDirect3DDevice9_SetVertexShader(pD3DDev9, prog->permu[p].handle.hlsl.vert);
				IDirect3DDevice9_SetVertexShaderConstantF(pD3DDev9, 0, v, 1);
			}
			uniformloc = D3D9Shader_FindUniform(&prog->permu[p].handle, 2, va("cvar_%s", tmpname));
			if (uniformloc != -1)
			{
				vec4_t v = {cvar->value, 0, 0, 0};
				IDirect3DDevice9_SetPixelShader(pD3DDev9, prog->permu[p].handle.hlsl.frag);
				IDirect3DDevice9_SetPixelShaderConstantF(pD3DDev9, 0, v, 1);
			}
		}
	}
	for (i = 0; shader_unif_names[i].name; i++)
	{
		found = false;
		for (p = 0; p < PERMUTATIONS; p++)
		{
			uniformloc = D3D9Shader_FindUniform(&prog->permu[p].handle, 0, shader_unif_names[i].name);
			if (uniformloc != -1)
				found = true;
			prog->permu[p].parm[prog->numparams] = uniformloc;
		}
		if (found)
		{
			prog->parm[prog->numparams].type = shader_unif_names[i].ptype;
			prog->numparams++;
		}
	}
	/*set texture uniforms*/
	for (p = 0; p < PERMUTATIONS; p++)
	{
		for (i = 0; i < 8; i++)
		{
			uniformloc = D3D9Shader_FindUniform(&prog->permu[p].handle, 2, va("s_t%i", i));
			if (uniformloc != -1)
			{
				int v[4] = {i};
				IDirect3DDevice9_SetPixelShader(pD3DDev9, prog->permu[p].handle.hlsl.frag);
				IDirect3DDevice9_SetPixelShaderConstantI(pD3DDev9, 0, v, 1);
			}
		}
	}
	IDirect3DDevice9_SetVertexShader(pD3DDev9, NULL);
	IDirect3DDevice9_SetPixelShader(pD3DDev9, NULL);
}

void D3D9Shader_DeleteProg(program_t *prog, unsigned int permu)
{
	if (prog->permu[permu].handle.hlsl.vert)
	{
		IDirect3DVertexShader9 *vs = prog->permu[permu].handle.hlsl.vert;
		prog->permu[permu].handle.hlsl.vert = NULL;
		IDirect3DVertexShader9_Release(vs);
	}
	if (prog->permu[permu].handle.hlsl.frag)
	{
		IDirect3DPixelShader9 *fs = prog->permu[permu].handle.hlsl.frag;
		prog->permu[permu].handle.hlsl.frag = NULL;
		IDirect3DPixelShader9_Release(fs);
	}
	if (prog->permu[permu].handle.hlsl.ctabv)
	{
		LPD3DXCONSTANTTABLE vct = prog->permu[permu].handle.hlsl.ctabv;
		prog->permu[permu].handle.hlsl.ctabv = NULL;
		IUnknown_Release(vct);
	}
	if (prog->permu[permu].handle.hlsl.ctabf)
	{
		LPD3DXCONSTANTTABLE fct = prog->permu[permu].handle.hlsl.ctabf;
		prog->permu[permu].handle.hlsl.ctabf = NULL;
		IUnknown_Release(fct);
	}
}

void D3D9Shader_Init(void)
{
	dllfunction_t funcs[] =
	{
		{(void**)&pD3DXCompileShader, "D3DXCompileShader"},
		{NULL,NULL}
	};

	if (!shaderlib)
		shaderlib = Sys_LoadLibrary("d3dx9_32", funcs);
	if (!shaderlib)
		shaderlib = Sys_LoadLibrary("d3dx9_34", funcs);

	memset(&sh_config, 0, sizeof(sh_config));
	sh_config.minver = 9;
	sh_config.maxver = 9;
	sh_config.blobpath = "hlsl/%s.blob";
	sh_config.progpath = shaderlib?"hlsl/%s.hlsl":NULL;
	sh_config.shadernamefmt = "%s_hlsl9";

	sh_config.progs_supported	= !!shaderlib;
	sh_config.progs_required	= false;

	sh_config.pDeleteProg		= D3D9Shader_DeleteProg;
	sh_config.pLoadBlob			= NULL;//D3D9Shader_LoadBlob;
	sh_config.pCreateProgram	= D3D9Shader_CreateProgram;
	sh_config.pProgAutoFields	= D3D9Shader_ProgAutoFields;

	sh_config.texture_non_power_of_two = 0;
	sh_config.tex_env_combine		= 1;
	sh_config.nv_tex_env_combine4	= 1;
	sh_config.env_add				= 1;
}
#endif
