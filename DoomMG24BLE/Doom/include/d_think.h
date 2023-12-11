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
 *  MapObj data. Map Objects or mobjs are actors, entities,
 *  thinker, take-your-pick... anything that moves, acts, or
 *  suffers state changes of more or less violent nature.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __D_THINK__
#define __D_THINK__

#ifdef __GNUG__
#pragma interface
#endif
#include "doomdef.h"
/*
 * Experimental stuff.
 * To compile this as "ANSI C with classes"
 *  we will need to handle the various
 *  action functions cleanly.
 */
// killough 11/98: convert back to C instead of C++
#define THINKFUN_ALIGN4  // __attribute__ ((aligned(4), section(".text.thinkers") ) )

#define THINKER_IDX(f) f ## _n

typedef void (*actionf_t)();
enum
{
  null_thinker_n = 0,
  T_MoveCeiling_n,
  T_VerticalDoor_n,
  T_MoveFloor_n,
  T_MoveElevator_n,
  T_FireFlicker_n,
  T_LightFlash_n,
  T_StrobeFlash_n,
  T_Glow_n,
  P_MobjBrainlessThinker_n,
  T_PlatRaise_n,
  T_Scroll_n,
  P_RemoveThinkerDelayed_n,
  P_RemoveStaticThinkerDelayed_n,
  P_RemoveThinker_n,
  P_RemoveStaticThinker_n,
  P_MobjThinker_n,
  num_thinker_functions
};
extern const actionf_t *const thinkerFunctions[num_thinker_functions];


//typedef  void (*actionf_v)();
//typedef  void (*actionf_p1)( void* );
//typedef  void (*actionf_p2)( void*, void* );

/* Note: In d_deh.c you will find references to these
 * wherever code pointers and function handlers exist
 */
/*
 typedef union
 {
 actionf_p1    acp1;
 actionf_v     acv;
 actionf_p2    acp2;

 } actionf_t;
 */

/* Historically, "think_t" is yet another
 *  function pointer to a routine to handle
 *  an actor.
 */
typedef actionf_t think_t;

/* Doubly linked list of actors. */
typedef struct thinker_s
{
    unsigned short next_sptr;
    //   think_t function;
    // next-hack: using index.
    unsigned short function_idx;     // note: less than 32 thinker functions, so this can be 5 bit. We can have 11 bits for something else in future.
} thinker_t __attribute ((aligned(4))); 

static inline thinker_t* getThinkerNext(thinker_t *pthinker)
{
    return (thinker_t*) getLongPtr(pthinker->next_sptr);
}
#endif
