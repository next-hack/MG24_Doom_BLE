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
 *      Teleportation.
 *
 *      next-hack: modified code to work with short pointers.
 *                 optimized a lot memory usage.
 *
 *-----------------------------------------------------------------------------*/

#include "doomdef.h"
#include "doomstat.h"
#include "p_spec.h"
#include "p_maputl.h"
#include "p_map.h"
#include "r_main.h"
#include "p_tick.h"
#include "s_sound.h"
#include "sounds.h"
#include "p_user.h"

#include "global_data.h"
#include "utility_functions.h"


static mobj_t* P_TeleportDestination(const line_t *line)
{
    int i;
    for (i = -1; (i = P_FindSectorFromLineTag(line, i)) >= 0;)
    {
        register thinker_t *th = NULL;
        while ((th = P_NextThinker(th)) != NULL)
            if (th->function_idx == THINKER_IDX(P_MobjThinker))
            {
                register mobj_t *m = (mobj_t*) th;
                if (m->type == MT_TELEPORTMAN && &_g->sectors[getMobjSubsector(m)->sector_num] - _g->sectors == i)
                    return m;
            }
    }
    return NULL;
}
//
// TELEPORTATION
//
// killough 5/3/98: reformatted, cleaned up

int EV_Teleport(const line_t *line, int side, mobj_t *thing)
{
    mobj_t *m;

    // don't teleport missiles
    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if (side || (getMobjFlags(thing) & MF_MISSILE))
        return 0;

    // killough 1/31/98: improve performance by using
    // P_FindSectorFromLineTag instead of simple linear search.

    if ((m = P_TeleportDestination(line)) != NULL)
    {
        fixed_t oldx = thing->x, oldy = thing->y, oldz = FIXED_Z_TO_FIXED32(thing->zr);
        player_t *player = getMobjPlayer(thing);

        // killough 5/12/98: exclude voodoo dolls:
        if (player && player->mo != thing)
            player = NULL;

        if (!P_TeleportMove(thing, m->x, m->y, false)) /* killough 8/9/98 */
            return 0;

        thing->zr = FIXED16_TO_FIXED_Z(thing->floorz16);

        if (player)
            player->viewz = FIXED_Z_TO_FIXED32(thing->zr) + player->viewheight;

        // spawn teleport fog and emit sound at source
        S_StartSound(P_SpawnMobj(oldx, oldy, oldz, MT_TFOG), sfx_telept);

        // spawn teleport fog and emit sound at destination
        S_StartSound(P_SpawnMobj(m->x + 20 * finecosine(ANGLE16_TO_ANGLE32(m->angle16) >> ANGLETOFINESHIFT), m->y + 20 * finesine(ANGLE16_TO_ANGLE32(m->angle16) >> ANGLETOFINESHIFT), FIXED_Z_TO_FIXED32(thing->zr), MT_TFOG), sfx_telept);

        /* don't move for a bit
         * cph - DEMOSYNC - BOOM had (player) here? */
        if (getMobjPlayer(thing))
            thing->reactiontime = 18;

        thing->angle16 = m->angle16;

        thing->momx = thing->momy = thing->momz16 = 0;

        /* killough 10/98: kill all bobbing momentum too */
        if (player)
            player->momx = player->momy = 0;

        return 1;
    }
    return 0;
}

//
// Silent TELEPORTATION, by Lee Killough
// Primarily for rooms-over-rooms etc.
//

int EV_SilentTeleport(const line_t *line, int side, mobj_t *thing)
{
    mobj_t *m;

    // don't teleport missiles
    // Don't teleport if hit back of line,
    // so you can get out of teleporter.
    if (getMobjFlags(thing) & MF_STATIC)
    {
        printf("Silent Teleport on static object. Blocking");
        while(1);
    }
    if (side || (getMobjFlags(thing) & MF_MISSILE))
        return 0;

    if ((m = P_TeleportDestination(line)) != NULL)
    {
        // Height of thing above ground, in case of mid-air teleports:
        fixed_t z = FIXED_Z_TO_FIXED32(thing->zr) - FIXED16_TO_FIXED32(thing->floorz16);

        // Get the angle between the exit thing and source linedef.
        // Rotate 90 degrees, so that walking perpendicularly across
        // teleporter linedef causes thing to exit in the direction
        // indicated by the exit thing.
        angle_t angle = R_PointToAngle2(0, 0, line->dx, line->dy) - ANGLE16_TO_ANGLE32(m->angle16) + ANG90;

        // Sine, cosine of angle adjustment
        fixed_t s = finesine(angle >> ANGLETOFINESHIFT);
        fixed_t c = finecosine(angle >> ANGLETOFINESHIFT);

        // Momentum of thing crossing teleporter linedef
        fixed_t momx = thing->momx;
        fixed_t momy = thing->momy;

        // Whether this is a player, and if so, a pointer to its player_t
        player_t *player = getMobjPlayer(thing);

        // Attempt to teleport, aborting if blocked
        if (!P_TeleportMove(thing, m->x, m->y, false)) /* killough 8/9/98 */
            return 0;

        // Rotate thing according to difference in angles
        thing->angle16 += ANGLE32_TO_ANGLE16(angle);

        // Adjust z position to be same height above ground as before
        thing->zr = FIXED32_TO_FIXED_Z(z) + FIXED16_TO_FIXED_Z(thing->floorz16);

        // Rotate thing's momentum to come out of exit just like it entered
        thing->momx = FixedMul(momx, c) - FixedMul(momy, s);
        thing->momy = FixedMul(momy, c) + FixedMul(momx, s);

        // Adjust player's view, in case there has been a height change
        // Voodoo dolls are excluded by making sure player->mo == thing.
        if (player && player->mo == thing)
        {
            // Save the current deltaviewheight, used in stepping
            fixed_t deltaviewheight = player->deltaviewheight;

            // Clear deltaviewheight, since we don't want any changes
            player->deltaviewheight = 0;

            // Set player's view according to the newly set parameters
            P_CalcHeight(player);

            // Reset the delta to have the same dynamics as before
            player->deltaviewheight = deltaviewheight;
        }

        return 1;
    }
    return 0;
}

//
// Silent linedef-based TELEPORTATION, by Lee Killough
// Primarily for rooms-over-rooms etc.
// This is the complete player-preserving kind of teleporter.
// It has advantages over the teleporter with thing exits.
//

// maximum fixed_t units to move object to avoid hiccups
#define FUDGEFACTOR 10

int EV_SilentLineTeleport(const line_t *line, int side, mobj_t *thing, boolean reverse)
{
    int i;
    const line_t *l;

    if (side || (getMobjFlags(thing) & MF_MISSILE))
        return 0;

    for (i = -1; (i = P_FindLineFromLineTag(line, i)) >= 0;)
        if ((l = _g->lines + i) != line && LN_BACKSECTOR(l))
        {
            // Get the thing's position along the source linedef
            fixed_t pos =
                    D_abs(line->dx) > D_abs(line->dy) ? FixedDiv(thing->x - line->v1.x, line->dx) : FixedDiv(thing->y - line->v1.y, line->dy);

            // Get the angle between the two linedefs, for rotating
            // orientation and momentum. Rotate 180 degrees, and flip
            // the position across the exit linedef, if reversed.
            angle_t angle = (reverse ? pos = FRACUNIT - pos, 0 : ANG180) + R_PointToAngle2(0, 0, l->dx, l->dy) - R_PointToAngle2(0, 0, line->dx, line->dy);

            // Interpolate position across the exit linedef
            fixed_t x = l->v2.x - FixedMul(pos, l->dx);
            fixed_t y = l->v2.y - FixedMul(pos, l->dy);

            // Sine, cosine of angle adjustment
            fixed_t s = finesine(angle >> ANGLETOFINESHIFT);
            fixed_t c = finecosine(angle >> ANGLETOFINESHIFT);

            // Maximum distance thing can be moved away from interpolated
            // exit, to ensure that it is on the correct side of exit linedef
            int fudge = FUDGEFACTOR;

            // Whether this is a player, and if so, a pointer to its player_t.
            // Voodoo dolls are excluded by making sure thing->player->mo==thing.
            player_t *player =
                    getMobjPlayer(thing) && getMobjPlayer(thing)->mo == thing ? getMobjPlayer(thing) : NULL;

            // Whether walking towards first side of exit linedef steps down
            int stepdown =
            getRamSector(LN_FRONTSECTOR(l))->floorheight16 < getRamSector(LN_BACKSECTOR(l))->floorheight16;

            // Height of thing above ground
            fixed_t z = FIXED_Z_TO_FIXED32(thing->zr) - FIXED16_TO_FIXED32(thing->floorz16);

            // Side to exit the linedef on positionally.
            //
            // Notes:
            //
            // This flag concerns exit position, not momentum. Due to
            // roundoff error, the thing can land on either the left or
            // the right side of the exit linedef, and steps must be
            // taken to make sure it does not end up on the wrong side.
            //
            // Exit momentum is always towards side 1 in a reversed
            // teleporter, and always towards side 0 otherwise.
            //
            // Exiting positionally on side 1 is always safe, as far
            // as avoiding oscillations and stuck-in-wall problems,
            // but may not be optimum for non-reversed teleporters.
            //
            // Exiting on side 0 can cause oscillations if momentum
            // is towards side 1, as it is with reversed teleporters.
            //
            // Exiting on side 1 slightly improves player viewing
            // when going down a step on a non-reversed teleporter.

            int side = reverse || (player && stepdown);

            // Make sure we are on correct side of exit linedef.
            while (P_PointOnLineSide(x, y, l) != side && --fudge >= 0)
                if (D_abs(l->dx) > D_abs(l->dy))
                    y -= (l->dx < 0) != side ? -1 : 1;
                else
                    x += (l->dy < 0) != side ? -1 : 1;

            // Attempt to teleport, aborting if blocked
            if (!P_TeleportMove(thing, x, y, false)) /* killough 8/9/98 */
                return 0;

            // Adjust z position to be same height above ground as before.
            // Ground level at the exit is measured as the higher of the
            // two floor heights at the exit linedef.
            thing->zr = FIXED32_TO_FIXED_Z(z) + FIXED16_TO_FIXED_Z(_g->ramsectors[_g->sides[l->sidenum[stepdown]].sector_num].floorheight16);

            // Rotate thing's orientation according to difference in linedef angles
            thing->angle16 += ANGLE32_TO_ANGLE16(angle);

            // Momentum of thing crossing teleporter linedef
            x = thing->momx;
            y = thing->momy;

            // Rotate thing's momentum to come out of exit just like it entered
            thing->momx = FixedMul(x, c) - FixedMul(y, s);
            thing->momy = FixedMul(y, c) + FixedMul(x, s);

            // Adjust a player's view, in case there has been a height change
            if (player)
            {
                // Save the current deltaviewheight, used in stepping
                fixed_t deltaviewheight = player->deltaviewheight;

                // Clear deltaviewheight, since we don't want any changes now
                player->deltaviewheight = 0;

                // Set player's view according to the newly set parameters
                P_CalcHeight(player);

                // Reset the delta to have the same dynamics as before
                player->deltaviewheight = deltaviewheight;
            }

            return 1;
        }
    return 0;
}
