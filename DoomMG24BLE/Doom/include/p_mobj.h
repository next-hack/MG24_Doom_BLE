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
 *      Map Objects, MObj, definition and handling.
 *
 *  next-hack: optimized a lot this, bringing the structure size from 140 b
 *  down to 52 bytes. Added also static_mobj_t, which uses only 20 bytes.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __P_MOBJ__
#define __P_MOBJ__
// Basics.
#include "tables.h"
#include "m_fixed.h"

// We need the thinker_t stuff.
#include "d_think.h"

// We need the WAD data structure for Map things,
// from the THINGS lump.
#include "doomdata.h"

// States are tied to finite states are
//  tied to animation frames.
// Needs precompiled tables/data structures.
#include "info.h"

//
// NOTES: mobj_t
//
// mobj_ts are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost allways set
// from state_t structures.
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the mobj_t.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when mobj_ts are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The mobj_t->flags element has various bit flags
// used by the simulation.
//
// Every mobj_t is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with R_PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any mobj_t that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable mobj_t that has its origin contained.
//
// A valid mobj_t is a mobj_t that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO? flags while a thing is valid.
//
// Any questions?
//

//
// Misc. mobj flags
//
// next-hack. Reordered flags, so that mutable ones:
// fits 16 bit for all mobj types
// fits first 6 bits for static
enum
{
    // Dropped by a demon, not level spawned.
    // E.g. ammo clips dropped by dying former humans. 
    // next-hack: this discriminates between fully static objects (where x, y are stored in flash) from dropped (where x, y are 16 bit in ram)
    MF_DROPPED = (1 << 0),
    // This allows jumps from high places.
    MF_DROPOFF = (1 << 1),
    // Blocks.
    MF_SOLID = (1 << 2),
    // Don't stop moving halfway off a step,
    //  that is, have dead bodies slide down all the way.
    MF_CORPSE = (1 << 3),
    // Don't apply gravity (every tic),
    //  that is, object will float, keeping current height
    //  or changing it actively.
    MF_NOGRAVITY = (1 << 4),
    // Not to be activated by sound, deaf monster.
    MF_AMBUSH = (1 << 5),
    // fully static have only 6 flags
    MF_FULL_STATIC_RAMFLAGS_MASK = (1 << 6) - 1,
    // Can be hit.
    MF_SHOOTABLE = (1 << 6),
    // Allow moves to any height, no gravity.
    // For active floaters, e.g. cacodemons, pain elementals.
    MF_FLOAT = (1 << 7),
    // friend
    MF_FRIEND = (1 << 8),
    // Floating to a height for a move, ???
    //  don't auto float to target's height.
    MF_INFLOAT = (1 << 9),
    // Will take at least one step before attacking.
    MF_JUSTATTACKED = (1 << 10),
    // Will try to attack right back.
    MF_JUSTHIT = (1 << 11),
    // Don't hit same species, explode on block.
    // Player missiles as well as fireballs of various kinds.
    MF_MISSILE = (1 << 12),
    // Player cheat. ???
    MF_NOCLIP = (1 << 13),
    // Use fuzzy draw (shadow demons or spectres),
    //  temporary player invisibility powerup.
    MF_SHADOW = (1 << 14),
    // Special handling: skull in flight.
    // Neither a cacodemon nor a missile.
    MF_SKULLFLY = (1 << 15),
    // MOBJS have only 16 mutable flags.
    MF_MOBJ_RAM_FLAGS = (1 << 16) - 1,
    // const flags
    // Call P_SpecialThing when touched.
    MF_SPECIAL = (1 << 16),
    // Don't use the sector links (invisible but touchable).
    MF_NOSECTOR = (1 << 17), 
    // Don't use the blocklinks (inert but displayable)
    MF_NOBLOCKMAP = (1 << 18),  
    // On level spawning (initial position),
    //  hang from ceiling instead of stand on floor.
    MF_SPAWNCEILING = (1 << 19),

    // For players, will pick up items.
    MF_PICKUP = (1 << 20), 
    // Player: keep info about sliding along walls.
    MF_SLIDE = (1 << 21), 
    // Don't cross lines
    //   ??? or look at heights on teleport.
    MF_TELEPORT = (1 << 22), 
    // Flag: don't bleed when shot (use puff),
    //  barrels and shootable furniture shall not bleed.
    MF_NOBLOOD  = (1 << 23), 
    // On kill, count this enemy object
    //  towards intermission kill total.
    // Happy gathering.
    MF_COUNTKILL = (1 << 24), 

    // On picking up, count this item object
    //  towards intermission item total.
    MF_COUNTITEM = (1 << 25), 
    // Player sprites in multiplayer modes are modified
    //  using an internal color lookup table for re-indexing.
    // If translation is non zero,
    //  use a translation table for player colormaps
    MF_TRANSSHIFT = 26,
    MF_TRANSLATION1 = (1 << MF_TRANSSHIFT),
    MF_TRANSLATION2 = (1 << (MF_TRANSSHIFT + 1)),
    MF_TRANSLATION = MF_TRANSLATION1 | MF_TRANSLATION2,
    // Don't spawn this object
    //  in death match mode (e.g. key cards).
    MF_NOTDMATCH = (1 << 28),  
    // next-hack. Used for decorations such as lamps, etc.
    MF_DECORATION = (1 << 29),  
    // Translucent sprite?                                          // phares
    MF_TRANSLUCENT = (1 << 30), 

};
/*
// Call P_SpecialThing when touched.
#define MF_SPECIAL      (unsigned int)(0x0000000000000001)
// Blocks.
#define MF_SOLID        (unsigned int)(0x0000000000000002)
// Can be hit.
#define MF_SHOOTABLE    (unsigned int)(0x0000000000000004)
// Don't use the sector links (invisible but touchable).
#define MF_NOSECTOR     (unsigned int)(0x0000000000000008)
// Don't use the blocklinks (inert but displayable)
#define MF_NOBLOCKMAP   (unsigned int)(0x0000000000000010)

// Not to be activated by sound, deaf monster.
#define MF_AMBUSH       (unsigned int)(0x0000000000000020)
// Will try to attack right back.
#define MF_JUSTHIT      (unsigned int)(0x0000000000000040)
// Will take at least one step before attacking.
#define MF_JUSTATTACKED (unsigned int)(0x0000000000000080)
// On level spawning (initial position),
//  hang from ceiling instead of stand on floor.
#define MF_SPAWNCEILING (unsigned int)(0x0000000000000100)
// Don't apply gravity (every tic),
//  that is, object will float, keeping current height
//  or changing it actively.
#define MF_NOGRAVITY    (unsigned int)(0x0000000000000200)

// Movement flags.
// This allows jumps from high places.
#define MF_DROPOFF      (unsigned int)(0x0000000000000400)
// For players, will pick up items.
#define MF_PICKUP       (unsigned int)(0x0000000000000800)
// Player cheat. ???
#define MF_NOCLIP       (unsigned int)(0x0000000000001000)
// Player: keep info about sliding along walls.
#define MF_SLIDE        (unsigned int)(0x0000000000002000)
// Allow moves to any height, no gravity.
// For active floaters, e.g. cacodemons, pain elementals.
#define MF_FLOAT        (unsigned int)(0x0000000000004000)
// Don't cross lines
//   ??? or look at heights on teleport.
#define MF_TELEPORT     (unsigned int)(0x0000000000008000)
// Don't hit same species, explode on block.
// Player missiles as well as fireballs of various kinds.
#define MF_MISSILE      (unsigned int)(0x0000000000010000)
// Dropped by a demon, not level spawned.
// E.g. ammo clips dropped by dying former humans.
#define MF_DROPPED      (unsigned int)(0x0000000000020000)
// Use fuzzy draw (shadow demons or spectres),
//  temporary player invisibility powerup.
#define MF_SHADOW       (unsigned int)(0x0000000000040000)
// Flag: don't bleed when shot (use puff),
//  barrels and shootable furniture shall not bleed.
#define MF_NOBLOOD      (unsigned int)(0x0000000000080000)
// Don't stop moving halfway off a step,
//  that is, have dead bodies slide down all the way.
#define MF_CORPSE       (unsigned int)(0x0000000000100000)
// Floating to a height for a move, ???
//  don't auto float to target's height.
#define MF_INFLOAT      (unsigned int)(0x0000000000200000)

// On kill, count this enemy object
//  towards intermission kill total.
// Happy gathering.
#define MF_COUNTKILL    (unsigned int)(0x0000000000400000)

// On picking up, count this item object
//  towards intermission item total.
#define MF_COUNTITEM    (unsigned int)(0x0000000000800000)

// Special handling: skull in flight.
// Neither a cacodemon nor a missile.
#define MF_SKULLFLY     (unsigned int)(0x0000000001000000)

// Don't spawn this object
//  in death match mode (e.g. key cards).
#define MF_NOTDMATCH    (unsigned int)(0x0000000002000000)

// Player sprites in multiplayer modes are modified
//  using an internal color lookup table for re-indexing.
// If 0x4 0x8 or 0xc,
//  use a translation table for player colormaps
#define MF_TRANSLATION  (unsigned int)(0x000000000c000000)
#define MF_TRANSLATION1 (unsigned int)(0x0000000004000000)
#define MF_TRANSLATION2 (unsigned int)(0x0000000008000000)
// Hmm ???.
#define MF_TRANSSHIFT 26

//#define MF_UNUSED2      (unsigned int)(0x0000000010000000)
#define MF_DECORATION      (unsigned int)(0x0000000010000000) // next-hack.

#define MF_UNUSED3      (unsigned int)(0x0000000020000000)

// Translucent sprite?                                          // phares
#define MF_TRANSLUCENT  (unsigned int)(0x0000000040000000)

#define MF_FRIEND       (unsigned int)(0x0000000080000000)
*/
#define MF_STATIC (MF_DECORATION | MF_SPECIAL | MF_DROPPED)	  // next-hack. Objects with this flags go in the static mobj zone, and occupy less RAM

#define OPTIMIZE_CORPSE 0       // still corpses will be converted to static object, until raised again.
// killough 9/15/98: Same, but internal flags, not intended for .deh
// (some degree of opaqueness is good, to avoid compatibility woes)

enum
{
    MIF_FALLING = 1      // Object is falling
};

// Map Object definition.
//
//
// killough 2/20/98:
//
// WARNING: Special steps must be taken in p_saveg.c if C pointers are added to
// this mobj_s struct, or else savegames will crash when loaded. See p_saveg.c.
// Do not add "struct mobj_s *fooptr" without adding code to p_saveg.c to
// convert the pointers to ordinals and back for savegames. This was the whole
// reason behind monsters going to sleep when loading savegames (the "target"
// pointer was simply nullified after loading, to prevent Doom from crashing),
// and the whole reason behind loadgames crashing on savegames of AV attacks.
//

// killough 9/8/98: changed some fields to shorts,
// for better memory usage (if only for cache).
/* cph 2006/08/28 - move Prev[XYZ] fields to the end of the struct. Add any
 * other new fields to the end, and make sure you don't break savegames! */

// 18/06/2021 next-hack. New Simplicity Studio 5.2 comes with a newer GNU
// toolchain version and it complains that member thinker might be unaligned as
// mobj_t and static_mobj_t are packed.
// We could pass simply the mobj address, and cast it to thinker_t*, but it is
// better to use unpacked structures at this point. Noticeably, there should
// not be any change in size because we carefully we reordered members to prevent
// paddings but to be sure we don't accidentally mess, we declare our expected
// size here, and an array that has null size if the size is correct, otherwise
// it will have a negative size, generating an error.
//
#define MOBJ_SIZE 52
#define STATIC_MOBJ_SIZE 20
//
#define MAX_MOVE_COUNT 511
//
typedef struct mobj_s
{
    // WARNING: the order of these variables is important! It must be kept the same as static_mobj
    // List: thinker links.
    thinker_t thinker;
    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    unsigned short bnext_sptr;
    // sector linked list. Note: short pointers.
    unsigned short snext_sptr;
    #if MOBJ_HAS_SPREV_AND_BPREV
        unsigned short bprev_sptr;
        unsigned short sprev_sptr;
    #endif
    unsigned int state_idx :11; // there are less than 2048 states       
    unsigned int subsector_num : 11;  // should be enough for stock level. We can save some bits if needed
    unsigned int lastlook:2; //next-hack: added for compatibility
    //
    unsigned short type : 8;   // less than 255 mobjtypes.
//    unsigned int frame : 8; // might be ORed with FF_FULLBRIGHT (30-01-2022 next-hack: was 0x8000 set to 0x80)
//    unsigned short sprite :8; // used to find patch_t and flip value
    union
    {   // this parameter is momz16 on full mobj_t, and fixed_xy_sptr on static mobj
        fixed_momz_t momz16;                // z momentum for full mobjs
        unsigned short pos_index;           // position index for static objects, not dropped
        unsigned short dropped_xy_sptr;       // pointer to position for dropped items
    };
    //
    unsigned int ramFlags : 16;  // next-hack: renumbered MF_* flags, so that mutable one take only 16 bits.
    //
    fixed_t zr : 21;
    //
    short tics:11;   // state tic counter. Can be as high as 1000, but we need negative values as well: 11 bits.
    //
    // AFTER THIS LINE, THE SUBSEQUENT PARAMETERS ARE MISSING IN THE STATIC MOBJ TYPE
    // note: momx and momy do not need to have a big integer part. So the width can be reduced.

    // Info for drawing: position.
    //
    fixed_t x;
    fixed_t y;

    fixed16_t ceilingz16;
    // killough 11/98: the lowest floor over all contacted Sectors.

    fixed16_t floorz16;
    // The closest interval over all contacted Sectors.

#if MOBJ_HAS_DROPOFFZ
    fixed16_t dropoffz16;
#endif
    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    //struct mobj_s*      target;
    unsigned short target_sptr;

    //More drawing info: to determine current sprite.
    angle16_t angle16;  // orientation
    //
    //
    // Momentums, used to update position.
    //fixed_momz_t momz16;
    // new field: last known enemy -- killough 2/15/98
    //struct mobj_s*      lastenemy;
#if MOBJ_HAS_LAST_ENEMY
//    unsigned short lastenemy_sptr;
#endif
    //
    // Thing being chased/attacked for tracers.
    //struct mobj_s*      tracer;

    //

    fixed_t momx : 24;
    uint32_t height_s : 8;  // no less than 8 bits

    fixed_t momy : 24;
    uint32_t radiusb : 8;  // max 128, so it must be 8 bits
    union
    {
        unsigned short tracer_sptr;
        // Additional info record for player avatars only.
        // Only valid if type == MT_PLAYER
        struct
        {
            uint8_t player_n;
            uint8_t playerCorpse_n;   // 2023-03-12 next-hack: made some changes for appropriately rendering translated corpses
        };
    };

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    unsigned short reactiontime : 7;  // at least 7 bits
    unsigned short movecount :9;      // when 0, select a new dir. max is 35*12 = 420

    // If >0, the current target will be chased no
    // matter what (even if shot by another object)
    uint32_t threshold : 7;  // could be uint8_t. At least 7 bit
    // killough 9/9/98: How long a monster pursues a target.
    uint32_t pursuecount:7;   // at least 7 bits

    int32_t health : 13;   // max is 4000 but we need a negative value: no less than 13 bits

    uint32_t movedir :4;        // 0-8 (8 = NO_DIR)
    //
    uint32_t dummy : 1;
    //
} mobj_t;
//
typedef struct static_mobj_s
{
    // WARNING: the order of these variables is important! It must be kept the same as static_mobj
    // List: thinker links.
    thinker_t thinker;
    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    unsigned short bnext_sptr;
    // sector linked list. Note: short pointers.
    unsigned short snext_sptr;
    #if MOBJ_HAS_SPREV_AND_BPREV
        unsigned short bprev_sptr;
        unsigned short sprev_sptr;
    #endif
    unsigned int state_idx :11; // there are less than 2048 states       
    unsigned int subsector_num : 11;  // should be enough for stock level. We can save some bits if needed
    unsigned int lastlook:2; //next-hack: added for compatibility
    //
    unsigned short type : 8;   // less than 255 mobjtypes.
//    unsigned int frame : 8; // might be ORed with FF_FULLBRIGHT (30-01-2022 next-hack: was 0x8000 set to 0x80)
//    unsigned short sprite :8; // used to find patch_t and flip value
    union
    {   // this parameter is momz16 on full mobj_t, and fixed_xy_sptr on static mobj
        fixed_momz_t momz16;                // z momentum for full mobjs
        unsigned short pos_index;           // position index for static objects, not dropped
        unsigned short dropped_xy_sptr;       // pointer to position for dropped items
    };

    //
    unsigned int ramFlags : 16;  // next-hack: renumbered MF_* flags, so that mutable one take only 16 bits.
    //
    fixed_t zr : 21;
    //
    short tics:11;   // state tic counter. Can be as high as 1000, but we need negative values as well: 11 bits.
    // Info for drawing: position.
    //
}  static_mobj_t;
typedef  struct
{
    static_mobj_t dummy;
    fixed_t x;
    fixed_t y;
} sound_mobj_t; // hack for sound mobj
// until full_static_mobj isn't implemented...
#define full_static_mobj_t static_mobj_t
static inline const mobjinfo_t* getMobjInfo(mobj_t *pmobj)
{
    return &mobjinfo[pmobj->type];
}

#if MOBJ_HAS_LAST_ENEMY
static inline mobj_t* getLastEnemy(mobj_t *pmobj)
{
    return (mobj_t*) getLongPtr(pmobj->lastenemy_sptr);
}
#endif
static inline mobj_t* getTracer(mobj_t *pmobj)
{
    return (mobj_t*) getLongPtr(pmobj->tracer_sptr);
}
static inline mobj_t* getTarget(mobj_t *pmobj)
{
    return (mobj_t*) getLongPtr(pmobj->target_sptr);
}
static inline mobj_t* getSNext(mobj_t *pmobj)
{
    return (mobj_t*) getLongPtr(pmobj->snext_sptr);
}
#if MOBJ_HAS_SPREV_AND_BPREV
static inline mobj_t* getSPrev(mobj_t *pmobj)
{
    return (mobj_t*) getLongPtr(pmobj->sprev_sptr);
}
static inline mobj_t* getBPrev(mobj_t *pmobj)
{
    return (mobj_t*) getLongPtr(pmobj->bprev_sptr);
}

#endif
static inline mobj_t* getBNext(mobj_t *pmobj)
{
    return (mobj_t*) getLongPtr(pmobj->bnext_sptr);
}

static inline const state_t* getMobjState(mobj_t *pmobj)
{
    unsigned short index = pmobj->state_idx;
    if (index >= NUMSTATES || index == S_NULL) // TODO: is really needed this one?
        return NULL;
    return &states[index];
}
// External declarations (fomerly in p_local.h) -- killough 5/2/98

#define VIEWHEIGHT      (41*FRACUNIT)

#define GRAVITY         FRACUNIT
#define MAXMOVE         (30*FRACUNIT)

#define ONFLOORZ16      (-32768) //INT_MIN
#define ONCEILINGZ16    (32767)  //INT_MAX

// Time interval for item respawning.
#define ITEMQUESIZE     128

#define FLOATSPEED      (FRACUNIT*4)
#define STOPSPEED       (FRACUNIT/16)

// killough 11/98:
// For torque simulation:

#define OVERDRIVE 6
#define MAXGEAR (OVERDRIVE+16)

// killough 11/98:
// Whether an object is "sentient" or not. Used for environmental influences.
#define sentient(mobj) ((mobj)->health > 0 && (mobj)->info->seestate)

void P_RespawnSpecials(void);
mobj_t* P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, uint32_t type);
static_mobj_t* P_SpawnStaticMobj(fixed_t x, fixed_t y, fixed_t z, uint32_t ntype, int respawnIndex); // 2021-03-13 next-hack. 2023-08-30 next-hack respawn support
void P_RemoveMobj(mobj_t *th);
void P_RemoveStaticMobj(static_mobj_t *mobj);  // 2021-03-13 next-hack

boolean P_SetMobjState(mobj_t *mobj, statenum_t state);

void THINKFUN_ALIGN4 P_MobjThinker(mobj_t *mobj);
void THINKFUN_ALIGN4 P_MobjBrainlessThinker(mobj_t *mobj);

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z);
void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage);
mobj_t* P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type);
void P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type);
boolean P_IsDoomnumAllowed(int doomnum);
void P_SpawnMapThing(const mapthing_t *mthing);
void P_SpawnPlayer(int n, const mapthing_t *mthing);
void P_CheckMissileSpawn(mobj_t*);  // killough 8/2/98
void P_ExplodeMissile(mobj_t*);    // killough
PUREFUNC int P_FindDoomedNum(int type);
#if 1
#define CLEAR_FLAGS 0
#define SET_FLAGS 1
// returns all flags (const and RAM)
static inline uint32_t getMobjFlags(mobj_t *mobj)
{
    uint32_t constFlags = mobjinfo[mobj->type].flags;
    return mobj->ramFlags | (constFlags & 0xFFFF0000);
}
static inline uint8_t getMobjFrame(mobj_t *mobj)
{
    return states[mobj->state_idx].frame;
}
static inline uint8_t getMobjSprite(mobj_t *mobj)
{
    return states[mobj->state_idx].sprite;
}
// set or clear a mobj state
static inline void setMobjFlagsBits(mobj_t *mobj, uint32_t flags, uint32_t value)
{
 #if CHECK_MOBJ_FLAGS
    if (flags & ~MF_MOBJ_RAM_FLAGS)
    {
        printf("Error, setting flags %x, type %d\r\n", flags, mobj->type);
        while(1);
    }
#endif
    if (value)
    {
        mobj->ramFlags |= flags;
    }
    else
    {
        mobj->ramFlags &= ~flags;
    }
}
static inline void setMobjFlagsValue(mobj_t *mobj, uint32_t flags)
{
 #if CHECK_MOBJ_FLAGS
    if (flags & ~MF_MOBJ_RAM_FLAGS)
    {
        printf("Error, setting flags %x\r\n", flags);
        while(1);
    }
#endif
    mobj->ramFlags = flags;
}
#endif

//
static inline fixed_t getMobjHeight(mobj_t *pmobj)
{
    if (!(getMobjFlags(pmobj) & MF_STATIC))
        return pmobj->height_s << FRACBITS;
    else
        return mobjinfo[pmobj->type].height;
}
static inline fixed_t getMobjRadius(mobj_t *pmobj)
{
    if (!(getMobjFlags(pmobj) & MF_STATIC))
        return pmobj->radiusb << FRACBITS;
    else
        return mobjinfo[pmobj->type].radius;

}

#endif

