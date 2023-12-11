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
 *  Movement, collision handling.
 *  Shooting and aiming.
 *
 *  next-hack: modified code to work with short pointers.
 *             many memory optimization
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "r_main.h"
#include "p_mobj.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_setup.h"
#include "p_spec.h"
#include "s_sound.h"
#include "sounds.h"
#include "p_inter.h"
#include "m_random.h"
#include "m_bbox.h"
#include "lprintf.h"

#include "global_data.h"
#include "utility_functions.h"

//
// TELEPORT MOVE
//

//
// PIT_StompThing
//

boolean PIT_StompThing(mobj_t *thing)
{
    fixed_t blockdist;

    // phares 9/10/98: moved this self-check to start of routine

    // don't clip against self

    if (thing == _g->tmthing)
        return true;

    if (!(getMobjFlags(thing) & MF_SHOOTABLE )) // Can't shoot it? Can't stomp it!
        return true;

    blockdist = getMobjRadius(thing) + getMobjRadius(_g->tmthing);

//    if (D_abs(thing->x - _g->tmx) >= blockdist || D_abs(thing->y - _g->tmy) >= blockdist)
    if (D_abs(getMobjX(thing) - _g->tmx) >= blockdist || D_abs(getMobjY(thing) - _g->tmy) >= blockdist)
        return true; // didn't hit it

    // monsters don't stomp things except on boss level
    if (!_g->telefrag)  // killough 8/9/98: make consistent across all levels
        return false;

    P_DamageMobj(thing, _g->tmthing, _g->tmthing, 10000); // Stomp!

    return true;
}

//
// P_TeleportMove
//

boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, boolean boss)
{
    int xl;
    int xh;
    int yl;
    int yh;
    int bx;
    int by;

    if (getMobjFlags(thing) & MF_STATIC)
    {
        printf("P_TeleportMove static, blocking\r\n");
        while(1);
    }
    subsector_t *newsubsec;

    /* killough 8/9/98: make telefragging more consistent, preserve compatibility */
    _g->telefrag = getMobjPlayer(thing) || boss;

    // kill anything occupying the position

    _g->tmthing = thing;

    _g->tmx = x;
    _g->tmy = y;

    _g->tmbbox[BOXTOP] = y + getMobjRadius(_g->tmthing);
    _g->tmbbox[BOXBOTTOM] = y - getMobjRadius(_g->tmthing);
    _g->tmbbox[BOXRIGHT] = x + getMobjRadius(_g->tmthing);
    _g->tmbbox[BOXLEFT] = x - getMobjRadius(_g->tmthing);

    newsubsec = R_PointInSubsector(x, y);
    _g->ceilingline = NULL;

    // The base floor/ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.

    _g->tmfloorz16 = _g->tmdropoffz16 = _g->ramsectors[newsubsec->sector_num].floorheight16;
    _g->tmceilingz16 = _g->ramsectors[newsubsec->sector_num].ceilingheight16;
#if OLD_VALIDCOUNT
    _g->validcount++;
#endif
    _g->numspechit = 0;

    // stomp on any things contacted

    xl = (_g->tmbbox[BOXLEFT] - _g->bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    xh = (_g->tmbbox[BOXRIGHT] - _g->bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
    yl = (_g->tmbbox[BOXBOTTOM] - _g->bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    yh = (_g->tmbbox[BOXTOP] - _g->bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

    for (bx = xl; bx <= xh; bx++)
        for (by = yl; by <= yh; by++)
            if (!P_BlockThingsIterator(bx, by, PIT_StompThing))
                return false;

    // the move is ok,
    // so unlink from the old position & link into the new position

    P_UnsetThingPosition(thing);

    thing->floorz16 = _g->tmfloorz16;
    thing->ceilingz16 = _g->tmceilingz16;
    #if MOBJ_HAS_DROPOFFZ
    thing->dropoffz16 = _g->tmdropoffz16;        // killough 11/98
    #endif
    thing->x = x;
    thing->y = y;

    P_SetThingPosition(thing);

    return true;
}

//
// MOVEMENT ITERATOR FUNCTIONS
//
#if FIX_PAIN_SHOOT_SKULL
//                                                                  // phares
// PIT_CrossLine                                                    //   |
// Checks to see if a PE->LS trajectory line crosses a blocking     //   V
// line. Returns false if it does.
//
// tmbbox holds the bounding box of the trajectory. If that box
// does not touch the bounding box of the line in question,
// then the trajectory is not blocked. If the PE is on one side
// of the line and the LS is on the other side, then the
// trajectory is blocked.
//
// Currently this assumes an infinite line, which is not quite
// correct. A more correct solution would be to check for an
// intersection of the trajectory and the line, but that takes
// longer and probably really isn't worth the effort.
//

static  // killough 3/26/98: make static
boolean PIT_CrossLine(const line_t *ld)
{
    if (!(ld->flags & ML_TWOSIDED) || (ld->flags & (ML_BLOCKING | ML_BLOCKMONSTERS)))
        if (!(_g->tmbbox[BOXLEFT] > ld->bbox[BOXRIGHT] || _g->tmbbox[BOXRIGHT] < ld->bbox[BOXLEFT] || _g->tmbbox[BOXTOP] < ld->bbox[BOXBOTTOM] || _g->tmbbox[BOXBOTTOM] > ld->bbox[BOXTOP]))
            if (P_PointOnLineSide(_g->pe_x, _g->pe_y, ld) != P_PointOnLineSide(_g->ls_x, _g->ls_y, ld))
                return (false); // line blocks trajectory                   //   ^
    return (true); // line doesn't block trajectory                    //   |
}                                                                 // phares
#endif
/* killough 8/1/98: used to test intersection between thing and line
 * assuming NO movement occurs -- used to avoid sticky situations.
 */

static int untouched(const line_t *ld)
{
    fixed_t x, y, tmbbox[4];
    return (tmbbox[BOXRIGHT] = (x = getMobjX(_g->tmthing)) + getMobjRadius(_g->tmthing)) <= ld->bbox[BOXLEFT] || (tmbbox[BOXLEFT] = x - getMobjRadius(_g->tmthing)) >= ld->bbox[BOXRIGHT] || (tmbbox[BOXTOP] = (y = getMobjY(_g->tmthing)) + getMobjRadius(_g->tmthing)) <= ld->bbox[BOXBOTTOM] || (tmbbox[BOXBOTTOM] = y - getMobjRadius(_g->tmthing)) >= ld->bbox[BOXTOP] || P_BoxOnLineSide(tmbbox, ld) != -1;
}


//
// PIT_CheckLine
// Adjusts tmfloorz and tmceilingz as lines are contacted
//

static                                          // killough 3/26/98: make static
boolean PIT_CheckLine(const line_t *ld)
{
    if (_g->tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT] || _g->tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT] || _g->tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM] || _g->tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
        return true; // didn't hit it

    if (P_BoxOnLineSide(_g->tmbbox, ld) != -1)
        return true; // didn't hit it

    // A line has been hit

    // The moving thing's destination position will cross the given line.
    // If this should not be allowed, return false.
    // If the line is special, keep track of it
    // to process later if the move is proven ok.
    // NOTE: specials are NOT sorted by order,
    // so two special lines that are only 8 pixels apart
    // could be crossed in either order.

    // killough 7/24/98: allow player to move out of 1s wall, to prevent sticking
    if (!LN_BACKSECTOR(ld)) // one sided line
    {
        _g->blockline = ld;
        return _g->tmunstuck && !untouched(ld) && FixedMul(_g->tmx - getMobjX(_g->tmthing), ld->dy) > FixedMul(_g->tmy - getMobjY(_g->tmthing), ld->dx);
    }

    // killough 8/10/98: allow bouncing objects to pass through as missiles
    if (!(getMobjFlags(_g->tmthing) & (MF_MISSILE )))
    {
        if (ld->flags & ML_BLOCKING)           // explicitly blocking everything
            return _g->tmunstuck && !untouched(ld); // killough 8/1/98: allow escape

        // killough 8/9/98: monster-blockers don't affect friends
        if (!((getMobjFlags(_g->tmthing) & MF_FRIEND ) || getMobjPlayer(_g->tmthing)) && (ld->flags & ML_BLOCKMONSTERS))
            return false; // block monsters only
    }

    // set openrange, opentop, openbottom
    // these define a 'window' from one sector to another across this line

    P_LineOpening(ld);

    // adjust floor & ceiling heights

    if (_g->opentop < FIXED16_TO_FIXED32(_g->tmceilingz16))
    {
        _g->tmceilingz16 = FIXED32_TO_FIXED16(_g->opentop);
        _g->ceilingline = ld;
        _g->blockline = ld;
    }

    if (_g->openbottom > FIXED16_TO_FIXED32(_g->tmfloorz16))
    {
        _g->tmfloorz16 = FIXED32_TO_FIXED16(_g->openbottom);
        _g->floorline = ld;          // killough 8/1/98: remember floor linedef
        _g->blockline = ld;
    }

    if (_g->lowfloor < FIXED16_TO_FIXED32(_g->tmdropoffz16))
        _g->tmdropoffz16 = FIXED32_TO_FIXED16(_g->lowfloor);

    // if contacted a special line, add it to the list

    if (0 != LN_SPECIAL(ld))  // note: had to add "0 !=" to avoid annoying warning
    {
        // 1/11/98 killough: remove limit on lines hit, by array doubling
        if (_g->numspechit < 4)
        {
            _g->spechit[_g->numspechit++] = ld;
        }
    }

    return true;
}

//
// PIT_CheckThing
//

static boolean PIT_CheckThing(mobj_t *thing) // killough 3/26/98: make static
{
    fixed_t blockdist;
    int damage;

    // killough 11/98: add touchy things
    if (!(getMobjFlags(thing) & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE )))
        return true;
    // note: getMobjRadius automatically detects if we have a static object.
    blockdist = getMobjRadius(thing) + getMobjRadius(_g->tmthing);

    if (D_abs(getMobjX(thing) - _g->tmx) >= blockdist || D_abs(getMobjY(thing) - _g->tmy) >= blockdist)
        return true; // didn't hit it

    // killough 11/98:
    //
    // This test has less information content (it's almost always false), so it
    // should not be moved up to first, as it adds more overhead than it removes.

    // don't clip against self

    if (thing == _g->tmthing)
        return true;

    // check for skulls slamming into things

    if (getMobjFlags(_g->tmthing) & MF_SKULLFLY)
    {
        // A flying skull is smacking something.
        // Determine damage amount, and the skull comes to a dead stop.

        int damage = ((P_Random() % 8) + 1) * getMobjInfo(_g->tmthing)->damage;

        P_DamageMobj(thing, _g->tmthing, _g->tmthing, damage);

        setMobjFlagsBits(_g->tmthing, MF_SKULLFLY, CLEAR_FLAGS);
        _g->tmthing->momx = _g->tmthing->momy = _g->tmthing->momz16 = 0;

        P_SetMobjState(_g->tmthing, getMobjInfo(_g->tmthing)->spawnstate);

        return false;   // stop moving
    }

    // missiles can hit other things
    // killough 8/10/98: bouncing non-solid things can hit other things too

    if (getMobjFlags(_g->tmthing) & MF_MISSILE)
    {
        // see if it went over / under

        if (_g->tmthing->zr > thing->zr + FIXED32_TO_FIXED_Z(getMobjHeight(thing)))
            return true;    // overhead

        if (_g->tmthing->zr + FIXED32_TO_FIXED_Z(getMobjHeight(_g->tmthing)) < thing->zr)
            return true;    // underneath

        if (getTarget(_g->tmthing) && (getTarget(_g->tmthing)->type == thing->type || (getTarget(_g->tmthing)->type == MT_KNIGHT && thing->type == MT_BRUISER) || (getTarget(_g->tmthing)->type == MT_BRUISER && thing->type == MT_KNIGHT)))
        {
            if (thing == getTarget(_g->tmthing))
                return true;            // Don't hit same species as originator.
            else if (thing->type != MT_PLAYER) // Explode, but do no damage.
                return false;         // Let players missile other players.
        }

        // killough 8/10/98: if moving thing is not a missile, no damage
        // is inflicted, and momentum is reduced if object hit is solid.

        if (!(getMobjFlags(_g->tmthing) & MF_MISSILE ))
        {
            if (!(getMobjFlags(thing) & MF_SOLID ))
            {
                return true;
            }
            else
            {
                _g->tmthing->momx = -_g->tmthing->momx;
                _g->tmthing->momy = -_g->tmthing->momy;
                if (!(getMobjFlags(_g->tmthing) & MF_NOGRAVITY ))
                {
                    _g->tmthing->momx >>= 2;
                    _g->tmthing->momy >>= 2;
                }
                return false;
            }
        }

        if (!(getMobjFlags(thing) & MF_SHOOTABLE ))
            return !(getMobjFlags(thing) & MF_SOLID ); // didn't do any damage

        // damage / explode

        damage = ((P_Random() % 8) + 1) * getMobjInfo(_g->tmthing)->damage;
        P_DamageMobj(thing, _g->tmthing, getTarget(_g->tmthing), damage);

        // don't traverse any more
        return false;
    }

    // check for special pickup

    if (getMobjFlags(thing) & MF_SPECIAL)
    {
        unsigned int solid = getMobjFlags(thing) & MF_SOLID;
        if (getMobjFlags(_g->tmthing) & MF_PICKUP)
            P_TouchSpecialThing(thing, _g->tmthing); // can remove thing
        return !solid;
    }

    // killough 3/16/98: Allow non-solid moving objects to move through solid
    // ones, by allowing the moving thing (tmthing) to move if it's non-solid,
    // despite another solid thing being in the way.
    // killough 4/11/98: Treat no-clipping things as not blocking
    // ...but not in demo_compatibility mode
    // e6y
    // Correction of wrong return value with demo_compatibility.
    // There is no more synch on http://www.doomworld.com/sda/dwdemo/w303-115.zip
    // (with correction in setMobjInfoValue)
    if (demo_compatibility)
        return !(getMobjFlags(thing) & MF_SOLID );
    else
        return !(getMobjFlags(thing) & MF_SOLID ) || (((getMobjFlags(thing) & MF_NOCLIP ) || !(getMobjFlags(_g->tmthing) & MF_SOLID )));
    // return !(thing->flags & MF_SOLID);   // old code -- killough
}
#if FIX_PAIN_SHOOT_SKULL
// This routine checks for Lost Souls trying to be spawned      // phares
// across 1-sided lines, impassible lines, or "monsters can't   //   |
// cross" lines. Draw an imaginary line between the PE          //   V
// and the new Lost Soul spawn spot. If that line crosses
// a 'blocking' line, then disallow the spawn. Only search
// lines in the blocks of the blockmap where the bounding box
// of the trajectory line resides. Then check bounding box
// of the trajectory vs. the bounding box of each blocking
// line to see if the trajectory and the blocking line cross.
// Then check the PE and LS to see if they're on different
// sides of the blocking line. If so, return true, otherwise
// false.

boolean Check_Sides(mobj_t *actor, int x, int y)
{
    int bx, by, xl, xh, yl, yh;

    _g->pe_x = actor->x;
    _g->pe_y = actor->y;
    _g->ls_x = x;
    _g->ls_y = y;

    // Here is the bounding box of the trajectory

    _g->tmbbox[BOXLEFT] = _g->pe_x < x ? _g->pe_x : x;
    _g->tmbbox[BOXRIGHT] = _g->pe_x > x ? _g->pe_x : x;
    _g->tmbbox[BOXTOP] = _g->pe_y > y ? _g->pe_y : y;
    _g->tmbbox[BOXBOTTOM] = _g->pe_y < y ? _g->pe_y : y;

    // Determine which blocks to look in for blocking lines

    xl = (_g->tmbbox[BOXLEFT] - _g->bmaporgx) >> MAPBLOCKSHIFT;
    xh = (_g->tmbbox[BOXRIGHT] - _g->bmaporgx) >> MAPBLOCKSHIFT;
    yl = (_g->tmbbox[BOXBOTTOM] - _g->bmaporgy) >> MAPBLOCKSHIFT;
    yh = (_g->tmbbox[BOXTOP] - _g->bmaporgy) >> MAPBLOCKSHIFT;

    // xl->xh, yl->yh determine the mapblock set to search

    _g->validcount++; // prevents checking same line twice

    #if !OLD_VALIDCOUNT
        clearArray32(_g->lineSectorChecked, (_g->numlines + 31) / 32);
    #endif

    for (bx = xl; bx <= xh; bx++)
        for (by = yl; by <= yh; by++)
            if (!P_BlockLinesIterator(bx, by, PIT_CrossLine))
                return true;                                              //   ^
    return (false);                                                    //   |
}                                                                 // phares
#endif
//
// MOVEMENT CLIPPING
//

//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up).
//
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//

boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y)
{
    int xl;
    int xh;
    int yl;
    int yh;
    int bx;
    int by;
    subsector_t *newsubsec;

    _g->tmthing = thing;

    _g->tmx = x;
    _g->tmy = y;

    _g->tmbbox[BOXTOP] = y + getMobjRadius(_g->tmthing);
    _g->tmbbox[BOXBOTTOM] = y - getMobjRadius(_g->tmthing);
    _g->tmbbox[BOXRIGHT] = x + getMobjRadius(_g->tmthing);
    _g->tmbbox[BOXLEFT] = x - getMobjRadius(_g->tmthing);

    newsubsec = R_PointInSubsector(x, y);
    _g->floorline = _g->blockline = _g->ceilingline = NULL; // killough 8/1/98

    // Whether object can get out of a sticky situation:
    _g->tmunstuck = !(getMobjFlags(thing) & MF_STATIC) && getMobjPlayer(thing) && /* only players */
                     getMobjPlayer(thing)->mo == thing && !demo_compatibility; /* not under old demos */
    // The base floor / ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.

    _g->tmfloorz16 = _g->tmdropoffz16 = _g->ramsectors[newsubsec->sector_num].floorheight16;
    _g->tmceilingz16 = _g->ramsectors[newsubsec->sector_num].ceilingheight16;
#if OLD_VALIDCOUNT
    _g->validcount++;
#endif
    _g->numspechit = 0;

    

    if (getMobjFlags(_g->tmthing) & MF_NOCLIP)
        return true;

    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.

    xl = (_g->tmbbox[BOXLEFT] - _g->bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    xh = (_g->tmbbox[BOXRIGHT] - _g->bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
    yl = (_g->tmbbox[BOXBOTTOM] - _g->bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    yh = (_g->tmbbox[BOXTOP] - _g->bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

    for (bx = xl; bx <= xh; bx++)
        for (by = yl; by <= yh; by++)
            if (!P_BlockThingsIterator(bx, by, PIT_CheckThing))
                return false;

    // check lines

    xl = (_g->tmbbox[BOXLEFT] - _g->bmaporgx) >> MAPBLOCKSHIFT;
    xh = (_g->tmbbox[BOXRIGHT] - _g->bmaporgx) >> MAPBLOCKSHIFT;
    yl = (_g->tmbbox[BOXBOTTOM] - _g->bmaporgy) >> MAPBLOCKSHIFT;
    yh = (_g->tmbbox[BOXTOP] - _g->bmaporgy) >> MAPBLOCKSHIFT;

    #if !OLD_VALIDCOUNT
        clearArray32(_g->lineSectorChecked, (_g->numlines + 31) / 32);
    #endif

    for (bx = xl; bx <= xh; bx++)
        for (by = yl; by <= yh; by++)
            if (!P_BlockLinesIterator(bx, by, PIT_CheckLine))
                return false; // doesn't fit

    return true;
}

//
// P_TryMove
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff) // killough 3/15/98: allow dropoff as option
{
    fixed_t oldx;
    fixed_t oldy;
    if (getMobjFlags(thing) & (MF_STATIC))
    {
        printf("Try Move on static, blocking");
        while(1);
    }
    _g->felldown = _g->floatok = false;               // killough 11/98

    if (!P_CheckPosition(thing, x, y))
        return false;   // solid wall or thing

    if (demo_compatibility)
    {
        if (!(getMobjFlags(thing) & MF_NOCLIP ))
        {

            if (FIXED16_TO_FIXED32(_g->tmceilingz16 - _g->tmfloorz16) < getMobjHeight(thing)) // doesn't fit
                return false;	// doesn't fit

            _g->floatok = true;

            if ( !(getMobjFlags(thing) & MF_TELEPORT )
                 && FIXED16_TO_FIXED32(_g->tmceilingz16) - FIXED_Z_TO_FIXED32(thing->zr) < getMobjHeight(thing))
                return false;	// mobj must lower itself to fit

            if ( !(getMobjFlags(thing) & MF_TELEPORT )
                 && FIXED16_TO_FIXED32(_g->tmfloorz16) - FIXED_Z_TO_FIXED32(thing->zr) > 24*FRACUNIT )
                return false;	// too big a step up

            if ( !(getMobjFlags(thing) & (MF_DROPOFF|MF_FLOAT))
                 && FIXED16_TO_FIXED32(_g->tmfloorz16 - _g->tmdropoffz16) > 24*FRACUNIT )
                return false;	// don't stand over a dropoff
        }
    }
    else
    {
        if (!(getMobjFlags(thing) & MF_NOCLIP ))
        {
            // killough 7/26/98: reformatted slightly
            // killough 8/1/98: Possibly allow escape if otherwise stuck

            if (FIXED16_TO_FIXED32(_g->tmceilingz16 - _g->tmfloorz16) < getMobjHeight(thing) || // doesn't fit
            // mobj must lower to fit
            (_g->floatok = true, !(getMobjFlags(thing) & MF_TELEPORT ) && FIXED16_TO_FIXED32(_g->tmceilingz16) - FIXED_Z_TO_FIXED32(thing->zr) < getMobjHeight(thing)) ||
            // too big a step up
            (!(getMobjFlags(thing) & MF_TELEPORT ) && FIXED16_TO_FIXED32(_g->tmfloorz16) - FIXED_Z_TO_FIXED32(thing->zr) > 24 * FRACUNIT))
                return _g->tmunstuck && !(_g->ceilingline && untouched(_g->ceilingline)) && !(_g->floorline && untouched(_g->floorline));

            /* killough 3/15/98: Allow certain objects to drop off
             * killough 7/24/98, 8/1/98:
             * Prevent monsters from getting stuck hanging off ledges
             * killough 10/98: Allow dropoffs in controlled circumstances
             * killough 11/98: Improve symmetry of clipping on stairs
             */

            if (!(getMobjFlags(thing) & (MF_DROPOFF | MF_FLOAT )))
            {
                if (!dropoff || (dropoff == 2 &&  // large jump down (e.g. dogs)
                (FIXED16_TO_FIXED32(_g->tmfloorz16 - _g->tmdropoffz16) > 128 * FRACUNIT || !getTarget(thing) || getTarget(thing)->zr > FIXED16_TO_FIXED_Z(_g->tmdropoffz16))))
                {
                    if (FIXED16_TO_FIXED32(_g->tmfloorz16 - _g->tmdropoffz16) > 24 * FRACUNIT)
                        return false;
                }
                else
                { /* dropoff allowed -- check for whether it fell more than 24 */
                    _g->felldown = !(getMobjFlags(thing) & MF_NOGRAVITY ) && FIXED_Z_TO_FIXED32(thing->zr) - FIXED16_TO_FIXED32(_g->tmfloorz16) > 24 * FRACUNIT;
                }
            }
        }
    }
    // the move is ok,
    // so unlink from the old position and link into the new position

    P_UnsetThingPosition(thing);

    oldx = thing->x;
    oldy = thing->y;
    thing->floorz16 = _g->tmfloorz16;
    thing->ceilingz16 = _g->tmceilingz16;
    #if MOBJ_HAS_DROPOFFZ
    thing->dropoffz16 = _g->tmdropoffz16;  // killough 11/98: keep track of dropoffs
    #endif
    thing->x = x;
    thing->y = y;

    P_SetThingPosition(thing);

    // if any special lines were hit, do the effect

    if (!(getMobjFlags(thing) & (MF_TELEPORT | MF_NOCLIP )))
        while (_g->numspechit--)
        {
            if (0 != LN_SPECIAL(_g->spechit[_g->numspechit])) // see if the line was crossed
            {
                int oldside;
                if ((oldside = P_PointOnLineSide(oldx, oldy, _g->spechit[_g->numspechit])) != P_PointOnLineSide(thing->x, thing->y, _g->spechit[_g->numspechit]))
                    P_CrossSpecialLine(_g->spechit[_g->numspechit], oldside, thing);
            }
        }

    return true;
}

//
// P_ThingHeightClip
// Takes a valid thing and adjusts the thing->floorz,
// thing->ceilingz, and possibly thing->z.
// This is called for all nearby monsters
// whenever a sector changes height.
// If the thing doesn't fit,
// the z will be set to the lowest value
// and false will be returned.
//

boolean P_ThingHeightClip(mobj_t *thing)
{
    if (getMobjFlags(thing) & MF_STATIC)
    {
        if (!(getMobjFlags(thing) & MF_NOGRAVITY ))
        {
            P_CheckPosition(thing, getMobjX(thing), getMobjY(thing));
            // subsector_t*  newsubsec = R_PointInSubsector(thing->x, thing->y);
            // if it is not floating then put it on the floor
            thing->zr = FIXED16_TO_FIXED_Z(_g->tmfloorz16); // newsubsec->sector->floorheight;

        }
        return FIXED16_TO_FIXED32(_g->tmceilingz16 - _g->tmfloorz16) >= getMobjHeight(thing);
    }
    boolean onfloor;

    onfloor = (thing->zr == FIXED16_TO_FIXED_Z(thing->floorz16));

    P_CheckPosition(thing, thing->x, thing->y); // note: not MF_STATIC by design here!

    /* what about stranding a monster partially off an edge?
     * killough 11/98: Answer: see below (upset balance if hanging off ledge)
     */

    thing->floorz16 = _g->tmfloorz16;
    thing->ceilingz16 = _g->tmceilingz16;
    #if MOBJ_HAS_DROPOFFZ
    thing->dropoffz16 = _g->tmdropoffz16; /* killough 11/98: remember dropoffs */
    #endif
    if (onfloor)
    {

        // walking monsters rise and fall with the floor

        thing->zr = FIXED16_TO_FIXED_Z(thing->floorz16);
    }
    else
    {

        // don't adjust a floating monster unless forced to

        if (FIXED_Z_TO_FIXED32(thing->zr) + getMobjHeight(thing) > FIXED16_TO_FIXED32(thing->ceilingz16))
            thing->zr = FIXED16_TO_FIXED_Z(thing->ceilingz16) - getMobjHeight(thing);
    }

    return FIXED16_TO_FIXED32(thing->ceilingz16 - thing->floorz16) >= getMobjHeight(thing);
}

//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//

//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
// If the floor is icy, then you can bounce off a wall.             // phares
//

void P_HitSlideLine(const line_t *ld)
{
    int side;
    angle_t lineangle;
    angle_t moveangle;
    angle_t deltaangle;
    fixed_t movelen;
    fixed_t newlen;
    //   |
    // Under icy conditions, if the angle of approach to the wall     //   V
    // is more than 45 degrees, then you'll bounce and lose half
    // your momentum. If less than 45 degrees, you'll slide along
    // the wall. 45 is arbitrary and is believable.

    // Check for the special cases of horz or vert walls.

    /* killough 10/98: only bounce if hit hard (prevents wobbling)
     * cph - DEMOSYNC - should only affect players in Boom demos? */

    if (ld->slopetype == ST_HORIZONTAL)
    {
        _g->tmymove = 0; // no more movement in the Y direction
        return;
    }

    if (ld->slopetype == ST_VERTICAL)
    {                                                          // phares
        _g->tmxmove = 0; // no more movement in the X direction
        return;
    }

    // The wall is angled. Bounce if the angle of approach is         // phares
    // less than 45 degrees.                                          // phares

    side = P_PointOnLineSide(_g->slidemo->x, _g->slidemo->y, ld);

    lineangle = R_PointToAngle2(0, 0, ld->dx, ld->dy);
    if (side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, _g->tmxmove, _g->tmymove);

    // killough 3/2/98:
    // The moveangle+=10 breaks v1.9 demo compatibility in
    // some demos, so it needs demo_compatibility switch.
    if (!demo_compatibility)
        moveangle += 10; // prevents sudden path reversal due to        // phares
    // rounding error                              //   |
    deltaangle = moveangle - lineangle;                                 //   V
    movelen = P_AproxDistance(_g->tmxmove, _g->tmymove);
    //   |
    // phares
    if (deltaangle > ANG180)
        deltaangle += ANG180;

    //  I_Error ("SlideLine: ang>ANG180");

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;
    newlen = FixedMul(movelen, finecosine(deltaangle));
    _g->tmxmove = FixedMul(newlen, finecosine(lineangle));
    _g->tmymove = FixedMul(newlen, finesine(lineangle));
    // phares
}

//
// PTR_SlideTraverse
//

boolean PTR_SlideTraverse(intercept_t *in)
{
    const line_t *li;
    int index = (int) (in - intercepts);
    boolean isaline = interceptIsALine[index];
    //if (!in->isaline)
    if (!isaline)
        I_Error("PTR_SlideTraverse: not a line?");

    li = in->d.line;

    if (!(li->flags & ML_TWOSIDED))
    {
        if (P_PointOnLineSide(_g->slidemo->x, _g->slidemo->y, li))
            return true; // don't hit the back side
        goto isblocking;
    }

    // set openrange, opentop, openbottom.
    // These define a 'window' from one sector to another across a line

    P_LineOpening(li);

    if (_g->openrange < getMobjHeight(_g->slidemo))
        goto isblocking;
    // doesn't fit

    if (_g->opentop - FIXED_Z_TO_FIXED32(_g->slidemo->zr) < getMobjHeight(_g->slidemo))
        goto isblocking;
    // mobj is too high

    if (_g->openbottom - FIXED_Z_TO_FIXED32(_g->slidemo->zr) > 24 * FRACUNIT)
        goto isblocking;
    // too big a step up

    // this line doesn't block movement

    return true;

    // the line does block movement,
    // see if it is closer than best so far

    isblocking:

    if (in->frac < _g->bestslidefrac)
    {
        _g->bestslidefrac = in->frac;
        _g->bestslideline = li;
    }

    return false; // stop
}

//
// P_SlideMove
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess.
//
// killough 11/98: reformatted

void P_SlideMove(mobj_t *mo)
{
    int hitcount = 3;

    _g->slidemo = mo; // the object that's sliding

    do
    {
        fixed_t leadx, leady, trailx, traily;

        if (!--hitcount)
            goto stairstep;
        // don't loop forever

        // trace along the three leading corners

        if (mo->momx > 0)
            leadx = mo->x + getMobjRadius(mo), trailx = mo->x - getMobjRadius(mo);
        else
            leadx = mo->x - getMobjRadius(mo), trailx = mo->x + getMobjRadius(mo);

        if (mo->momy > 0)
            leady = mo->y + getMobjRadius(mo), traily = mo->y - getMobjRadius(mo);
        else
            leady = mo->y - getMobjRadius(mo), traily = mo->y + getMobjRadius(mo);

        _g->bestslidefrac = FRACUNIT + 1;

        P_PathTraverse(leadx, leady, leadx + mo->momx, leady + mo->momy,
        PT_ADDLINES, PTR_SlideTraverse);
        P_PathTraverse(trailx, leady, trailx + mo->momx, leady + mo->momy,
        PT_ADDLINES, PTR_SlideTraverse);
        P_PathTraverse(leadx, traily, leadx + mo->momx, traily + mo->momy,
        PT_ADDLINES, PTR_SlideTraverse);

        // move up to the wall

        if (_g->bestslidefrac == FRACUNIT + 1)
        {
            // the move must have hit the middle, so stairstep

            stairstep:

            /* killough 3/15/98: Allow objects to drop off ledges
             *
             * phares 5/4/98: kill momentum if you can't move at all
             * This eliminates player bobbing if pressed against a wall
             * while on ice.
             *
             * killough 10/98: keep buggy code around for old Boom demos
             *
             * cph 2000/09//23: buggy code was only in Boom v2.01
             */

            if (!P_TryMove(mo, mo->x, mo->y + mo->momy, true))
                P_TryMove(mo, mo->x + mo->momx, mo->y, true);

            break;
        }

        // fudge a bit to make sure it doesn't hit

        if ((_g->bestslidefrac -= 0x800) > 0)
        {
            fixed_t newx = FixedMul(mo->momx, _g->bestslidefrac);
            fixed_t newy = FixedMul(mo->momy, _g->bestslidefrac);

            // killough 3/15/98: Allow objects to drop off ledges

            if (!P_TryMove(mo, mo->x + newx, mo->y + newy, true))
                goto stairstep;
        }

        // Now continue along the wall.
        // First calculate remainder.

        _g->bestslidefrac = FRACUNIT - (_g->bestslidefrac + 0x800);

        if (_g->bestslidefrac > FRACUNIT)
            _g->bestslidefrac = FRACUNIT;

        if (_g->bestslidefrac <= 0)
            break;

        _g->tmxmove = FixedMul(mo->momx, _g->bestslidefrac);
        _g->tmymove = FixedMul(mo->momy, _g->bestslidefrac);

        P_HitSlideLine(_g->bestslideline); // clip the moves

        mo->momx = _g->tmxmove;
        mo->momy = _g->tmymove;

        /* killough 10/98: affect the bobbing the same way (but not voodoo dolls)
         * cph - DEMOSYNC? */
        if (getMobjPlayer(mo) && getMobjPlayer(mo)->mo == mo)
        {
            if (D_abs(getMobjPlayer(mo)->momx) > D_abs(_g->tmxmove))
                getMobjPlayer(mo)->momx = _g->tmxmove;
            if (D_abs(getMobjPlayer(mo)->momy) > D_abs(_g->tmymove))
                getMobjPlayer(mo)->momy = _g->tmymove;
        }
    }  // killough 3/15/98: Allow objects to drop off ledges:
    while (!P_TryMove(mo, mo->x + _g->tmxmove, mo->y + _g->tmymove, true));
}

//
// P_LineAttack
//

//
// PTR_AimTraverse
// Sets linetaget and aimslope when a target is aimed at.
//
boolean PTR_AimTraverse(intercept_t *in)
{
    const line_t *li;
    mobj_t *th;
    fixed_t slope;
    fixed_t thingtopslope;
    fixed_t thingbottomslope;
    fixed_t dist;
    int index = (int) (in - intercepts);
    boolean isaline = interceptIsALine[index];
    //if (in->isaline)
    if (isaline)
    {
        li = in->d.line;

        if (!(li->flags & ML_TWOSIDED))
            return false;   // stop

        // Crosses a two sided line.
        // A two sided line will restrict
        // the possible target ranges.

        P_LineOpening(li);

        if (_g->openbottom >= _g->opentop)
            return false;   // stop

        dist = FixedMul(_g->attackrange, in->frac);

        if (getRamSector(LN_FRONTSECTOR(li))->floorheight16 != getRamSector(LN_BACKSECTOR(li))->floorheight16)
        {
            slope = FixedDiv(_g->openbottom - _g->shootz, dist);

            if (slope > _g->bottomslope)
                _g->bottomslope = slope;
        }

        if (getRamSector(LN_FRONTSECTOR(li))->ceilingheight16 != getRamSector(LN_BACKSECTOR(li))->ceilingheight16)
        {
            slope = FixedDiv(_g->opentop - _g->shootz, dist);
            if (slope < _g->topslope)
                _g->topslope = slope;
        }

        if (_g->topslope <= _g->bottomslope)
            return false;   // stop

        return true;    // shot continues
    }

    // shoot a thing

    th = in->d.thing;
    if (th == _g->shootthing)
        return true;    // can't shoot self

    if (!(getMobjFlags(th) & MF_SHOOTABLE ))
        return true;    // corpse or something

    /* killough 7/19/98, 8/2/98:
     * friends don't aim at friends (except players), at least not first
     */
    if ((getMobjFlags(th) & getMobjFlags(_g->shootthing) & _g->aim_flags_mask) && !getMobjPlayer(th))
        return true;

    // check angles to see if the thing can be aimed at

    dist = FixedMul(_g->attackrange, in->frac);
    thingtopslope = FixedDiv(FIXED_Z_TO_FIXED32(th->zr) + getMobjHeight(th) - _g->shootz, dist);

    if (thingtopslope < _g->bottomslope)
        return true;    // shot over the thing

    thingbottomslope = FixedDiv(FIXED_Z_TO_FIXED32(th->zr) - _g->shootz, dist);

    if (thingbottomslope > _g->topslope)
        return true;    // shot under the thing

    // this thing can be hit!

    if (thingtopslope > _g->topslope)
        thingtopslope = _g->topslope;

    if (thingbottomslope < _g->bottomslope)
        thingbottomslope = _g->bottomslope;

    _g->aimslope = (thingtopslope + thingbottomslope) / 2;
    _g->linetarget = th;

    return false;   // don't go any farther
}

//
// PTR_ShootTraverse
//
boolean PTR_ShootTraverse(intercept_t *in)
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
    fixed_t frac;

    mobj_t *th;

    fixed_t slope;
    fixed_t dist;
    fixed_t thingtopslope;
    fixed_t thingbottomslope;
    int index = (int) (in - intercepts);
    boolean isaline = interceptIsALine[index];
    //if (in->isaline)
    if (isaline)
    {
        const line_t *li = in->d.line;

        if (0 != LN_SPECIAL(li))  // note: had to add "0 !=" to avoid annoying warning
            P_ShootSpecialLine(_g->shootthing, li);

        if (li->flags & ML_TWOSIDED)
        {  // crosses a two sided (really 2s) line
            P_LineOpening(li);
            dist = FixedMul(_g->attackrange, in->frac);

            // killough 11/98: simplify
           // e6y: emulation of missed back side on two-sided lines.
            // backsector can be NULL if overrun_missedbackside_emulate is 1
            if (!LN_BACKSECTOR(li))
            {
              if ((slope = FixedDiv(_g->openbottom - _g->shootz , dist)) <= _g->aimslope &&
                  (slope = FixedDiv(_g->opentop - _g->shootz , dist)) >= _g->aimslope)
                return true;      // shot continues
            }
            else if ((getRamSector(LN_FRONTSECTOR(li))->floorheight16 == getRamSector(LN_BACKSECTOR(li))->floorheight16 
            || (slope = FixedDiv(_g->openbottom - _g->shootz, dist)) <= _g->aimslope) 
            && (getRamSector(LN_FRONTSECTOR(li))->ceilingheight16 == getRamSector(LN_BACKSECTOR(li))->ceilingheight16 
            || (slope = FixedDiv(_g->opentop - _g->shootz, dist)) >= _g->aimslope))
                return true;      // shot continues
        }

        // hit line
        // position a bit closer

        frac = in->frac - FixedDiv(4 * FRACUNIT, _g->attackrange);
        x = _g->trace.x + FixedMul(_g->trace.dx, frac);
        y = _g->trace.y + FixedMul(_g->trace.dy, frac);
        z = _g->shootz + FixedMul(_g->aimslope, FixedMul(frac, _g->attackrange));

        if (LN_FRONTSECTOR(li)->ceilingpic == _g->skyflatnum)
        {
            // don't shoot the sky!

            if (z > FIXED16_TO_FIXED32(getRamSector(LN_FRONTSECTOR(li))->ceilingheight16))
                return false;

            // it's a sky hack wall

            if (LN_BACKSECTOR(li) && LN_BACKSECTOR(li)->ceilingpic == _g->skyflatnum)

                // fix bullet-eaters -- killough:
                // WARNING: Almost all demos will lose sync without this
                // demo_compatibility flag check!!! killough 1/18/98
                if (demo_compatibility || FIXED16_TO_FIXED32(getRamSector(LN_BACKSECTOR(li))->ceilingheight16) < z)
                    return false;
        }

        // Spawn bullet puffs.

        P_SpawnPuff(x, y, z);

        // don't go any farther

        return false;
    }

    // shoot a thing

    th = in->d.thing;
    if (th == _g->shootthing)
        return true;  // can't shoot self

    if (!(getMobjFlags(th) & MF_SHOOTABLE ))
        return true;  // corpse or something

    // check angles to see if the thing can be aimed at

    dist = FixedMul(_g->attackrange, in->frac);
    thingtopslope = FixedDiv(FIXED_Z_TO_FIXED32(th->zr) + getMobjHeight(th) - _g->shootz, dist);

    if (thingtopslope < _g->aimslope)
        return true;  // shot over the thing

    thingbottomslope = FixedDiv(FIXED_Z_TO_FIXED32(th->zr) - _g->shootz, dist);

    if (thingbottomslope > _g->aimslope)
        return true;  // shot under the thing

    // hit thing
    // position a bit closer

    frac = in->frac - FixedDiv(10 * FRACUNIT, _g->attackrange);

    x = _g->trace.x + FixedMul(_g->trace.dx, frac);
    y = _g->trace.y + FixedMul(_g->trace.dy, frac);
    z = _g->shootz + FixedMul(_g->aimslope, FixedMul(frac, _g->attackrange));

    // Spawn bullet puffs or blod spots,
    // depending on target type.
    if (getMobjFlags(in->d.thing) & MF_NOBLOOD)
        P_SpawnPuff(x, y, z);
    else
        P_SpawnBlood(x, y, z, _g->la_damage);

    if (_g->la_damage)
        P_DamageMobj(th, _g->shootthing, _g->shootthing, _g->la_damage);

    // don't go any farther
    return false;
}

//
// P_AimLineAttack
//
fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance, uint_64_t mask)
{
    fixed_t x2;
    fixed_t y2;

    angle >>= ANGLETOFINESHIFT;
    _g->shootthing = t1;

    x2 = t1->x + (distance >> FRACBITS) * finecosine(angle);
    y2 = t1->y + (distance >> FRACBITS) * finesine(angle);
    _g->shootz = FIXED_Z_TO_FIXED32(t1->zr) + (getMobjHeight(t1) >> 1) + 8 * FRACUNIT;

    // can't shoot outside view angles

    if (demo_compatibility) // nexthack: demo_compatibilty
    {
        _g->topslope = 100 * FRACUNIT / 160;       
        _g->bottomslope = -100 * FRACUNIT / 160;
    }
    else
    {
        _g->topslope = (SCREENHEIGHT / 2) * FRACUNIT / (SCREENWIDTH / 2);     
        _g->bottomslope = -(SCREENHEIGHT / 2) * FRACUNIT / (SCREENWIDTH / 2);
    }

    _g->attackrange = distance;
    _g->linetarget = NULL;

    /* killough 8/2/98: prevent friends from aiming at friends */
    _g->aim_flags_mask = mask;

    P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, PTR_AimTraverse);

    if (_g->linetarget)
        return _g->aimslope;

    return 0;
}

//
// P_LineAttack
// If damage == 0, it is just a test trace
// that will leave linetarget set.
//

void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope, int damage)
{
    fixed_t x2;
    fixed_t y2;

    angle >>= ANGLETOFINESHIFT;
    _g->shootthing = t1;
    _g->la_damage = damage;
    x2 = t1->x + (distance >> FRACBITS) * finecosine(angle);
    y2 = t1->y + (distance >> FRACBITS) * finesine(angle);
    _g->shootz = FIXED_Z_TO_FIXED32(t1->zr) + (getMobjHeight(t1) >> 1) + 8 * FRACUNIT;
    _g->attackrange = distance;
    _g->aimslope = slope;

    P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, PTR_ShootTraverse);
}

//
// USE LINES
//

boolean PTR_UseTraverse(intercept_t *in)
{
    int side;

    if (0 == LN_SPECIAL(in->d.line))  // note: had to use "0 ==" instead of using ! operator to avoid annoying warning
    {
        P_LineOpening(in->d.line);
        if (_g->openrange <= 0)
        {
            S_StartSound(_g->usething, sfx_noway);

            // can't use through a wall
            return false;
        }

        // not a special line, but keep checking

        return true;
    }

    side = 0;
    if (P_PointOnLineSide(getMobjX(_g->usething), getMobjY(_g->usething), in->d.line) == 1)
        side = 1;

    //  return false;   // don't use back side

    P_UseSpecialLine(_g->usething, in->d.line, side);

    //WAS can't use for than one special line in a row
    //jff 3/21/98 NOW multiple use allowed with enabling line flag

    return ((in->d.line->flags & ML_PASSUSE)) ?
    true : false;
}

// Returns false if a "oof" sound should be made because of a blocking
// linedef. Makes 2s middles which are impassable, as well as 2s uppers
// and lowers which block the player, cause the sound effect when the
// player tries to activate them. Specials are excluded, although it is
// assumed that all special linedefs within reach have been considered
// and rejected already (see P_UseLines).
//
// by Lee Killough
//

boolean PTR_NoWayTraverse(intercept_t *in)
{
    const line_t *ld = in->d.line;
    // This linedef
    return (0 != LN_SPECIAL(ld)) || !(                 // Ignore specials. note: had to add "0 !=" to avoid annoying warning
    (ld->flags & ML_BLOCKING) || (            // Always blocking
    P_LineOpening(ld),                      // Find openings
    _g->openrange <= 0 ||                       // No opening
    _g->openbottom > FIXED_Z_TO_FIXED32(_g->usething->zr) + 24 * FRACUNIT || // Too high it blocks
    _g->opentop < FIXED_Z_TO_FIXED32(_g->usething->zr) + getMobjHeight(_g->usething) // Too low it blocks
    ));
}

//
// P_UseLines
// Looks for special lines in front of the player to activate.
//
void P_UseLines(player_t *player)
{
    int angle;
    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    _g->usething = player->mo;

    angle = ANGLE16_TO_ANGLE32(player->mo->angle16) >> ANGLETOFINESHIFT;

    x1 = player->mo->x;
    y1 = player->mo->y;
    x2 = x1 + (USERANGE >> FRACBITS) * finecosine(angle);
    y2 = y1 + (USERANGE >> FRACBITS) * finesine(angle);

    // old code:
    //
    // P_PathTraverse ( x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse );
    //
    // This added test makes the "oof" sound work on 2s lines -- killough:

    if (P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse))
        if (!P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_NoWayTraverse))
            S_StartSound(_g->usething, sfx_noway);
}

//
// RADIUS ATTACK
//

//
// PIT_RadiusAttack
// "bombsource" is the creature
// that caused the explosion at "bombspot".
//

boolean PIT_RadiusAttack(mobj_t *thing)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t dist;

    /* killough 8/20/98: allow bouncers to take damage
     * (missile bouncers are already excluded with MF_NOBLOCKMAP)
     */

    if (!(getMobjFlags(thing) & MF_SHOOTABLE ))
        return true;

    // Boss spider and cyborg
    // take no damage from concussion.

    // killough 8/10/98: allow grenades to hurt anyone, unless
    // fired by Cyberdemons, in which case it won't hurt Cybers.

    if (thing->type == MT_CYBORG || thing->type == MT_SPIDER)
        return true;

    dx = D_abs(thing->x - _g->bombspot->x);
    dy = D_abs(thing->y - _g->bombspot->y);

    dist = dx > dy ? dx : dy;
    dist = (dist - getMobjRadius(thing)) >> FRACBITS;

    if (dist < 0)
        dist = 0;

    if (dist >= _g->bombdamage)
        return true;  // out of range

    if (P_CheckSight(thing, _g->bombspot))
    {
        // must be in direct path
        P_DamageMobj(thing, _g->bombspot, _g->bombsource, _g->bombdamage - dist);
    }

    return true;
}

//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage)
{
    int x;
    int y;

    int xl;
    int xh;
    int yl;
    int yh;

    fixed_t dist;

    dist = (damage + MAXRADIUS) << FRACBITS;
    yh = (spot->y + dist - _g->bmaporgy) >> MAPBLOCKSHIFT;
    yl = (spot->y - dist - _g->bmaporgy) >> MAPBLOCKSHIFT;
    xh = (spot->x + dist - _g->bmaporgx) >> MAPBLOCKSHIFT;
    xl = (spot->x - dist - _g->bmaporgx) >> MAPBLOCKSHIFT;
    _g->bombspot = spot;
    _g->bombsource = source;
    _g->bombdamage = damage;

    for (y = yl; y <= yh; y++)
        for (x = xl; x <= xh; x++)
            P_BlockThingsIterator(x, y, PIT_RadiusAttack);
}

//
// SECTOR HEIGHT CHANGING
// After modifying a sectors floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
// If crunch is true, they will take damage
//  as they are being crushed.
// If Crunch is false, you should set the sector height back
//  the way it was and call P_ChangeSector again
//  to undo the changes.
//

//
// PIT_ChangeSector
//

boolean PIT_ChangeSector(mobj_t *thing)
{
    mobj_t *mo;

    if (P_ThingHeightClip(thing))
        return true; // keep checking

    // crunch bodies to giblets
    if (!(getMobjFlags(thing) & MF_STATIC) && thing->health <= 0)
    {
        P_SetMobjState(thing, S_GIBS);

        setMobjFlagsBits(thing, MF_SOLID, CLEAR_FLAGS);
        if (!(getMobjFlags(thing) & MF_STATIC))
        {
            thing->height_s = 0;
            thing->radiusb = 0;
        }
        return true; // keep checking
    }

    // crunch dropped items

    if (getMobjFlags(thing) & MF_DROPPED)
    {
        P_RemoveMobj(thing);

        // keep checking
        return true;
    }

    if (!(getMobjFlags(thing) & MF_SHOOTABLE ))
    {
        // assume it is bloody gibs or something
        return true;
    }

    _g->nofit = true;

    if (!(getMobjFlags(thing) & MF_STATIC) && (_g->crushchange && !(_g->leveltime & 3)))
    {
        int t;
        P_DamageMobj(thing, NULL, NULL, 10);

        // spray blood in a random direction
        mo = P_SpawnMobj(thing->x, thing->y, FIXED_Z_TO_FIXED32(thing->zr) + getMobjHeight(thing) / 2, MT_BLOOD);

        /* killough 8/10/98: remove dependence on order of evaluation */
        t = P_Random();
        mo->momx = (t - P_Random()) << 12;
        t = P_Random();
        mo->momy = (t - P_Random()) << 12;
    }

    // keep checking (crush other things)
    return true;
}

//
// P_CheckSector
// jff 3/19/98 added to just check monsters on the periphery
// of a moving sector instead of all in bounding box of the
// sector. Both more accurate and faster.
//
#if USE_MSECNODE
boolean P_CheckSector(sector_t *sector, boolean crunch)
{
    msecnode_t *n;

    _g->nofit = false;
    _g->crushchange = crunch;

    // killough 4/4/98: scan list front-to-back until empty or exhausted,
    // restarting from beginning after each thing is processed. Avoids
    // crashes, and is sure to examine all things in the sector, and only
    // the things which are in the sector, until a steady-state is reached.
    // Things can arbitrarily be inserted and removed and it won't mess up.
    //
    // killough 4/7/98: simplified to avoid using complicated counter

    // Mark all things invalid
    // 2021-02-28 next-hack. Removed boolean and used the most significant bit
    // of one address (not used)
//  for (n=sector->touching_thinglist; n; n=n->m_snext)
    uint8_t highBitBackup[1024 / 8] =
    { 0 }; 	// 1024 nodes are enough.
    //
    uint32_t number = 0;
    // save most signiicant bit
    for (n = getSectorTouchingThingList(sector); n; n = getMsecnodeSNext(n))
    {
        if (n->m_tprev_sptr & 0x8000)
        {
            // save bit in backup
            highBitBackup[number / 8] |= (1 << (number % 8));
            // clear it to use it as visited
            n->m_tprev_sptr &= ~0x8000;
        }
        number++;
    }
    do
    {
        for (n = getSectorTouchingThingList(sector); n; n = getMsecnodeSNext(n)) // go through list
        {
            if (!(n->m_tprev_sptr & 0x8000))
            {
                n->m_tprev_sptr |= 0x8000;          // mark thing as processed
                if (!(getMsecnodeThing(n)->flags & MF_NOBLOCKMAP )) //jff 4/7/98 don't do these
                    PIT_ChangeSector(getMsecnodeThing(n));    // process it
                break;                 // exit and start over
            }
        }
    } while (n);  // repeat from scratch until all things left are marked valid
    // restore high bits
    number = 0;
    for (n = getSectorTouchingThingList(sector); n; n = getMsecnodeSNext(n))
    {
        if (highBitBackup[number / 8] & (1 << (number % 8)))
        {
            n->m_tprev_sptr |= 0x8000;
        }
        else
        {
            n->m_tprev_sptr &= ~0x8000;
        }
        number++;
    }
    return _g->nofit;
}
#else
// next-hack: restored P_ChangeSector does not use msecnodes
boolean P_CheckSector( sector_t*	sector,  boolean	crunch )
{
    int		x;
    int		y;

   _g->nofit = false;
   _g->crushchange = crunch;
	
    // re-check heights for all things near the moving sector

     // adjust bounding box to map blocks
	int block, blocktop, blockbottom, blockright, blockleft;
    block = ((sector->sbbox[BOXTOP] << FRACBITS) - _g->bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
	block = block >= _g->bmapheight ? _g->bmapheight - 1 : block;
	blocktop = block;

	block = ((sector->sbbox[BOXBOTTOM] << FRACBITS) - _g->bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	blockbottom = block;

	block = ((sector->sbbox[BOXRIGHT] << FRACBITS) - _g->bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
	block = block >= _g->bmapwidth ? _g->bmapwidth-1 : block;
	blockright = block;

	block = ((sector->sbbox[BOXLEFT] << FRACBITS) - _g->bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	blockleft = block;



    for (x = blockleft ; x <= blockright ; x++)
    {
        for (y = blockbottom; y <= blocktop ; y++)
        {
            P_BlockThingsIterator (x, y , PIT_ChangeSector);
        }
	}
    return _g->nofit;
}
#endif

// CPhipps -
// Use block memory allocator here

#include "z_bmalloc.h"
#if USE_MSECNODE
IMPLEMENT_BLOCK_MEMORY_ALLOC_ZONE(secnodezone, sizeof(msecnode_t), PU_LEVEL, 32, "SecNodes");

inline static msecnode_t* P_GetSecnode(void)
{
    return (msecnode_t*) Z_BMalloc(&secnodezone);
}

// P_PutSecnode() returns a node to the freelist.

inline static void P_PutSecnode(msecnode_t *node)
{
    Z_BFree(&secnodezone, node);
}

// phares 3/16/98
//
// P_AddSecnode() searches the current list to see if this sector is
// already there. If not, it adds a sector node at the head of the list of
// sectors this object appears in. This is called when creating a list of
// nodes that will get linked in later. Returns a pointer to the new node.

msecnode_t* P_AddSecnode(sector_t *s, mobj_t *thing, msecnode_t *nextnode)
{
    msecnode_t *node;

    node = nextnode;
    while (node)
    {
        if (getMsecnodeSector(node) == s) // Already have a node for this sector?
        {
            node->m_thing_sptr = getShortPtr(thing); // Yes. Setting m_thing says 'keep it'.
            return (nextnode);
        }
        node = getMsecnodeTNext(node);
    }

    // Couldn't find an existing node for this sector. Add one at the head
    // of the list.

    node = P_GetSecnode();

    // killough 4/4/98, 4/7/98: mark new nodes unvisited.
    //node->visited = 0;

    node->m_sector_sptr = getShortPtr(s);       // sector
    node->m_thing_sptr = getShortPtr(thing);     // mobj
    node->m_tprev_sptr = 0L;    // prev node on Thing thread
    node->m_tnext_sptr = getShortPtr(nextnode);  // next node on Thing thread
    if (nextnode)
        nextnode->m_tprev_sptr = getShortPtr(node); // set back link on Thing

    // Add new node at head of sector thread starting at s->touching_thinglist

    node->m_sprev_sptr = 0;    // prev node on sector thread
    //node->m_snext  = s->touching_thinglist; // next node on sector thread
//  node->m_snext  = getLongPtr(s->touching_thinglist_sptr); // next node on sector thread
    node->m_snext_sptr = s->touching_thinglist_sptr; // next node on sector thread

    //if (s->touching_thinglist)
    if (s->touching_thinglist_sptr)
        getMsecnodeSNext(node)->m_sprev_sptr = getShortPtr(node);
//  s->touching_thinglist = node;
    s->touching_thinglist_sptr = getShortPtr(node);
    return (node);
}

// P_DelSecnode() deletes a sector node from the list of
// sectors this object appears in. Returns a pointer to the next node
// on the linked list, or NULL.

msecnode_t* P_DelSecnode(msecnode_t *node)
{
    msecnode_t *tp;  // prev node on thing thread
    msecnode_t *tn;  // next node on thing thread
    msecnode_t *sp;  // prev node on sector thread
    msecnode_t *sn;  // next node on sector thread

    if (node)
    {

        // Unlink from the Thing thread. The Thing thread begins at
        // sector_list and not from mobj_t->touching_sectorlist.

        tp = getMsecnodeTPrev(node);
        tn = getMsecnodeTNext(node);
        if (tp)
            tp->m_tnext_sptr = getShortPtr(tn);
        if (tn)
            tn->m_tprev_sptr = getShortPtr(tp);

        // Unlink from the sector thread. This thread begins at
        // sector_t->touching_thinglist.

        sp = getMsecnodeSPrev(node);
        sn = getLongPtr(node->m_snext_sptr);
        if (sp)
            sp->m_snext_sptr = getShortPtr(sn);
        else
        {
//      node->m_sector->touching_thinglist = sn;
            getMsecnodeSector(node)->touching_thinglist_sptr = getShortPtr(sn);
        }
        if (sn)
            sn->m_sprev_sptr = getShortPtr(sp);

        // Return this node to the freelist

        P_PutSecnode(node);
        return (tn);
    }
    return (NULL);
}                             // phares 3/13/98

// Delete an entire sector list

void P_DelSeclist(msecnode_t *node)

{
    while (node)
        node = P_DelSecnode(node);
}

// phares 3/14/98
//
// PIT_GetSectors
// Locates all the sectors the object is in by looking at the lines that
// cross through it. You have already decided that the object is allowed
// at this location, so don't bother with checking impassable or
// blocking lines.
boolean PIT_GetSectors(const line_t *ld)
{
    if (_g->tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT] || _g->tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT] || _g->tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM] || _g->tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
        return true;

    if (P_BoxOnLineSide(_g->tmbbox, ld) != -1)
        return true;

    // This line crosses through the object.

    // Collect the sector(s) from the line and add to the
    // sector_list you're examining. If the Thing ends up being
    // allowed to move to this position, then the sector_list
    // will be attached to the Thing's mobj_t at touching_sectorlist.

    _g->sector_list = P_AddSecnode(LN_FRONTSECTOR(ld), _g->tmthing, _g->sector_list);

    /* Don't assume all lines are 2-sided, since some Things
     * like MT_TFOG are allowed regardless of whether their radius takes
     * them beyond an impassable linedef.
     *
     * killough 3/27/98, 4/4/98:
     * Use sidedefs instead of 2s flag to determine two-sidedness.
     * killough 8/1/98: avoid duplicate if same sector on both sides
     * cph - DEMOSYNC? */

    if (LN_BACKSECTOR(ld) && LN_BACKSECTOR(ld) != LN_FRONTSECTOR(ld))
        _g->sector_list = P_AddSecnode(LN_BACKSECTOR(ld), _g->tmthing, _g->sector_list);

    return true;
}
#endif
// phares 3/14/98
//
// P_CreateSecNodeList alters/creates the sector_list that shows what sectors
// the object resides in.
#if USE_MSECNODE
void P_CreateSecNodeList(mobj_t *thing, fixed_t x, fixed_t y)
{
    int xl;
    int xh;
    int yl;
    int yh;
    int bx;
    int by;
    msecnode_t *node;
    mobj_t *saved_tmthing = _g->tmthing; /* cph - see comment at func end */

    // First, clear out the existing m_thing fields. As each node is
    // added or verified as needed, m_thing will be set properly. When
    // finished, delete all nodes where m_thing is still NULL. These
    // represent the sectors the Thing has vacated.

    node = _g->sector_list;
    while (node)
    {
        node->m_thing_sptr = 0;
        node = getMsecnodeTNext(node);
    }

    _g->tmthing = thing;

    _g->tmx = x;
    _g->tmy = y;

    _g->tmbbox[BOXTOP] = y + getMobjRadius(_g->tmthing);
    _g->tmbbox[BOXBOTTOM] = y - getMobjRadius(_g->tmthing);
    _g->tmbbox[BOXRIGHT] = x + getMobjRadius(_g->tmthing);
    _g->tmbbox[BOXLEFT] = x - getMobjRadius(_g->tmthing);

    _g->validcount++; // used to make sure we only process a line once

    xl = (_g->tmbbox[BOXLEFT] - _g->bmaporgx) >> MAPBLOCKSHIFT;
    xh = (_g->tmbbox[BOXRIGHT] - _g->bmaporgx) >> MAPBLOCKSHIFT;
    yl = (_g->tmbbox[BOXBOTTOM] - _g->bmaporgy) >> MAPBLOCKSHIFT;
    yh = (_g->tmbbox[BOXTOP] - _g->bmaporgy) >> MAPBLOCKSHIFT;

    for (bx = xl; bx <= xh; bx++)
        for (by = yl; by <= yh; by++)
            P_BlockLinesIterator(bx, by, PIT_GetSectors);

    // Add the sector of the (x,y) point to sector_list.

    _g->sector_list = P_AddSecnode(&_g->sectors[getMobjSubsector(thing)->sector_num], thing, _g->sector_list);

    // Now delete any nodes that won't be used. These are the ones where
    // m_thing is still NULL.

    node = _g->sector_list;
    while (node)
    {
        if (node->m_thing_sptr == 0)
        {
            if (node == _g->sector_list)
                _g->sector_list = getMsecnodeTNext(node);
            node = P_DelSecnode(node);
        }
        else
            node = getMsecnodeTNext(node);
    }

    /* cph -
     * This is the strife we get into for using global variables. tmthing
     *  is being used by several different functions calling
     *  P_BlockThingIterator, including functions that can be called *from*
     *  P_BlockThingIterator. Using a global tmthing is not reentrant.
     * OTOH for Boom/MBF demos we have to preserve the buggy behavior.
     *  Fun. We restore its previous value unless we're in a Boom/MBF demo.
     */
    _g->tmthing = saved_tmthing;
}
#endif
/* cphipps 2004/08/30 - 
 * Must clear tmthing at tic end, as it might contain a pointer to a removed thinker, or the level might have ended/been ended and we clear the objects it was pointing too. Hopefully we don't need to carry this between tics for sync. */
void P_MapStart(void)
{
    if (_g->tmthing)
        I_Error("P_MapStart: tmthing set!");
}

void P_MapEnd(void)
{
    _g->tmthing = NULL;
}
