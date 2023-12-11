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
 *      Refresh/rendering module, shared data struct definitions.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __R_DEFS__
#define __R_DEFS__
//#define USE_MSECNODE 1
// Screenwidth.
#include "doomdef.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"

// SECTORS do store MObjs anyway.
#include "p_mobj.h"
 #ifdef __GNUG__
#pragma interface
#endif

// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE    0
#define SIL_BOTTOM  1
#define SIL_TOP     2
#define SIL_BOTH    3

#define MAXDRAWSEGS   192  // same as doomhack GBA port 

#define MAXOPENINGS (SCREENWIDTH * 16) // same as doomhack GBA port

#define MAXVISSPRITES 128

//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
// like some DOOM-alikes ("wt", "WebView") do.
//
// next-hack: this stores the x, y positions of full static objects.
// It also stores the type, because it is useful for items respawning in multiplayer.
typedef struct
{
  // static object will have only 16 bit integer part.
    short x;
    short y;
    short objtype;
} full_static_mobj_xy_and_type_t;
typedef struct
{
    fixed_t x;
    fixed_t y;
} dropped_xy_t;
typedef struct
{
    fixed_t x, y;
} vertex_t;

// Each sector has a degenmobj_t in its center for sound origin purposes.
typedef struct
{
    fixed_t x;
    fixed_t y;
} degenmobj_t;
// next-hack: split between ramSector_t and sector_t (constant data)
typedef struct
{
    fixed16_t floorheight16;
    fixed16_t ceilingheight16;
    //
 
    
    // thinker_t for reversable actions
    unsigned short floordata_sptr;
    // floors, ceilings, lighting,
    unsigned short ceilingdata_sptr;
#if USE_MSECNODE
    //struct msecnode_s *touching_thinglist;               // phares 3/14/98
    unsigned short touching_thinglist_sptr;
#endif
    // list of mobjs that are at least partially in the sector
    // thinglist is a subset of touching_thinglist
    unsigned short thinglist_sptr;
    //  mobj_t *soundtarget;   // thing that made a sound (or null)
    unsigned short soundtarget_sptr;
    //unsigned short thinglist_sptr;
#if OLD_VALID_COUNT
    int validcount :24;        // if == validcount, already checked
    uint8_t lightlevel: 8;

    unsigned int soundtraversed :2;    // 0 = untraversed, 1,2 = sndlines-1
    unsigned int special : 9;
    unsigned int oldspecial : 9;     //jff 2/16/98 remembers if sector WAS secret (automap)
    unsigned short floorpic : 11;    // 11 bit
#else

    unsigned int lightlevel: 8;
    unsigned int soundtraversed :2;    // 0 = untraversed, 1,2 = sndlines-1
    unsigned int special : 9;           // can be as small as 5 bits
    unsigned int wasSecret : 1;     //jff 2/16/98 remembers if sector WAS secret (automap)
    unsigned int floorpic : 11;    // can be as small as 8 bits

#endif

} ramSector_t;
//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
typedef struct
{

#if !USE_MSECNODE
    union
    {
    // TODO check this , can be 8bit ?
       short sbbox[4];
       short soundorg[4];
    };
#else
    degenmobj_t soundorg;  // origin for any sounds played by the sector
#endif
    unsigned int linecount :11;   // 11 bit but can be much smaller
    unsigned int ceilingpic :10;  // Can be as small as 8 bit
    unsigned int tag : 11;           // 667 tag exists
    #if PACKED_ADDRESS
        packedAddress_t lines_ppptr;	// packed pointer to packed address
    #else
        uint16_t lineStartIndex;    // next-hack
    #endif
    short sectorNumber;          // this makes pointer easier. Can be as small as 9 bit
} sector_t;
//static inline packedAddress_t * getSectorLines(sector_t *psec) {return (packedAddress_t*) unpackAddress(psec->lines_ppptr);}
//static inline const struct line_s*  getSectorLineByIndex(sector_t *psec, int i) {return (const struct line_s*) unpackAddress(getSectorLines(psec)[i]);}
#if PACKED_ADDRESS
static inline const struct line_s** getSectorLines(sector_t *psec)
{
    return (const struct line_s**) return unpackAddress(psec->lines_ppptr);
}
static inline const struct line_s* getSectorLineByIndex(sector_t *psec, int i)
{
    return getSectorLines(psec)[i];
}
#endif

//
// The SideDef.
//

typedef struct
{
// 2021-02-13 next-hack: changed to reflect map side def, so we do not have to copy a lot of bytes in RAM
    short textureoffset;
    short rowoffset;
    short toptexture;
    short bottomtexture;
    short midtexture;
    short sector_num;  // Front sector, towards viewer.
    //    sector_t* sector;      // Sector the SideDef is facing.
    // short textureoffset; // add this to the calculated texture column
    // short rowoffset;     // add this to the calculated texture top
    // short toptexture;      // Texture indices. We do not maintain names here.
    // short bottomtexture;
    // short midtexture;
} PACKEDATTR side_t;

//
// Move clipping aid for LineDefs.
//
typedef enum
{
    ST_HORIZONTAL, ST_VERTICAL, ST_POSITIVE, ST_NEGATIVE
} slopetype_t;

typedef enum
{                 // cph:
/*	// not used
 RF_TOP_TILE  = 1,     // Upper texture needs tiling
 // not used
 RF_MID_TILE = 2,     // Mid texture needs tiling
 // Not used
 RF_BOT_TILE = 4,     // Lower texture needs tiling
 RF_IGNORE   = 8,     // Renderer can skip this line
 RF_CLOSED   =16,     // Line blocks view
 RF_MAPPED   =32      // Seen so show on automap.*/
// 2020-03-14 next-hack changed value to fit 3 bits
    RF_IGNORE = 1,     // Renderer can skip this line
    RF_CLOSED = 2,     // Line blocks view
    RF_MAPPED = 4      // Seen so show on automap.*/

} r_flags;

//Runtime mutable data for lines.
#define LINE_VALIDCOUNT_BITS 6
#define LINE_R_VALIDCOUNT_BITS 5

#define LINE_VALIDCOUNT_MASK ((1 << LINE_VALIDCOUNT_BITS) - 1)
#define LINE_R_VALIDCOUNT_MASK ((1 << LINE_R_VALIDCOUNT_BITS) - 1)
#if OLD_VALIDCOUNT

typedef struct linedata_s
{
//    unsigned short validcount;        // if == validcount, already checked
//    unsigned short r_validcount;      // cph: if == gametic, r_flags already done
    uint16_t special :1;
    uint16_t stairDirection : 1;
    uint16_t validcount6 :LINE_VALIDCOUNT_BITS;        // if == validcount, already checked
    uint16_t r_validcount5 :LINE_R_VALIDCOUNT_BITS;      // cph: if == gametic, r_flags already done
    uint16_t r_flags :3;
} PACKEDATTR linedata_t;
#endif

typedef struct line_s
{
    vertex_t v1;
    vertex_t v2;     // Vertices, from v1 to v2.
    unsigned int lineno;         //line number.

    fixed_t dx, dy;        // Precalculated v2 - v1 for side checking.

    unsigned short sidenum[2];        // Visual appearance: SideDefs.
    fixed_t bbox[4];        //Line bounding box.

    unsigned short flags;           // Animation related.
    unsigned short const_special;
    short tag;
    short slopetype; // To aid move clipping.

} line_t;

//#define LN_FRONTSECTOR(l) (_g->sides[(l)->sidenum[0]].sector)
#define LN_FRONTSECTOR(l) (&_g->sectors[_g->sides[(l)->sidenum[0]].sector_num])  //nh: replaced to sector num (saves ram)
#define LN_BACKSECTOR(l) ((l)->sidenum[1] != NO_INDEX ? &_g->sectors[_g->sides[(l)->sidenum[1]].sector_num] : NULL)
#if OLD_VALID_COUNT
    #define LN_SPECIAL(l) ( _g->lines[(l)->lineno].const_special * _g->linedata[(l)->lineno].special )

    #define LN_CLR_SPECIAL(l) do {_g->linedata[(l)->lineno].special = 0; } while (0)
    #define LN_SET_SPECIAL(l) (_g->linedata[(l)->lineno].special)

    #define LN_TOGGLE_SPECIAL_STAIR_DIRECTION(l) do {_g->linedata[(l)->lineno].stairDirection ^= 1; } while (0)
    #define LN_SPECIAL_STAIR_DIRECTION(l) (((_g->linedata[(l)->lineno].stairDirection) << StairDirectionShift)  ^ _g->lines[(l)->lineno].const_special)
#else
    #define LN_SPECIAL(l) ( (l)->const_special * (uint32_t) getBitMask32(_g->lineIsSpecial, (l)->lineno))

    #define LN_CLR_SPECIAL(l) clrBitMask32(_g->lineIsSpecial, (l)->lineno)

    #define LN_TOGGLE_SPECIAL_STAIR_DIRECTION(l) setBitMask32Value(_g->lineStairDirection, (l)->lineno, !getBitMask32(_g->lineStairDirection, (l)->lineno)) //do {_g->linedata[(l)->lineno].stairDirection ^= 1; } while (0)
    #define LN_SPECIAL_STAIR_DIRECTION(l) (( getBitMask32(_g->lineStairDirection, (l)->lineno) << StairDirectionShift)  ^ l->const_special)
#endif
// phares 3/14/98
//
// Sector list node showing all sectors an object appears in.
//
// There are two threads that flow through these nodes. The first thread
// starts at touching_thinglist in a sector_t and flows through the m_snext
// links to find all mobjs that are entirely or partially in the sector.
// The second thread starts at touching_sectorlist in an mobj_t and flows
// through the m_tnext links to find all sectors a thing touches. This is
// useful when applying friction or push effects to sectors. These effects
// can be done as thinkers that act upon all objects touching their sectors.
// As an mobj moves through the world, these nodes are created and
// destroyed, with the links changed appropriately.
//
// For the links, NULL means top or end of list.
#if USE_MSECNODE
typedef struct msecnode_s
{
//  sector_t          *m_sector; // a sector containing this object
    unsigned short m_sector_sptr;
//  struct mobj_s     *m_thing;  // this object
    unsigned short m_thing_sptr;
    //struct msecnode_s *m_tprev;  // prev msecnode_t for this thing
    unsigned short m_tprev_sptr;
//  struct msecnode_s *m_tnext;  // next msecnode_t for this thing
    unsigned short m_tnext_sptr;
//   struct msecnode_s *m_sprev;  // prev msecnode_t for this sector
    unsigned short m_sprev_sptr;
//  struct msecnode_s *m_snext;  // next msecnode_t for this sector
    unsigned short m_snext_sptr;
//  boolean visited; // killough 4/4/98, 4/7/98: used in search algorithms

} __attribute__ ((aligned (4))) msecnode_t;
static inline sector_t* getMsecnodeSector(const msecnode_t *p_msn)
{
    return (sector_t*) getLongPtr(p_msn->m_sector_sptr);
}
static inline struct mobj_s* getMsecnodeThing(msecnode_t *p_msn)
{
    return (struct mobj_s*) getLongPtr(p_msn->m_thing_sptr);
}
static inline msecnode_t* getMsecnodeTPrev(msecnode_t *p_msn)
{
    return (msecnode_t*) getLongPtr(p_msn->m_tprev_sptr);
}
static inline msecnode_t* getMsecnodeTNext(const msecnode_t *p_msn)
{
    return (msecnode_t*) getLongPtr(p_msn->m_tnext_sptr);
}
static inline msecnode_t* getMsecnodeSPrev(msecnode_t *p_msn)
{
    return (msecnode_t*) getLongPtr(p_msn->m_sprev_sptr);
}
static inline msecnode_t* getMsecnodeSNext(msecnode_t *p_msn)
{
    return (msecnode_t*) getLongPtr(p_msn->m_snext_sptr);
}
//
static inline msecnode_t* getSectorTouchingThingList(sector_t *psec)
{
    return (msecnode_t*) getLongPtr(psec->touching_thinglist_sptr);
}
#endif
//
// The LineSeg.
//

/*
 typedef struct
 {
 vertex_t *v1, *v2;
 fixed_t offset;
 angle_t angle;
 side_t* sidedef;
 const line_t* linedef;

 // Sector references.
 // Could be retrieved from linedef, too
 // (but that would be slower -- killough)
 // backsector is NULL for one sided lines

 sector_t *frontsector, *backsector;
 } seg_t;
 */

//
// The LineSeg.
//
typedef struct
{
    vertex_t v1;
    vertex_t v2;            // Vertices, from v1 to v2.

    fixed_t offset;
    angle_t angle;

    unsigned short sidenum;
    unsigned short linenum;

    unsigned short frontsectornum;
    unsigned short backsectornum;
} seg_t;

#define SG_FRONTSECTOR(s) ((s)->frontsectornum != NO_INDEX ? &_g->sectors[(s)->frontsectornum] : NULL)
#define SG_BACKSECTOR(s) ((s)->backsectornum != NO_INDEX ? &_g->sectors[(s)->backsectornum] : NULL)

//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs,
//  indicating the visible walls that define
//  (all or some) sides of a convex BSP leaf.
//
#define MAX_SECTOR_NUM ((1 << 9) - 1)
typedef struct subsector_s
{
//    sector_t *sector;
    unsigned int sector_num : 9;         // maximum number of sectors is smaller than 400 on stock doom levels
    unsigned int numlines : 10;         // we do not expect so many lines on a sector.
    unsigned int firstline : 13;        // lines can be over 2048, but less than 4096. First line will be smaller.
    //unsigned short numlines, firstline;
} subsector_t;


//
// OTHER TYPES
//

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
// precalculating 24bpp lightmap/colormap LUT.
// from darkening PLAYPAL to all black.
// Could use even more than 32 levels.

typedef byte lighttable_t;

//
// Masked 2s linedefs
//
typedef struct drawseg_s
{
    fixed_t scale1, scale2;
//    fixed_t scalestep;                   // 30-01-2022: next-hack removed and calculated when required.
    fixed16_t bsilheight16;                   // do not clip sprites above this
    fixed16_t tsilheight16;                   // do not clip sprites below this
//  short *sprtopclip, *sprbottomclip;
    //short *maskedtexturecol; // dropoff overflow. NOTE: can't use short ptr, as it does not point to 4-byte aligned memory.
    signed short maskedtexturecol_sptrb;       // short pointer to byte
    //const seg_t *curline;
    unsigned short curlineindex: 14;
    unsigned short silhouette : 2;                       // 0=none, 1=bottom, 2=top, 3=both
    //unsigned short maskedtexturecolindex;        // note: used index to openings.
    unsigned short sprtopclip_ssptr, sprbottomclip_ssptr; // short pointer to short
    // Pointers to lists for sprite clipping,
    // all three adjusted so [x1] is first value.
 #if SCREENWIDTH > 255
    uint16_t x1, x2;
 #else
    uint8_t x1, x2;                           // the screen is 240 pix wide...
 #endif
} drawseg_t;

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.
typedef struct
{
    short width;		// bounding box size
    short height;
    short leftoffset;	// pixels to the left of origin
    short topoffset;	// pixels below the origin
    int columnofs[8];	// only [width] used
    // the [0] is &columnofs[width]
} patch_t;

typedef struct
{
#if !FAST_CPU_SMALL_FLASH
    short width;		// bounding box size
    short height;
    short leftoffset;	// pixels to the left of origin
    short topoffset;	// pixels below the origin
#else
    int32_t width : 10;		// max is 320
    int32_t leftoffset : 10 ;	// pixels to the left of origin
    int32_t topoffset : 10;	// pixels below the origin
    byte height;        // max is 200

#endif

} PACKEDATTR patchsizeoffsets_t; // like patch, but only size and offset. 2022-05-06: resized to save flash space.
typedef struct
{
    short width;		// bounding box size
    short height;
    short leftoffset;	// pixels to the left of origin
    short topoffset;	// pixels below the origin
} fullpatchsizeoffsets_t;
// posts are runs of non masked source pixels
typedef struct
{
    byte topdelta;	// -1 is the last post in a column
    byte length; 	// length data bytes follows
} post_t;

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t column_t;

//
// A vissprite_t is a thing that will be drawn during a refresh.
// i.e. a sprite object that is partly visible.
//

typedef struct vissprite_s
{
    //fixed_t gx, gy;              // for line side calculation
    fixed_t startfrac;           // horizontal position of x1. We could reduce this...
    fixed_t scale;
    fixed_t xiscale;             // negative if flipped
    fixed_t texturemid : 26;          // we could reduce this if we need some bits...
    uint32_t colormap_idx : 6;

// 2021/03/27 next-hack: removed iscale and calculated when necessary. Saved some bytes...
//  fixed_t iscale;
    // for color translation and shadow draw, maxbright frames as well
//  const lighttable_t *colormap;
    //const patch_t *patch;
    // 30-01-2022: next-hack: put to 16 bit to save something
    //fixed16_t gz16;                  // global bottom for silhouette clipping. Top calculated at runtime
    uint16_t mobj_sptr;
    uint16_t lumpNum:14;
    uint16_t mobjflags:2;
    //
 #if SCREENWIDTH > 255
    uint16_t x1, x2;
 #else
    uint8_t x1, x2;
#endif
} PACKEDATTR vissprite_t;

//
// Sprites are patches with a special naming convention
//  so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with
//  x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//  is range checked at run time.
// A sprite is a patch_t that is assumed to represent
//  a three dimensional object and may have multiple
//  rotations pre drawn.
// Horizontal flipping is used to save space,
//  thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//

typedef struct
{
    // Lump to use for view angles 0-7.
    short lump[8];
    // Flip bit (1 = flip) to use for view angles 0-7.
    //byte  flip[8];
    byte flipmask;

    // If false use 0 for any position.
    // Note: as eight entries are available,
    //  we might as well insert the same name eight times.
    boolean rotate;

} spriteframe_t;

#define SPR_FLIPPED(s, r) (s->flipmask & (1 << r))

//
// A sprite definition:
//  a number of animation frames.
//

typedef struct
{
    short numframes;
#ifndef USE_SPRITEFRAME_SPTR
    spriteframe_t *spriteframes;
#else
  unsigned short spriteframes_sptr;
#endif
} spritedef_t __attribute__ ((aligned (4)));
static inline spriteframe_t* getSpriteFrames(const spritedef_t *p_sd)
{
#ifndef USE_SPRITEFRAME_SPTR
    return p_sd->spriteframes;
#else
    return (spriteframe_t *) getLongPtr(p_sd->spriteframes_sptr);
#endif
}
//
// Now what is a visplane, anyway?
//
// Go to http://classicgaming.com/doom/editing/ to find out -- killough
//
#define USE_VISPLANE_MODIFIED 1
typedef struct visplane
{
#if 1
//    struct visplane *next;        // Next visplane in hash chain -- killough
    unsigned short next_sptr; 
    fixed16_t height16;
#if USE_VISPLANE_MODIFIED
    int32_t modified : 1;
#endif
#if SCREENWIDTH < 255
// next-hack: used padding bytes (which were used to hold initial and final plane drawing top and bottom values) to store plane info.
// these would be modified AFTER their value is actually copied in the dsvars, so it is useless to keep them.
    int32_t picnum : 8;         // will be used also for top[minx - 1] when minx = 0
    byte top[SCREENWIDTH];
    byte minx;//byte pad2;      // used for top[maxx]
    // made unsigned maxx so it requires one bit less.
    byte umaxx; //byte pad3;      // used for minx-1 for bottom
    byte bottom[SCREENWIDTH];  
    byte lightlevel; //byte pad4;      // used for bottom[maxx].
#else
    short minx, umaxx:15;
    int32_t picnum : 8;         // will be used also for top[minx - 1] when minx = 0
    byte lightlevel; //byte pad4;      // used for bottom[maxx].
    byte top[SCREENWIDTH];
    byte pad2;      // used for top[maxx]
    byte pad3;      // used for minx-1 for bottom
    byte bottom[SCREENWIDTH];
    byte pad4;      // used for bottom[maxx].
#endif
#else
  struct visplane *next;        // Next visplane in hash chain -- killough
  short picnum, lightlevel;
  short minx, maxx;
  fixed_t height;
  unsigned int modified;

  //byte    pad1;
  // Here lies the rub for all
  //  dynamic resize/change of resolution.
  byte    top[SCREENWIDTH];
  byte    pad2;
  byte    pad3;
  // See above.
  byte    bottom[SCREENWIDTH];
  byte    pad4;
#endif
} visplane_t __attribute((aligned(4)));
static inline int32_t getVisplaneMaxx(visplane_t *vp)
{
#if SCREENWIDTH < 255
    return vp->umaxx == 0xFF ? -1 : vp->umaxx;
#else
    return vp->umaxx;
#endif
}
//inline mobj_t * getSectorThingList(sector_t * ps)  {return (mobj_t*) getLongPtr(ps->thinglist_sptr);}
#endif
