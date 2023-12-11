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
 *      Enemy thinking, AI.
 *      Action Pointer Functions
 *      that are associated with states/frames.
 *
 * next-hack:
 * modified code to work with short pointers.
 * Added partial demo compatibility.
 * Reduced memory usage.
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "m_random.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_setup.h"
#include "p_spec.h"
#include "s_sound.h"
#include "sounds.h"
#include "p_inter.h"
#include "g_game.h"
#include "p_enemy.h"
#include "p_tick.h"
#include "m_bbox.h"
#include "lprintf.h"

#include "global_data.h"
#include "utility_functions.h"
const int distfriend = 128;

typedef enum
{
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
} dirtype_t;

static void P_NewChaseDir(mobj_t *actor);

//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//

//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//
// killough 5/5/98: reformatted, cleaned up

static void P_RecursiveSound(sector_t *sec, int soundblocks, mobj_t *soundtarget)
{
    // int i;
    short i;
    // wake up all monsters in this sector
#if OLD_VALID_COUNT
    if (getRamSector(sec)->validcount  == _g->validcount && getRamSector(sec)->soundtraversed <= soundblocks + 1)
        return;             // already flooded
    getRamSector(sec)->validcount = _g->validcount;
#else
    if (getBitMask32(_g->lineSectorChecked, sec->sectorNumber) && getRamSector(sec)->soundtraversed <= soundblocks + 1)
        return;             // already flooded
    
    setBitMask32(_g->lineSectorChecked, sec->sectorNumber);
#endif
    getRamSector(sec)->soundtraversed = soundblocks + 1;
    //P_SetTarget(&sec->soundtarget, soundtarget);
    getRamSector(sec)->soundtarget_sptr = getShortPtr(soundtarget);
    for (i = 0; i < sec->linecount; i++)
    {
        sector_t *other;
//      const line_t *check = sec->lines[i];
        const line_t *check = getSectorLineByIndex(sec, i);

        if (!(check->flags & ML_TWOSIDED))
            continue;

        P_LineOpening(check);

        if (_g->openrange <= 0)
            continue;       // closed door

        other = &_g->sectors[_g->sides[check->sidenum[&_g->sectors[_g->sides[check->sidenum[0]].sector_num] == sec]].sector_num];

        if (!(check->flags & ML_SOUNDBLOCK))
            P_RecursiveSound(other, soundblocks, soundtarget);
        else if (!soundblocks)
            P_RecursiveSound(other, 1, soundtarget);
    }
}

//
// P_NoiseAlert
// If a monster yells at a player,
// it will alert other monsters to the player.
//
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
#if !OLD_VALIDCOUNT
    clearArray32(_g->lineSectorChecked, (_g->numsectors + 31) / 32);
#else
    _g->validcount++;
#endif
    P_RecursiveSound(&_g->sectors[getMobjSubsector(emitter)->sector_num], 0, target);
}

//
// P_CheckMeleeRange
//

static boolean P_CheckMeleeRange(mobj_t *actor)
{
    mobj_t *pl = getTarget(actor);

    if (!pl)
        return false;
    fixed_t dist = P_AproxDistance (pl->x-actor->x, pl->y-actor->y);

    if (dist >= MELEERANGE - 20 * FRACUNIT + getMobjInfo(pl)->radius)
        return false;

    if (! P_CheckSight (actor, pl) )
        return false;

    return true;

    /* next-hack: removed this unreadable sh*t
    return  // killough 7/18/98: friendly monsters don't attack other friends
    pl && !(actor->flags & pl->flags & MF_FRIEND ) && 
    (P_AproxDistance(pl->x - actor->x, pl->y - actor->y) <
    MELEERANGE - 20 * FRACUNIT + getMobjInfo(pl)->radius) 
    && P_CheckSight(actor, getTarget(actor));
    */
}

//
// P_HitFriend()
//
// killough 12/98
// This function tries to prevent shooting at friends

static boolean P_HitFriend(mobj_t *actor)
{
    return (getMobjFlags(actor) & MF_FRIEND ) && actor->target_sptr && (P_AimLineAttack(actor, R_PointToAngle2(actor->x, actor->y, getTarget(actor)->x, getTarget(actor)->y), P_AproxDistance(actor->x - getTarget(actor)->x, actor->y - getTarget(actor)->y), 0), _g->linetarget) && _g->linetarget != getTarget(actor) && !((getMobjFlags(_g->linetarget) ^ getMobjFlags(actor)) & MF_FRIEND );
}

//
// P_CheckMissileRange
//
static boolean P_CheckMissileRange(mobj_t *actor)
{
    fixed_t dist;

    if (!P_CheckSight(actor, getTarget(actor)))
        return false;

    if (getMobjFlags(actor) & MF_JUSTHIT)
    {      // the target just hit the enemy, so fight back!
        setMobjFlagsBits(actor, MF_JUSTHIT, CLEAR_FLAGS);

        /* killough 7/18/98: no friendly fire at corpses
         * killough 11/98: prevent too much infighting among friends
         * cph - yikes, talk about fitting everything on one line... */
        if (demo_compatibility)
            return true;

        return !(getMobjFlags(actor) & MF_FRIEND ) || (getTarget(actor)->health > 0 && (!(getMobjFlags(getTarget(actor)) & MF_FRIEND ) || (
                getMobjPlayer(getTarget(actor)) ? true : !(getMobjFlags(getTarget(actor)) & MF_JUSTHIT ) && P_Random() > 128)));
    }

    /* killough 7/18/98: friendly monsters don't attack other friendly
     * monsters or players (except when attacked, and then only once)
     */
    if (!demo_compatibility && (getMobjFlags(actor) & getMobjFlags(getTarget(actor)) & MF_FRIEND))
        return false;

    if (actor->reactiontime)
        return false;       // do not attack yet

    // OPTIMIZE: get this from a global checksight
    dist = P_AproxDistance(actor->x - getTarget(actor)->x, actor->y - getTarget(actor)->y) - 64 * FRACUNIT;

    if (!getMobjInfo(actor)->meleestate)
        dist -= 128 * FRACUNIT;       // no melee attack, so fire more

    dist >>= FRACBITS;

    if (actor->type == MT_VILE)
        if (dist > 14 * 64)
            return false;     // too far away

    if (actor->type == MT_UNDEAD)
    {
        if (dist < 196)
            return false;   // close for fist attack
        dist >>= 1;
    }

    if (actor->type == MT_CYBORG || actor->type == MT_SPIDER || actor->type == MT_SKULL)
        dist >>= 1;

    if (dist > 200)
        dist = 200;

    if (actor->type == MT_CYBORG && dist > 160)
        dist = 160;

    if (P_Random() < dist)
        return false;

    if (!demo_compatibility && P_HitFriend(actor))
        return false;

    return true;
}
#if SMART_MOVE
/*
 * P_IsOnLift
 *
 * killough 9/9/98:
 *
 * Returns true if the object is on a lift. Used for AI,
 * since it may indicate the need for crowded conditions,
 * or that a monster should stay on the lift for a while
 * while it goes up or down.
 */

static boolean P_IsOnLift(const mobj_t *actor)
{
    sector_t *sec = &_g->sectors[getMobjSubsector((mobj_t*) actor)->sector_num];

    // Short-circuit: it's on a lift which is active.
    if (getRamSector(sec)->floordata_sptr && ((thinker_t*) getSectorFloorData(sec))->function_idx == THINKER_IDX(T_PlatRaise))
        return true;

    return false;
}
/*
 * P_IsUnderDamage
 *
 * killough 9/9/98:
 *
 * Returns nonzero if the object is under damage based on
 * their current position. Returns 1 if the damage is moderate,
 * -1 if it is serious. Used for AI.
 */
static int P_IsUnderDamage(mobj_t *actor)
{
#if USE_MSECNODE
    const struct msecnode_s *seclist;
    const ceiling_t *cl;             // Crushing ceiling
    int dir = 0;
    for (seclist = getTouchingSectorList(actor); seclist;
            seclist = getMsecnodeTNext(seclist))
        if ((cl = getSectorCeilingData(getMsecnodeSector(seclist))) && cl->thinker.function == T_MoveCeiling)
            dir |= cl->direction;
    return dir;
#else
    return 0;
#endif
}
#endif

//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//

static const fixed_t xspeed[8] =
{ FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000, 0, 47000 };
static const fixed_t yspeed[8] =
{ 0, 47000, FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000 };

static boolean P_Move(mobj_t *actor, boolean dropoff) /* killough 9/12/98 */
{
    fixed_t tryx, tryy, deltax, deltay, origx, origy;
    boolean try_ok;
    int speed;

    if (actor->movedir == DI_NODIR)
        return false;

#ifdef RANGECHECK
  if ((unsigned)actor->movedir >= 8)
    I_Error ("P_Move: Weird actor->movedir!");
#endif

    speed = getMobjInfo(actor)->speed;

    tryx = (origx = actor->x) + (deltax = speed * xspeed[actor->movedir]);
    tryy = (origy = actor->y) + (deltay = speed * yspeed[actor->movedir]);

    try_ok = P_TryMove(actor, tryx, tryy, dropoff);

    if (!try_ok)
    {      // open any specials
        int good;

        if ((getMobjFlags(actor) & MF_FLOAT ) && _g->floatok)
        {
            if (actor->zr < FIXED16_TO_FIXED_Z(_g->tmfloorz16))          // must adjust height
                actor->zr += FIXED32_TO_FIXED_Z(FLOATSPEED);
            else
                actor->zr -= FIXED32_TO_FIXED_Z(FLOATSPEED);

            setMobjFlagsBits(actor, MF_INFLOAT, SET_FLAGS);

            return true;
        }

        if (!_g->numspechit)
            return false;

        actor->movedir = DI_NODIR;

        /* if the special is not a door that can be opened, return false
         *
         * killough 8/9/98: this is what caused monsters to get stuck in
         * doortracks, because it thought that the monster freed itself
         * by opening a door, even if it was moving towards the doortrack,
         * and not the door itself.
         *
         * killough 9/9/98: If a line blocking the monster is activated,
         * return true 90% of the time. If a line blocking the monster is
         * not activated, but some other line is, return false 90% of the
         * time. A bit of randomness is needed to ensure it's free from
         * lockups, but for most cases, it returns the correct result.
         *
         * Do NOT simply return false 1/4th of the time (causes monsters to
         * back out when they shouldn't, and creates secondary stickiness).
         */

        for (good = false; _g->numspechit--;)
            if (P_UseSpecialLine(actor, _g->spechit[_g->numspechit], 0))
                good |= _g->spechit[_g->numspechit] == _g->blockline ? 1 : 2;

        /* cph - compatibility maze here
         * Boom v2.01 and orig. Doom return "good"
         * Boom v2.02 and LxDoom return good && (P_Random(pr_trywalk)&3)
         * MBF plays even more games
         */
        if (demo_compatibility)
            return good;
        if (!good)
            return good;
        return ((P_Random() >= 230) ^ (good & 1));
    }
    else
        setMobjFlagsBits(actor, MF_INFLOAT, CLEAR_FLAGS);

    /* killough 11/98: fall more slowly, under gravity, if felldown==true */
    if (!(getMobjFlags(actor) & MF_FLOAT ) && (!_g->felldown))
        actor->zr = FIXED16_TO_FIXED_Z(actor->floorz16);

    return true;
}
#if SMART_MOVE
/*
 * P_SmartMove
 *
 * killough 9/12/98: Same as P_Move, except smarter
 */

static boolean P_SmartMove(mobj_t *actor)
{
    if (demo_compatibility)
        return P_Move(actor, false);
    mobj_t *target = getTarget(actor);
    int on_lift, dropoff = false, under_damage;

    /* killough 9/12/98: Stay on a lift if target is on one */
    on_lift = target && target->health > 0 && _g->sectors[getMobjSubsector(target)->sector_num].tag == _g->sectors[getMobjSubsector(actor)->sector_num].tag && P_IsOnLift(actor);

    if (!P_Move(actor, dropoff))
        return false;

    under_damage = P_IsUnderDamage(actor);

    // killough 9/9/98: avoid crushing ceilings or other damaging areas
    if ((on_lift && P_Random() < 230 &&      // Stay on lift
    !P_IsOnLift(actor)) || (!under_damage &&  // Get away from damage
    (under_damage = P_IsUnderDamage(actor)) && (under_damage < 0 || P_Random() < 200)))
        actor->movedir = DI_NODIR;   // avoid the area (most of the time anyway)

    return true;
}
#else
static boolean P_SmartMove(mobj_t *actor)
{
    return P_Move(actor, false);
}    

#endif
//
// TryWalk
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//

static boolean P_TryWalk(mobj_t *actor)
{
    if (!P_SmartMove(actor))
        return false;
    actor->movecount = P_Random() & 15;
    return true;
}

//
// P_DoNewChaseDir
//
// killough 9/8/98:
//
// Most of P_NewChaseDir(), except for what
// determines the new direction to take
//

static void P_DoNewChaseDir(mobj_t *actor, fixed_t deltax, fixed_t deltay)
{
    int xdir, ydir, tdir;
    int olddir = actor->movedir;
    int turnaround = olddir;

    if (turnaround != DI_NODIR)         // find reverse direction
        turnaround ^= 4;

    xdir = deltax > 10 * FRACUNIT ? DI_EAST :
           deltax < -10 * FRACUNIT ? DI_WEST : DI_NODIR;

    ydir = deltay < -10 * FRACUNIT ? DI_SOUTH :
           deltay > 10 * FRACUNIT ? DI_NORTH : DI_NODIR;

    // try direct route
    if (xdir != DI_NODIR && ydir != DI_NODIR && turnaround != (actor->movedir =
            deltay < 0 ? deltax > 0 ? DI_SOUTHEAST : DI_SOUTHWEST :
            deltax > 0 ? DI_NORTHEAST : DI_NORTHWEST) && P_TryWalk(actor))
        return;

    // try other directions
    if (P_Random() > 200 || D_abs(deltay) > D_abs(deltax))
        tdir = xdir, xdir = ydir, ydir = tdir;

    if ((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR && (actor->movedir = xdir, P_TryWalk(actor)))
        return;         // either moved forward or attacked

    if ((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR && (actor->movedir = ydir, P_TryWalk(actor)))
        return;

    // there is no direct path to the player, so pick another direction.
    if (olddir != DI_NODIR && (actor->movedir = olddir, P_TryWalk(actor)))
        return;

    // randomly determine direction of search
    if (P_Random() & 1)
    {
        for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
            if (tdir != turnaround && (actor->movedir = tdir, P_TryWalk(actor)))
                return;
    }
    else
    {
        for (tdir = DI_SOUTHEAST; tdir >= DI_EAST; tdir--)
            if (tdir != turnaround && (actor->movedir = tdir, P_TryWalk(actor)))
                return;
    }

    if ((actor->movedir = turnaround) != DI_NODIR && !P_TryWalk(actor))
        actor->movedir = DI_NODIR;
}

//
// killough 11/98:
//
// Monsters try to move away from tall dropoffs.
//
// In Doom, they were never allowed to hang over dropoffs,
// and would remain stuck if involuntarily forced over one.
// This logic, combined with p_map.c (P_TryMove), allows
// monsters to free themselves without making them tend to
// hang over dropoffs.
#if MOBJ_HAS_DROPOFFZ
static boolean PIT_AvoidDropoff(const line_t *line)
{

    if (LN_BACKSECTOR(line) && // Ignore one-sided linedefs
    _g->tmbbox[BOXRIGHT] > line->bbox[BOXLEFT] && _g->tmbbox[BOXLEFT] < line->bbox[BOXRIGHT] && _g->tmbbox[BOXTOP] > line->bbox[BOXBOTTOM] && // Linedef must be contacted
    _g->tmbbox[BOXBOTTOM] < line->bbox[BOXTOP] && P_BoxOnLineSide(_g->tmbbox, line) == -1)
    {
        fixed_t front = FIXED16_TO_FIXED32(getRamSector(LN_FRONTSECTOR(line))->floorheight16);
        fixed_t back = FIXED16_TO_FIXED32(getRamSector(LN_BACKSECTOR(line))->floorheight16);
        angle_t angle;

        // The monster must contact one of the two floors,
        // and the other must be a tall dropoff (more than 24).

        if (back == _g->floorz && front < _g->floorz - FRACUNIT * 24)
            angle = R_PointToAngle2(0, 0, line->dx, line->dy); // front side dropoff
        else if (front == _g->floorz && back < _g->floorz - FRACUNIT * 24)
            angle = R_PointToAngle2(line->dx, line->dy, 0, 0); // back side dropoff
        else
            return true;

        // Move away from dropoff at a standard speed.
        // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
        _g->dropoff_deltax -= finesine(angle >> ANGLETOFINESHIFT) * 32;
        _g->dropoff_deltay += finecosine(angle >> ANGLETOFINESHIFT) * 32;
    }
    return true;
}

//
// Driver for above
//

static fixed_t P_AvoidDropoff(mobj_t *actor)
{
    int yh = ((_g->tmbbox[BOXTOP] = actor->y + (getMobjRadius(actor))) - _g->bmaporgy) >> MAPBLOCKSHIFT;
    int yl = ((_g->tmbbox[BOXBOTTOM] = actor->y - (getMobjRadius(actor))) - _g->bmaporgy) >> MAPBLOCKSHIFT;
    int xh = ((_g->tmbbox[BOXRIGHT] = actor->x + (getMobjRadius(actor))) - _g->bmaporgx) >> MAPBLOCKSHIFT;
    int xl = ((_g->tmbbox[BOXLEFT] = actor->x - (getMobjRadius(actor))) - _g->bmaporgx) >> MAPBLOCKSHIFT;
    int bx, by;

    _g->floorz = FIXED_Z_TO_FIXED32(actor->zr);            // remember floor height

    _g->dropoff_deltax = _g->dropoff_deltay = 0;

    // check lines

    _g->validcount++;
    #if !OLD_VALIDCOUNT
        clearArray32(_g->lineSectorChecked, (_g->numlines + 31) / 32);
    #endif
    for (bx = xl; bx <= xh; bx++)
        for (by = yl; by <= yh; by++)
            P_BlockLinesIterator(bx, by, PIT_AvoidDropoff); // all contacted lines

    return _g->dropoff_deltax | _g->dropoff_deltay; // Non-zero if movement prescribed
}
#endif
//
// P_NewChaseDir
//
// killough 9/8/98: Split into two functions
//

static void P_NewChaseDir(mobj_t *actor)
{
    mobj_t *target = getTarget(actor);
    fixed_t deltax = target->x - actor->x;
    fixed_t deltay = target->y - actor->y;

    // killough 8/8/98: sometimes move away from target, keeping distance
    //
    // 1) Stay a certain distance away from a friend, to avoid being in their way
    // 2) Take advantage over an enemy without missiles, by keeping distance

#if MOBJ_HAS_DROPOFFZ
    if (!demo_compatibility)
    {
        if (FIXED16_TO_FIXED32(actor->floorz16 - actor->dropoffz16) > FRACUNIT * 24 && actor->z <= FIXED16_TO_FIXED32(actor->floorz16) && !(getMobjFlags(actor) & (MF_DROPOFF | MF_FLOAT )) && P_AvoidDropoff(actor)) /* Move away from dropoff */
        {
            P_DoNewChaseDir(actor, _g->dropoff_deltax, _g->dropoff_deltay);

            // If moving away from dropoff, set movecount to 1 so that
            // small steps are taken to get monster away from dropoff.

            actor->movecount = 1;
            return;
        }
        else
        {
            fixed_t dist = P_AproxDistance(deltax, deltay);

            // Move away from friends when too close, except
            // in certain situations (e.g. a crowded lift)

            if ((getMobjFlags(actor) & getMobjFlags(target) & MF_FRIEND ) && distfriend << FRACBITS > dist && !P_IsOnLift(target) && !P_IsUnderDamage(actor))
            {
                deltax = -deltax, deltay = -deltay;
            }

        }
    }
 #endif
    P_DoNewChaseDir(actor, deltax, deltay);

    // If strafing, set movecount to strafecount so that old Doom
    // logic still works the same, except in the strafing part

}

//
// P_IsVisible
//
// killough 9/9/98: whether a target is visible to a monster
//

static boolean P_IsVisible(mobj_t *actor, mobj_t *mo, boolean allaround)
{
    fixed_t dist = P_AproxDistance(mo->x - actor->x, mo->y - actor->y);

    //Fix for icon of sin being blind, and having the others have bad depth perception ~Kippykip
    if (!demo_compatibility && (dist > LOOKRANGE) && actor->type != MT_BOSSSPIT && actor->type != MT_CYBORG && actor->type != MT_SPIDER)
        return false;

    if (!allaround)
    {
        angle_t an = R_PointToAngle2(actor->x, actor->y, mo->x, mo->y) - ANGLE16_TO_ANGLE32(actor->angle16);

        if (an > ANG90 && an < ANG270 && dist > MELEERANGE)
            return false;
    }
    return P_CheckSight(actor, mo);
}

//
// P_LookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//

static boolean P_LookForPlayers(mobj_t *actor, boolean allaround)
{
#if 1
  player_t *player;
  int stop, stopc, c;

  if (getMobjFlags(actor) & MF_FRIEND)
  {  // killough 9/9/98: friendly monsters go about players differently
    int anyone;

    // Go back to a player, no matter whether it's visible or not
    for (anyone = 0; anyone <= 1; anyone++)
    {
      for (c = 0; c < MAXPLAYERS; c++)
      {
        if (_g->playeringame[c] && _g->players[c].playerstate == PST_LIVE && (anyone || P_IsVisible(actor, _g->players[c].mo, allaround)))
        {
          //P_SetTarget(&actor->target, players[c].mo);
          actor->target_sptr = getShortPtr(_g->players[c].mo);

          // killough 12/98:
          // get out of refiring loop, to avoid hitting player accidentally

          if (getMobjInfo(actor)->missilestate)
          {
            P_SetMobjState(actor, getMobjInfo(actor)->seestate);
            setMobjFlagsBits(actor, MF_JUSTHIT, CLEAR_FLAGS);
            //setMobjFlags actor->flags &= ~MF_JUSTHIT;
          }

          return true;
        }
      }
      return false;
    }
  }

  // Change mask of 3 to (MAXPLAYERS-1) -- killough 2/15/98:
  stop = (actor->lastlook - 1) & (MAXPLAYERS - 1);

  c = 0;

  stopc =  2;       // killough 9/9/98

  for (;; actor->lastlook = (actor->lastlook + 1) & (MAXPLAYERS - 1))
  {
    if (!_g->playeringame[actor->lastlook])
    {
      continue;
    }
    // killough 2/15/98, 9/9/98:
    if (c++ == stopc || actor->lastlook == stop)  // done looking
    {
      // e6y
      // Fixed Boom incompatibilities. The following code was missed.
      // There are no more desyncs on Donce's demos on horror.wad
      return false;
    }

    player = &_g->players[actor->lastlook];

    if (player->health <= 0)
      continue;               // dead

    if (!P_IsVisible(actor, player->mo, allaround))
      continue;

    //P_SetTarget(&actor->target, player->mo);
    actor->target_sptr = getShortPtr(player->mo);

    /* killough 9/9/98: give monsters a threshold towards getting players
    * (we don't want it to be too easy for a player with dogs :)
    */

    actor->threshold = 60;
    return true;
  }
  return false;
#else
    // TODO: restore multiplayer
    player_t *player;

    if (_g->playeringame)
    {
        player = &_g->player;

        if (player->health <= 0)
            return false;               // dead

        if (!P_IsVisible(actor, player->mo, allaround))
            return false;

        //P_SetTarget(&actor->target, player->mo);
        actor->target_sptr = getShortPtr(player->mo);
        /* killough 9/9/98: give monsters a threshold towards getting players
         * (we don't want it to be too easy for a player with dogs :)
         */
        actor->threshold = 60;

        return true;
    }

    return false;
#endif
}

//
// P_LookForTargets
//
// killough 9/5/98: look for targets to go after, depending on kind of monster
//

static boolean P_LookForTargets(mobj_t *actor, int allaround)
{
    return P_LookForPlayers(actor, allaround);
}

//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie(mobj_t *mo)
{
    thinker_t *th;
    line_t junk;

    A_Fall(mo);

    // scan the remaining thinkers to see if all Keens are dead

    for (th = getThinkerNext(&thinkercap); th != &thinkercap; th=getThinkerNext(th))
    if (th->function_idx == THINKER_IDX(P_MobjThinker))
    {
        mobj_t *mo2 = (mobj_t *) th;
        if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        return;                           // other Keen not dead
    }

    junk.tag = 666;
    EV_DoDoor(&junk, dopen);
}

//
// ACTION ROUTINES
//

//
// A_Look
// Stay in state until a player is sighted.
//

void A_Look(mobj_t *actor)
{
    mobj_t *targ = getSectorSoundTarget(&_g->sectors[getMobjSubsector(actor)->sector_num]);
    actor->threshold = 0; // any shot will wake up

    /* killough 7/18/98:
     * Friendly monsters go after other monsters first, but
     * also return to player, without attacking them, if they
     * cannot find any targets. A marine's best friend :)
     */
    actor->pursuecount = 0;

    boolean seen = false;

    if (targ && (getMobjFlags(targ) & MF_SHOOTABLE ))
    {
        //actor->target = targ;
        actor->target_sptr = getShortPtr(targ);
        if (getMobjFlags(actor) & MF_AMBUSH)
        {
            demodbgprintf("Ambush\r\n");

            if (P_CheckSight(actor, getTarget(actor)))
            {
                seen = true;
                demodbgprintf("Seen CS\r\n");
            }
        }
        else
        {
            seen = true;
            demodbgprintf("Seen not ambush\r\n");

        }
    }
    if (!seen)
    {
        demodbgprintf("Check for player ");
    }
    if ((!seen) && (!P_LookForPlayers(actor, false)))
    {
        demodbgprintf("not found player\r\n");
        return;
    }

    // go into chase state
    demodbgprintf("Seen 1\r\n");
    if (getMobjInfo(actor)->seesound)
    {
        int sound;
        switch (getMobjInfo(actor)->seesound)
        {
            case sfx_posit1:
            case sfx_posit2:
            case sfx_posit3:
                sound = sfx_posit1 + P_Random() % 3;
                break;

            case sfx_bgsit1:
            case sfx_bgsit2:
                sound = sfx_bgsit1 + P_Random() % 2;
                break;

            default:
                sound = getMobjInfo(actor)->seesound;
                break;
        }
        if (actor->type == MT_SPIDER || actor->type == MT_CYBORG)
            S_StartSound(NULL, sound);          // full volume
        else
            S_StartSound(actor, sound);
    }
    P_SetMobjState(actor, getMobjInfo(actor)->seestate);
}

//
// A_Chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//

void A_Chase(mobj_t *actor)
{
    if (actor->reactiontime)
        actor->reactiontime--;

    if (actor->threshold)
    { /* modify target threshold */
        if (!getTarget(actor) || getTarget(actor)->health <= 0)
            actor->threshold = 0;
        else
            actor->threshold--;
    }

    /* turn towards movement direction if not there yet
     * killough 9/7/98: keep facing towards target if strafing or backing out
     */

    if (actor->movedir < 8)
    {
        int delta = (ANGLE16_TO_ANGLE32(actor->angle16) & (7 << 29)) - (actor->movedir << 29);
        actor->angle16 &= ANGLE32_TO_ANGLE16(7 << 29);
        if (delta > 0)
            actor->angle16 -= ANGLE32_TO_ANGLE16(ANG90 / 2);
        else if (delta < 0)
            actor->angle16 += ANGLE32_TO_ANGLE16(ANG90 / 2);
    }

    if (!getTarget(actor) || !(getMobjFlags(getTarget(actor)) & MF_SHOOTABLE ))
    {
        if (!P_LookForTargets(actor, true)) // look for a new target
            P_SetMobjState(actor, getMobjInfo(actor)->spawnstate); // no new target
        return;
    }

    // do not attack twice in a row
    if (getMobjFlags(actor) & MF_JUSTATTACKED)
    {
        setMobjFlagsBits(actor, MF_JUSTATTACKED, CLEAR_FLAGS);
        if (_g->gameskill != sk_nightmare)
            P_NewChaseDir(actor);
        return;
    }

    // check for melee attack
    if (getMobjInfo(actor)->meleestate && P_CheckMeleeRange(actor))
    {
        if (getMobjInfo(actor)->attacksound)
            S_StartSound(actor, getMobjInfo(actor)->attacksound);

        P_SetMobjState(actor, getMobjInfo(actor)->meleestate);
        /* killough 8/98: remember an attack
         * cph - DEMOSYNC? */
        if (!getMobjInfo(actor)->missilestate)
            setMobjFlagsBits(actor, MF_JUSTHIT, SET_FLAGS);
        return;
    }

    // check for missile attack
    if (getMobjInfo(actor)->missilestate)
    {
        if (!(_g->gameskill < sk_nightmare && actor->movecount))
        {
            if (P_CheckMissileRange(actor))
            {
                P_SetMobjState(actor, getMobjInfo(actor)->missilestate);
                setMobjFlagsBits(actor, MF_JUSTATTACKED, SET_FLAGS);
                return;
            }
        }
    }

    if (!actor->threshold)
    {
        if (actor->pursuecount)
            actor->pursuecount--;
        else
        {
            /* Our pursuit time has expired. We're going to think about
             * changing targets */
            actor->pursuecount = BASETHRESHOLD;

            /* Unless (we have a live target
             *         and it's not friendly
             *         and we can see it)
             *  try to find a new one; return if sucessful */

            if (!(getTarget(actor) && getTarget(actor)->health > 0 && (((((getMobjFlags(getTarget(actor)) ^ getMobjFlags(actor)) & MF_FRIEND ) || (!(getMobjFlags(actor) & MF_FRIEND ))) && P_CheckSight(actor, getTarget(actor))))) && P_LookForTargets(actor, true))
                return;

            /* (Current target was good, or no new target was found.)
             *
             * If monster is a missile-less friend, give up pursuit and
             * return to player, if no attacks have occurred recently.
             */

            if (!getMobjInfo(actor)->missilestate && (getMobjFlags(actor) & MF_FRIEND ))
            {
                if (getMobjFlags(actor) & MF_JUSTHIT) /* if recent action, */
                    setMobjFlagsBits(actor, MF_JUSTHIT, CLEAR_FLAGS); /* keep fighting */
                else if (P_LookForPlayers(actor, true)) /* else return to player */
                    return;
            }
        }
    }

    // chase towards player
    /*
    if (--actor->movecount < 0 || !P_SmartMove(actor))
        P_NewChaseDir(actor);
    
    */
    // rewritten so that movecount can be positive only
    if (actor->movecount == 0 || actor->movecount == MAX_MOVE_COUNT)
    {
        actor->movecount = MAX_MOVE_COUNT;
        P_NewChaseDir(actor);
    }
    else
    {
        actor->movecount--;
         if (!P_SmartMove(actor))
            P_NewChaseDir(actor);
    }

    // make active sound
    if (getMobjInfo(actor)->activesound && P_Random() < 3)
        S_StartSound(actor, getMobjInfo(actor)->activesound);
}

//
// A_FaceTarget
//
void A_FaceTarget(mobj_t *actor)
{
//  if (!actor->target)
    if (!actor->target_sptr)
        return;
    setMobjFlagsBits(actor, MF_AMBUSH, CLEAR_FLAGS);
    actor->angle16 = ANGLE32_TO_ANGLE16(R_PointToAngle2(actor->x, actor->y, getTarget(actor)->x, getTarget(actor)->y));
    if (getMobjFlags(getTarget(actor)) & MF_SHADOW)
    { // killough 5/5/98: remove dependence on order of evaluation:
        int t = P_Random();
        actor->angle16 += ANGLE32_TO_ANGLE16((t - P_Random()) << 21);
    }
}

//
// A_PosAttack
//

void A_PosAttack(mobj_t *actor)
{
    int angle, damage, slope, t;

    // if (!actor->target)
    if (!actor->target_sptr)
        return;
    A_FaceTarget(actor);
    angle = ANGLE16_TO_ANGLE32(actor->angle16);
    slope = P_AimLineAttack(actor, angle, MISSILERANGE, 0); /* killough 8/2/98 */
    S_StartSound(actor, sfx_pistol);

    // killough 5/5/98: remove dependence on order of evaluation:
    t = P_Random();
    angle += (t - P_Random()) << 20;
    damage = (P_Random() % 5 + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void A_SPosAttack(mobj_t *actor)
{
    int i, bangle, slope;

    // if (!actor->target)
    if (!actor->target_sptr)
        return;
    S_StartSound(actor, sfx_shotgn);
    A_FaceTarget(actor);
    bangle = ANGLE16_TO_ANGLE32(actor->angle16);
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE, 0); /* killough 8/2/98 */
    for (i = 0; i < 3; i++)
    {  // killough 5/5/98: remove dependence on order of evaluation:
        int t = P_Random();
        int angle = bangle + ((t - P_Random()) << 20);
        int damage = ((P_Random() % 5) + 1) * 3;
        P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

void A_CPosAttack(mobj_t *actor)
{
    int angle, bangle, damage, slope, t;

    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    S_StartSound(actor, sfx_shotgn);
    A_FaceTarget(actor);
    bangle = ANGLE16_TO_ANGLE32(actor->angle16);
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE, 0); /* killough 8/2/98 */

    // killough 5/5/98: remove dependence on order of evaluation:
    t = P_Random();
    angle = bangle + ((t - P_Random()) << 20);
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void A_CPosRefire(mobj_t *actor)
{
    // keep firing unless target got out of sight
    A_FaceTarget(actor);

    /* killough 12/98: Stop firing if a friend has gotten in the way */
    if (P_HitFriend(actor))
        goto stop;

    /* killough 11/98: prevent refiring on friends continuously */
    if (P_Random() < 40)
    {
        if (getTarget(actor) && (getMobjFlags(actor) & getMobjFlags(getTarget(actor)) & MF_FRIEND ))
            goto stop;
        else
            return;
    }

    if (!getTarget(actor) || getTarget(actor)->health <= 0 || !P_CheckSight(actor, getTarget(actor)))
        stop: P_SetMobjState(actor, getMobjInfo(actor)->seestate);
}

void A_SpidRefire(mobj_t *actor)
{
    // keep firing unless target got out of sight
    A_FaceTarget(actor);

    /* killough 12/98: Stop firing if a friend has gotten in the way */
    if (P_HitFriend(actor))
        goto stop;

    if (P_Random() < 10)
        return;

    // killough 11/98: prevent refiring on friends continuously
    if (!getTarget(actor) || getTarget(actor)->health <= 0 || (getMobjFlags(actor) & getMobjFlags(getTarget(actor)) & MF_FRIEND ) || !P_CheckSight(actor, getTarget(actor)))
        stop: P_SetMobjState(actor, getMobjInfo(actor)->seestate);
}

void A_BspiAttack(mobj_t *actor)
{
    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    A_FaceTarget(actor);
    P_SpawnMissile(actor, getTarget(actor), MT_ARACHPLAZ); // launch a missile
}

//
// A_TroopAttack
//

void A_TroopAttack(mobj_t *actor)
{
    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    A_FaceTarget(actor);
    if (P_CheckMeleeRange(actor))
    {
        int damage;
        S_StartSound(actor, sfx_claw);
        damage = (P_Random() % 8 + 1) * 3;
        P_DamageMobj(getTarget(actor), actor, actor, damage);
        return;
    }
    P_SpawnMissile(actor, getTarget(actor), MT_TROOPSHOT); // launch a missile
}

void A_SargAttack(mobj_t *actor)
{
    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    A_FaceTarget(actor);
    if (P_CheckMeleeRange(actor))
    {
        int damage = ((P_Random() % 10) + 1) * 4;
        P_DamageMobj(getTarget(actor), actor, actor, damage);
    }
}

void A_HeadAttack(mobj_t *actor)
{
    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    A_FaceTarget(actor);
    if (P_CheckMeleeRange(actor))
    {
        int damage = (P_Random() % 6 + 1) * 10;
        P_DamageMobj(getTarget(actor), actor, actor, damage);
        return;
    }
    P_SpawnMissile(actor, getTarget(actor), MT_HEADSHOT); // launch a missile
}

void A_CyberAttack(mobj_t *actor)
{
    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    A_FaceTarget(actor);
    S_StartSound(actor, sfx_rlaunc);
    P_SpawnMissile(actor, getTarget(actor), MT_ROCKET);
}

void A_BruisAttack(mobj_t *actor)
{
    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    if (P_CheckMeleeRange(actor))
    {
        int damage;
        S_StartSound(actor, sfx_claw);
        damage = (P_Random() % 8 + 1) * 10;
        P_DamageMobj(getTarget(actor), actor, actor, damage);
        return;
    }
    P_SpawnMissile(actor, getTarget(actor), MT_BRUISERSHOT); // launch a missile
}

//
// A_SkelMissile
//

void A_SkelMissile(mobj_t *actor)
{
    mobj_t *mo;

    //if (!actor->target)
    if (!actor->target_sptr)
        return;

    A_FaceTarget(actor);
    actor->zr += FIXED32_TO_FIXED_Z(16 * FRACUNIT);      // so missile spawns higher
    mo = P_SpawnMissile(actor, getTarget(actor), MT_TRACER);
    actor->zr -= FIXED32_TO_FIXED_Z(16 * FRACUNIT);      // back to normal

    mo->x += mo->momx;
    mo->y += mo->momy;
    //P_SetTarget(&mo->tracer, actor->target);
    mo->tracer_sptr = actor->target_sptr;
}

static const int TRACEANGLE = 0xc000000;

void A_Tracer(mobj_t *actor)
{
    angle_t exact;
    fixed_t dist;
    fixed_t slope;
    mobj_t *dest;
    mobj_t *th;

    /* killough 1/18/98: this is why some missiles do not have smoke
     * and some do. Also, internal demos start at random gametics, thus
     * the bug in which revenants cause internal demos to go out of sync.
     *
     * killough 3/6/98: fix revenant internal demo bug by subtracting
     * levelstarttic from gametic.
     *
     * killough 9/29/98: use new "basetic" so that demos stay in sync
     * during pauses and menu activations, while retaining old demo sync.
     *
     * leveltime would have been better to use to start with in Doom, but
     * since old demos were recorded using gametic, we must stick with it,
     * and improvise around it (using leveltime causes desync across levels).
     */

    if ((_g->gametic - _g->basetic) & 3)
        return;

    // spawn a puff of smoke behind the rocket
    P_SpawnPuff(actor->x, actor->y, FIXED_Z_TO_FIXED32(actor->zr));

    th = P_SpawnMobj(actor->x - actor->momx, actor->y - actor->momy, FIXED_Z_TO_FIXED32(actor->zr), MT_SMOKE);

    th->momz16 = FIXED32_TO_FIXED_MOMZ(FRACUNIT);
    th->tics -= P_Random() & 3;
    if (th->tics < 1)
        th->tics = 1;

    // adjust direction
    dest = getTracer(actor);

    if (!dest || dest->health <= 0)
        return;

    // change angle
    exact = R_PointToAngle2(actor->x, actor->y, dest->x, dest->y);

    if (ANGLE32_TO_ANGLE16(exact) != actor->angle16)
    {
        if (exact - ANGLE16_TO_ANGLE32(actor->angle16) > 0x80000000)
        {
            actor->angle16 -= ANGLE32_TO_ANGLE16(TRACEANGLE);
            if (exact - ANGLE16_TO_ANGLE32(actor->angle16) < 0x80000000)
                actor->angle16 = ANGLE32_TO_ANGLE16(exact);
        }
        else
        {
            actor->angle16 += ANGLE32_TO_ANGLE16(TRACEANGLE);
            if (exact - ANGLE16_TO_ANGLE32(actor->angle16) > 0x80000000)
                actor->angle16 = ANGLE32_TO_ANGLE16(exact);
        }
    }

    exact = ANGLE16_TO_ANGLE32(actor->angle16) >> ANGLETOFINESHIFT;
    actor->momx = FixedMul(getMobjInfo(actor)->speed, finecosine(exact));
    actor->momy = FixedMul(getMobjInfo(actor)->speed, finesine(exact));

    // change slope
    dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);

    dist = dist / getMobjInfo(actor)->speed;

    if (dist < 1)
        dist = 1;

    slope = (FIXED_Z_TO_FIXED32(dest->zr) + 40 * FRACUNIT - FIXED_Z_TO_FIXED32(actor->zr)) / dist;

    if (slope < FIXED_MOMZ_TO_FIXED32(actor->momz16))
        actor->momz16 -= FIXED32_TO_FIXED_MOMZ(FRACUNIT / 8);
    else
        actor->momz16 += FIXED32_TO_FIXED_MOMZ(FRACUNIT / 8);
}

void A_SkelWhoosh(mobj_t *actor)
{
    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    A_FaceTarget(actor);
    S_StartSound(actor, sfx_skeswg);
}

void A_SkelFist(mobj_t *actor)
{
    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    A_FaceTarget(actor);
    if (P_CheckMeleeRange(actor))
    {
        int damage = ((P_Random() % 10) + 1) * 6;
        S_StartSound(actor, sfx_skepch);
        P_DamageMobj(getTarget(actor), actor, actor, damage);
    }
}

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//

static boolean PIT_VileCheck(mobj_t *thing)
{
    int maxdist;
    boolean check;

    if (!(getMobjFlags(thing) & MF_CORPSE ))
        return true;        // not a monster

    if (thing->tics != -1)
        return true;        // not lying still yet

    if (getMobjInfo(thing)->raisestate == S_NULL)
        return true;        // monster doesn't have a raise state

    maxdist = getMobjInfo(thing)->radius + mobjinfo[MT_VILE].radius;

    if (D_abs(thing->x - _g->viletryx) > maxdist || D_abs(thing->y - _g->viletryy) > maxdist)
        return true;                // not actually touching

// Check to see if the radius and height are zero. If they are      // phares
// then this is a crushed monster that has been turned into a       //   |
// gib. One of the options may be to ignore this guy.               //   V

// Option 1: the original, buggy method, -> ghost (compatibility)
// Option 2: ressurect the monster, but not as a ghost
// Option 3: ignore the gib

//    if (Option3)                                                  //   ^
//        if ((thing->height == 0) && (thing->radius == 0))         //   |
//            return true;                                          // phares

#if OPTIMIZE_CORPSE
    // next-hack: added to save memory, when killing enemies. Note, this will break demo compatibility. Luckily, stock demos do not feature such thing
    // found object! we have to create a new one and delete previous one.
    if (!_g->respawnmonsters)
    {
        fixed_t cx = getMobjX(thing);
        fixed_t cy = getMobjY(thing);
        fixed_t cz = FIXED_Z_TO_FIXED32(thing->zr);
        int ctype = thing->type;

        int cstate = thing->state_idx;
        unsigned short cthinker = thing->thinker.function_idx;


        // delete old Mobj.
        P_RemoveMobj(thing);
        // create new one
        thing = P_SpawnMobj(cx, cy, cz, ctype);
        // we need to set state and thinker.
        thing->state_idx = cstate;
        thing->thinker.function_idx = cthinker;
        thing->tics = -1;
    }
#endif

    _g->corpsehit = thing;
    _g->corpsehit->momx = _g->corpsehit->momy = 0;
    {
        int height, radius;

        height = _g->corpsehit->height_s; // save temporarily
        radius = _g->corpsehit->radiusb; // save temporarily
        _g->corpsehit->height_s = getMobjInfo(_g->corpsehit)->height >> FRACBITS;
        _g->corpsehit->radiusb = getMobjInfo(_g->corpsehit)->radius >> FRACBITS;
        setMobjFlagsBits(_g->corpsehit, MF_SOLID, SET_FLAGS);
        check = P_CheckPosition(_g->corpsehit, _g->corpsehit->x, _g->corpsehit->y);
        _g->corpsehit->height_s = height; // restore
        _g->corpsehit->radiusb = radius; // restore                      //   ^
        setMobjFlagsBits(_g->corpsehit, MF_SOLID, CLEAR_FLAGS);
    }                                                             //   |
                                                                  // phares
    if (!check)
        return true;              // doesn't fit here
    return false;               // got one, so stop checking
}

//
// A_VileChase
// Check for ressurecting a body
//

void A_VileChase(mobj_t *actor)
{
    int xl, xh;
    int yl, yh;
    int bx, by;

    if (actor->movedir != DI_NODIR)
    {
        // check for corpses to raise
        _g->viletryx = actor->x + getMobjInfo(actor)->speed * xspeed[actor->movedir];
        _g->viletryy = actor->y + getMobjInfo(actor)->speed * yspeed[actor->movedir];

        xl = (_g->viletryx - _g->bmaporgx - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
        xh = (_g->viletryx - _g->bmaporgx + MAXRADIUS * 2) >> MAPBLOCKSHIFT;
        yl = (_g->viletryy - _g->bmaporgy - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
        yh = (_g->viletryy - _g->bmaporgy + MAXRADIUS * 2) >> MAPBLOCKSHIFT;

        for (bx = xl; bx <= xh; bx++)
        {
            for (by = yl; by <= yh; by++)
            {
                // Call PIT_VileCheck to check
                // whether object is a corpse
                // that canbe raised.
                if (!P_BlockThingsIterator(bx, by, PIT_VileCheck))
                {
                    const mobjinfo_t *info;

                    // got one!
                    mobj_t *temp = getTarget(actor);
                    //actor->target = _g->corpsehit;
                    actor->target_sptr = getShortPtr(_g->corpsehit);
                    A_FaceTarget(actor);
                    //actor->target = temp;
                    actor->target_sptr = getShortPtr(temp);
                    P_SetMobjState(actor, S_VILE_HEAL1);
                    S_StartSound(_g->corpsehit, sfx_slop);
                    info = getMobjInfo(_g->corpsehit);

                    P_SetMobjState(_g->corpsehit, info->raisestate);

                    {
                        _g->corpsehit->height_s = info->height >> FRACBITS; // fix Ghost bug
                        _g->corpsehit->radiusb = info->radius >> FRACBITS; // fix Ghost bug
                    }                                          // phares

                    /* killough 7/18/98:
                     * friendliness is transferred from AV to raised corpse
                     */
                    setMobjFlagsValue(_g->corpsehit, (info->flags & ~MF_FRIEND ) | (getMobjFlags(actor) & MF_FRIEND ));

                    if (!((getMobjFlags(_g->corpsehit) ^ MF_COUNTKILL ) & (MF_FRIEND | MF_COUNTKILL )))
                        _g->totallive++;

                    _g->corpsehit->health = info->spawnhealth;
//          P_SetTarget(&_g->corpsehit->target, NULL);  // killough 11/98
                    _g->corpsehit->target_sptr = 0;

                    //P_SetTarget(&_g->corpsehit->lastenemy, NULL);
                    #if MOBJ_HAS_LAST_ENEMY
                    _g->corpsehit->lastenemy_sptr = 0;
                    #endif
                    setMobjFlagsBits(_g->corpsehit, MF_JUSTHIT, CLEAR_FLAGS);

                    return;
                }
            }
        }
    }
    A_Chase(actor);  // Return to normal attack.
}

//
// A_VileStart
//

void A_VileStart(mobj_t *actor)
{
    S_StartSound(actor, sfx_vilatk);
}

//
// A_Fire
// Keep fire in front of player unless out of sight
//

void A_StartFire(mobj_t *actor)
{
    S_StartSound(actor, sfx_flamst);
    A_Fire(actor);
}

void A_FireCrackle(mobj_t *actor)
{
    S_StartSound(actor, sfx_flame);
    A_Fire(actor);
}

void A_Fire(mobj_t *actor)
{
    unsigned an;
    mobj_t *dest = getTracer(actor);

    if (!dest)
        return;

    // don't move it if the vile lost sight
    if (!P_CheckSight(getTarget(actor), dest))
        return;

    an = ANGLE16_TO_ANGLE32(dest->angle16) >> ANGLETOFINESHIFT;

    P_UnsetThingPosition(actor);
    actor->x = dest->x + FixedMul(24 * FRACUNIT, finecosine(an));
    actor->y = dest->y + FixedMul(24 * FRACUNIT, finesine(an));
    actor->zr = dest->zr;
    P_SetThingPosition(actor);
}

//
// A_VileTarget
// Spawn the hellfire
//

void A_VileTarget(mobj_t *actor)
{
    mobj_t *fog;

    //if (!actor->target)
    if (!actor->target_sptr)
        return;

    A_FaceTarget(actor);

    // killough 12/98: fix Vile fog coordinates // CPhipps - compatibility optioned
    fog = P_SpawnMobj(getTarget(actor)->x, getTarget(actor)->y, FIXED_Z_TO_FIXED32(getTarget(actor)->zr), MT_FIRE);

    //P_SetTarget(&actor->tracer, fog);
    actor->tracer_sptr = getShortPtr(fog);
    //P_SetTarget(&fog->target, actor);
    fog->target_sptr = getShortPtr(actor);
    //P_SetTarget(&fog->tracer, actor->target);
    fog->tracer_sptr = actor->target_sptr; //getShortPtr(actor->target);
    A_Fire(fog);
}

//
// A_VileAttack
//

void A_VileAttack(mobj_t *actor)
{
    mobj_t *fire;
    int an;

    //if (!actor->target)
    if (!actor->target_sptr)
        return;

    A_FaceTarget(actor);

    if (!P_CheckSight(actor, getTarget(actor)))
        return;

    S_StartSound(actor, sfx_barexp);
    P_DamageMobj(getTarget(actor), actor, actor, 20);
    getTarget(actor)->momz16 = FIXED32_TO_FIXED_MOMZ(1000 * FRACUNIT / getMobjInfo(getTarget(actor))->mass);

    an = ANGLE16_TO_ANGLE32(actor->angle16) >> ANGLETOFINESHIFT;

    fire = getTracer(actor);

    if (!fire)
        return;

    // move the fire between the vile and the player
    fire->x = getTarget(actor)->x - FixedMul(24 * FRACUNIT, finecosine(an));
    fire->y = getTarget(actor)->y - FixedMul(24 * FRACUNIT, finesine(an));
    P_RadiusAttack(fire, actor, 70);
}

//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it.
//

#define FATSPREAD       (ANG90/8)

void A_FatRaise(mobj_t *actor)
{
    A_FaceTarget(actor);
    S_StartSound(actor, sfx_manatk);
}

void A_FatAttack1(mobj_t *actor)
{
    mobj_t *mo;
    int an;

    //if (!actor->target)
    if (!actor->target_sptr)
        return;

    A_FaceTarget(actor);

    // Change direction  to ...
    actor->angle16 += ANGLE32_TO_ANGLE16(FATSPREAD);

    P_SpawnMissile(actor, getTarget(actor), MT_FATSHOT);

    mo = P_SpawnMissile(actor, getTarget(actor), MT_FATSHOT);
    mo->angle16 += ANGLE32_TO_ANGLE16(FATSPREAD);
    an = ANGLE16_TO_ANGLE32(mo->angle16) >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(getMobjInfo(mo)->speed, finecosine(an));
    mo->momy = FixedMul(getMobjInfo(mo)->speed, finesine(an));
}

void A_FatAttack2(mobj_t *actor)
{
    mobj_t *mo;
    int an;

    //if (!actor->target)
    if (!actor->target_sptr)
        return;

    A_FaceTarget(actor);
    // Now here choose opposite deviation.
    actor->angle16 -= ANGLE32_TO_ANGLE16(FATSPREAD);
    P_SpawnMissile(actor, getTarget(actor), MT_FATSHOT);

    mo = P_SpawnMissile(actor, getTarget(actor), MT_FATSHOT);
    mo->angle16 -= ANGLE32_TO_ANGLE16(FATSPREAD * 2);
    an = ANGLE16_TO_ANGLE32(mo->angle16) >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(getMobjInfo(mo)->speed, finecosine(an));
    mo->momy = FixedMul(getMobjInfo(mo)->speed, finesine(an));
}

void A_FatAttack3(mobj_t *actor)
{
    mobj_t *mo;
    int an;

    //if (!actor->target)
    if (!actor->target_sptr)
        return;

    A_FaceTarget(actor);

    mo = P_SpawnMissile(actor, getTarget(actor), MT_FATSHOT);
    mo->angle16 -= ANGLE32_TO_ANGLE16(FATSPREAD / 2);
    an = ANGLE16_TO_ANGLE32(mo->angle16) >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(getMobjInfo(mo)->speed, finecosine(an));
    mo->momy = FixedMul(getMobjInfo(mo)->speed, finesine(an));

    mo = P_SpawnMissile(actor, getTarget(actor), MT_FATSHOT);
    mo->angle16 += ANGLE32_TO_ANGLE16(FATSPREAD / 2);
    an = ANGLE16_TO_ANGLE32(mo->angle16) >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(getMobjInfo(mo)->speed, finecosine(an));
    mo->momy = FixedMul(getMobjInfo(mo)->speed, finesine(an));
}

//
// SkullAttack
// Fly at the player like a missile.
//
#define SKULLSPEED              (20*FRACUNIT)

void A_SkullAttack(mobj_t *actor)
{
    mobj_t *dest;
    angle_t an;
    int dist;

    //if (!actor->target)
    if (!actor->target_sptr)
        return;

    dest = getTarget(actor);
    setMobjFlagsBits(actor, MF_SKULLFLY, SET_FLAGS);

    S_StartSound(actor, getMobjInfo(actor)->attacksound);
    A_FaceTarget(actor);
    an = ANGLE16_TO_ANGLE32(actor->angle16) >> ANGLETOFINESHIFT;
    actor->momx = FixedMul(SKULLSPEED, finecosine(an));
    actor->momy = FixedMul(SKULLSPEED, finesine(an));
    dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);
    dist = dist / SKULLSPEED;

    if (dist < 1)
        dist = 1;
    actor->momz16 = FIXED32_TO_FIXED_MOMZ((FIXED_Z_TO_FIXED32(dest->zr - actor->zr) + (getMobjHeight(dest) >> 1)) / dist);
}

//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//

static void A_PainShootSkull(mobj_t *actor, angle_t angle)
{
    fixed_t x, y, z;
    mobj_t *newmobj;
    angle_t an;
    int prestep;

// The original code checked for 20 skulls on the level,            // phares
// and wouldn't spit another one if there were. If not in           // phares
// compatibility mode, we remove the limit.                         // phares
    // phares
    // okay, there's room for another one

    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 * FRACUNIT + 3 * (getMobjInfo(actor)->radius + mobjinfo[MT_SKULL].radius) / 2;

    x = actor->x + FixedMul(prestep, finecosine(an));
    y = actor->y + FixedMul(prestep, finesine(an));
    z = FIXED_Z_TO_FIXED32(actor->zr) + 8 * FRACUNIT;
    //   V
    {
        // Check whether the Lost Soul is being fired through a 1-sided
        // wall or an impassible line, or a "monsters can't cross" line.
        // If it is, then we don't allow the spawn. This is a bug fix, but
        // it should be considered an enhancement, since it may disturb
        // existing demos, so don't do it in compatibility mode.
#if FIX_PAIN_SHOOT_SKULL
        if (Check_Sides(actor, x, y))
            return;
#endif
        newmobj = P_SpawnMobj(x, y, z, MT_SKULL);

        // Check to see if the new Lost Soul's z value is above the
        // ceiling of its new sector, or below the floor. If so, kill it.

        if ((newmobj->zr > FIXED16_TO_FIXED_Z((_g->ramsectors[getMobjSubsector(newmobj)->sector_num].ceilingheight16) - getMobjHeight(newmobj))) || (newmobj->zr < FIXED16_TO_FIXED_Z(_g->ramsectors[getMobjSubsector(newmobj)->sector_num].floorheight16)))
        {
            // kill it immediately
            P_DamageMobj(newmobj, actor, actor, 10000);
            return;                                               //   ^
        }                                                         //   |
    }                                                          // phares

    /* killough 7/20/98: PEs shoot lost souls with the same friendliness */
    setMobjFlagsValue(newmobj, (getMobjFlags(newmobj) & ~MF_FRIEND ) | (getMobjFlags(actor) & MF_FRIEND ));

    // Check for movements.
    // killough 3/15/98: don't jump over dropoffs:

    if (!P_TryMove(newmobj, newmobj->x, newmobj->y, false))
    {
        // kill it immediately
        P_DamageMobj(newmobj, actor, actor, 10000);
        return;
    }

    //P_SetTarget(&newmobj->target, actor->target);
    newmobj->target_sptr = actor->target_sptr;
    A_SkullAttack(newmobj);
}

//
// A_PainAttack
// Spawn a lost soul and launch it at the target
//

void A_PainAttack(mobj_t *actor)
{
    //if (!actor->target)
    if (!actor->target_sptr)
        return;
    A_FaceTarget(actor);
    A_PainShootSkull(actor, ANGLE16_TO_ANGLE32(actor->angle16));
}

void A_PainDie(mobj_t *actor)
{
    A_Fall(actor);
    A_PainShootSkull(actor, ANGLE16_TO_ANGLE32(actor->angle16) + ANG90);
    A_PainShootSkull(actor, ANGLE16_TO_ANGLE32(actor->angle16) + ANG180);
    A_PainShootSkull(actor, ANGLE16_TO_ANGLE32(actor->angle16) + ANG270);
}

void A_Scream(mobj_t *actor)
{
    int sound;

    switch (getMobjInfo(actor)->deathsound)
    {
        case 0:
            return;

        case sfx_podth1:
        case sfx_podth2:
        case sfx_podth3:
            sound = sfx_podth1 + P_Random() % 3;
            break;

        case sfx_bgdth1:
        case sfx_bgdth2:
            sound = sfx_bgdth1 + P_Random() % 2;
            break;

        default:
            sound = getMobjInfo(actor)->deathsound;
            break;
    }

    // Check for bosses.
    if (actor->type == MT_SPIDER || actor->type == MT_CYBORG)
        S_StartSound(NULL, sound); // full volume
    else
        S_StartSound(actor, sound);
}

void A_XScream(mobj_t *actor)
{
    S_StartSound(actor, sfx_slop);
}

void A_Pain(mobj_t *actor)
{
    if (getMobjInfo(actor)->painsound)
        S_StartSound(actor, getMobjInfo(actor)->painsound);
}

void A_Fall(mobj_t *actor)
{
    // actor is on ground, it can be walked over
    setMobjFlagsBits(actor, MF_SOLID, CLEAR_FLAGS);
}

//
// A_Explode
//
void A_Explode(mobj_t *thingy)
{
    P_RadiusAttack(thingy, getTarget(thingy), 128);
}

//
// A_BossDeath
// Possibly trigger special effects
// if on first boss level
//

void A_BossDeath(mobj_t *mo)
{
    thinker_t *th;
    line_t junk;

    if (_g->gamemode == commercial)
    {
        if (_g->gamemap != 7)
            return;

        if ((mo->type != MT_FATSO) && (mo->type != MT_BABY))
            return;
    }
    else
    {
        {
            switch (_g->gameepisode)
            {
                case 1:
                    if (_g->gamemap != 8)
                        return;

                    if (mo->type != MT_BRUISER)
                        return;
                    break;

                case 2:
                    if (_g->gamemap != 8)
                        return;

                    if (mo->type != MT_CYBORG)
                        return;
                    break;

                case 3:
                    if (_g->gamemap != 8)
                        return;

                    if (mo->type != MT_SPIDER)
                        return;

                    break;

                case 4:
                    switch (_g->gamemap)
                    {
                        case 6:
                            if (mo->type != MT_CYBORG)
                                return;
                            break;

                        case 8:
                            if (mo->type != MT_SPIDER)
                                return;
                            break;

                        default:
                            return;
                    }
                    break;

                default:
                    if (_g->gamemap != 8)
                        return;
                    break;
            }
        }

    }
    int i;
    for (i = 0; i < MAXPLAYERS; i++)
    {
      if (_g->playeringame[i] && _g->players[i].health > 0)
        break;
    }
    if (i == MAXPLAYERS)
    {
      return;     // no one left alive, so do not end game
    }

    // scan the remaining thinkers to see
    // if all bosses are dead
    for (th = getThinkerNext(&thinkercap); th != &thinkercap; th= getThinkerNext(th))
    if (th->function_idx == THINKER_IDX(P_MobjThinker))
    {
        mobj_t *mo2 = (mobj_t *) th;
        if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        return;         // other boss not dead
    }

    // victory!
    if ( _g->gamemode == commercial)
    {
        if (_g->gamemap == 7)
        {
            if (mo->type == MT_FATSO)
            {
                junk.tag = 666;
                EV_DoFloor(&junk, lowerFloorToLowest);
                return;
            }

            if (mo->type == MT_BABY)
            {
                junk.tag = 667;
                EV_DoFloor(&junk, raiseToTexture);
                return;
            }
        }
    }
    else
    {
        switch (_g->gameepisode)
        {
            case 1:
                junk.tag = 666;
                EV_DoFloor(&junk, lowerFloorToLowest);
                return;

            case 4:
                switch (_g->gamemap)
                {
                    case 6:
                        junk.tag = 666;
                        EV_DoDoor(&junk, blazeOpen);
                        return;

                    case 8:
                        junk.tag = 666;
                        EV_DoFloor(&junk, lowerFloorToLowest);
                        return;
                }
        }
    }
    G_ExitLevel();
}

void A_Hoof(mobj_t *mo)
{
    S_StartSound(mo, sfx_hoof);
    A_Chase(mo);
}

void A_Metal(mobj_t *mo)
{
    S_StartSound(mo, sfx_metal);
    A_Chase(mo);
}

void A_BabyMetal(mobj_t *mo)
{
    S_StartSound(mo, sfx_bspwlk);
    A_Chase(mo);
}

void A_OpenShotgun2(player_t *player, pspdef_t *psp)
{
    (void) psp;
    S_StartSound(player->mo, sfx_dbopn);
}

void A_LoadShotgun2(player_t *player, pspdef_t *psp)
{
    (void) psp;
    S_StartSound(player->mo, sfx_dbload);
}

void A_CloseShotgun2(player_t *player, pspdef_t *psp)
{
    S_StartSound(player->mo, sfx_dbcls);
    A_ReFire(player, psp);
}

// killough 3/26/98: initialize icon landings at level startup,
// rather than at boss wakeup, to prevent savegame-related crashes

        void P_SpawnBrainTargets(void) // killough 3/26/98: renamed old function
        {
            thinker_t *thinker;

            // find all the target spots
            _g->numbraintargets = 0;
            _g->brain.targeton = 0;
            _g->brain.easy = 0; // killough 3/26/98: always init easy to 0

            for (thinker = getThinkerNext(&thinkercap);
            thinker != &thinkercap;
            thinker = getThinkerNext(thinker))
            if (thinker->function_idx == THINKER_IDX(P_MobjThinker))
            {
                mobj_t *m = (mobj_t *) thinker;

                if (m->type == MT_BOSSTARGET )
                {   // killough 2/7/98: remove limit on icon landings:
                    if (_g->numbraintargets >= _g->numbraintargets_alloc)
                    _g->braintargets = realloc(_g->braintargets,
                    (_g->numbraintargets_alloc = _g->numbraintargets_alloc ?
                            _g->numbraintargets_alloc*2 : 32) *sizeof *_g->braintargets);
                    _g->braintargets[_g->numbraintargets++] = m;
                }
            }
        }

void A_BrainAwake(mobj_t *mo)
{
    (void) mo;
    S_StartSound(NULL, sfx_bossit); // killough 3/26/98: only generates sound now
}

void A_BrainPain(mobj_t *mo)
{
    (void) mo;
    S_StartSound(NULL, sfx_bospn);
}

void A_BrainScream(mobj_t *mo)
{
    int x;
    for (x = mo->x - 196 * FRACUNIT; x < mo->x + 320 * FRACUNIT;
            x += FRACUNIT * 8)
    {
        int y = mo->y - 320 * FRACUNIT;
        int z = 128 + P_Random() * 2 * FRACUNIT;
        mobj_t *th = P_SpawnMobj(x, y, z, MT_ROCKET);
        // TODO next-hack: after momz16, this will produce some quantization..
        th->momz16 = FIXED32_TO_FIXED_MOMZ(P_Random() * 512);
        P_SetMobjState(th, S_BRAINEXPLODE1);
        th->tics -= P_Random() & 7;
        if (th->tics < 1)
            th->tics = 1;
    }
    S_StartSound(NULL, sfx_bosdth);
}

void A_BrainExplode(mobj_t *mo)
{  // killough 5/5/98: remove dependence on order of evaluation:
    int t = P_Random();
    int x = mo->x + (t - P_Random()) * 2048;
    int y = mo->y;
    int z = 128 + P_Random() * 2 * FRACUNIT;
    mobj_t *th = P_SpawnMobj(x, y, z, MT_ROCKET);
    // TODO next-hack: after mom16 this will produce some quantization...
    th->momz16 = FIXED32_TO_FIXED_MOMZ(P_Random() * 512);
    P_SetMobjState(th, S_BRAINEXPLODE1);
    th->tics -= P_Random() & 7;
    if (th->tics < 1)
        th->tics = 1;
}

void A_BrainDie(mobj_t *mo)
{
    (void) mo;
    G_ExitLevel();
}

void A_BrainSpit(mobj_t *mo)
{
    mobj_t *targ, *newmobj;

    if (!_g->numbraintargets)     // killough 4/1/98: ignore if no targets
        return;

    _g->brain.easy ^= 1;          // killough 3/26/98: use brain struct
    if (_g->gameskill <= sk_easy && !_g->brain.easy)
        return;

    // shoot a cube at current target
    targ = _g->braintargets[_g->brain.targeton++]; // killough 3/26/98:
    _g->brain.targeton %= _g->numbraintargets;   // Use brain struct for targets

    // spawn brain missile
    newmobj = P_SpawnMissile(mo, targ, MT_SPAWNSHOT);
    //P_SetTarget(&newmobj->target, targ);
    newmobj->target_sptr = getShortPtr(targ);
    newmobj->reactiontime = (short) (((targ->y - mo->y) / newmobj->momy) / getMobjState(newmobj)->tics);

    // killough 7/18/98: brain friendliness is transferred
    setMobjFlagsValue(newmobj, (getMobjFlags(newmobj) & ~MF_FRIEND ) | (getMobjFlags(mo) & MF_FRIEND ));

    S_StartSound(NULL, sfx_bospit);
}

// travelling cube sound
void A_SpawnSound(mobj_t *mo)
{
    S_StartSound(mo, sfx_boscub);
    A_SpawnFly(mo);
}

void A_SpawnFly(mobj_t *mo)
{
    mobj_t *newmobj;
    mobj_t *fog;
    mobj_t *targ;
    int r;
    mobjtype_t type;

    if (--mo->reactiontime)
        return;     // still flying

    targ = getTarget(mo);

    // First spawn teleport fog.
    fog = P_SpawnMobj(targ->x, targ->y, FIXED_Z_TO_FIXED32(targ->zr), MT_SPAWNFIRE);
    S_StartSound(fog, sfx_telept);

    // Randomly select monster to spawn.
    r = P_Random();

    // Probability distribution (kind of :), decreasing likelihood.
    if (r < 50)
        type = MT_TROOP;
    else if (r < 90)
        type = MT_SERGEANT;
    else if (r < 120)
        type = MT_SHADOWS;
    else if (r < 130)
        type = MT_PAIN;
    else if (r < 160)
        type = MT_HEAD;
    else if (r < 162)
        type = MT_VILE;
    else if (r < 172)
        type = MT_UNDEAD;
    else if (r < 192)
        type = MT_BABY;
    else if (r < 222)
        type = MT_FATSO;
    else if (r < 246)
        type = MT_KNIGHT;
    else
        type = MT_BRUISER;

    newmobj = P_SpawnMobj(targ->x, targ->y, FIXED_Z_TO_FIXED32(targ->zr), type);

    /* killough 7/18/98: brain friendliness is transferred */
    setMobjFlagsValue(newmobj, (getMobjFlags(newmobj) & ~MF_FRIEND ) | (getMobjFlags(mo) & MF_FRIEND ));

    if (P_LookForTargets(newmobj, true)) /* killough 9/4/98 */
        P_SetMobjState(newmobj, getMobjInfo(newmobj)->seestate);

    // telefrag anything in this spot
    P_TeleportMove(newmobj, newmobj->x, newmobj->y, true); /* killough 8/9/98 */

    // remove self (i.e., cube).
    P_RemoveMobj(mo);
}

void A_PlayerScream(mobj_t *mo)
{
    int sound = sfx_pldeth;  // Default death sound.
    if (_g->gamemode != shareware && mo->health < -50)
        sound = sfx_pdiehi; // IF THE PLAYER DIES LESS THAN -50% WITHOUT GIBBING
    S_StartSound(mo, sound);
}

/* cph - MBF-added codepointer functions */

// killough 11/98: kill an object
void A_Die(mobj_t *actor)
{
    P_DamageMobj(actor, NULL, NULL, actor->health);
}

//
// A_Detonate
// killough 8/9/98: same as A_Explode, except that the damage is variable
//

void A_Detonate(mobj_t *mo)
{
    P_RadiusAttack(mo, getTarget(mo), getMobjInfo(mo)->damage);
}

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//

void A_Mushroom(mobj_t *actor)
{
    int i, j, n = getMobjInfo(actor)->damage;

    A_Explode(actor);  // First make normal explosion

    // Now launch mushroom cloud
    for (i = -n; i <= n; i += 8)
        for (j = -n; j <= n; j += 8)
        {
            mobj_t target = *actor, *mo;
            target.x += i << FRACBITS;    // Aim in many directions from source
            target.y += j << FRACBITS;
            target.zr += FIXED32_TO_FIXED_Z(P_AproxDistance(i, j) << (FRACBITS + 2)); // Aim up fairly high
            mo = P_SpawnMissile(actor, &target, MT_FATSHOT);  // Launch fireball
            mo->momx >>= 1;
            mo->momy >>= 1;                                // Slow it down a bit
            mo->momz16 >>= 1;
            setMobjFlagsBits(mo, MF_NOGRAVITY, CLEAR_FLAGS);   // Make debris fall under gravity
        }
}

//
// killough 11/98
//
// The following were inspired by Len Pitre
//
// A small set of highly-sought-after code pointers
//

void A_Spawn(mobj_t *mo)
{
    if (getMobjState(mo)->misc1)
    {
        P_SpawnMobj(mo->x, mo->y, (getMobjState(mo)->misc2 << FRACBITS) + FIXED_Z_TO_FIXED32(mo->zr), getMobjState(mo)->misc1 - 1);
    }
}

void A_Turn(mobj_t *mo)
{
    mo->angle16 += ANGLE32_TO_ANGLE16((unsigned int) (((uint_64_t) getMobjState(mo)->misc1 << 32) / 360));
}

void A_Face(mobj_t *mo)
{
    mo->angle16 = ANGLE32_TO_ANGLE16((unsigned int) (((uint_64_t) getMobjState(mo)->misc1 << 32) / 360));
}

void A_Scratch(mobj_t *mo)
{
//  mo->target && (A_FaceTarget(mo), P_CheckMeleeRange(mo)) ?
    mo->target_sptr && (A_FaceTarget(mo), P_CheckMeleeRange(mo)) ?
            getMobjState(mo)->misc2 ? S_StartSound(mo, getMobjState(mo)->misc2) : (void) 0, P_DamageMobj(getTarget(mo), mo, mo, getMobjState(mo)->misc1) : (void) 0;
}

void A_PlaySound(mobj_t *mo)
{
    S_StartSound(getMobjState(mo)->misc2 ? NULL : mo, getMobjState(mo)->misc1);
}

void A_RandomJump(mobj_t *mo)
{
    if (P_Random() < getMobjState(mo)->misc2)
        P_SetMobjState(mo, getMobjState(mo)->misc1);
}

