/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// view.c -- player eye positioning

#include "quakedef.h"

#include "winquake.h"
#include "glquake.h"

#include <ctype.h> // for isdigit();

cvar_t ffov = SCVAR("ffov", "0");

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

#ifdef SIDEVIEWS
cvar_t	vsec_enabled[SIDEVIEWS] = {SCVAR("v2_enabled", "2"),	SCVAR("v3_enabled", "0"),	SCVAR("v4_enabled", "0"),	SCVAR("v5_enabled", "0")};
cvar_t	vsec_x[SIDEVIEWS]		= {SCVAR("v2_x", "0"),			SCVAR("v3_x", "0.25"),		SCVAR("v4_x", "0.5"),		SCVAR("v5_x", "0.75")};
cvar_t	vsec_y[SIDEVIEWS]		= {SCVAR("v2_y", "0"),			SCVAR("v3_y", "0"),			SCVAR("v4_y", "0"),			SCVAR("v5_y", "0")};
cvar_t	vsec_scalex[SIDEVIEWS]	= {SCVAR("v2_scalex", "0.25"),	SCVAR("v3_scalex", "0.25"),	SCVAR("v4_scalex", "0.25"),	SCVAR("v5_scalex", "0.25")};
cvar_t	vsec_scaley[SIDEVIEWS]	= {SCVAR("v2_scaley", "0.25"),	SCVAR("v3_scaley", "0.25"),	SCVAR("v4_scaley", "0.25"),	SCVAR("v5_scaley", "0.25")};
cvar_t	vsec_yaw[SIDEVIEWS]		= {SCVAR("v2_yaw", "180"),		SCVAR("v3_yaw", "90"),		SCVAR("v4_yaw", "270"),		SCVAR("v5_yaw", "0")};
#endif

cvar_t	cl_rollspeed = SCVAR("cl_rollspeed", "200");
cvar_t	cl_rollangle = SCVAR("cl_rollangle", "2.0");
cvar_t	v_deathtilt = SCVAR("v_deathtilt", "1");

cvar_t	cl_bob = SCVAR("cl_bob","0.02");
cvar_t	cl_bobcycle = SCVAR("cl_bobcycle","0.6");
cvar_t	cl_bobup = SCVAR("cl_bobup","0.5");

cvar_t	v_kicktime = SCVAR("v_kicktime", "0.5");
cvar_t	v_kickroll = SCVAR("v_kickroll", "0.6");
cvar_t	v_kickpitch = SCVAR("v_kickpitch", "0.6");

cvar_t	v_iyaw_cycle = SCVAR("v_iyaw_cycle", "2");
cvar_t	v_iroll_cycle = SCVAR("v_iroll_cycle", "0.5");
cvar_t	v_ipitch_cycle = SCVAR("v_ipitch_cycle", "1");
cvar_t	v_iyaw_level = SCVAR("v_iyaw_level", "0.3");
cvar_t	v_iroll_level = SCVAR("v_iroll_level", "0.1");
cvar_t	v_ipitch_level = SCVAR("v_ipitch_level", "0.3");
cvar_t	v_idlescale = SCVAR("v_idlescale", "0");

cvar_t	crosshair = SCVARF("crosshair", "1", CVAR_ARCHIVE);
cvar_t	crosshaircolor = SCVARF("crosshaircolor", "255 255 255", CVAR_ARCHIVE);
cvar_t	crosshairsize = SCVARF("crosshairsize", "8", CVAR_ARCHIVE);

cvar_t  cl_crossx = SCVARF("cl_crossx", "0", CVAR_ARCHIVE);
cvar_t  cl_crossy = SCVARF("cl_crossy", "0", CVAR_ARCHIVE);
cvar_t	crosshaircorrect = SCVARF("crosshaircorrect", "0", CVAR_SEMICHEAT);
cvar_t	crosshairimage = SCVAR("crosshairimage", "");
cvar_t	crosshairalpha = SCVAR("crosshairalpha", "1");

cvar_t	gl_cshiftpercent = SCVAR("gl_cshiftpercent", "100");
cvar_t	gl_cshiftenabled = CVARF("gl_polyblend", "1", CVAR_ARCHIVE);

cvar_t	v_bonusflash = SCVAR("v_bonusflash", "1");

cvar_t  v_contentblend = SCVARF("v_contentblend", "1", CVAR_ARCHIVE);
cvar_t	v_damagecshift = SCVAR("v_damagecshift", "1");
cvar_t	v_quadcshift = SCVAR("v_quadcshift", "1");
cvar_t	v_suitcshift = SCVAR("v_suitcshift", "1");
cvar_t	v_ringcshift = SCVAR("v_ringcshift", "1");
cvar_t	v_pentcshift = SCVAR("v_pentcshift", "1");
cvar_t	v_gunkick = SCVAR("v_gunkick", "0");
cvar_t	v_gunkick_q2 = SCVAR("v_gunkick_q2", "1");

cvar_t	v_viewheight = SCVAR("v_viewheight", "0");
cvar_t	v_projectionmode = SCVAR("v_projectionmode", "0");

cvar_t	scr_autoid = SCVAR("scr_autoid", "1");


extern cvar_t cl_chasecam;

player_state_t		*view_message;

/*
===============
V_CalcRoll

===============
*/
float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t	forward, right, up;
	float	sign;
	float	side;
	float	value;

	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);

	value = cl_rollangle.value;

	if (side < cl_rollspeed.value)
		side = side * value / cl_rollspeed.value;
	else
		side = value;

	return side*sign;

}


/*
===============
V_CalcBob

===============
*/
float V_CalcBob (playerview_t *pv, qboolean queryold)
{
	float	cycle;

	if (cl.spectator)
		return 0;

	if (!pv->onground || cl.paused)
	{
		pv->bobcltime = cl.time;
		return pv->bob;		// just use old value
	}

	if (cl_bobcycle.value <= 0)
		return 0;

	pv->bobtime += cl.time - pv->bobcltime;
	pv->bobcltime = cl.time;
	cycle = pv->bobtime - (int)(pv->bobtime/cl_bobcycle.value)*cl_bobcycle.value;
	cycle /= cl_bobcycle.value;
	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI*(cycle-cl_bobup.value)/(1.0 - cl_bobup.value);

// bob is proportional to simulated velocity in the xy plane
// (don't count Z, or jumping messes it up)
//FIXME: gravitydir

	pv->bob = sqrt(pv->simvel[0]*pv->simvel[0] + pv->simvel[1]*pv->simvel[1]) * cl_bob.value;
	pv->bob = pv->bob*0.3 + pv->bob*0.7*sin(cycle);
	if (pv->bob > 4)
		pv->bob = 4;
	else if (pv->bob < -7)
		pv->bob = -7;
	return pv->bob;

}


//=============================================================================


cvar_t	v_centermove = SCVAR("v_centermove", "0.15");
cvar_t	v_centerspeed = SCVAR("v_centerspeed","500");


void V_StartPitchDrift (playerview_t *pv)
{
#if 1
	if (pv->laststop == cl.time)
	{
		return;		// something else is keeping it from drifting
	}
#endif
	if (pv->nodrift || !pv->pitchvel)
	{
		pv->pitchvel = v_centerspeed.value;
		pv->nodrift = false;
		pv->driftmove = 0;
	}
}

void V_StopPitchDrift (playerview_t *pv)
{
	pv->laststop = cl.time;
	pv->nodrift = true;
	pv->pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when
===============
*/
void V_DriftPitch (playerview_t *pv)
{
	float		delta, move;

	if (!pv->onground || cls.demoplayback )
	{
		pv->driftmove = 0;
		pv->pitchvel = 0;
		return;
	}

// don't count small mouse motion
	if (pv->nodrift)
	{
		if (Length(pv->simvel) < 200)
			pv->driftmove = 0;
		else
			pv->driftmove += host_frametime;

		if ( pv->driftmove > v_centermove.value)
		{
			V_StartPitchDrift (pv);
		}
		return;
	}

	delta = 0 - pv->viewangles[PITCH];

	if (!delta)
	{
		pv->pitchvel = 0;
		return;
	}

	move = host_frametime * pv->pitchvel;
	pv->pitchvel += host_frametime * v_centerspeed.value;

//Con_Printf ("move: %f (%f)\n", move, host_frametime);

	if (delta > 0)
	{
		if (move > delta)
		{
			pv->pitchvel = 0;
			move = delta;
		}
		pv->viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			pv->pitchvel = 0;
			move = -delta;
		}
		pv->viewangles[PITCH] -= move;
	}
}





/*
==============================================================================

						PALETTE FLASHES

==============================================================================
*/


cshift_t	cshift_empty = { {130,80,50}, 0 };
cshift_t	cshift_water = { {130,80,50}, 128 };
cshift_t	cshift_slime = { {0,25,5}, 150 };
cshift_t	cshift_lava = { {255,80,0}, 150 };

cshift_t	cshift_server = { {130,80,50}, 0 };

cvar_t		v_gamma = SCVARF("gamma", "0.8", CVAR_ARCHIVE|CVAR_RENDERERCALLBACK);
cvar_t		v_contrast = SCVARF("contrast", "1.3", CVAR_ARCHIVE);
cvar_t		v_brightness = SCVARF("brightness", "0.0", CVAR_ARCHIVE);

qbyte		gammatable[256];	// palette is sent through this


unsigned short		ramps[3][256];
//extern qboolean		gammaworks;
float		sw_blend[4];		// rgba 0.0 - 1.0
float		hw_blend[4];		// rgba 0.0 - 1.0
/*
void BuildGammaTable (float g)
{
	int		i, inf;

	if (g == 1.0)
	{
		for (i=0 ; i<256 ; i++)
			gammatable[i] = i;
		return;
	}

	for (i=0 ; i<256 ; i++)
	{
		inf = 255 * pow ( (i+0.5)/255.5 , g ) + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		gammatable[i] = inf;
	}
}*/
void BuildGammaTable (float g, float c, float b)
{
	int i, inf;

//	g = bound (0.1, g, 3);
//	c = bound (1, c, 3);

	if (g == 1 && c == 1)
	{
		for (i = 0; i < 256; i++)
			gammatable[i] = i;
		return;
	}

	for (i = 0; i < 256; i++)
	{
		//the 0.5s are for rounding.
		inf = 255 * (pow ((i + 0.5) / 255.5 * c, g) + b) + 0.5;
		if (inf < 0)
			inf = 0;
		else if (inf > 255)
			inf = 255;
		gammatable[i] = inf;
	}
}

/*
=================
V_CheckGamma
=================
*/
#if defined(GLQUAKE) || defined(D3DQUAKE)
void GLV_Gamma_Callback(struct cvar_s *var, char *oldvalue)
{
	BuildGammaTable (v_gamma.value, v_contrast.value, v_brightness.value);
	V_UpdatePalette (true);
}
#endif

/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (playerview_t *pv)
{
	int		armor, blood;
	vec3_t	from;
	int		i;
	vec3_t	forward, right, up;
	float	side;
	float	count;

	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord ();

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

#ifdef ANDROID
	Sys_Vibrate(count);
#endif

	if (v_damagecshift.value >= 0)
		count *= v_damagecshift.value;

	pv->faceanimtime = cl.time + 0.2;		// but sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	if (armor > blood)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

//
// calculate view angle kicks
//
	VectorSubtract (from, pv->simorg, from);
	VectorNormalize (from);

	AngleVectors (pv->simangles, forward, right, up);

	side = DotProduct (from, right);
	pv->v_dmg_roll = count*side*v_kickroll.value;

	side = DotProduct (from, forward);
	pv->v_dmg_pitch = count*side*v_kickpitch.value;

	pv->v_dmg_time = v_kicktime.value;
}


/*
==================
V_cshift_f
==================
*/
void V_cshift_f (void)
{
	int r, g, b, p;

	r = g = b = p = 0;

	if (Cmd_Argc() >= 5)
	{
		r = atoi(Cmd_Argv(1));
		g = atoi(Cmd_Argv(2));
		b = atoi(Cmd_Argv(3));
		p = atoi(Cmd_Argv(4));
	}

	if (Cmd_FromGamecode())
	{
		if (Cmd_Argc() >= 5)
		{
			qboolean term = false;
			int i;
			char *c = Cmd_Argv(4);

			// malice jumbles commands into a v_cshift so this attempts to fix
			while (isdigit(*c) || *c == '.')
				c++;

			if (*c)
			{
				Cbuf_AddText(c, RESTRICT_SERVER);
				term = true;
			}
			for (i = 5; i < Cmd_Argc(); i++)
			{
				Cbuf_AddText(" ", RESTRICT_SERVER);
				Cbuf_AddText(Cmd_Argv(i), RESTRICT_SERVER);
				term = true;
			}
			if (term)
				Cbuf_AddText("\n", RESTRICT_SERVER);
		}
		else if (Cmd_Argc() > 1)
			Con_DPrintf("broken v_cshift from gamecode\n");

		// ensure we always clear out or set for nehahra
		cl.cshifts[CSHIFT_SERVER].destcolor[0] = r;
		cl.cshifts[CSHIFT_SERVER].destcolor[1] = g;
		cl.cshifts[CSHIFT_SERVER].destcolor[2] = b;
		cl.cshifts[CSHIFT_SERVER].percent = p;
		return;
	}

	if (Cmd_Argc() != 5 && Cmd_Argc() != 1)
	{
		Con_Printf("v_cshift: v_cshift <r> <g> <b> <alpha>\n");
		return;
	}

	cshift_empty.destcolor[0] = r;
	cshift_empty.destcolor[1] = g;
	cshift_empty.destcolor[2] = b;
	cshift_empty.percent = p;
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
	if (v_bonusflash.value || !Cmd_FromGamecode())
	{
		cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
		cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
		cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
		cl.cshifts[CSHIFT_BONUS].percent = 50*v_bonusflash.value;
	}
}
void V_DarkFlash_f (void)
{
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 0;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 0;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 0;
	cl.cshifts[CSHIFT_BONUS].percent = 255;
}
void V_WhiteFlash_f (void)
{
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 255;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 255;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 255;
	cl.cshifts[CSHIFT_BONUS].percent = 255;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift

FIXME: Uses Q1 contents
=============
*/
void V_SetContentsColor (int contents)
{
	int i;
	if (contents & FTECONTENTS_LAVA)
		cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
	else if (contents & (FTECONTENTS_SLIME | FTECONTENTS_SOLID))
		cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
	else if (contents & FTECONTENTS_WATER)
		cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	else
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;

	cl.cshifts[CSHIFT_CONTENTS].percent *= v_contentblend.value;

	if (cl.cshifts[CSHIFT_CONTENTS].percent)
	{	//bound contents so it can't go negative
		if (cl.cshifts[CSHIFT_CONTENTS].percent < 0)
			cl.cshifts[CSHIFT_CONTENTS].percent = 0;

		for (i = 0; i < 3; i++)
			if (cl.cshifts[CSHIFT_CONTENTS].destcolor[0] < 0)
				cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 0;
	}
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift (void)
{
	int im = 0;
	int s;

	//we only have one palette, so combine the mask

	for (s = 0; s < cl.splitclients; s++)
		im |= cl.playerview[s].stats[STAT_ITEMS];

	if (im & IT_QUAD)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30*v_quadcshift.value;
	}
	else if (im & IT_SUIT)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20*v_suitcshift.value;
	}
	else if (im & IT_INVISIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 100*v_ringcshift.value;
	}
	else if (im & IT_INVULNERABILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 30*v_pentcshift.value;
	}
	else
		cl.cshifts[CSHIFT_POWERUP].percent = 0;

	if (cl.cshifts[CSHIFT_POWERUP].percent<0)
		cl.cshifts[CSHIFT_POWERUP].percent=0;
}


/*
=============
V_CalcBlend
=============
*/
void V_CalcBlend (float *hw_blend)
{
	extern qboolean r2d_noshadergamma;
	float	a2;
	int		j;
	float *blend;

	memset(hw_blend, 0, sizeof(float)*4);
	memset(sw_blend, 0, sizeof(float)*4);

	//don't apply it to the server, we'll blend the two later if the user has no hardware gamma (if they do have it, we use just the server specified value) This way we avoid winnt users having a cheat with flashbangs and stuff.
	for (j=0 ; j<NUM_CSHIFTS ; j++)
	{
		if (j != CSHIFT_SERVER)
		{
			if (!gl_cshiftpercent.value || !gl_cshiftenabled.ival)
				continue;

			a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) / 255.0;
		}
		else
		{
			a2 = cl.cshifts[j].percent / 255.0;	//don't allow modification of this one.
		}

		if (!a2)
			continue;

		if (j == CSHIFT_SERVER)
		{
			/*server blend always goes into sw, ALWAYS*/
			blend = sw_blend;
		}
		else
		{
			if (j == CSHIFT_BONUS || j == CSHIFT_DAMAGE || gl_nohwblend.ival || r2d_noshadergamma)
				blend = sw_blend;
			else	//powerup or contents?
				blend = hw_blend;
		}

		blend[3] = blend[3] + a2*(1-blend[3]);
		a2 = a2/blend[3];
		blend[0] = blend[0]*(1-a2) + cl.cshifts[j].destcolor[0]*a2/255.0;
		blend[1] = blend[1]*(1-a2) + cl.cshifts[j].destcolor[1]*a2/255.0;
		blend[2] = blend[2]*(1-a2) + cl.cshifts[j].destcolor[2]*a2/255.0;
	}

	if (hw_blend[3] > 1)
		hw_blend[3] = 1;
	if (hw_blend[3] < 0)
		hw_blend[3] = 0;
	if (sw_blend[3] > 1)
		sw_blend[3] = 1;
	if (sw_blend[3] < 0)
		sw_blend[3] = 0;
}

/*
=============
V_UpdatePalette
=============
*/
void V_UpdatePalette (qboolean force)
{
	extern	qboolean r2d_noshadergamma;
	int		i;
	float	newhw_blend[4];
	int		ir, ig, ib;
	float	ftime;
	qboolean applied;
	static double oldtime;
	RSpeedMark();

	ftime = cl.time - oldtime;
	oldtime = cl.time;
	if (ftime < 0)
		ftime = 0;

	V_CalcPowerupCshift ();

// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= ftime*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= ftime*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	V_CalcBlend(newhw_blend);

	if (hw_blend[0] != newhw_blend[0] || hw_blend[1] != newhw_blend[1] || hw_blend[2] != newhw_blend[2] || hw_blend[3] != newhw_blend[3] || force)
	{
		float r,g,b,a;
		Vector4Copy(newhw_blend, hw_blend);

		a = hw_blend[3];
		r = 255*hw_blend[0]*a;
		g = 255*hw_blend[1]*a;
		b = 255*hw_blend[2]*a;

		a = 1-a;
		for (i=0 ; i<256 ; i++)
		{
			ir = i*a + r;
			ig = i*a + g;
			ib = i*a + b;
			if (ir > 255)
				ir = 255;
			if (ig > 255)
				ig = 255;
			if (ib > 255)
				ib = 255;

			ramps[0][i] = gammatable[ir]<<8;
			ramps[1][i] = gammatable[ig]<<8;
			ramps[2][i] = gammatable[ib]<<8;
		}

		applied = rf->VID_ApplyGammaRamps ((unsigned short*)ramps);
		if (!applied && r2d_noshadergamma)
			rf->VID_ApplyGammaRamps (NULL);
		r2d_noshadergamma = applied;
	}

	RSpeedEnd(RSPEED_PALETTEFLASHES);
}

/*
=============
V_UpdatePalette
=============
*/

void V_ClearCShifts (void)
{
	int i;

	for (i = 0; i < NUM_CSHIFTS; i++)
		cl.cshifts[i].percent = 0;
}

/*
==============================================================================

						VIEW RENDERING

==============================================================================
*/

float angledelta (float a)
{
	a = anglemod(a);
	if (a > 180)
		a -= 360;
	return a;
}

/*
==================
CalcGunAngle
==================
*/
void V_CalcGunPositionAngle (playerview_t *pv, float bob)
{
	float	yaw, pitch, move;
	static float oldyaw = 0;
	static float oldpitch = 0;
	int i;

	yaw = r_refdef.viewangles[YAW];
	pitch = -r_refdef.viewangles[PITCH];

	yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;
	if (yaw > 10)
		yaw = 10;
	if (yaw < -10)
		yaw = -10;
	pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;
	if (pitch > 10)
		pitch = 10;
	if (pitch < -10)
		pitch = -10;
	move = host_frametime*20;
	if (yaw > oldyaw)
	{
		if (oldyaw + move < yaw)
			yaw = oldyaw + move;
	}
	else
	{
		if (oldyaw - move > yaw)
			yaw = oldyaw - move;
	}

	if (pitch > oldpitch)
	{
		if (oldpitch + move < pitch)
			pitch = oldpitch + move;
	}
	else
	{
		if (oldpitch - move > pitch)
			pitch = oldpitch - move;
	}

	oldyaw = yaw;
	oldpitch = pitch;

	pv->viewent.angles[YAW] = r_refdef.viewangles[YAW] + yaw;
	pv->viewent.angles[PITCH] = r_refdef.viewangles[PITCH] + pitch;

	pv->viewent.angles[YAW] = r_refdef.viewangles[YAW];
	pv->viewent.angles[PITCH] = r_refdef.viewangles[PITCH];
	pv->viewent.angles[ROLL] = r_refdef.viewangles[ROLL];

	AngleVectors(pv->viewent.angles, pv->viewent.axis[0], pv->viewent.axis[1], pv->viewent.axis[2]);
	VectorInverse(pv->viewent.axis[1]);
	pv->viewent.angles[PITCH]*=-1;



	VectorCopy (r_refdef.vieworg, pv->viewent.origin);
	for (i=0 ; i<3 ; i++)
	{
		pv->viewent.origin[i] += pv->viewent.axis[0][i]*bob*0.4;
//		pv->viewent.origin[i] += pv->viewent.axis[1][i]*sin(cl.time*5.5342452354235)*0.1;
//		pv->viewent.origin[i] += pv->viewent.axis[2][i]*bob*0.8;
	}

// fudge position around to keep amount of weapon visible
// roughly equal with different FOV
	if (scr_viewsize.value == 110)
		pv->viewent.origin[2] += 1;
	else if (scr_viewsize.value == 100)
		pv->viewent.origin[2] += 2;
	else if (scr_viewsize.value == 90)
		pv->viewent.origin[2] += 1;
	else if (scr_viewsize.value == 80)
		pv->viewent.origin[2] += 0.5;
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (int pnum)
{
// absolutely bound refresh reletive to entity clipping hull
// so the view can never be inside a solid wall

	if (r_refdef.vieworg[0] < cl.playerview[pnum].simorg[0] - 14)
		r_refdef.vieworg[0] = cl.playerview[pnum].simorg[0] - 14;
	else if (r_refdef.vieworg[0] > cl.playerview[pnum].simorg[0] + 14)
		r_refdef.vieworg[0] = cl.playerview[pnum].simorg[0] + 14;
	if (r_refdef.vieworg[1] < cl.playerview[pnum].simorg[1] - 14)
		r_refdef.vieworg[1] = cl.playerview[pnum].simorg[1] - 14;
	else if (r_refdef.vieworg[1] > cl.playerview[pnum].simorg[1] + 14)
		r_refdef.vieworg[1] = cl.playerview[pnum].simorg[1] + 14;
	if (r_refdef.vieworg[2] < cl.playerview[pnum].simorg[2] - 22)
		r_refdef.vieworg[2] = cl.playerview[pnum].simorg[2] - 22;
	else if (r_refdef.vieworg[2] > cl.playerview[pnum].simorg[2] + 30)
		r_refdef.vieworg[2] = cl.playerview[pnum].simorg[2] + 30;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (playerview_t *pv)
{
	//defaults: for use if idlescale is locked and the var isn't.
	float yaw_cycle		= 2;
	float roll_cycle	= 0.5;
	float pitch_cycle	= 1;
	float yaw_level		= 0.3;
	float roll_level	= 0.1;
	float pitch_level	= 0.3;

	if (v_iyaw_cycle.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		yaw_cycle = v_iyaw_cycle.value;
	if (v_iroll_cycle.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		roll_cycle = v_iroll_cycle.value;
	if (v_ipitch_cycle.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		pitch_cycle = v_ipitch_cycle.value;

	if (v_iyaw_level.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		yaw_level = v_iyaw_level.value;
	if (v_iroll_level.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		roll_level = v_iroll_level.value;
	if (v_ipitch_level.flags & CVAR_SERVEROVERRIDE || !(v_idlescale.flags & CVAR_SERVEROVERRIDE))
		pitch_level = v_ipitch_level.value;

	r_refdef.viewangles[ROLL] += v_idlescale.value * sin(cl.time*roll_cycle) * roll_level;
	r_refdef.viewangles[PITCH] += v_idlescale.value * sin(cl.time*pitch_cycle) * pitch_level;
	r_refdef.viewangles[YAW] += v_idlescale.value * sin(cl.time*yaw_cycle) * yaw_level;

	pv->viewent.angles[ROLL] -= v_idlescale.value * sin(cl.time*roll_cycle) * roll_level;
	pv->viewent.angles[PITCH] -= v_idlescale.value * sin(cl.time*pitch_cycle) * pitch_level;
	pv->viewent.angles[YAW] -= v_idlescale.value * sin(cl.time*yaw_cycle) * yaw_level;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (playerview_t *pv)
{
	float		side;
	float	adjspeed;

	side = V_CalcRoll (pv->simangles, pv->simvel);

	adjspeed = fabs(cl_rollangle.value);
	if (adjspeed<1)
		adjspeed=1;
	if (adjspeed>45)
		adjspeed = 45;
	adjspeed*=20;
	if (side > pv->rollangle)
	{
		pv->rollangle += host_frametime * adjspeed;
		if (pv->rollangle > side)
			pv->rollangle = side;
	}
	else if (side < pv->rollangle)
	{
		pv->rollangle -= host_frametime * adjspeed;
		if (pv->rollangle < side)
			pv->rollangle = side;
	}
	r_refdef.viewangles[ROLL] += pv->rollangle;

	if (pv->v_dmg_time > 0)
	{
		r_refdef.viewangles[ROLL] += pv->v_dmg_time/v_kicktime.value*pv->v_dmg_roll;
		r_refdef.viewangles[PITCH] += pv->v_dmg_time/v_kicktime.value*pv->v_dmg_pitch;
		pv->v_dmg_time -= host_frametime;
	}

}


/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef (playerview_t *pv)
{
	entity_t	*view;
	float		old;

// view is the weapon model
	view = &pv->viewent;

	VectorCopy (pv->simorg, r_refdef.vieworg);
	VectorCopy (pv->simangles, r_refdef.viewangles);
	view->model = NULL;

// always idle in intermission
	old = v_idlescale.value;
	v_idlescale.value = 1;
	V_AddIdle (pv);
	v_idlescale.value = old;
}

float CalcFov (float fov_x, float width, float height)
{
    float   a;
    float   x;

    if (fov_x < 1 || fov_x > 179)
            Sys_Error ("Bad fov: %f", fov_x);

	x = fov_x/360*M_PI;
	x = tan(x);
    x = width/x;

    a = atan (height/x);

    a = a*360/M_PI;

    return a;
}
void V_ApplyAFov(void)
{
	//explicit fov overrides aproximate fov.
	//aproximate fov is our regular fov value. explicit is settable by gamecode for weird aspect ratios
	if (!r_refdef.fov_x || !r_refdef.fov_y)
	{
		extern cvar_t r_stereo_method, r_stereo_separation;
		float ws;

		float afov = r_refdef.afov;
		if (!afov)	//make sure its sensible.
			afov = scr_fov.value;
		if (r_refdef.playerview->stats[STAT_VIEWZOOM])
			afov *= r_refdef.playerview->stats[STAT_VIEWZOOM]/255.0f;

		ws = 1;
		if (r_stereo_method.ival == 5 && r_stereo_separation.value)
			ws = 0.5;

		//attempt to retain a classic fov
		if (ws*r_refdef.vrect.width < (r_refdef.vrect.height*640)/432)
		{
			r_refdef.fov_y = CalcFov(afov, (ws*r_refdef.vrect.width*vid.pixelwidth)/vid.width, (r_refdef.vrect.height*vid.pixelheight)/vid.height);
			r_refdef.fov_x = afov;//CalcFov(r_refdef.fov_y, 432, 640);
		}
		else
		{
			r_refdef.fov_y = CalcFov(afov, 640, 432);
			r_refdef.fov_x = CalcFov(r_refdef.fov_y, r_refdef.vrect.height, r_refdef.vrect.width*ws);
		}
	}
}
/*
=================
v_ApplyRefdef

called to apply any dirty refdef bits and recalculates pending data.
=================
*/
void V_ApplyRefdef (void)
{
	float           size;
	int             h;
	qboolean		full = false;

// force the status bar to redraw
	Sbar_Changed ();

//========================================

	r_refdef.flags = 0;

// intermission is always full screen
	if (cl.intermission || !r_refdef.drawsbar)
		size = 120;
	else
		size = scr_viewsize.value;

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)	//q2 never has a hud.
		sb_lines = 0;
	else
#endif
	     if (size >= 120)
		sb_lines = 0;           // no status bar at all
	else if (size >= 110)
		sb_lines = 24;          // no inventory
	else
		sb_lines = 24+16+8;

	if (scr_viewsize.value >= 100.0 || scr_chatmode)
	{
		full = true;
		size = 100.0;
	}
	else
		size = scr_viewsize.value;

	if (cl.intermission || !r_refdef.drawsbar)
	{
		full = true;
		size = 100.0;
		sb_lines = 0;
	}
	size /= 100.0;

	if (cl_sbar.value!=1 && full)
		h = r_refdef.grect.height;
	else
		h = r_refdef.grect.height - sb_lines;
	if (h < 0)
		h = 0;

	r_refdef.vrect.width = r_refdef.grect.width * size;
	if (r_refdef.vrect.width < 96)
		r_refdef.vrect.width = 96;      // min for icons

	r_refdef.vrect.height = r_refdef.grect.height * size;
	if (cl_sbar.value==1 || !full)
	{
  		if (r_refdef.vrect.height > r_refdef.grect.height - sb_lines)
  			r_refdef.vrect.height = r_refdef.grect.height - sb_lines;
	}
	else if (r_refdef.vrect.height > r_refdef.grect.height)
		r_refdef.vrect.height = r_refdef.grect.height;
	if (r_refdef.vrect.height < 0)
		r_refdef.vrect.height = 0;

	r_refdef.vrect.x = (r_refdef.grect.width - r_refdef.vrect.width)/2;
	if (full)
		r_refdef.vrect.y = 0;
	else
		r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;

	if (scr_chatmode)
	{
		if (scr_chatmode != 2)
			r_refdef.vrect.height = r_refdef.vrect.y=r_refdef.grect.height/2;
		r_refdef.vrect.width = r_refdef.vrect.x=r_refdef.grect.width/2;
		if (r_refdef.vrect.width<320 || r_refdef.vrect.height<200)	//disable hud if too small
			sb_lines=0;
	}
	r_refdef.vrect.x += r_refdef.grect.x;
	r_refdef.vrect.y += r_refdef.grect.y;

	if (r_refdef.dirty & RDFD_FOV)
		V_ApplyAFov();

	r_refdef.dirty = 0;
}

//if the view entities differ, removes all externalmodel flags except for adding it to the new entity, and removes weaponmodels.
void CL_EditExternalModels(int newviewentity)
{
	int i;
	for (i = 0; i < cl_numvisedicts; )
	{
		if (cl_visedicts[i].keynum == newviewentity && newviewentity)
			cl_visedicts[i].flags |= Q2RF_EXTERNALMODEL;
		else
			cl_visedicts[i].flags &= ~Q2RF_EXTERNALMODEL;

		if (cl_visedicts[i].flags & Q2RF_WEAPONMODEL)
		{
			memmove(&cl_visedicts[i], &cl_visedicts[i+1], sizeof(*cl_visedicts) * (cl_numvisedicts-(i+1)));
			cl_numvisedicts--;
		}
		else
			i++;
	}
}

/*
clears the refdef to defaults.
*/
void V_ClearRefdef(playerview_t *pv)
{
	r_refdef.playerview = pv;
	r_refdef.dirty = ~0;

	r_refdef.grect.x = 0;
	r_refdef.grect.y = 0;
	r_refdef.grect.width = vid.width;
	r_refdef.grect.height = vid.height;

	r_refdef.afov = scr_fov.value;	//will have a better value applied if fov is bad. this allows setting.
	r_refdef.fov_x = 0;
	r_refdef.fov_y = 0;

	r_refdef.drawsbar = !cl.intermission;
}

/*
==================
V_CalcRefdef

==================
*/
void V_CalcRefdef (playerview_t *pv)
{
	entity_t	*view;
	int			i;
	float		bob;
	float		viewheight;
	r_refdef.playerview = pv;

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
		return;
#endif

	VectorCopy(cl.fog_colour, r_refdef.gfog_rgbd);
	r_refdef.gfog_rgbd[3] = cl.fog_density / 64;

// view is the weapon model (only visible from inside body)
	view = &pv->viewent;

	if (v_viewheight.value < -7)
		bob=-7;
	else if (v_viewheight.value > 4)
		bob=4;
	else if (v_viewheight.value)
		bob=v_viewheight.value;
	else
		bob = V_CalcBob (pv, false);

// refresh position from simulated origin
	VectorCopy (pv->simorg, r_refdef.vieworg);

	r_refdef.useperspective = true;

// never let it sit exactly on a node line, because a water plane can
// dissapear when viewed with the eye exactly on it.
// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
	r_refdef.vieworg[0] += 1.0/16;
	r_refdef.vieworg[1] += 1.0/16;
	r_refdef.vieworg[2] += 1.0/16;

	if (pv->fixangle)
	{
		if (pv->oldfixangle)
		{
			float frac, move;
			if (cl.gametime <= cl.oldgametime)
				frac = 1;
			else
			{
				frac = (realtime - cl.gametimemark) / (cl.gametime - cl.oldgametime);
				frac = bound(0, frac, 1);
			}
			for (i = 0; i < 3; i++)
			{
				move = pv->fixangles[i] - pv->oldfixangles[i];
				if (move >= 180)
					move -= 360;
				if (move <= -180)
					move += 360;
				r_refdef.viewangles[i] = pv->oldfixangles[i] + frac * move;
			}
		}
		else
		{
			VectorCopy (pv->fixangles, r_refdef.viewangles);
		}
	}
	else
	{
		VectorCopy (pv->simangles, r_refdef.viewangles);
	}
	V_CalcViewRoll (pv);
	V_AddIdle (pv);

	viewheight = pv->viewheight;
	if (viewheight == DEFAULT_VIEWHEIGHT)
	{
		if (view_message && view_message->flags & PF_GIB)
			viewheight = 8;	// gib view height
		else if (view_message && view_message->flags & PF_DEAD)
			viewheight = 16;	// corpse view height
	}

	viewheight += pv->crouch;

	if (pv->stats[STAT_HEALTH] < 0 && pv->cam_spec_track >= 0 && v_deathtilt.value)		// PF_GIB will also set PF_DEAD
	{
		if (!cl.spectator || !cl_chasecam.ival)
			r_refdef.viewangles[ROLL] = 80*v_deathtilt.value;	// dead view angle
	}
	else
	{
		// v_viewheight only affects the view if the player is alive
		viewheight += bob;
	}

	VectorMA(r_refdef.vieworg, -viewheight, pv->gravitydir, r_refdef.vieworg);

// set up gun position
	V_CalcGunPositionAngle (pv, bob);

	if (pv->statsf[STAT_HEALTH] <= 0 || (unsigned int)pv->stats[STAT_WEAPON] >= MAX_MODELS)
 		view->model = NULL;
 	else
		view->model = cl.model_precache[pv->stats[STAT_WEAPON]];
#ifdef HLCLIENT
	if (!CLHL_AnimateViewEntity(view))
#endif
		view->framestate.g[FS_REG].frame[0] = pv->stats[STAT_WEAPONFRAME];

// set up the refresh position
	if (v_gunkick.value)
		r_refdef.viewangles[PITCH] += pv->punchangle*v_gunkick.value;

	r_refdef.time = realtime;

// smooth out stair step ups


	{
		extern model_t *loadmodel;
		loadmodel = cl.worldmodel;
	}
}

/*
=============
DropPunchAngle
=============
*/
void DropPunchAngle (playerview_t *pv)
{
	if (pv->punchangle < 0)
	{
		pv->punchangle += 10*host_frametime;
		if (pv->punchangle > 0)
			pv->punchangle = 0;
	}
	else
	{
		pv->punchangle -= 10*host_frametime;
		if (pv->punchangle < 0)
			pv->punchangle = 0;
	}
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
extern vrect_t scr_vrect;

qboolean r_secondaryview;
#ifdef SIDEVIEWS

#ifdef PEXT_VIEW2
entity_t *CL_EntityNum(int num)
{
	int i;
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		if (cl_visedicts[i].keynum == num)
			return &cl_visedicts[i];
	}
	return NULL;
}
#endif
#endif

float CalcFov (float fov_x, float width, float height);
void SCR_VRectForPlayer(vrect_t *vrect, int pnum)
{
#if MAX_SPLITS > 4
#pragma warning "Please change this function to cope with the new MAX_SPLITS value"
#endif
	switch(cl.splitclients)
	{
	case 1:
		vrect->width = vid.width;
		vrect->height = vid.height;
		vrect->x = 0;
		vrect->y = 0;

		if (scr_chatmode == 2)
		{
			vrect->height/=2;
			vrect->y += vrect->height;
		}
		break;

	case 2:	//horizontal bands
	case 3:
#ifdef GLQUAKE
		if (qrenderer == QR_OPENGL && vid.rotpixelwidth > vid.rotpixelheight * 2
			&& ffov.value >= 0 /*panoramic view always stacks player views*/
			)
		{	//over twice as wide as high, assume dual moniter, horizontal.
			vrect->width = vid.width/cl.splitclients;
			vrect->height = vid.height;
			vrect->x = 0 + vrect->width*pnum;
			vrect->y = 0;
		}
		else
#endif
		{
			//stack them vertically
			vrect->width = vid.width;
			vrect->height = vid.height/cl.splitclients;
			vrect->x = 0;
			vrect->y = 0 + vrect->height*pnum;
		}

		break;

	case 4:	//4 squares
		vrect->width = vid.width/2;
		vrect->height = vid.height/2;
		vrect->x = (pnum&1) * vrect->width;
		vrect->y = (pnum&2)/2 * vrect->height;
		break;

	default:
		Sys_Error("cl.splitclients is invalid.");
	}
}

void Draw_ExpandedString(int x, int y, conchar_t *str);
extern vec3_t nametagorg[MAX_CLIENTS];
extern qboolean nametagseen[MAX_CLIENTS];
void R_DrawNameTags(void)
{
	conchar_t buffer[256];
	int i;
	int len;
	vec3_t center;
	vec3_t tagcenter;

	if (!cl.spectator && !cls.demoplayback)
		return;
	if (!scr_autoid.ival)
		return;
	if (cls.state != ca_active || !cl.validsequence)
		return;

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		GL_Set2D(false);
	}
#endif

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!nametagseen[i])
			continue;
		if (i == r_refdef.playerview->playernum)
			continue;	// Don't draw tag for the local player
		if (cl.players[i].spectator)
			continue;
		if (i == Cam_TrackNum(r_refdef.playerview))
			continue;

		if (TP_IsPlayerVisible(nametagorg[i]))
		{
			VectorCopy(nametagorg[i], tagcenter);
			tagcenter[2] += 32;
			if (!Matrix4x4_CM_Project(tagcenter, center, r_refdef.viewangles, r_refdef.vieworg, r_refdef.fov_x, r_refdef.fov_y))
				continue;

			len = COM_ParseFunString(CON_WHITEMASK, cl.players[i].name, buffer, sizeof(buffer), false) - buffer;
			Draw_ExpandedString(center[0]*r_refdef.vrect.width+r_refdef.vrect.x - len*4, (1-center[1])*r_refdef.vrect.height+r_refdef.vrect.y, buffer);
		}
	}
}

void R2D_PolyBlend (void);
void V_RenderPlayerViews(playerview_t *pv)
{
	int oldnuments;
	int oldstris;
#ifdef SIDEVIEWS
	int viewnum;
#endif
//	cl.simangles[plnum][ROLL] = 0;	// FIXME @@@

	DropPunchAngle (pv);

	Cam_SelfTrack(pv);
	if (cl.intermission)
	{	// intermission / finale rendering
		V_CalcIntermissionRefdef (pv);
	}
	else
	{
		V_DriftPitch (pv);
		V_CalcRefdef (pv);
	}
	V_ApplyRefdef();

	oldnuments = cl_numvisedicts;
	oldstris = cl_numstris;
	CL_LinkViewModel ();

	R_RenderView ();
	R2D_PolyBlend ();
	R_DrawNameTags();

	cl_numvisedicts = oldnuments;
	cl_numstris = oldstris;

	if (scr_chatmode == 2)
	{
		vec3_t dir;

		r_refdef.vrect.y -= r_refdef.vrect.height;
		r_secondaryview = 2;

		VectorSubtract(r_refdef.vieworg, pv->cam_desired_position, dir);
		VectorAngles(dir, NULL, r_refdef.viewangles);
		r_refdef.viewangles[0] = -r_refdef.viewangles[0];	//flip the pitch. :(


		VectorCopy(pv->cam_desired_position, r_refdef.vieworg);
		R_RenderView ();
	}


#ifdef SIDEVIEWS
/*	//adjust main view height to strip off the rearviews at the top
	if (vsecwidth >= 1)
	{
		r_refdef.vrect.y -= vsecheight;
		r_refdef.vrect.height += vsecheight;
	}
*/
	for (viewnum = 0; viewnum < SIDEVIEWS; viewnum++)
	if (vsec_scalex[viewnum].value>0&&vsec_scaley[viewnum].value>0
		&& ((vsec_enabled[viewnum].value && vsec_enabled[viewnum].value != 2 && cls.allow_rearview) 	//rearview if v2_enabled = 1 and not 2
		|| (vsec_enabled[viewnum].value && pv->stats[STAT_VIEW2]&&viewnum==0)))			//v2 enabled if v2_enabled is non-zero
	{
		vrect_t oldrect;
		vec3_t oldangles;
		vec3_t oldposition;
//		int oldviewent;
		struct entity_s *e;
		float ofx;
		float ofy;

		r_secondaryview = true;

		if (vsec_x[viewnum].value < 0)
			vsec_x[viewnum].value = 0;
		if (vsec_y[viewnum].value < 0)
			vsec_y[viewnum].value = 0;

		if (vsec_scalex[viewnum].value+vsec_x[viewnum].value > 1)
			continue;
		if (vsec_scaley[viewnum].value+vsec_y[viewnum].value > 1)
			continue;

		oldrect = r_refdef.vrect;
		memcpy(oldangles, r_refdef.viewangles, sizeof(vec3_t));
		memcpy(oldposition, r_refdef.vieworg, sizeof(vec3_t));
		ofx = r_refdef.fov_x;
		ofy = r_refdef.fov_y;

		r_refdef.vrect.x += r_refdef.vrect.width*vsec_x[viewnum].value;
		r_refdef.vrect.y += r_refdef.vrect.height*vsec_y[viewnum].value;
		r_refdef.vrect.width *= vsec_scalex[viewnum].value;
		r_refdef.vrect.height *= vsec_scaley[viewnum].value;
#ifdef PEXT_VIEW2
			//secondary view entity.
		e=NULL;
		if (viewnum==0&&pv->stats[STAT_VIEW2])
		{
			e = CL_EntityNum (pv->stats[STAT_VIEW2]);
		}
		if (e)
		{
			memcpy(r_refdef.viewangles, e->angles, sizeof(vec3_t));
			memcpy(r_refdef.vieworg, e->origin, sizeof(vec3_t));
//			cl.viewentity = cl.viewentity2;

			r_refdef.vieworg[0]=r_refdef.vieworg[0];//*s+(1-s)*e->lerporigin[0];
			r_refdef.vieworg[1]=r_refdef.vieworg[1];//*s+(1-s)*e->lerporigin[1];
			r_refdef.vieworg[2]=r_refdef.vieworg[2];//*s+(1-s)*e->lerporigin[2];

			r_refdef.viewangles[0]=e->angles[0];//*s+(1-s)*e->msg_angles[1][0];
			r_refdef.viewangles[1]=e->angles[1];//*s+(1-s)*e->msg_angles[1][1];
			r_refdef.viewangles[2]=e->angles[2];//*s+(1-s)*e->msg_angles[1][2];
			r_refdef.viewangles[PITCH] *= -1;

			if (e->keynum >= 1 && e->keynum <= cl.allocated_client_slots)
			{
				r_refdef.viewangles[PITCH] *= 3;
				r_refdef.vieworg[2] += pv->statsf[STAT_VIEWHEIGHT];
			}


			CL_EditExternalModels(e->keynum);

			R_RenderView ();
//				r_framecount = old_framecount;
		}
		else
#endif
		{
			//rotate the view, keeping pitch and roll.
			r_refdef.viewangles[YAW] += vsec_yaw[viewnum].value;
			r_refdef.viewangles[ROLL] += sin(vsec_yaw[viewnum].value / 180 * 3.14) * r_refdef.viewangles[PITCH];
			r_refdef.viewangles[PITCH] *= -cos((vsec_yaw[viewnum].value / 180 * 3.14)+3.14);
			if (vsec_enabled[viewnum].value!=2)
			{
				CL_EditExternalModels(0);
				R_RenderView ();
			}
		}

		r_refdef.vrect = oldrect;
		memcpy(r_refdef.viewangles, oldangles, sizeof(vec3_t));
		memcpy(r_refdef.vieworg, oldposition, sizeof(vec3_t));
		r_refdef.fov_x = ofx;
		r_refdef.fov_y = ofy;
	}
#endif
	r_refdef.externalview = false;
}

void V_RenderView (void)
{
	int viewnum;

	R_LessenStains();

	if (cls.state != ca_active)
		return;

	R_PushDlights ();

	r_secondaryview = 0;
	for (viewnum = 0; viewnum < cl.splitclients; viewnum++)
	{
		V_ClearRefdef(&cl.playerview[viewnum]);
		if (viewnum)
		{
			//should be enough to just hack a few things.
			CL_EditExternalModels(cl.playerview[viewnum].viewentity);
		}
		else
		{
			if (r_worldentity.model)
			{
				RSpeedMark();

				CL_AllowIndependantSendCmd(false);

				CL_TransitionEntities();

				CL_PredictMove ();

				// build a refresh entity list
				CL_EmitEntities ();

				CL_AllowIndependantSendCmd(true);

				RSpeedEnd(RSPEED_LINKENTITIES);
			}
		}
		SCR_VRectForPlayer(&r_refdef.grect, viewnum);
		V_RenderPlayerViews(r_refdef.playerview);

		GL_Set2D (false);
		Plug_SBar(r_refdef.playerview);
		SCR_TileClear ();
	}
	r_refdef.playerview = NULL;
}

//============================================================================

/*
=============
V_Init
=============
*/
void V_Init (void)
{
#define VIEWVARS "View variables"
#ifdef SIDEVIEWS
	int i;
#endif
	Cmd_AddCommand ("v_cshift", V_cshift_f);
	Cmd_AddCommand ("bf", V_BonusFlash_f);
	Cmd_AddCommand ("df", V_DarkFlash_f);
	Cmd_AddCommand ("wf", V_WhiteFlash_f);
//	Cmd_AddCommand ("centerview", V_StartPitchDrift);

	Cvar_Register (&v_centermove, VIEWVARS);
	Cvar_Register (&v_centerspeed, VIEWVARS);

	Cvar_Register (&v_idlescale, VIEWVARS);
	Cvar_Register (&v_iyaw_cycle, VIEWVARS);
	Cvar_Register (&v_iroll_cycle, VIEWVARS);
	Cvar_Register (&v_ipitch_cycle, VIEWVARS);
	Cvar_Register (&v_iyaw_level, VIEWVARS);
	Cvar_Register (&v_iroll_level, VIEWVARS);
	Cvar_Register (&v_ipitch_level, VIEWVARS);

	Cvar_Register (&v_contentblend, VIEWVARS);
	Cvar_Register (&v_damagecshift, VIEWVARS);
	Cvar_Register (&v_quadcshift, VIEWVARS);
	Cvar_Register (&v_suitcshift, VIEWVARS);
	Cvar_Register (&v_ringcshift, VIEWVARS);
	Cvar_Register (&v_pentcshift, VIEWVARS);
	Cvar_Register (&v_gunkick, VIEWVARS);
	Cvar_Register (&v_gunkick_q2, VIEWVARS);

	Cvar_Register (&v_bonusflash, VIEWVARS);

	Cvar_Register (&v_viewheight, VIEWVARS);

	Cvar_Register (&crosshaircolor, VIEWVARS);
	Cvar_Register (&crosshair, VIEWVARS);
	Cvar_Register (&crosshairsize, VIEWVARS);
	Cvar_Register (&crosshaircorrect, VIEWVARS);
	Cvar_Register (&crosshairimage, VIEWVARS);
	Cvar_Register (&crosshairalpha, VIEWVARS);
	Cvar_Register (&cl_crossx, VIEWVARS);
	Cvar_Register (&cl_crossy, VIEWVARS);
	Cvar_Register (&gl_cshiftpercent, VIEWVARS);
	Cvar_Register (&gl_cshiftenabled, VIEWVARS);

	Cvar_Register (&cl_rollspeed, VIEWVARS);
	Cvar_Register (&cl_rollangle, VIEWVARS);
	Cvar_Register (&cl_bob, VIEWVARS);
	Cvar_Register (&cl_bobcycle, VIEWVARS);
	Cvar_Register (&cl_bobup, VIEWVARS);

	Cvar_Register (&v_kicktime, VIEWVARS);
	Cvar_Register (&v_kickroll, VIEWVARS);
	Cvar_Register (&v_kickpitch, VIEWVARS);

	Cvar_Register (&v_deathtilt, VIEWVARS);

	Cvar_Register (&scr_autoid, VIEWVARS);

#ifdef SIDEVIEWS
#define SECONDARYVIEWVARS "Secondary view vars"
	for (i = 0; i < SIDEVIEWS; i++)
	{
		Cvar_Register (&vsec_enabled[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_x[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_y[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_scalex[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_scaley[i], SECONDARYVIEWVARS);
		Cvar_Register (&vsec_yaw[i], SECONDARYVIEWVARS);
	}
#endif

	Cvar_Register (&ffov, VIEWVARS);

	BuildGammaTable (1.0, 1.0, 0.0);	// no gamma yet
	Cvar_Register (&v_gamma, VIEWVARS);
	Cvar_Register (&v_contrast, VIEWVARS);
	Cvar_Register (&v_brightness, VIEWVARS);
}
