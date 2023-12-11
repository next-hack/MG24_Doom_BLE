/**
 *  Doom Port to Silicon Labs EFR32xG24 devices and MGM240 modules
 *  by Nicola Wrachien (next-hack in the comments).
 *
 *  This port is based on the excellent doomhack's GBA Doom Port,
 *  with Kippykip additions.
 *  
 *  Several data structures and functions have been optimized 
 *  to fit in only 256kB RAM (GBA has 384 kB RAM).
 *  Z-Depth Light has been restored with almost no RAM consumption!
 *  Added BLE-based multiplayer.
 *  Added OPL2-based music.
 *  Restored screen melt effect!
 *  Tons of speed optimizations have been done, and the game now
 *  runs extremely fast, despite the much higher 3D resolution with
 *  respect to GBA.
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *  Copyright (C) 2021-2023 Nicola Wrachien (next-hack in the comments)
 *  on the EFR32xG24 and MGM240 port.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *      Moving object handling. Spawn functions.
 *
 *      next-hack: modified code to work with short pointers
 *                 added support for static_mobj_t, which are reduced size
 *                 mobj_t structures, for fixed-position objects.
 *
 *-----------------------------------------------------------------------------*/

#include "doomdef.h"
#include "doomstat.h"
#include "m_random.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_tick.h"
#include "sounds.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "info.h"
#include "g_game.h"
#include "p_inter.h"
#include "lprintf.h"

#include "global_data.h"

#include "utility_functions.h"

// 18-06-2021 next-hack: static assert to check mobj_t and static_mobj_t size
// if you get an error, make sure that you did not mess up with different versions
const char dummy1[(MOBJ_SIZE == sizeof(mobj_t)) - 1];
const char dummy2[(STATIC_MOBJ_SIZE == sizeof(static_mobj_t)) - 1];

//
// P_ExplodeMissile
//

void P_ExplodeMissile(mobj_t *mo)
{
    mo->momx = mo->momy = mo->momz16 = 0;

    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);

    mo->tics -= P_Random() & 3;

    if (mo->tics < 1)
        mo->tics = 1;

    setMobjFlagsBits(mo, MF_MISSILE, CLEAR_FLAGS);

    if (getMobjInfo(mo)->deathsound)
        S_StartSound(mo, getMobjInfo(mo)->deathsound);
}

//
// P_XYMovement
//
// Attempts to move something if it has momentum.
//

void P_XYMovement(mobj_t *mo)
{
    if (getMobjFlags(mo) & MF_STATIC)
        printf("Move on static object!\r\n");
    player_t *player;
    fixed_t xmove, ymove;

    if (!(mo->momx | mo->momy)) // Any momentum?
    {
        if (getMobjFlags(mo) & MF_SKULLFLY)
        {

            // the skull slammed into something

            setMobjFlagsBits(mo, MF_SKULLFLY, CLEAR_FLAGS);
            mo->momz16 = 0;

            P_SetMobjState(mo, getMobjInfo(mo)->spawnstate);
        }
        return;
    }

    player = getMobjPlayer(mo);

    if (mo->momx > MAXMOVE)
        mo->momx = MAXMOVE;
    else if (mo->momx < -MAXMOVE)
        mo->momx = -MAXMOVE;

    if (mo->momy > MAXMOVE)
        mo->momy = MAXMOVE;
    else if (mo->momy < -MAXMOVE)
        mo->momy = -MAXMOVE;

    xmove = mo->momx;
    ymove = mo->momy;

    do
    {
        fixed_t ptryx, ptryy;
        // killough 8/9/98: fix bug in original Doom source:
        // Large negative displacements were never considered.
        // This explains the tendency for Mancubus fireballs
        // to pass through walls.
        // CPhipps - compatibility optioned

        if (xmove > MAXMOVE / 2 || ymove > MAXMOVE / 2 || ((!demo_compatibility) && ((xmove < -MAXMOVE / 2 || ymove < -MAXMOVE / 2))))
        {
            ptryx = mo->x + xmove / 2;
            ptryy = mo->y + ymove / 2;
            xmove >>= 1;
            ymove >>= 1;
        }
        else
        {
            ptryx = mo->x + xmove;
            ptryy = mo->y + ymove;
            xmove = ymove = 0;
        }

        // killough 3/15/98: Allow objects to drop off

        if (!P_TryMove(mo, ptryx, ptryy, true))
        {
            // blocked move

            if (player)   // try to slide along it
                P_SlideMove(mo);
            else
            {
                if (getMobjFlags(mo) & MF_MISSILE)
                {
                    // explode a missile

                    if (_g->ceilingline)
                    {
                        const sector_t *ceilingBackSector = LN_BACKSECTOR(_g->ceilingline);

                        if (ceilingBackSector && ceilingBackSector->ceilingpic == _g->skyflatnum)
                        {
                            if (demo_compatibility ||  // killough
                            mo->zr > FIXED16_TO_FIXED_Z(getRamSector(ceilingBackSector)->ceilingheight16))
                            {
                                // Hack to prevent missiles exploding
                                // against the sky.
                                // Does not handle sky floors.

                                P_RemoveMobj(mo);
                                return;
                            }
                        }
                    }

                    P_ExplodeMissile(mo);
                }
                else // whatever else it is, it is now standing still in (x,y)
                {
                    mo->momx = mo->momy = 0;
                }
            }
        }
    } while (xmove || ymove);

    // slow down

    /* no friction for missiles or skulls ever, no friction when airborne */
    if ((getMobjFlags(mo) & (MF_MISSILE | MF_SKULLFLY )) || mo->zr > FIXED16_TO_FIXED_Z(mo->floorz16))
        return;

    /* killough 8/11/98: add bouncers
     * killough 9/15/98: add objects falling off ledges
     * killough 11/98: only include bouncers hanging off ledges
     */
    if ((getMobjFlags(mo) & MF_CORPSE ) 
    && (mo->momx > FRACUNIT / 4 || mo->momx < -FRACUNIT / 4 || mo->momy > FRACUNIT / 4 || mo->momy < -FRACUNIT / 4) && mo->floorz16 != _g->ramsectors[getMobjSubsector(mo)->sector_num].floorheight16)
        return;  // do not stop sliding if halfway off a step with some momentum

    // killough 11/98:
    // Stop voodoo dolls that have come to rest, despite any
    // moving corresponding player, except in old demos:

    if (mo->momx > -STOPSPEED && mo->momx < STOPSPEED && mo->momy > -STOPSPEED && mo->momy < STOPSPEED && (!player || !(player->cmd.forwardmove | player->cmd.sidemove) || (player->mo != mo)))
    {
        // if in a walking frame, stop moving

        // killough 10/98:
        // Don't affect main player when voodoo dolls stop, except in old demos:

        //    if ( player&&(unsigned)((player->mo->state - states)- S_PLAY_RUN1) < 4)
        //      P_SetMobjState (player->mo, S_PLAY);
        if (player && (unsigned) (getMobjState(player->mo) - states - S_PLAY_RUN1) < 4 && (player->mo == mo))
            P_SetMobjState(player->mo, S_PLAY);

        mo->momx = mo->momy = 0;        
        /* killough 10/98: kill any bobbing momentum too (except in voodoo dolls)
         * cph - DEMOSYNC - needs compatibility check?
         */
        if (player && player->mo == mo)
            player->momx = player->momy = 0;

    }
    else
    {
        /* phares 3/17/98
         *
         * Friction will have been adjusted by friction thinkers for
         * icy or muddy floors. Otherwise it was never touched and
         * remained set at ORIG_FRICTION
         *
         * killough 8/28/98: removed inefficient thinker algorithm,
         * instead using touching_sectorlist in P_GetFriction() to
         * determine friction (and thus only when it is needed).
         *
         * killough 10/98: changed to work with new bobbing method.
         * Reducing player momentum is no longer needed to reduce
         * bobbing, so ice works much better now.
         *
         * cph - DEMOSYNC - need old code for Boom demos?
         */

        fixed_t friction = ORIG_FRICTION;

        mo->momx = FixedMul(mo->momx, friction);
        mo->momy = FixedMul(mo->momy, friction);

        /* killough 10/98: Always decrease player bobbing by ORIG_FRICTION.
         * This prevents problems with bobbing on ice, where it was not being
         * reduced fast enough, leading to all sorts of kludges being developed.
         */

        if (player && player->mo == mo) /* Not voodoo dolls */
        {
            player->momx = FixedMul(player->momx, ORIG_FRICTION);
            player->momy = FixedMul(player->momy, ORIG_FRICTION);
        }
    }
}

//
// P_ZMovement
//
// Attempt vertical movement.

void P_ZMovement(mobj_t *mo)
{
    if (getMobjFlags(mo) & MF_STATIC)
    {
        printf("P_ZMovement static, blocking\r\n");
        while(1);
    }
    // check for smooth step up

    if (getMobjPlayer(mo) && getMobjPlayer(mo)->mo == mo   &&// killough 5/12/98: exclude voodoo dolls
    mo->zr < FIXED16_TO_FIXED_Z(mo->floorz16))
    {
        getMobjPlayer(mo)->viewheight -= FIXED16_TO_FIXED32(mo->floorz16) - FIXED_Z_TO_FIXED32(mo->zr);
        getMobjPlayer(mo)->deltaviewheight = (VIEWHEIGHT - getMobjPlayer(mo)->viewheight) >> 3;
    }

    // adjust altitude

    mo->zr += FIXED_MOMZ_TO_FIXED_Z(mo->momz16);

    if ((getMobjFlags(mo) & MF_FLOAT ) && getTarget(mo))

        // float down towards target if too close

        if (!((getMobjFlags(mo) ^ MF_FLOAT ) & (MF_FLOAT | MF_SKULLFLY | MF_INFLOAT )) && getTarget(mo)) /* killough 11/98: simplify */
        {
            fixed_t delta;
            if (P_AproxDistance(mo->x - getTarget(mo)->x, mo->y - getTarget(mo)->y) < D_abs(delta = FIXED_Z_TO_FIXED32(getTarget(mo)->zr) + (getMobjHeight(mo) >> 1) - FIXED_Z_TO_FIXED32(mo->zr)) * 3)
                mo->zr += FIXED32_TO_FIXED_Z(delta < 0 ? -FLOATSPEED : FLOATSPEED);
        }

    // clip movement

    if (mo->zr <= FIXED16_TO_FIXED_Z(mo->floorz16))
    {
        // hit the floor

        /* Note (id):
         *  somebody left this after the setting momz to 0,
         *  kinda useless there.
         * cph - This was the a bug in the linuxdoom-1.10 source which
         *  caused it not to sync Doom 2 v1.9 demos. Someone
         *  added the above comment and moved up the following code. So
         *  demos would desync in close lost soul fights.
         * cph - revised 2001/04/15 -
         * This was a bug in the Doom/Doom 2 source; the following code
         *  is meant to make charging lost souls bounce off of floors, but it
         *  was incorrectly placed after momz was set to 0.
         *  However, this bug was fixed in Doom95, Final/Ultimate Doom, and
         *  the v1.10 source release (which is one reason why it failed to sync
         *  some Doom2 v1.9 demos)
         * I've added a comp_soul compatibility option to make this behavior
         *  selectable for PrBoom v2.3+. For older demos, we do this here only
         *  if we're in a compatibility level above Doom 2 v1.9 (in which case we
         *  mimic the bug and do it further down instead)
         */

        if (getMobjFlags(mo) & MF_SKULLFLY)
            mo->momz16 = -mo->momz16; // the skull slammed into something

        if (mo->momz16 < 0)
        {
            if (getMobjPlayer(mo) && getMobjPlayer(mo)->mo && /* killough 5/12/98: exclude voodoo dolls */
            getMobjPlayer(mo)->mo == mo && FIXED_MOMZ_TO_FIXED32(mo->momz16) < -GRAVITY * 8)
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.

                getMobjPlayer(mo)->deltaviewheight = FIXED_MOMZ_TO_FIXED32(mo->momz16) >> 3;
                if (mo->health > 0) /* cph - prevent "oof" when dead */
                    S_StartSound(mo, sfx_oof);
            }
            mo->momz16 = 0;
        }
        mo->zr = FIXED16_TO_FIXED_Z(mo->floorz16);

        if ((getMobjFlags(mo) & MF_MISSILE ) && !(getMobjFlags(mo) & MF_NOCLIP ))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }
    else // still above the floor                                     // phares
    if (!(getMobjFlags(mo) & MF_NOGRAVITY ))
    {
        if (!mo->momz16)
            mo->momz16 = FIXED32_TO_FIXED_MOMZ(-GRAVITY);
        mo->momz16 -= FIXED32_TO_FIXED_MOMZ(GRAVITY);
    }

    if (FIXED_Z_TO_FIXED32(mo->zr) + getMobjHeight(mo) > FIXED16_TO_FIXED32(mo->ceilingz16))
    {
        /* cph 2001/04/15 -
         * Lost souls were meant to bounce off of ceilings;
         *  new comp_soul compatibility option added
         */
        if (getMobjFlags(mo) & MF_SKULLFLY)
            mo->momz16 = -mo->momz16; // the skull slammed into something

        // hit the ceiling

        if (mo->momz16 > 0)
            mo->momz16 = 0;

        mo->zr = FIXED16_TO_FIXED_Z(mo->ceilingz16) - FIXED32_TO_FIXED_Z(getMobjHeight(mo));

        if ((getMobjFlags(mo) & MF_MISSILE ) && !(getMobjFlags(mo) & MF_NOCLIP ))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }
}

//
// P_NightmareRespawn
//

void P_NightmareRespawn(mobj_t *mobj)
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
    subsector_t *ss;
    mobj_t *mo;

    /* haleyjd: stupid nightmare respawning bug fix
     *
     * 08/09/00: compatibility added, time to ramble :)
     * This fixes the notorious nightmare respawning bug that causes monsters
     * that didn't spawn at level startup to respawn at the point (0,0)
     * regardless of that point's nature. SMMU and Eternity need this for
     * script-spawned things like Halif Swordsmythe, as well.
     *
     * cph - copied from eternity, except comp_respawnfix becomes comp_respawn
     *   and the logic is reversed (i.e. like the rest of comp_ it *disables*
     *   the fix)
     */

    //ZLB: Everything respawns at its death point.
    //The spawnpoint is removed from the mobj.
    x = mobj->x;
    y = mobj->y;

    if (!x && !y)
    {
        return;
    }

    // something is occupying its position?

    if (!P_CheckPosition(mobj, x, y))
        return; // no respwan

    // spawn a teleport fog at old spot
    // because of removal of the body?

    mo = P_SpawnMobj(mobj->x, mobj->y, FIXED16_TO_FIXED32(_g->ramsectors[getMobjSubsector(mobj)->sector_num].floorheight16), MT_TFOG);

    // initiate teleport sound

    S_StartSound(mo, sfx_telept);

    // spawn a teleport fog at the new spot

    ss = R_PointInSubsector(x, y);

    mo = P_SpawnMobj(x, y, FIXED16_TO_FIXED32(_g->ramsectors[ss->sector_num].floorheight16), MT_TFOG);

    S_StartSound(mo, sfx_telept);

    // spawn the new monster

    //mthing = &mobj->spawnpoint;
    if (getMobjInfo(mobj)->flags & MF_SPAWNCEILING)
        z = FIXED16_TO_FIXED32(ONCEILINGZ16);
    else
        z = FIXED16_TO_FIXED32(ONFLOORZ16);

    // inherit attributes from deceased one

    mo = P_SpawnMobj(x, y, z, mobj->type);
    mo->angle16 = mobj->angle16;

    /* killough 11/98: transfer friendliness from deceased */
    setMobjFlagsValue(mo, (getMobjFlags(mo) & ~MF_FRIEND ) | (getMobjFlags(mobj) & MF_FRIEND ));

    mo->reactiontime = 18;

    // remove the old monster,

    P_RemoveMobj(mobj);
}

//Thinker function for stuff that doesn't need to do anything
//interesting.
//Just cycles through the states. Allows sprite animation to work.
void THINKFUN_ALIGN4 P_MobjBrainlessThinker(mobj_t *mobj)
{
    // cycle through states,
    // calling action functions at transitions

    if (mobj->tics != -1)
    {
        mobj->tics--;

        // you can cycle through multiple states in a tic

        if (!mobj->tics)
            P_SetMobjState(mobj, getMobjState(mobj)->nextstate);
    }
}

static uint32_t P_ThinkerFunctionIdxForType(mobjtype_t type, mobj_t *mobj)
{
    //Full thinking ability.
    if (type < MT_MISC0)
        return THINKER_IDX(P_MobjThinker);

    //Just state cycles.
    if (mobj->tics != -1)
        return THINKER_IDX(P_MobjBrainlessThinker);

    //No thinking at all.
    return 0;
}
//
#include "z_bmalloc.h"
IMPLEMENT_BLOCK_MEMORY_ALLOC_ZONE(mobjzone, ((sizeof(mobj_t) + 3) / 4) * 4, PU_POOL, 8, "Mobj");
IMPLEMENT_BLOCK_MEMORY_ALLOC_ZONE(static_mobjzone, ((sizeof(static_mobj_t) + 3) / 4) * 4, PU_POOL, 8, "Mobj");
IMPLEMENT_BLOCK_MEMORY_ALLOC_ZONE(dropped_mobj_xy_zone, ((sizeof(dropped_xy_t) + 3) / 4) * 4, PU_POOL, 8, "Mobj");

inline static static_mobj_t* P_GetStaticMobj(void)
{
    return (static_mobj_t*) Z_BMalloc(&static_mobjzone);
}

inline static mobj_t* P_GetMobj(void)
{
    return (mobj_t*) Z_BMalloc(&mobjzone);
}

//
// P_SpawnMobj
//
mobj_t* P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, uint32_t ntype)
{
    mobj_t *mobj;
    const state_t *st;
    const mobjinfo_t *info;
    uint32_t type = ntype & ~(MOBJ_TYPE_DROPPED | MOBJ_TYPE_CORPSE);
    info = &mobjinfo[type];

    if ((info->flags & MF_STATIC) || (ntype & (MOBJ_TYPE_DROPPED | MOBJ_TYPE_CORPSE)) )
        return (mobj_t*) P_SpawnStaticMobj(x, y, z, ntype, -1);
    mobj = P_GetMobj();    //*/ Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
    //printf("Mobj 0x%08X\r\n", (uint32_t) mobj);
    memset(mobj, 0, sizeof(*mobj));
    // info removed. Redundant and consumes 4 bytes per mobj.
    mobj->type = type;
    // mobj->info = info;
    mobj->x = x;
    mobj->y = y;
    mobj->radiusb = info->radius >> FRACBITS;
    mobj->height_s = info->height >> FRACBITS;                         // phares
    //setMobjFlagsValue(mobj, info->flags);
    mobj->ramFlags = info->flags;
    if (type == MT_PLAYER)         // Except in old demos, players
        setMobjFlagsBits(mobj, MF_FRIEND, SET_FLAGS);    // are always friends.

    mobj->health = info->spawnhealth;

    if (_g->gameskill != sk_nightmare)
        mobj->reactiontime = info->reactiontime;

    mobj->lastlook = P_Random () % MAXPLAYERS;

    // do not set the state with P_SetMobjState,
    // because action routines can not be called yet

    st = &states[info->spawnstate];

    //mobj->state  = st;
    mobj->state_idx = info->spawnstate;
    mobj->tics = st->tics;
    //mobj->sprite = st->sprite;
    //mobj->frame = st->frame;
    //mobj->touching_sectorlist = NULL; // NULL head of sector list // phares 3/13/98
#if USE_MSECNODE
    mobj->touching_sectorlist_sptr = 0;
    // set subsector and/or block links
#endif
    P_SetThingPosition(mobj);
#if MOBJ_HAS_DROPOFFZ
    mobj->dropoffz16 = /* killough 11/98: for tracking dropoffs */
#endif
    mobj->floorz16 = _g->ramsectors[getMobjSubsector(mobj)->sector_num].floorheight16;
    mobj->ceilingz16 = _g->ramsectors[getMobjSubsector(mobj)->sector_num].ceilingheight16;

    mobj->zr = FIXED32_TO_FIXED_Z(z == FIXED16_TO_FIXED32(ONFLOORZ16) ? FIXED16_TO_FIXED32(mobj->floorz16) :
              z == FIXED16_TO_FIXED32(ONCEILINGZ16) ? FIXED16_TO_FIXED32(mobj->ceilingz16) - getMobjHeight(mobj) : z);

    mobj->thinker.function_idx = P_ThinkerFunctionIdxForType(type, mobj);

    //mobj->target =  NULL;
    mobj->target_sptr = 0;
    mobj->tracer_sptr = 0;
    #if MOBJ_HAS_LAST_ENEMY
    mobj->lastenemy_sptr = 0;
    #endif
    P_AddThinker(&mobj->thinker);
    if (!((getMobjFlags(mobj) ^ MF_COUNTKILL ) & (MF_FRIEND | MF_COUNTKILL )))
        _g->totallive++;
    return mobj;
}
//
//
//
// 2021-03-13 next-hack
// P_SpawnStaticMobj
//
static_mobj_t* P_SpawnStaticMobj(fixed_t x, fixed_t y, fixed_t z, uint32_t ntype, int respawnIndex)
{
    static_mobj_t *mobj;
    const state_t *st;
    const mobjinfo_t *info;
    uint32_t type = ntype & ~(MOBJ_TYPE_DROPPED | MOBJ_TYPE_CORPSE);
    mobj = P_GetStaticMobj();  //*/ Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
    memset(mobj, 0, sizeof(*mobj));
    // info removed. Redundant and consumes 4 bytes per mobj.
    info = &mobjinfo[type];
    mobj->type = type;
    // mobj->info = info;
 

    //mobj->radiusb = info->radius >> FRACBITS;
    //mobj->height_s = info->height >> FRACBITS;                                      // phares
    mobj->ramFlags = info->flags;
    // add dropped case
    if (ntype & MOBJ_TYPE_DROPPED)
    {
        if (ntype & MOBJ_TYPE_CORPSE)
        {
            mobj->ramFlags |= MF_DROPPED | MF_CORPSE;
            // this is for corspe already on the ground. They are neither shootable, nor solid.
            mobj->ramFlags &= ~(MF_SHOOTABLE | MF_SOLID | MF_FLOAT | MF_SKULLFLY );
        }
        else
        {
             mobj->ramFlags |= MF_DROPPED;
        }
        // 
        dropped_xy_t *dp = Z_BMalloc(&dropped_mobj_xy_zone); 
        dp->x = x;
        dp->y = y;
        mobj->dropped_xy_sptr = getShortPtr(dp);
//        mobj->x = x;
//        mobj->y = y;
    }
    else
    {
      if (respawnIndex < 0)
      {
        // we are adding through spawnMobj, i.e. during setup
        mobj->pos_index = _g->totalstatic;
        _g->full_static_mobj_xy_and_type_values[_g->totalstatic].x = x >> FRACBITS;
        _g->full_static_mobj_xy_and_type_values[_g->totalstatic].y = y >> FRACBITS;
        _g->full_static_mobj_xy_and_type_values[_g->totalstatic].objtype = ntype;
        _g->totalstatic++;
      }
      else
      {
        // adding as respawn
        mobj->pos_index = respawnIndex;
        _g->full_static_mobj_xy_and_type_values[respawnIndex].x = x >> FRACBITS;
        _g->full_static_mobj_xy_and_type_values[respawnIndex].y = y >> FRACBITS;
        _g->full_static_mobj_xy_and_type_values[respawnIndex].objtype = ntype;
      }
    }
    if (type == MT_PLAYER)         // Except in old demos, players
        mobj->ramFlags |= MF_FRIEND;    // are always friends.

    st = &states[info->spawnstate];

    //mobj->state  = st;
    mobj->state_idx = info->spawnstate;
    mobj->tics   = st->tics;
    //mobj->sprite = st->sprite;
    //mobj->frame = st->frame;
    //mobj->touching_sectorlist = NULL; // NULL head of sector list // phares 3/13/98
#if USE_MSECNODE
    mobj->touching_sectorlist_sptr = 0;
#endif
    // next-hack: we need to call P_Random() because Doom set "lastlook" for any object.
    // not calling this, will result in demo desync.
    // however we must call it only if this is not a corpse mobj conversion hack.
    if (!(MOBJ_TYPE_CORPSE & ntype))
        P_Random (); 
    // set subsector and/or block links

    P_SetThingPosition((mobj_t*) mobj);

    mobj->zr = FIXED32_TO_FIXED_Z(
            z == FIXED16_TO_FIXED32(ONFLOORZ16) ? FIXED16_TO_FIXED32(_g->ramsectors[getMobjSubsector((mobj_t*) mobj)->sector_num].floorheight16) :
            z == FIXED16_TO_FIXED32(ONCEILINGZ16) ? FIXED16_TO_FIXED32(_g->ramsectors[getMobjSubsector((mobj_t*) mobj)->sector_num].ceilingheight16) - getMobjHeight((mobj_t*) mobj) : z);

    mobj->thinker.function_idx = P_ThinkerFunctionIdxForType(type, (mobj_t*) mobj);

    P_AddThinker(&mobj->thinker);

    return mobj;
}

//
// P_RemoveMobj
//
uint16_t itemrespawnque[ITEMQUESIZE];
uint16_t   itemrespawntime[ITEMQUESIZE];
uint8_t     iquehead;
uint8_t     iquetail;
void P_RemoveMobj(mobj_t *mobj)
{
  uint32_t flags = getMobjFlags(mobj);
  if ((flags & MF_SPECIAL)
    && !(flags & MF_DROPPED)
    && (mobj->type != MT_INV)
    && (mobj->type != MT_INS))
    {
      itemrespawnque[iquehead] = mobj->pos_index;
      itemrespawntime[iquehead] = _g->leveltime;
      iquehead = (iquehead+1)&(ITEMQUESIZE-1);

      // lose one off the end?
      if (iquehead == iquetail)
        iquetail = (iquetail+1)&(ITEMQUESIZE-1);
    }

    if (getMobjFlags(mobj) & MF_STATIC)
    {
        P_RemoveStaticMobj((static_mobj_t*) mobj);  // this will also unset thing position.
        return;
    }
    P_UnsetThingPosition(mobj);

    // Delete all nodes on the current sector_list               phares 3/16/98
#if USE_MSECNODE
    if (_g->sector_list)
    {
        P_DelSeclist(_g->sector_list);
        _g->sector_list = NULL;
    }
#endif
    // stop any playing sound

    S_StopSound(mobj);

    // killough 11/98:
    //
    // Remove any references to other mobjs.
    //
    // Older demos might depend on the fields being left alone, however,
    // if multiple thinkers reference each other indirectly before the
    // end of the current tic.
    // CPhipps - only leave dead references in old demos; I hope lxdoom_1 level
    // demos are rare and don't rely on this. I hope.

    if (!_g->demoplayback)
    {
        //P_SetTarget(&mobj->target,    NULL);
        mobj->target_sptr = 0;
        //P_SetTarget(&mobj->tracer,    NULL);
        mobj->tracer_sptr = 0;
        //P_SetTarget(getLastEnemy(mobj), NULL);
        #if MOBJ_HAS_LAST_ENEMY
        mobj->lastenemy_sptr = 0;
        #endif
    }
    // free block
    P_RemoveThinker(&mobj->thinker);
}
// 2021-03-13 next-hack special function for bonus or other unmovable objects
void P_RemoveStaticMobj(static_mobj_t *mobj)
{
    P_UnsetThingPosition((mobj_t*) mobj);

    // Delete all nodes on the current sector_list               phares 3/16/98
#if USE_MSECNODE
    if (_g->sector_list)
    {
        P_DelSeclist(_g->sector_list);
        _g->sector_list = NULL;
    }
#endif
    // killough 11/98:
    //
    // Remove any references to other mobjs.
    //
    // Older demos might depend on the fields being left alone, however,
    // if multiple thinkers reference each other indirectly before the
    // end of the current tic.
    // CPhipps - only leave dead references in old demos; I hope lxdoom_1 level
    // demos are rare and don't rely on this. I hope.

    P_RemoveStaticThinker(&mobj->thinker);
}

/*
 * P_FindDoomedNum
 *
 * Finds a mobj type with a matching doomednum
 *
 * killough 8/24/98: rewrote to use hashing
 */

PUREFUNC int P_FindDoomedNum(int type)
{
    // find which type to spawn
    for (int i = 0; i < NUMMOBJTYPES; i++)
    {
        if (type == mobjinfo[i].doomednum)
            return i;
    }

    return NUMMOBJTYPES;
}

//
// P_RespawnSpecials
//

void P_RespawnSpecials (void)
{
#if 1
    fixed_t   x;
    fixed_t   y;
    fixed_t   z;

    subsector_t*  ss;
    mobj_t*   mo;
//    mapthing_t*   mthing;

    int     i;

    // only respawn items if respawn is enabled
    if (!_g->items_respawn)
      return; //

    // nothing left to respawn?
    if (iquehead == iquetail)
      return;

    // wait at least 30 seconds
    if (_g->leveltime - itemrespawntime[iquetail] < 30*TICRATE)
      return;

    // 2023/08/29 next-hack: support for mapthings which are in ext flash
//    mapthing_t mt;
//    extMemSetCurrentAddress((uint32_t) _g->mapthings[itemrespawnque[iquetail]]);

 //   extMemGetDataFromCurrentAddress(&mt, sizeof(mt));


//    mthing = &mt;

//    x = mthing->x << FRACBITS;
//    y = mthing->y << FRACBITS;
     x = _g->full_static_mobj_xy_and_type_values[itemrespawnque[iquetail]].x << FRACBITS;
     y = _g->full_static_mobj_xy_and_type_values[itemrespawnque[iquetail]].y << FRACBITS;

     // spawn a teleport fog at the new spot
    ss = R_PointInSubsector (x,y);
    mo = P_SpawnMobj (x, y, FIXED16_TO_FIXED32(_g->ramsectors[ss->sector_num].floorheight16) , MT_IFOG);
    S_StartSound (mo, sfx_itmbk);

    // find which type to spawn
 /*   for (i=0 ; i< NUMMOBJTYPES ; i++)
    {
  if (mthing->type == mobjinfo[i].doomednum)
      break;
    }*/
    i = _g->full_static_mobj_xy_and_type_values[itemrespawnque[iquetail]].objtype;

    if (i >= NUMMOBJTYPES)
    {
    /*    I_Error("P_RespawnSpecials: Failed to find mobj type with doomednum "
                "%d when respawning thing. This would cause a buffer overrun "
                "in vanilla Doom", mthing->type);*/
      printf("Respawn Error, blocking\r\n"); while(1);
    }

    // spawn it
    if (mobjinfo[i].flags & MF_SPAWNCEILING)
      z = FIXED16_TO_FIXED32(ONCEILINGZ16);
    else
      z = FIXED16_TO_FIXED32(ONFLOORZ16);

    P_SpawnStaticMobj(x, y, z, i, itemrespawnque[iquetail]);
    // pull it from the que
    iquetail = (iquetail+1)&(ITEMQUESIZE-1);
#endif
}

//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//  between levels.
//

void P_SpawnPlayer(int n, const mapthing_t *mthing)
{
    player_t *p;
    fixed_t x;
    fixed_t y;
    fixed_t z;
    mobj_t *mobj;

    // not playing?

    if (!_g->playeringame[n])
        return;

    p = &_g->players[n];

    if (p->playerstate == PST_REBORN)
        G_PlayerReborn(n);

    /* cph 2001/08/14 - use the options field of memorised player starts to
     * indicate whether the start really exists in the level.
     */
    if (!mthing->options)
        I_Error("P_SpawnPlayer: attempt to spawn player at unavailable start point");

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;
    z = FIXED16_TO_FIXED32(ONFLOORZ16);
    mobj = P_SpawnMobj(x, y, z, MT_PLAYER);

    // set color translations for player sprites
    // 2023-03-12 next-hack: no. Unfortunately these flags are constant.
    // So we infer if translation is required by looking at the mobj pointer, when drawing vissprites.
    //setMobjFlagsBits(mobj, MF_TRANSLATION1, 0 != (n & 1));
    //setMobjFlagsBits(mobj, MF_TRANSLATION2, 0 != (n & 2));

    mobj->angle16 = ANGLE32_TO_ANGLE16( ANG45 * (mthing->angle / 45));

    mobj->player_n = n;
    mobj->playerCorpse_n = n;     // 2023-03-12 next-hack for properly redereing corpse colors.
    mobj->health = p->health;

    p->mo = mobj;
    p->playerstate = PST_LIVE;
    p->refire = 0;
    p->message = NULL;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->extralight = 0;
    p->fixedcolormap = 0;
    p->viewheight = VIEWHEIGHT;

    p->momx = p->momy = 0;   // killough 10/98: initialize bobbing to 0.

    // give all cards in death match mode

    if (_g->deathmatch)
    {
      for (int i = 0 ; i < NUMCARDS ; i++)
      {
        p->cards[i] = true;
      }
    }
    // setup gun psprite

    P_SetupPsprites(p);

    if (mthing->type - 1 == 0)
    {
        ST_Start(); // wake up the status bar
        HU_Start(); // wake up the heads up text
    }
}

/*
 * P_IsDoomnumAllowed()
 * Based on code taken from P_LoadThings() in src/p_setup.c  Return TRUE
 * if the thing in question is expected to be available in the gamemode used.
 */

boolean P_IsDoomnumAllowed(int doomnum)
{
    // Do not spawn cool, new monsters if !commercial
    if (_g->gamemode != commercial)
        switch (doomnum)
        {
            case 64:  // Archvile
            case 65:  // Former Human Commando
            case 66:  // Revenant
            case 67:  // Mancubus
            case 68:  // Arachnotron
            case 69:  // Hell Knight
            case 71:  // Pain Elemental
            case 84:  // Wolf SS
            case 88:  // Boss Brain
            case 89:  // Boss Shooter
                return false;
        }

    return true;
}

//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//

void P_SpawnMapThing(const mapthing_t *mthing)
{
    int i;
    //int     bit;
    mobj_t *mobj;
    fixed_t x;
    fixed_t y;
    fixed_t z;
    int options = mthing->options; /* cph 2001/07/07 - make writable copy */

    // count deathmatch start positions
    if (mthing->type == 11)
    {
      if (deathmatch_p < &deathmatchstarts[MAX_DM_STARTS])
      {
          memcpy (deathmatch_p, mthing, sizeof(*mthing));
          deathmatch_p++;
      }
      return;
    }

    // killough 2/26/98: Ignore type-0 things as NOPs
    // phares 5/14/98: Ignore Player 5-8 starts (for now)

    switch (mthing->type)
    {
        case 0:
        case DEN_PLAYER5:
        case DEN_PLAYER6:
        case DEN_PLAYER7:
        case DEN_PLAYER8:
            return;
    }

    // killough 11/98: clear flags unused by Doom
    //
    // We clear the flags unused in Doom if we see flag mask 256 set, since
    // it is reserved to be 0 under the new scheme. A 1 in this reserved bit
    // indicates it's a Doom wad made by a Doom editor which puts 1's in
    // bits that weren't used in Doom (such as HellMaker wads). So we should
    // then simply ignore all upper bits.

    if ((options & MTF_RESERVED) || demo_compatibility)
    {
      if (!demo_compatibility)
      {
        lprintf(LO_WARN, "P_SpawnMapThing: correcting bad flags (%u) (thing type %d)\n", options, mthing->type);
      }
      options &= MTF_EASY | MTF_NORMAL | MTF_HARD | MTF_AMBUSH | MTF_NOTSINGLE;
    }

    // check for players specially
    // check for players specially
    if (mthing->type <= 4)
    {
      // save spots for respawning in network games
      _g->playerstarts[mthing->type - 1] = *mthing;
      _g->playerstarts[mthing->type - 1].options = 1;
      if (!_g->deathmatch)
      {
        P_SpawnPlayer(mthing->type - 1, &_g->playerstarts[mthing->type - 1]);
        printf("spawn player\r\n");
      }
      return;
    }
    // check for apropriate skill level
    // check for appropriate skill level
    if (!_g->netgame && (mthing->options & 16) )
      return;

    /* jff "not single" thing flag */
    if (!_g->coop_spawns && ! _g->netgame && (options & MTF_NOTSINGLE))
        return;

    // killough 11/98: simplify
    if (_g->gameskill == sk_baby || _g->gameskill == sk_easy ? !(options & MTF_EASY) :
        _g->gameskill == sk_hard || _g->gameskill == sk_nightmare ? !(options & MTF_HARD) : !(options & MTF_NORMAL))
        return;

    // find which type to spawn

    // killough 8/23/98: use table for faster lookup
    i = P_FindDoomedNum(mthing->type);

    // don't spawn keycards and players in deathmatch
    if (_g->deathmatch && (mobjinfo[i].flags & MF_NOTDMATCH))
      return;

    // don't spawn any monsters if -nomonsters
    if (_g->nomonsters && ( i == MT_SKULL || (mobjinfo[i].flags & MF_COUNTKILL)) )
    {
      return;
    }

    // phares 5/16/98:
    // Do not abort because of an unknown thing. Ignore it, but post a
    // warning message for the player.

    if (i == NUMMOBJTYPES)
    {
        return;
    }

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (mobjinfo[i].flags & MF_SPAWNCEILING)
        z = FIXED16_TO_FIXED32(ONCEILINGZ16);
    else
        z = FIXED16_TO_FIXED32(ONFLOORZ16);
    //
    if (mobjinfo[i].flags & MF_STATIC)
    {
        _g->full_static_mobj_xy_and_type_values[_g->totalstatic].x = mthing->x;
        _g->full_static_mobj_xy_and_type_values[_g->totalstatic].y = mthing->y;
    }
    mobj = P_SpawnMobj(x, y, z, i);
    //mobj->spawnpoint = *mthing;
    if (mobj->tics > 0)
        mobj->tics = 1 + (P_Random() % mobj->tics);

    if (!(getMobjFlags(mobj) & MF_FRIEND ) && (options & MTF_FRIEND))
    {
        setMobjFlagsBits(mobj, MF_FRIEND, SET_FLAGS);            // killough 10/98:
    }

    /* killough 7/20/98: exclude friends */
    if (!((getMobjFlags(mobj) ^ MF_COUNTKILL ) & (MF_FRIEND | MF_COUNTKILL )))
        _g->totalkills++;

    if (getMobjFlags(mobj) & MF_COUNTITEM)
        _g->totalitems++;
    if (!(getMobjFlags(mobj) & MF_STATIC))
        mobj->angle16 = ANGLE32_TO_ANGLE16(ANG45 * (mthing->angle / 45));
    if (options & MTF_AMBUSH)
        setMobjFlagsBits(mobj, MF_AMBUSH, SET_FLAGS);
}

//
// GAME SPAWN FUNCTIONS
//

//
// P_SpawnPuff
//
void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z)
{
    mobj_t *th;
    // killough 5/5/98: remove dependence on order of evaluation:
    int t = P_Random();
    z += (t - P_Random()) << 10;

    th = P_SpawnMobj(x, y, z, MT_PUFF);
    th->momz16 = FIXED32_TO_FIXED_MOMZ(FRACUNIT);
    th->tics -= P_Random() & 3;

    if (th->tics < 1)
        th->tics = 1;

    // don't make punches spark on the wall

    if (_g->attackrange == MELEERANGE)
        P_SetMobjState(th, S_PUFF3);
}

//
// P_SpawnBlood
//
void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage)
{
    mobj_t *th;
    // killough 5/5/98: remove dependence on order of evaluation:
    int t = P_Random();
    z += (t - P_Random()) << 10;
    th = P_SpawnMobj(x, y, z, MT_BLOOD);
    th->momz16 = FIXED32_TO_FIXED_MOMZ(FRACUNIT * 2);
    th->tics -= P_Random() & 3;

    if (th->tics < 1)
        th->tics = 1;

    if (damage <= 12 && damage >= 9)
        P_SetMobjState(th, S_BLOOD2);
    else if (damage < 9)
        P_SetMobjState(th, S_BLOOD3);
}

//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//

void P_CheckMissileSpawn(mobj_t *th)
{
    th->tics -= P_Random() & 3;
    if (th->tics < 1)
        th->tics = 1;

    // move a little forward so an angle can
    // be computed if it immediately explodes

    th->x += (th->momx >> 1);
    th->y += (th->momy >> 1);
    th->zr += (FIXED_MOMZ_TO_FIXED_Z(th->momz16) >> 1);

    // killough 8/12/98: for non-missile objects (e.g. grenades)
    if (!(getMobjFlags(th) & MF_MISSILE ))
        return;

    // killough 3/15/98: no dropoff (really = don't care for missiles)

    if (!P_TryMove(th, th->x, th->y, false))
        P_ExplodeMissile(th);
}

//
// P_SpawnMissile
//

mobj_t* P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type)
{
    mobj_t *th;
    angle_t an;
    int dist;

    th = P_SpawnMobj(source->x, source->y, FIXED_Z_TO_FIXED32(source->zr) + 4 * 8 * FRACUNIT, type);

    if (getMobjInfo(th)->seesound)
        S_StartSound(th, getMobjInfo(th)->seesound);

    //P_SetTarget(&th->target, source);    // where it came from
    th->target_sptr = getShortPtr(source);
    an = R_PointToAngle2(source->x, source->y, dest->x, dest->y);

    // fuzzy player

    if (getMobjFlags(dest) & MF_SHADOW)
    {  // killough 5/5/98: remove dependence on order of evaluation:
        int t = P_Random();
        an += (t - P_Random()) << 20;
    }

    th->angle16 = ANGLE32_TO_ANGLE16(an);
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul(getMobjInfo(th)->speed, finecosine(an));
    th->momy = FixedMul(getMobjInfo(th)->speed, finesine(an));

    dist = P_AproxDistance(dest->x - source->x, dest->y - source->y);
    dist = dist / getMobjInfo(th)->speed;

    if (dist < 1)
        dist = 1;
    // next-hack: For long distance this will cause errors. Approximation might be necessary
    th->momz16 = FIXED32_TO_FIXED_MOMZ((FIXED_Z_TO_FIXED32(dest->zr - source->zr)) / dist);  
    P_CheckMissileSpawn(th);

    return th;
}

//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//

void P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type)
{
    mobj_t *th;
    fixed_t x, y, z, slope = 0;

    // see which target is to be aimed at

    angle_t an = ANGLE16_TO_ANGLE32(source->angle16);

    // killough 7/19/98: autoaiming was not in original beta
    {
        // killough 8/2/98: prefer autoaiming at enemies
        unsigned int mask = MF_FRIEND;

        do
        {
            slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT, mask);
            if (!_g->linetarget)
                slope = P_AimLineAttack(source, an += 1 << 26, 16 * 64 * FRACUNIT, mask);
            if (!_g->linetarget)
                slope = P_AimLineAttack(source, an -= 2 << 26, 16 * 64 * FRACUNIT, mask);
            if (!_g->linetarget)
                an = ANGLE16_TO_ANGLE32(source->angle16), slope = 0;
        } while (mask && (mask = 0, !_g->linetarget));  // killough 8/2/98
    }

    x = source->x;
    y = source->y;
    z = FIXED_Z_TO_FIXED32(source->zr) + 4 * 8 * FRACUNIT;

    th = P_SpawnMobj(x, y, z, type);

    if (getMobjInfo(th)->seesound)
        S_StartSound(th, getMobjInfo(th)->seesound);

    //P_SetTarget(&th->target, source);
    th->target_sptr = getShortPtr(source);
    th->angle16 = ANGLE32_TO_ANGLE16(an);
    th->momx = FixedMul(getMobjInfo(th)->speed, finecosine(an >> ANGLETOFINESHIFT));
    th->momy = FixedMul(getMobjInfo(th)->speed, finesine(an >> ANGLETOFINESHIFT));
    th->momz16 = FIXED32_TO_FIXED_MOMZ(FixedMul(getMobjInfo(th)->speed, slope));

    P_CheckMissileSpawn(th);
}
