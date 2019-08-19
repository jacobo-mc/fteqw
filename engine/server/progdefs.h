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

/* file generated by qcc, do not modify */

typedef struct globalvars_s
{
	int null;
	union {
		vec3_t vec;
		float f;
		int i;
	} ret;
	union {
		vec3_t vec;
		float f;
		int i;
	} param[8];
} globalvars_t;

#define	NUM_SPAWN_PARMS			64
typedef struct nqglobalvars_s
{
	int	*self;
	int	*other;
	int	*world;
	float	*time;
	float	*frametime;	
	int		*newmis;
	float	*force_retouch;
	string_t	*mapname;
	float	*deathmatch;
	float	*coop;
	float	*teamplay;
	float	*serverflags;
	float	*total_secrets;
	float	*total_monsters;
	float	*found_secrets;
	float	*killed_monsters;
	vec3_t	*v_forward;
	vec3_t	*v_up;
	vec3_t	*v_right;
	float	*trace_allsolid;
	float	*trace_startsolid;
	float	*trace_fraction;
#ifdef HAVE_LEGACY
	float	*trace_surfaceflagsf;
#endif
	int		*trace_surfaceflagsi;
	string_t*trace_surfacename;
#ifdef HAVE_LEGACY
	float	*trace_endcontentsf;
#endif
	int		*trace_endcontentsi;
	int		*trace_brush_id;
	int		*trace_brush_faceid;
	int		*trace_surface_id;
	int		*trace_bone_id;
	int		*trace_triangle_id;
	vec3_t	*trace_endpos;
	vec3_t	*trace_plane_normal;
	float	*trace_plane_dist;
	int	*trace_ent;
	float	*trace_inopen;
	float	*trace_inwater;
#ifdef HAVE_LEGACY
	string_t*trace_dphittexturename;
	float *trace_dpstartcontents;
	float *trace_dphitcontents;
	float *trace_dphitq3surfaceflags;
#endif
	int	*msg_entity;
	func_t	*main;
	func_t	*StartFrame;
	func_t	*PlayerPreThink;
	func_t	*PlayerPostThink;
	func_t	*ClientKill;
	func_t	*ClientConnect;
	func_t	*PutClientInServer;
	func_t	*ClientDisconnect;
	func_t	*SetNewParms;
	func_t	*SetChangeParms;
	float *cycle_wrapped;
	float *dimension_send;
	float *dimension_default;

	float *physics_mode;
	float *clientcommandframe;
	float *input_timelength;
	float *input_impulse;
	vec3_t *input_angles;
	vec3_t *input_movevalues;
	float *input_buttons;
	vec3_t *global_gravitydir;
	float *spawnparamglobals[NUM_SPAWN_PARMS];
	string_t *parm_string;
	int *serverid;
} globalptrs_t;

#define P_VEC(v) (pr_global_struct->v)


#ifndef HAVE_LEGACY
#define comfieldfloat_legacy(n,desc)
#else
#define comfieldfloat_legacy comfieldfloat
#endif


/*my hands are tied when it comes to the layout of this structure
On the server side, the structure *must* match original quakeworld, or we break compatibility with mvdsv's qvm api
On the client, it really doesn't matter what order fields are in, qclib will remap.
But fields that are actually useful on both sides need to be in the same locations.
But if we include all, that's a waste for csqc...
But we can overlap useful csqc-only ones with ssqc ones that are not going to be used on the client, so long as the types match.
This list isn't shared with the menu.

so the base fields are a fixed size
and the extension fields are added on the end and can have extra vm-specific stuff added on the end
*/
/*DO NOT ADD TO THIS STRUCTURE (base-qw-compat for q1qvm)*/
#define comqcfields	\
	comfieldfloat(modelindex,"This is the model precache index for the model that was set on the entity, instead of having to look up the model according to the .model field. Use setmodel to change it.")\
	comfieldvector(absmin,"Set by the engine when the entity is relinked (by setorigin, setsize, or setmodel). This is in world coordinates.")\
	comfieldvector(absmax,"Set by the engine when the entity is relinked (by setorigin, setsize, or setmodel). This is in world coordinates.")\
	comfieldfloat(ltime,"On MOVETYPE_PUSH entities, this is used as an alternative to the 'time' global, and .nextthink is synced to this instead of time. This allows time to effectively freeze if the entity is blocked, ensuring the think happens when the entity reaches the target point instead of randomly.")\
	comfieldfloat(lastruntime,"This field used to be used to avoid running an entity multiple times in a single frame due to quakeworld's out-of-order thinks. It is no longer used by FTE due to precision issues, but may still be updated for compatibility reasons.")	/*type doesn't match the qc, we use a hidden double instead. this is dead.*/ 	\
	comfieldfloat(movetype,"Describes how the entity moves. One of the MOVETYPE_ constants.")\
	comfieldfloat(solid,"Describes whether the entity is solid or not, and any special properties infered by that. Must be one of the SOLID_ constants")\
	comfieldvector(origin,"The current location of the entity in world space. Inline bsp entities (ie: ones placed by a mapper) will typically have a value of '0 0 0' in their neutral pose, as the geometry is offset from that. It is the reference point of the entity rather than the center of its geometry, for non-bsp models, this is often not a significant distinction.")\
	comfieldvector(oldorigin,"This is often used on players to reset the player back to where they were last frame if they somehow got stuck inside something due to fpu precision. Never change a player's oldorigin field to inside a solid, because that might cause them to become pemanently stuck.")\
	comfieldvector(velocity,"The direction and speed that the entity is moving in world space.")\
	comfieldvector(angles,"The eular angles the entity is facing in, in pitch, yaw, roll order. Due to a legacy bug, mdl/iqm/etc formats use +x=UP, bsp/spr/etc formats use +x=DOWN.")\
	comfieldvector(avelocity,"The amount the entity's angles change by each frame. Note that this is direct eular angles, and thus the angular change is non-linear and often just looks buggy.")\
	comfieldstring(classname,"Identifies the class/type of the entity. Useful for debugging, also used for loading, but its value is not otherwise significant to the engine, this leaves the mod free to set it to whatever it wants and randomly test strings for values in whatever inefficient way it chooses fit.")\
	comfieldstring(model,"The model name that was set via setmodel, in theory. Often, this is cleared to null to prevent the engine from being seen by clients while not changing modelindex. This behaviour allows inline models to remain solid yet be invisible.")\
	comfieldfloat(frame,"The current frame the entity is meant to be displayed in. In CSQC, note the lerpfrac and frame2 fields as well. if it specifies a framegroup, the framegroup will autoanimate in ssqc, but not in csqc.")\
	comfieldfloat(skin,"The skin index to use. on a bsp entity, setting this to 1 will switch to the 'activated' texture instead. A negative value will be understood as a replacement contents value, so setting it to CONTENTS_WATER will make a movable pool of water.")\
	comfieldfloat(effects,"Lots of random flags that change random effects.")\
	comfieldvector(mins,"The minimum extent of the model (ie: the bottom-left coordinate relative to the entity's origin). Change via setsize. May also be changed by setmodel.")\
	comfieldvector(maxs,"like mins, but in the other direction.")\
	comfieldvector(size,"maxs-mins. Updated when the entity is relinked (by setorigin, setsize, setmodel)")\
	comfieldfunction(touch, ".void()",NULL)\
	comfieldfunction(use, ".void()",NULL)\
	comfieldfunction(think, ".void()",NULL)\
	comfieldfunction(blocked, ".void()",NULL)\
	comfieldfloat(nextthink,"The time at which the entity is next scheduled to fire its think event. For MOVETYPE_PUSH entities, this is relative to that entity's ltime field, for all other entities it is relative to the time gloal.")\
	comfieldentity(groundentity,NULL)\
	comfieldfloat(health,NULL)\
	comfieldfloat(frags,NULL)\
	comfieldfloat(weapon,NULL)\
	comfieldstring(weaponmodel,NULL)\
	comfieldfloat(weaponframe,NULL)\
	comfieldfloat(currentammo,NULL)\
	comfieldfloat(ammo_shells,NULL)\
	comfieldfloat(ammo_nails,NULL)\
	comfieldfloat(ammo_rockets,NULL)\
	comfieldfloat(ammo_cells,NULL)\
	comfieldfloat(items,NULL)\
	comfieldfloat(takedamage,NULL)\
	comfieldentity(chain,NULL)\
	comfieldfloat(deadflag,NULL)\
	comfieldvector(view_ofs,NULL)\
	comfieldfloat(button0,NULL)\
	comfieldfloat(button1,NULL)	/*dead field in nq mode*/	\
	comfieldfloat(button2,NULL)\
	comfieldfloat(impulse,NULL)\
	comfieldfloat(fixangle,NULL)\
	comfieldvector(v_angle,"The angles a player is viewing. +x is DOWN (pitch, yaw, roll)")\
	comfieldstring(netname,NULL)\
	comfieldentity(enemy,NULL)\
	comfieldfloat(flags,NULL)\
	comfieldfloat(colormap,NULL)\
	comfieldfloat(team,NULL)\
	comfieldfloat(max_health,NULL)\
	comfieldfloat(teleport_time,NULL)\
	comfieldfloat(armortype,NULL)\
	comfieldfloat(armorvalue,NULL)\
	comfieldfloat(waterlevel,NULL)\
	comfieldfloat(watertype,NULL)\
	comfieldfloat(ideal_yaw,NULL)\
	comfieldfloat(yaw_speed,NULL)\
	comfieldentity(aiment,NULL)\
	comfieldentity(goalentity,NULL)\
	comfieldfloat(spawnflags,NULL)\
	comfieldstring(target,NULL)\
	comfieldstring(targetname,NULL)\
	comfieldfloat(dmg_take,NULL)\
	comfieldfloat(dmg_save,NULL)\
	comfieldentity(dmg_inflictor,NULL)\
	comfieldentity(owner,NULL)\
	comfieldvector(movedir,NULL)\
	comfieldstring(message,NULL)	/*don't use directly, hexen2 uses floats, so we go via qclib for message*/\
	comfieldfloat(sounds,NULL)\
	comfieldstring(noise,NULL)\
	comfieldstring(noise1,NULL)\
	comfieldstring(noise2,NULL)\
	comfieldstring(noise3,NULL)
/*DO NOT ADD TO THE ABOVE STRUCTURE (unless you want to break qvms)*/

#ifdef HEXEN2
#define comextqcfieldshexen2	\
	comfieldfloat(drawflags,"Various flags that affect lighting values and scaling. Typically set to 96 in quake for proper compatibility with DP_QC_SCALE.")/*hexen2*/\
	comfieldfloat(abslight,"Allows overriding light levels. Use drawflags to state that this field should actually be used.")/*hexen2's force a lightlevel*/\

#define svextqcfieldshexen2	\
	comfieldfloat(playerclass,NULL)/*hexen2 requirements*/\
	comfieldfloat(hasted,NULL)/*hexen2 uses this AS WELL as maxspeed*/\
	comfieldfloat(light_level,"Used by hexen2 to indicate the light level where the player is standing.")\

#else
#define comextqcfieldshexen2
#define svextqcfieldshexen2
#endif

#define comextqcfields	\
	comfieldvector(punchangle,NULL) /*std in nq*/\
	comfieldfloat(gravity,"Multiplier applied in addition to sv_gravity (not absolute units), to control the gravity affecting this entity specifically.")	/*added in quake 1.09 (for hipnotic)*/\
	comfieldfloat(hull,"Overrides the hull used by the entity for walkmove/movetogoal and not traceline/tracebox.")/*PEXT_HEXEN2*/\
	comfieldentity(movechain,"This is a linked list of entities which will be moved whenever this entity moves, logically they are attached to this entity.")/*hexen2*/\
	comfieldfunction(chainmoved, ".void()","Called when the entity is moved as a result of being part of another entity's .movechain")/*hexen2*/\
	comfieldfunction(contentstransition, ".void(float old, float new)","This function is called when the entity moves between water and air. If specified, default splash sounds will be disabled allowing you to provide your own.")/*ENTITYCONTENTSTRANSITION*/\
	comfieldfloat(dimension_solid,"This is the bitmask of dimensions which the entity is solid within.")/*EXT_DIMENSION_PHYSICS*/\
	comfieldfloat(dimension_hit,"This is the bitmask of dimensions which the entity will be blocked by. If other.dimension_solid & self.dimension_hit, our traces will impact and not proceed. If its false, the traces will NOT impact, allowing self to pass straight through.")/*EXT_DIMENSION_PHYSICS*/\
	/*comfieldfloat_legacy(hitcontentsmask,"Traces performed for this entity will impact against surfaces that match this contents mask.")*/ \
	comfieldint(hitcontentsmaski,"Traces performed for this entity will impact against surfaces that match this contents mask.")\
	comfieldfloat_legacy(dphitcontentsmask, "Some crappy field that inefficiently requires translating to the native contents flags. Ditch the 'dp', do it properly.")\
	comfieldfloat(scale,"Multiplier that resizes the entity. 1 is normal sized, 2 is double sized. scale 0 is remapped to 1. In SSQC, this is limited to 1/16th precision, with a maximum just shy of 16.")/*DP_ENT_SCALE*/\
	comfieldfloat(fatness,"How many QuakeUnits to push the entity's verticies along their normals by.")/*FTE_PEXT_FATNESS*/\
	comfieldfloat(alpha,"The transparency of the entity. 1 means opaque, 0.0001 means virtually invisible. 0 is remapped to 1, for compatibility.")/*DP_ENT_ALPHA*/\
	comfieldfloat(modelflags,"Used to override the flags set in the entity's model. Should be set according to the MF_ constants. Use effects|=EF_NOMODELFLAGS to ignore the model's flags completely. The traileffectnum field is more versatile.")\
	comfieldfloat(frame1time,"This controls the time into the framegroup/animation named by .frame, you should increment this value according to frametime or to distance moved, depending on the sort of animation you're attempting. You may wish to avoid incrementing this while lerpfrac is still changing, to avoid wasting parts of the animation.")	/*EXT_CSQC_1*/\
	comfieldfloat(basebone,"The base* frame animations are equivelent to their non-base versions, except that they only affect bone numbers below the 'basebone' value. This means that the base* animation can affect the legs of a skeletal model independantly of the normal animation fields affecting the torso area. For more complex animation than this, use skeletal objects.")	/*FTE_QC_BASEFRAME*/\
	comfieldfloat(baseframe,"See basebone")	/*FTE_QC_BASEFRAME*/\
	comfieldfunction(customphysics,".void()", "Called once each physics frame, overriding the entity's .movetype field and associated logic. You'll probably want to use tracebox to move it through the world. Be sure to call .think as appropriate.")\
	comfieldentity(tag_entity,NULL)\
	comfieldfloat(tag_index,NULL)\
	comfieldfloat(skeletonindex,"This object serves as a container for the skeletal bone states used to override the animation data.")		/*FTE_CSQC_SKELETONOBJECTS*/\
	comfieldvector(colormod,"Provides a colour tint for the entity.")\
	comfieldvector(glowmod,NULL)\
	comfieldvector(gravitydir,"Specifies the direction in which gravity acts. Must be normalised. '0 0 0' also means down. Use '0 0 1' if you want the player to be able to run on ceilings.")\
	comfieldfunction(camera_transform,".vector(vector org, vector ang)", "Provides portal transform information for portal surfaces attached to this entity. Also used to open up pvs in ssqc.")\
	comfieldfloat(pmove_flags,NULL)/*EXT_CSQC_1*/\
	comfieldfloat(geomtype,NULL)/*DP_...PHYSICS*/\
	comfieldfloat(friction,NULL)/*DP_...PHYSICS*/\
	comfieldfloat(erp,NULL)/*DP_...PHYSICS*/\
	comfieldfloat(jointtype,NULL)/*DP_...PHYSICS*/\
	comfieldfloat(mass,NULL)/*DP_...PHYSICS*/\
	comfieldfloat(bouncefactor,NULL)/*DP_...PHYSICS*/\
	comfieldfloat(bouncestop,NULL)/*DP_...PHYSICS*/\
	comfieldfloat(idealpitch,NULL)/*DP_QC_CHANGEPITCH (inconsistant naming)*/\
	comfieldfloat(pitch_speed,NULL)/*DP_QC_CHANGEPITCH*/\
	comextqcfieldshexen2	\
	comfieldvector(color,"This affects the colour of realtime lights that were enabled via the pflags field.")/*Hexen2 has a .float color, the warnings should be benign*/ \
	comfieldfloat(light_lev,"This is the radius of an entity's light. This is not normally used by the engine, but is used for realtime lights (ones that are enabled with the pflags field).")\
	comfieldfloat(style,"Used by the light util to decide how an entity's light should animate. On an entity with pflags set, this also affects realtime lights.")\
	comfieldfloat(pflags,"Realtime lighting flags")

#ifdef HEXEN2
#else
#define svextqcfieldshexen2
#endif

#ifdef PEXT_VIEW2
#define svextqcfield_clientcamera comfieldentity(clientcamera,"Controls which entity to use for this client's camera.")
#else
#define svextqcfield_clientcamera
#endif

#define svextqcfields \
	comfieldfloat(maxspeed,NULL)/*added in quake 1.09*/\
	comfieldentity(view2,"defines a second viewpoint, typically displayed in a corner of the screen (also punches open pvs).")/*FTE_PEXT_VIEW2*/\
	comfieldvector(movement,"These are the directions that the player is currently trying to move in (ie: which +forward/+moveright/+moveup etc buttons they have held), expressed relative to that player's angles. Order is forward, right, up.")\
	comfieldfloat(vw_index,"This acts as a second modelindex, using the same frames etc.")\
	comfieldentity(nodrawtoclient,"This entity will not be sent to the player named by this field. They will be invisible and not emit dlights/particles. Does not work in MVD-recorded game.")\
	comfieldentity(drawonlytoclient,"This entity will be sent *only* to the player named by this field. To other players they will be invisible and not emit dlights/particles. Does not work in MVD-recorded game.")\
	comfieldentity(viewmodelforclient,"This entity will be sent only to the player named by this field, and this entity will be attached to the player's view as an additional weapon model.")/*DP_ENT_VIEWMODEL*/\
	comfieldentity(exteriormodeltoclient,"This entity will be invisible to the player named by this field, except in mirrors or mirror-like surfaces, where it will be visible as normal. It may still cast shadows as normal, and generate lights+particles, depending on client settings. Does not affect how other players see the entity.")\
	svextqcfield_clientcamera\
	comfieldfloat(glow_size,NULL)\
	comfieldfloat(glow_color,NULL)\
	comfieldfloat(glow_trail,NULL)\
	comfieldfloat(traileffectnum,"This should be set to the result of particleeffectnum, in order to attach a custom trail effect to an entity as it moves.")/*DP_ENT_TRAILEFFECTNUM*/\
	comfieldfloat(emiteffectnum,"This should be set to the result of particleeffectnum, in order to continually spawn particles in the direction that this entity faces.")/*DP_ENT_TRAILEFFECTNUM*/\
	/*comfieldfloat(baseframe,"Specifies the current frame(group) to use for the lower (numerically) bones of a skeletal model. The basebone field specifies the bone where the regular frame field takes over.")*/	/*FTESS_QC_BASEFRAME*/\
	/*comfieldfloat(basebone,"Specifies the bone at which the baseframe* fields stop being effective.")*/	/*FTE_SSQC_BASEFRAME*/\
	comfieldfloat(dimension_see,"This is the dimension mask (bitfield) that the client is allowed to see. Entities and events not in this dimension mask will be invisible.")/*EXT_DIMENSION_VISIBLE*/\
	comfieldfloat(dimension_seen,"This is the dimension mask (bitfield) that the client is visible within. Clients that cannot see this dimension mask will not see this entity.")/*EXT_DIMENSION_VISIBLE*/\
	comfieldfloat(dimension_ghost,"If this entity is visible only within these dimensions, it will become transparent, as if a ghost.")/*EXT_DIMENSION_GHOST*/\
	comfieldfloat(dimension_ghost_alpha,"If this entity is subject to dimension_ghost, this is the scaler for its alpha value. If 0, 0.5 will be used instead.")/*EXT_DIMENSION_GHOST*/\
	comfieldfunction(SendEntity, ".float(entity playerent, float changedflags)","Called by the engine whenever an entity needs to be (re)sent to a client's csprogs, either because SendFlags was set or because data was lost. Must write its data to the MSG_ENTITY buffer. Will be called at the engine's leasure.")/*EXT_CSQC*/\
	comfieldfloat(SendFlags,"Indicates that something in the entity has been changed, and that it needs to be updated to all players that can see it. The engine will clear it at some point, with the cleared bits appearing in the 'changedflags' argument of the SendEntity method.")/*EXT_CSQC_1 (one of the DP guys came up with it)*/\
	comfieldfloat_legacy(Version,"Obsolete, set a SendFlags bit instead.")/*EXT_CSQC (obsolete)*/\
	comfieldfloat_legacy(clientcolors,NULL)\
	comfieldfloat_legacy(viewzoom,NULL)/*DP_VIEWZOOM, stats*/\
	comfieldfloat_legacy(items2,NULL)		/*added in quake 1.09 (for hipnotic). legacy because of stats*/\
	svextqcfieldshexen2 \
	comfieldfloat(pvsflags,"Reconfigures when the entity is visible to clients")/*EXT_CSQC_1*/\
	comfieldfloat(uniquespawnid,"Incremented by 1 whenever the entity is respawned. Persists across remove calls, for when the two-second grace period is insufficient.")/*FTE_ENT_UNIQUESPAWNID*/\
	comfieldfunction(customizeentityforclient, ".float()","Called just before an entity is sent to a client (non-csqc protocol). This gives you a chance to tailor 'self' according to what 'other' should see.")

#ifdef HALFLIFEMODELS
#define HALFLIFEMODEL_FIELDS	\
  	comfieldfloat(bonecontrol1,"Halflife model format bone controller. On player models, this typically affects the spine's yaw.")	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(bonecontrol2,"Halflife model format bone controller. On player models, this typically affects the spine's yaw.")	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(bonecontrol3,"Halflife model format bone controller. On player models, this typically affects the spine's yaw.")	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(bonecontrol4,"Halflife model format bone controller. On player models, this typically affects the spine's yaw.")	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(bonecontrol5,"Halflife model format bone controller. This typically affects the mouth.")	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(subblendfrac,"Weird animation value specific to halflife models. On player models, this typically affects the spine's pitch, or yaw, or...")	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(subblend2frac,"Weird animation value specific to halflife models. I've no idea what this does, probably nothing for most models.")	/*FTE_CSQC_HALFLIFE_MODELS*/\
	comfieldfloat(basesubblendfrac,"See basebone")	/*FTE_CSQC_HALFLIFE_MODELS+FTE_CSQC_BASEFRAME*/\
	comfieldfloat(basesubblend2frac,"See basebone")	/*FTE_CSQC_HALFLIFE_MODELS+FTE_CSQC_BASEFRAME*/
#else
#define HALFLIFEMODEL_FIELDS
#endif

#if FRAME_BLENDS >= 4
#define frame34fields \
	comfieldfloat(frame3,"Some people just don't understand how to use framegroups...")		/**/\
	comfieldfloat(frame3time,".frame3 equivelent of frame1time.")	/*EXT_CSQC_1*/\
	comfieldfloat(lerpfrac3,"Weight of .frame3 - .frame's weight is automatically calculated as 1-(lerpfrac+lerpfrac3+lerpfrac4), as a result these fields should NEVER add to above 1.")	/**/\
	comfieldfloat(frame4,NULL)		/**/\
	comfieldfloat(frame4time,".frame4 equivelent of frame1time.")	/*EXT_CSQC_1*/\
	comfieldfloat(lerpfrac4,NULL)	/**/\

#else
#define frame34fields
#endif

//this is the list for all the csqc fields.
//(the #define is so the list always matches the ones pulled out)
#define csqcextfields	\
	comfieldfloat(entnum,"This is the number of the entity that the ssqc is using.")		\
	comfieldfloat(frame2,"This is typically the old frame of the entity. if lerpfrac is 1, .frame will be ignored and .frame2 will be used solely. lerpfrac 0.5 will give an even 50/50 blend.")		/*EXT_CSQC_1*/\
	comfieldfloat(frame2time,".frame2 equivelent of frame1time.")	/*EXT_CSQC_1*/\
	comfieldfloat(lerpfrac,"The weight of .frame2. A value of 0 means the entity will animate using only .frame, while 1 would exclusively be .frame2. As this value is incremented, more of frame2 will be used. If you wish to use .frame2 as the 'old' frame, it is generally recommended to start this field with the value 1, to decrement it by frametime, and when it drops below 0 add 1 to it and update .frame2 and .frame to lerp into the new frame.")	/*EXT_CSQC_1*/\
	frame34fields	\
	comfieldfloat(renderflags,NULL)\
	comfieldfloat(forceshader,"Contains a shader handle used to replace all surfaces upon the entity.")/*FTE_CSQC_SHADERS*/\
							\
	comfieldfloat(baseframe2,"See basebone")	/*FTE_CSQC_BASEFRAME*/\
	comfieldfloat(baseframe1time,"See basebone")	/*FTE_CSQC_BASEFRAME*/\
	comfieldfloat(baseframe2time,"See basebone")	/*FTE_CSQC_BASEFRAME*/\
	comfieldfloat(baselerpfrac,"See basebone")	/*FTE_CSQC_BASEFRAME*/\
	HALFLIFEMODEL_FIELDS	\
	comfieldfloat(drawmask, "Matces the bitmask passed to the addentities builtin, to easily submit entities to the renderer. Not otherwise meaningful.")	/*So that the qc can specify all rockets at once or all bannanas at once*/	\
	comfieldfunction(predraw, ".float()","Called as part of the addentities builtin. Returns one of the PREDRAW_ constants. This gives you a chance to interpolate or animate entities as desired.")	/*If present, is called just before it's drawn.*/	

typedef struct stdentvars_s //standard = standard for qw
{
#define comfieldfloat(sharedname,desc) float sharedname;
#define comfieldvector(sharedname,desc) vec3_t sharedname;
#define comfieldentity(sharedname,desc) int sharedname;
#define comfieldstring(sharedname,desc) string_t sharedname;
#define comfieldfunction(sharedname, typestr,desc) func_t sharedname;
comqcfields
#undef comfieldfloat
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction
#ifdef VM_Q1
} stdentvars_t;

typedef struct extentvars_s
{
#endif
#define comfieldfloat(name,desc) float name;
#define comfieldint(name,desc) int name;
#define comfieldvector(name,desc) vec3_t name;
#define comfieldentity(name,desc) int name;
#define comfieldstring(name,desc) string_t name;
#define comfieldfunction(name, typestr,desc) func_t name;
comextqcfields
svextqcfields
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

#ifdef VM_Q1
} extentvars_t;
#else
} stdentvars_t;
#endif

typedef struct {
#define comfieldfloat(sharedname,desc) float sharedname;
#define comfieldvector(sharedname,desc) vec3_t sharedname;
#define comfieldentity(sharedname,desc) int sharedname;
#define comfieldstring(sharedname,desc) string_t sharedname;
#define comfieldfunction(sharedname, typestr,desc) func_t sharedname;
comqcfields
#undef comfieldfloat
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

#ifdef VM_Q1
} comentvars_t;
typedef struct {
#endif

#define comfieldfloat(name,desc) float name;
#define comfieldint(name,desc) int name;
#define comfieldvector(name,desc) vec3_t name;
#define comfieldentity(name,desc) int name;
#define comfieldstring(name,desc) string_t name;
#define comfieldfunction(name, typestr,desc) func_t name;
comextqcfields
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

#ifdef VM_Q1
} comextentvars_t;
#else
} comentvars_t;
#endif

#ifdef USEAREAGRID
#define AREAGRIDPERENT 16
#endif

#ifdef USERBE
typedef struct
{
	void *body;
	void *geom;
} rbebody_t;
typedef struct
{
	//doll info
	char name[32];
	int bone;
	float animate;
	qboolean draw:1;
	qboolean orient:1;
	qboolean isoffset:1;
	int orientpeer;

	//physics engine info
	int geomshape;
	float relmatrix[12];
	float inverserelmatrix[12];
	vec3_t dimensions;
	float mass;
} rbebodyinfo_t;

typedef struct
{
	void *joint;
} rbejoint_t;
typedef struct
{
	//doll info
	char name[32];
//	unsigned int disablebits;
	qboolean draw:1;
	qboolean startenabled:1;

	//ode info
	int type;
	int body1;	//handled by the ragdoll code, rather than the physics library.
	int body2;	//handled by the ragdoll code.
	int bonepivot;	//pivot is specified relative to this bone.

	float FMax,		FMax2;
	float HiStop,	HiStop2;
	float LoStop,	LoStop2;
	float CFM,		CFM2;
	float ERP,		ERP2;
	float Vel,		Vel2;
	vec3_t offset,	offset2;
	vec3_t axis,	axis2;
} rbejointinfo_t;

enum rbecommands_e
{
	RBECMD_ENABLE,
	RBECMD_DISABLE,
	RBECMD_FORCE,
	RBECMD_TORQUE,
};

typedef struct rbecommandqueue_s
{
	struct rbecommandqueue_s *next;
	enum rbecommands_e command;
	struct wedict_s *edict;
	vec3_t v1;
	vec3_t v2;
} rbecommandqueue_t;

typedef struct
{
	// physics parameters
	qboolean physics;
	rbebody_t body;
	rbejoint_t joint;
	float *vertex3f;
	int *element3i;
	int numvertices;
	int numtriangles;
	vec3_t mins;
	vec3_t maxs;
	vec_t mass;
	vec3_t origin;
	vec3_t velocity;
	vec3_t angles;
	vec3_t avelocity;
	qboolean gravity;
	int modelindex;
	vec_t movelimit; // smallest component of (maxs[]-mins[])
	float offsetmatrix[16];
	float offsetimatrix[16];
	int joint_type;
	int joint_enemy;
	int joint_aiment;
	vec3_t joint_origin; // joint anchor
	vec3_t joint_angles; // joint axis
	vec3_t joint_velocity; // second joint axis
	vec3_t joint_movedir; // parameters
	void *massbuf;
} entityrbe_t;
#endif
