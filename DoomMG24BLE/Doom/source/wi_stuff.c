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
 *
 * DESCRIPTION:
 *  Intermission screens.
 *
 * next-hack:
 * added support for loading from external flash.
 * Improved speed by loading most of the patches by number, and not by name.
 * (the name to num conversion is done at startup)
 * Restored Multiplayer stats.
 *-----------------------------------------------------------------------------
 */

#include "doomstat.h"
#include "m_random.h"
#include "w_wad.h"
#include "g_game.h"
#include "r_main.h"
#include "v_video.h"
#include "wi_stuff.h"
#include "s_sound.h"
#include "sounds.h"
#include "lprintf.h"  // jff 08/03/98 - declaration of lprintf
#include "r_draw.h"

#include "global_data.h"
#include "extMemory.h"

static void WI_loadData(void);
//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//

//
// Different vetween registered DOOM (1994) and
//  Ultimate DOOM - Final edition (retail, 1995?).
// This is supposedly ignored for commercial
//  release (aka DOOM II), which had 34 maps
//  in one episode. So there.
#define NUMEPISODES 4
#define NUMMAPS     9

// Not used
// in tics
//U #define PAUSELEN    (TICRATE*2)
//U #define SCORESTEP    100
//U #define ANIMPERIOD    32
// pixel distance from "(YOU)" to "PLAYER N"
//U #define STARDIST  10
//U #define WK 1

// GLOBAL LOCATIONS
#define WI_TITLEY      2
#define WI_SPACINGY   33

// SINGLE-PLAYER STUFF
#define SP_STATSX     50
#define SP_STATSY     50

#define SP_TIMEX      8
// proff/nicolas 09/20/98 -- changed for hi-res
#define SP_TIMEY      160
//#define SP_TIMEY      (SCREENHEIGHT-32)

// NET GAME STUFF
#define NG_STATSY     50
#define NG_STATSX     (32 + R_NumPatchWidth(star_lump)/2 + 32*!dofrags)

#define NG_SPACINGX   64

// Used to display the frags matrix at endgame
// DEATHMATCH STUFF
#define DM_MATRIXX    42
#define DM_MATRIXY    68

#define DM_SPACINGX   40

#define DM_TOTALSX   269

#define DM_KILLERSX   10
#define DM_KILLERSY  100
#define DM_VICTIMSX    5
#define DM_VICTIMSY   50


// These animation variables, structures, etc. are used for the
// DOOM/Ultimate DOOM intermission screen animations.  This is
// totally different from any sprite or texture/flat animations
typedef enum
{
  ANIM_ALWAYS,   // determined by patch entry
  ANIM_RANDOM,   // occasional
  ANIM_LEVEL     // continuous
} animenum_t;

typedef struct
{
    uint16_t x;       // x/y coordinate pair structure
    uint16_t y;
} point_t;


//
// Animation.
// There is another anim_t used in p_spec.
// next-hack: Separated const and variable anim data structure. Changed name as well.
//
typedef struct
{
  uint8_t  type;

  // period in tics between animations
  uint8_t   period;

  // number of animation frames
  uint8_t   nanims;

  // location of animation
  point_t loc;

  // ALWAYS: n/a,
  // RANDOM: period deviation (<256),
  // LEVEL: level
  uint8_t  data1;
} const_wi_anim_t;

typedef struct
{
  /* actual graphics for frames of animations
   * cphipps - const
   */
  uint16_t p_num[3];

  // following must be initialized to zero before use!

  // next value of bcnt (used in conjunction with period)
  uint8_t   nexttic;

  // last drawn animation frame
  int8_t   lastdrawn;

  // next frame number to animate
  int8_t   ctr;

} wi_anim_t;
static const point_t lnodes[NUMEPISODES][NUMMAPS] =
{
    // Episode 0 World Map
    {
        { 185, 164 }, // location of level 0 (CJ)
        { 148, 143 }, // location of level 1 (CJ)
        { 69, 122 },  // location of level 2 (CJ)
        { 209, 102 }, // location of level 3 (CJ)
        { 116, 89 },  // location of level 4 (CJ)
        { 166, 55 },  // location of level 5 (CJ)
        { 71, 56 },   // location of level 6 (CJ)
        { 135, 29 },  // location of level 7 (CJ)
        { 71, 24 }    // location of level 8 (CJ)
    },

    // Episode 1 World Map should go here
    {
        { 254, 25 },  // location of level 0 (CJ)
        { 97, 50 },   // location of level 1 (CJ)
        { 188, 64 },  // location of level 2 (CJ)
        { 128, 78 },  // location of level 3 (CJ)
        { 214, 92 },  // location of level 4 (CJ)
        { 133, 130 }, // location of level 5 (CJ)
        { 208, 136 }, // location of level 6 (CJ)
        { 148, 140 }, // location of level 7 (CJ)
        { 235, 158 }  // location of level 8 (CJ)
    },

    // Episode 2 World Map should go here
    {
        { 156, 168 }, // location of level 0 (CJ)
        { 48, 154 },  // location of level 1 (CJ)
        { 174, 95 },  // location of level 2 (CJ)
        { 265, 75 },  // location of level 3 (CJ)
        { 130, 48 },  // location of level 4 (CJ)
        { 279, 23 },  // location of level 5 (CJ)
        { 198, 48 },  // location of level 6 (CJ)
        { 140, 25 },  // location of level 7 (CJ)
        { 281, 136 }  // location of level 8 (CJ)
    } 
};


//
// Animation locations for episode 0 (1).
// Using patches saves a lot of space,
//  as they replace 320x200 full screen frames.
//
static const const_wi_anim_t epsd0animinfo[] =
{
  { ANIM_ALWAYS, TICRATE/3, 3, { 224, 104 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 184, 160 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 112, 136 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 72, 112 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 88, 96 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 64, 48 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 192, 40 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 136, 16 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 80, 16 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 64, 24 }, 0 }
};

static const const_wi_anim_t epsd1animinfo[] =
{
  { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 1 },
  { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 2 },
  { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 3 },
  { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 4 },
  { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 5 },
  { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 6 },
  { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 7 },
  { ANIM_LEVEL,  TICRATE/3, 3, { 192, 144 }, 8 },
  { ANIM_LEVEL,  TICRATE/3, 1, { 128, 136 }, 8 }
};

static const const_wi_anim_t epsd2animinfo[] =
{
  { ANIM_ALWAYS, TICRATE/3, 3, { 104, 168 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 40, 136 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 160, 96 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 104, 80 }, 0 },
  { ANIM_ALWAYS, TICRATE/3, 3, { 120, 32 }, 0 },
  { ANIM_ALWAYS, TICRATE/4, 3, { 40, 0 }, 0 }
};

static const int NUMANIMS[NUMEPISODES] =
{
  sizeof(epsd0animinfo)/sizeof(wi_anim_t),
  sizeof(epsd1animinfo)/sizeof(wi_anim_t),
  sizeof(epsd2animinfo)/sizeof(wi_anim_t),
  0
};

static const const_wi_anim_t *anims[NUMEPISODES] =
{
  epsd0animinfo,
  epsd1animinfo,
  epsd2animinfo
};
// next-hack: this is a dirty hack
#define MAX_EPI__ANIM sizeof(epsd0animinfo)/sizeof(wi_anim_t)
static wi_anim_t ram_anims[MAX_EPI__ANIM];
//
// GENERAL DATA
//

//
// Locally used stuff.
//
#define FB 0

// States for single-player
#define SP_KILLS    0
#define SP_ITEMS    2
#define SP_SECRET   4
#define SP_FRAGS    6
#define SP_TIME     8
#define SP_PAR      ST_TIME

#define SP_PAUSE    1

// in seconds
#define SHOWNEXTLOCDELAY  4
//#define SHOWLASTLOCDELAY  SHOWNEXTLOCDELAY

//
//  GRAPHICS
//
// You Are Here graphic
static const char *const yah_names[2] =
{ "WIURH0", "WIURH1" };

// splat
static const char *const splat_name = "WISPLAT";
// %, : graphics
static const char *const percent_name = "WIPCNT";
static const char *const colon_name = "WICOLON";

// minus sign
static const char *const wiminus_name = "WIMINUS";

// "Finished!" graphics
static const char *const finished_name = "WIF";

// "Entering" graphic
static const char *const entering_name = "WIENTER";

// "secret"
static const char *const sp_secret_name = "WISCRT2";

// "Kills", "Scrt", "Items", "Frags"
static const char *const kills_name = "WIOSTK";
static const char *const items_name = "WIOSTI";
static const char *const secret_name = "WIOSTS";
static const char *const frags_name = "WIFRGS";

// "killers", "victims"
static const char *const killers_name = "WIKILRS";
static const char *const victims_name = "WIVCTMS";
// "red P[1..MAXPLAYERS]"
static const char *const facebackp_names[MAXPLAYERS] = {"STPB0", "STPB1", "STPB2", "STPB3"};
// Time sucks.
static const char *const time1_name = "WITIME";
static const char *const par_name = "WIPAR";
static const char *sucks_name = "WISUCKS";

// "Total", your face, your dead face
static const char *const total_name = "WIMSTT";
static const char *const star_name = "STFST01";
static const char *const bstar_name = "STFDEAD0";

// lump numbers

// You Are Here graphic
static short yah_lumps[2];

// splat
static short splat_lump;
// %, : graphics
static short percent_lump;
static short colon_lump;

// minus sign
static short wiminus_lump;

// "Finished!" graphics
static short finished_lump;

// "Entering" graphic
static short entering_lump;

// "secret"
static short sp_secret_lump;

// "Kills", "Scrt", "Items", "Frags"
static short kills_lump;
static short items_lump;

// Time sucks.
static short time1_lump;
static short par_lump;
static short sucks_lump;

// "Total", your face, your dead face
static short total_lump;
//
static short facebackp_lumps[MAXPLAYERS];
//
static short killers_lump;
static short victims_lump;
static short star_lump;
static short bstar_lump;
static short frags_lump;
static short secret_lump;
//
static uint8_t    cnt_pause;

//
// CODE
//

static void WI_endDeathmatchStats(void);
static void WI_endNetgameStats(void);
#define WI_endStats WI_endNetgameStats


#define getPatchLump(patch) patch##_lump = W_GetNumForName(patch##_name)
void WI_Init(void)
{
    char name[9];
    for (int i = 0; i < 10; i++)
    {
        // numbers 0-9
        sprintf(name, "WINUM%d", i);
        _g->num[i] = W_CacheLumpName(name);
    }
    // You Are Here graphic
    yah_lumps[0] = W_GetNumForName(yah_names[0]);
    yah_lumps[1] = W_GetNumForName(yah_names[1]);
    // splat
    getPatchLump(splat);
    // %, : graphics
    getPatchLump(percent);
    getPatchLump(colon);

    // minus sign
    getPatchLump(wiminus);

    // "Finished!" graphics
    getPatchLump(finished);

    // "Entering" graphic
    getPatchLump(entering);

    // "secret"
    getPatchLump(sp_secret);

    // "Kills", "Scrt", "Items", "Frags"
    getPatchLump(kills);
    getPatchLump(items);

    // Time sucks.
    getPatchLump(time1);
    getPatchLump(par);
    getPatchLump(sucks);

    // "Total", your face, your dead face
    getPatchLump(total);
    //
    for (int i = 0; i < MAXPLAYERS; i++)
      facebackp_lumps[i] = W_GetNumForName(facebackp_names[i]);
    //
    getPatchLump(killers);
    getPatchLump(victims);
    getPatchLump(star);
    getPatchLump(bstar);
    getPatchLump(frags);
    getPatchLump(secret);
}

/* ====================================================================
 * WI_levelNameLump
 * Purpore: Returns the name of the graphic lump containing the name of
 *          the given level.
 * Args:    Episode and level, and buffer (must by 9 chars) to write to
 * Returns: void
 */
void WI_levelNameLump(int epis, int map, char *buf)
{
    if (_g->gamemode == commercial)
    {
        sprintf(buf, "CWILV%2.2d", map);
    }
    else
    {
        sprintf(buf, "WILV%d%d", epis, map);
    }
}

// ====================================================================
// WI_slamBackground
// Purpose: Put the full-screen background up prior to patches
// Args:    none
// Returns: void
//
static void WI_slamBackground(void)
{
    char name[9];  // limited to 8 characters

    if (_g->gamemode == commercial || (_g->gamemode == retail && _g->wbs->epsd == 3))
        strcpy(name, "INTERPIC");
    else
        sprintf(name, "WIMAP%d", _g->wbs->epsd);

    // background
    V_DrawNamePatch(0, 0, FB, name, CR_DEFAULT, VPT_STRETCH);
}

// ====================================================================
// WI_Responder
// Purpose: Draw animations on intermission background screen
// Args:    ev    -- event pointer, not actually used here.
// Returns: False -- dummy routine
//
// The ticker is used to detect keys
//  because of timing issues in netgames.
boolean WI_Responder(event_t *ev)
{
  (void) ev;
    return false;
}

// ====================================================================
// WI_drawLF
// Purpose: Draw the "Finished" level name before showing stats
// Args:    none
// Returns: void
//
void WI_drawLF(void)
{
    int y = WI_TITLEY;
    char lname[9];

    // draw <LevelName>
    /* cph - get the graphic lump name and use it */
    WI_levelNameLump(_g->wbs->epsd, _g->wbs->last, lname);
    // CPhipps - patch drawing updated
    V_DrawNamePatch((320 - V_NamePatchWidth(lname))/2, y, FB, lname, CR_DEFAULT, VPT_STRETCH);

    // draw "Finished!"
    y += (5 * V_NamePatchHeight(lname)) / 4;

    // CPhipps - patch drawing updated
//  V_DrawNamePatch((320 - V_NamePatchWidth(finished))/2, y, FB, finished, CR_DEFAULT, VPT_STRETCH);
    V_DrawNumPatch((320 - R_NumPatchWidth(finished_lump)) / 2, y, FB, finished_lump, CR_DEFAULT, VPT_STRETCH);
}

// ====================================================================
// WI_drawEL
// Purpose: Draw introductory "Entering" and level name
// Args:    none
// Returns: void
//
void WI_drawEL(void)
{
    int y = WI_TITLEY;
    char lname[9];

    /* cph - get the graphic lump name */
    WI_levelNameLump(_g->wbs->epsd, _g->wbs->next, lname);

    // draw "Entering"
    // CPhipps - patch drawing updated
    // V_DrawNamePatch((320 - V_NamePatchWidth(entering))/2, y, FB, entering, CR_DEFAULT, VPT_STRETCH);
    V_DrawNumPatch((320 - R_NumPatchWidth(entering_lump)) / 2, y, FB, entering_lump, CR_DEFAULT, VPT_STRETCH);
    // draw level
    y += (5 * V_NamePatchHeight(lname)) / 4;

    // CPhipps - patch drawing updated
    V_DrawNamePatch((320 - V_NamePatchWidth(lname))/2, y, FB, lname, CR_DEFAULT, VPT_STRETCH);
}

/* ====================================================================
 * WI_drawOnLnode
 * Purpose: Draw patches at a location based on episode/map
 * Args:    n   -- index to map# within episode
 *          c[] -- array of names of patches to be drawn
 * Returns: void
 */
void WI_drawOnLnode  // draw stuff at a location by episode/map#
(int n, short *lumps)
{
    int i;
    boolean fits = false;

    i = 0;
    do
    {
        int left;
        int top;
        int right;
        int bottom;
        const patch_t *patch = W_CacheLumpNum(lumps[i]);  //Name(c[i]);
        patch_t tmpPatch;
        // support for patch on external flash
        if (isOnExternalFlash(patch))
        {
            extMemSetCurrentAddress((uint32_t) patch);
            extMemGetDataFromCurrentAddress(&tmpPatch, sizeof(tmpPatch));
            patch = &tmpPatch;
        }
        left = lnodes[_g->wbs->epsd][n].x - patch->leftoffset;
        top = lnodes[_g->wbs->epsd][n].y - patch->topoffset;
        right = left + patch->width;
        bottom = top + patch->height;
        if (left >= 0 && right < 320 && top >= 0 && bottom < 200)
        {
            fits = true;
        }
        else
        {
            i++;
        }
    } while (!fits && i != 2);

    if (fits && i < 2)
    {
        // CPhipps - patch drawing updated
        V_DrawNumPatch(lnodes[_g->wbs->epsd][n].x, lnodes[_g->wbs->epsd][n].y, FB, lumps[i], CR_DEFAULT, VPT_STRETCH);
    }
    else
    {
        // DEBUG
        //jff 8/3/98 use logical output routine
        lprintf(LO_DEBUG, "Could not place patch on level %d", n + 1);
    }
}
// ====================================================================
// WI_initAnimatedBack
// Purpose: Initialize pointers and styles for background animation
// Args:    none
// Returns: void
//
void WI_initAnimatedBack()
{
  int   i;
  wi_anim_t* a;
  const const_wi_anim_t* ca;
  if (_g->gamemode == commercial)  // no animation for DOOM2
    return;

  if (_g->wbs->epsd > 2)
    return;


  for (i = 0; i < NUMANIMS[_g->wbs->epsd]; i++)
  {
    ca = &anims[_g->wbs->epsd][i];
    a = &ram_anims[i];
    // init variables
    a->ctr = -1;

    // specify the next time to draw it
    if (ca->type == ANIM_ALWAYS)
      a->nexttic = _g->bcnt + 1 + (M_Random()%ca->period);
    else
      if (ca->type == ANIM_RANDOM)
        a->nexttic = _g->bcnt + 1 + (M_Random() % ca->data1);
      else
        if (ca->type == ANIM_LEVEL)
          a->nexttic = _g->bcnt + 1;
  }
}


// ====================================================================
// WI_updateAnimatedBack
// Purpose: Figure out what animation we do on this iteration
// Args:    none
// Returns: void
//
void WI_updateAnimatedBack(void)
{
  int   i;
  const const_wi_anim_t* ca;
  wi_anim_t *a;

  if (_g->gamemode == commercial)
    return;

  if (_g->wbs->epsd > 2)
    return;

  for (i = 0; i < NUMANIMS[_g->wbs->epsd]; i++)
  {
    ca = &anims[_g->wbs->epsd][i];
    a = &ram_anims[i];
    if (_g->bcnt == a->nexttic)
    {
      switch (ca->type)
      {
        case ANIM_ALWAYS:
             if (++a->ctr >= ca->nanims) a->ctr = 0;
             a->nexttic = _g->bcnt + ca->period;
             break;

        case ANIM_RANDOM:
             a->ctr++;
             if (a->ctr == ca->nanims)
             {
               a->ctr = -1;
               a->nexttic = _g->bcnt + (M_Random() % ca->data1);
             }
             else
               a->nexttic = _g->bcnt + ca->period;
             break;

        case ANIM_LEVEL:
             // gawd-awful hack for level anims
             if (!(_g->state == StatCount && i == 7)
                && _g->wbs->next == ca->data1)
             {
               a->ctr++;
               if (a->ctr == ca->nanims)
               {
                 a->ctr--;
               }
               a->nexttic = _g->bcnt + ca->period;
             }
             break;
      }
    }
  }
}


// ====================================================================
// WI_drawAnimatedBack
// Purpose: Actually do the animation (whew!)
// Args:    none
// Returns: void
//
void WI_drawAnimatedBack(void)
{
  int     i;
  wi_anim_t*   a;
  const const_wi_anim_t*   ca;
  if (_g->gamemode == commercial) //jff 4/25/98 Someone forgot commercial an enum
    return;

  if (_g->wbs->epsd > 2)
    return;

  for (i=0 ; i<NUMANIMS[_g->wbs->epsd] ; i++)
  {
    a = &ram_anims[i];
    ca = &anims[_g->wbs->epsd][i];

    if (a->ctr >= 0)
      // CPhipps - patch drawing updated
      V_DrawNumPatch(ca->loc.x, ca->loc.y, FB, a->p_num[a->ctr], CR_DEFAULT, VPT_STRETCH);
  }
}

// ====================================================================
// WI_drawNum
// Purpose: Draws a number.  If digits > 0, then use that many digits
//          minimum, otherwise only use as many as necessary
// Args:    x, y   -- location
//          n      -- the number to be drawn
//          digits -- number of digits minimum or zero
// Returns: new x position after drawing (note we are going to the left)
// CPhipps - static
static int WI_drawNum(int x, int y, int n, int digits)
{
  return V_DrawNum(x, y, n, digits);
}

// ====================================================================
// WI_drawPercent
// Purpose: Draws a percentage, really just a call to WI_drawNum
//          after putting a percent sign out there
// Args:    x, y   -- location
//          p      -- the percentage value to be drawn, no negatives
// Returns: void
// CPhipps - static
static void WI_drawPercent(int x, int y, int p)
{
    if (p < 0)
        return;
    // CPhipps - patch drawing updated
    V_DrawNumPatch(x, y, FB, percent_lump, CR_DEFAULT, VPT_STRETCH);

    WI_drawNum(x, y, p, -1);

}

// ====================================================================
// WI_drawTime
// Purpose: Draws the level completion time or par time, or "Sucks"
//          if 1 hour or more
// Args:    x, y   -- location
//          t      -- the time value to be drawn
// Returns: void
//
// CPhipps - static
//         - largely rewritten to display hours and use slightly better algorithm

static void WI_drawTime(int x, int y, int t)
{
    int n;

    if (t < 0)
        return;

    if (t < 100 * 60 * 60)
        for (;;)
        {
            n = t % 60;
            t /= 60;
            x = WI_drawNum(x, y, n, (t || n > 9) ? 2 : 1) - R_NumPatchWidth(colon_lump);

            // draw
            if (t)
                // CPhipps - patch drawing updated
                V_DrawNumPatch(x, y, FB, colon_lump, CR_DEFAULT, VPT_STRETCH);
            else
                break;
        }
    else
        // "sucks" (maybe should be "addicted", even I've never had a 100 hour game ;)
        V_DrawNumPatch(x - R_NumPatchWidth(sucks_lump), y, FB, sucks_lump, CR_DEFAULT, VPT_STRETCH);
}

// ====================================================================
// WI_End
// Purpose: Unloads data structures (inverse of WI_Start)
// Args:    none
// Returns: void
//
void WI_End(void)
{
  if (_g->deathmatch)
    WI_endDeathmatchStats();
  else if (_g->netgame)
    WI_endNetgameStats();
  else
    WI_endStats();}

// ====================================================================
// WI_initNoState
// Purpose: Clear state, ready for end of level activity
// Args:    none
// Returns: void
//
void WI_initNoState(void)
{
    _g->state = NoState;
    _g->acceleratestage = 0;
    _g->cnt = 10;
}

// ====================================================================
// WI_drawTimeStats
// Purpose: Put the times on the screen
// Args:    time, total time, par time, in seconds
// Returns: void
//
// cph - pulled from WI_drawStats below

static void WI_drawTimeStats(int cnt_time, int cnt_total_time, int cnt_par)
{
    V_DrawNumPatch(SP_TIMEX, SP_TIMEY, FB, time1_lump, CR_DEFAULT, VPT_STRETCH);
    WI_drawTime(320 / 2 - SP_TIMEX, SP_TIMEY, cnt_time);

    V_DrawNumPatch(SP_TIMEX, (SP_TIMEY + 200) / 2, FB, total_lump, CR_DEFAULT, VPT_STRETCH);
    WI_drawTime(320 / 2 - SP_TIMEX, (SP_TIMEY + 200) / 2, cnt_total_time);

    // Ty 04/11/98: redid logic: should skip only if with pwad but
    // without deh patch
    // killough 2/22/98: skip drawing par times on pwads
    // Ty 03/17/98: unless pars changed with deh patch

    if (_g->wbs->epsd < 3)
    {
        V_DrawNumPatch(320 / 2 + SP_TIMEX, SP_TIMEY, FB, par_lump, CR_DEFAULT, VPT_STRETCH);
        WI_drawTime(320 - SP_TIMEX, SP_TIMEY, cnt_par);
    }

}

// ====================================================================
// WI_updateNoState
// Purpose: Cycle until end of level activity is done
// Args:    none
// Returns: void
//
void WI_updateNoState(void)
{
  WI_updateAnimatedBack();

    if (!--_g->cnt)
        G_WorldDone();
}

// ====================================================================
// WI_initShowNextLoc
// Purpose: Prepare to show the next level's location
// Args:    none
// Returns: void
//
void WI_initShowNextLoc(void)
{
    if ((_g->gamemode != commercial) && (_g->gamemap == 8))
    {
        G_WorldDone();
        return;
    }

    _g->state = ShowNextLoc;


    _g->acceleratestage = 0;

    _g->cnt = SHOWNEXTLOCDELAY * TICRATE;
    WI_initAnimatedBack();

}

// ====================================================================
// WI_updateShowNextLoc
// Purpose: Prepare to show the next level's location
// Args:    none
// Returns: void
//
void WI_updateShowNextLoc(void)
{

    if (!--_g->cnt || _g->acceleratestage)
        WI_initNoState();
    else
        _g->snl_pointeron = (_g->cnt & 31) < 20;
}

// ====================================================================
// WI_drawShowNextLoc
// Purpose: Show the next level's location on animated backgrounds
// Args:    none
// Returns: void
//
void WI_drawShowNextLoc(void)
{
    int i;
    int last;

    WI_slamBackground();
    // draw animated background
    WI_drawAnimatedBack();
    if (_g->gamemode != commercial)
    {
        if (_g->wbs->epsd > 2)
        {
            WI_drawEL();  // "Entering..." if not E1 or E2
            return;
        }

        last = (_g->wbs->last == 8) ? _g->wbs->next - 1 : _g->wbs->last;
        // draw a splat on taken cities.
        for (i = 0; i <= last; i++)
            WI_drawOnLnode(i, &splat_lump);
        // splat the secret level?
        if (_g->wbs->didsecret)
            WI_drawOnLnode(8, &splat_lump);
        // draw flashing ptr
        if (_g->snl_pointeron)
            WI_drawOnLnode(_g->wbs->next, yah_lumps);
    }

    // draws which level you are entering..
    if ((_g->gamemode != commercial) || _g->wbs->next != 30) // check for MAP30 end game
        WI_drawEL();
}

// ====================================================================
// WI_drawNoState
// Purpose: Draw the pointer and next location
// Args:    none
// Returns: void
//
void WI_drawNoState(void)
{
    _g->snl_pointeron = true;
    WI_drawShowNextLoc();
}

// ====================================================================
// WI_fragSum
// Purpose: Calculate frags for this player based on the current totals
//          of all the other players.  Subtract self-frags.
// Args:    playernum -- the player to be calculated
// Returns: the total frags for this player
//
int WI_fragSum(int playernum)
{
  int   i;
  int   frags = 0;

  for (i=0 ; i<MAXPLAYERS ; i++)
  {
    if (_g->playeringame[i]  // is this player playing?
       && i != playernum) // and it's not the player we're calculating
    {
      frags += _g->plrs[playernum].frags[i];
    }
  }


  // JDC hack - negative frags.
  frags -= _g->plrs[playernum].frags[playernum];

  return frags;
}

static char          dm_state;
// CPhipps - short, dynamically allocated
static short int  **dm_frags;  // frags matrix
static short int   *dm_totals;  // totals by player

// ====================================================================
// WI_initDeathmatchStats
// Purpose: Set up to display DM stats at end of level.  Calculate
//          frags for all players.
// Args:    none
// Returns: void
//
void WI_initDeathmatchStats(void)
{
  int   i; // looping variables

  // CPhipps - allocate data structures needed
  dm_frags  = calloc(MAXPLAYERS, sizeof(*dm_frags));
  dm_totals = calloc(MAXPLAYERS, sizeof(*dm_totals));

  _g->state = StatCount;  // We're doing stats
  _g->acceleratestage = 0;
  dm_state = 1;  // count how many times we've done a complete stat

  cnt_pause = TICRATE;

  for (i=0 ; i<MAXPLAYERS ; i++)
  {
    if (_g->playeringame[i])
    {
      // CPhipps - allocate frags line
      dm_frags[i] = calloc(MAXPLAYERS, sizeof(**dm_frags)); // set all counts to zero

      dm_totals[i] = 0;
    }
  }
  WI_initAnimatedBack();
}
// ====================================================================
// CPhipps - WI_endDeathmatchStats
// Purpose: Deallocate dynamically allocated DM stats data
// Args:    none
// Returns: void
//

void WI_endDeathmatchStats(void)
{
  int i;
  for (i=0; i<MAXPLAYERS; i++)
    free(dm_frags[i]);

  free(dm_frags); free(dm_totals);
}
// ====================================================================
// WI_updateDeathmatchStats
// Purpose: Advance Deathmatch stats screen animation.  Calculate
//          frags for all players.  Lots of noise and drama around
//          the presentation.
// Args:    none
// Returns: void
//
void WI_updateDeathmatchStats(void)
{
  int   i;
  int   j;

  uint8_t stillticking;

  WI_updateAnimatedBack();

  if (_g->acceleratestage && dm_state != 4)  // still ticking
  {
    _g->acceleratestage = 0;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (_g->playeringame[i])
      {
        for (j=0 ; j<MAXPLAYERS ; j++)
          if (_g->playeringame[j])
            dm_frags[i][j] = _g->plrs[i].frags[j];

        dm_totals[i] = WI_fragSum(i);
      }
    }


    S_StartSound(0, sfx_barexp);  // bang
    dm_state = 4;  // we're done with all 4 (or all we have to do)
  }


  if (dm_state == 2)
  {
    if (!(_g->bcnt&3))
      S_StartSound(0, sfx_pistol);  // noise while counting

    stillticking = false;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (_g->playeringame[i])
      {
        for (j=0 ; j<MAXPLAYERS ; j++)
        {
          if (_g->playeringame[j]
             && dm_frags[i][j] != _g->plrs[i].frags[j])
          {
            if (_g->plrs[i].frags[j] < 0)
              dm_frags[i][j]--;
            else
              dm_frags[i][j]++;

            if (dm_frags[i][j] > 999) // Ty 03/17/98 3-digit frag count
              dm_frags[i][j] = 999;

            if (dm_frags[i][j] < -999)
              dm_frags[i][j] = -999;

            stillticking = true;
          }
        }
        dm_totals[i] = WI_fragSum(i);

        if (dm_totals[i] > 999)
          dm_totals[i] = 999;

        if (dm_totals[i] < -999)
          dm_totals[i] = -999;  // Ty 03/17/98 end 3-digit frag count
      }
    }

    if (!stillticking)
    {
      S_StartSound(0, sfx_barexp);
      dm_state++;
    }
  }
  else if (dm_state == 4)
  {
    if (_g->acceleratestage)
    {
      S_StartSound(0, sfx_slop);

      if ( _g->gamemode == commercial)
        WI_initNoState();
      else
        WI_initShowNextLoc();
    }
  }
  else if (dm_state & 1)
  {
    if (!--cnt_pause)
    {
      dm_state++;
      cnt_pause = TICRATE;
    }
  }
}

// ====================================================================
// WI_drawDeathmatchStats
// Purpose: Draw the stats on the screen in a matrix
// Args:    none
// Returns: void
//
// proff/nicolas 09/20/98 -- changed for hi-res
// CPhipps - patch drawing updated
void WI_drawDeathmatchStats(void)
{
  int   i;
  int   j;
  int   x;
  int   y;
  int   w;

  int   halfface = R_NumPatchWidth(facebackp_lumps[0]) / 2;

  WI_slamBackground();

  // draw animated background
  WI_drawAnimatedBack();
  WI_drawLF();

  // draw stat titles (top line)
  V_DrawNumPatch(DM_TOTALSX-R_NumPatchWidth(total_lump) / 2,
     DM_MATRIXY-WI_SPACINGY+10, FB, total_lump, CR_DEFAULT, VPT_STRETCH);

  V_DrawNumPatch(DM_KILLERSX, DM_KILLERSY, FB, killers_lump, CR_DEFAULT, VPT_STRETCH);
  V_DrawNumPatch(DM_VICTIMSX, DM_VICTIMSY, FB, victims_lump, CR_DEFAULT, VPT_STRETCH);

  // draw P?
  x = DM_MATRIXX + DM_SPACINGX;
  y = DM_MATRIXY;

  for (i = 0 ; i < MAXPLAYERS ; i++)
  {
    if (_g->playeringame[i])
    {
      //int trans = playernumtotrans[i];
      V_DrawNumPatch(x - halfface, DM_MATRIXY - WI_SPACINGY,
         FB, facebackp_lumps[i], i ? CR_LIMIT+i : CR_DEFAULT,
         VPT_STRETCH | (i ? VPT_TRANS : 0));
      V_DrawNumPatch(DM_MATRIXX-halfface, y,
         FB, facebackp_lumps[i], i ? CR_LIMIT+i : CR_DEFAULT,
         VPT_STRETCH | (i ? VPT_TRANS : 0));

      if (i == _g->me)
      {
        V_DrawNumPatch(x - halfface, DM_MATRIXY - WI_SPACINGY,
           FB, bstar_lump, CR_DEFAULT, VPT_STRETCH);
        V_DrawNumPatch(DM_MATRIXX-halfface, y,
           FB, star_lump, CR_DEFAULT, VPT_STRETCH);
      }
    }
    x += DM_SPACINGX;
    y += WI_SPACINGY;
  }

  // draw stats
  y = DM_MATRIXY + 10;

  if (isOnExternalFlash(&_g->num[0]->width))
  {
      w = extMemFlashGetShortFromAddress(&_g->num[0]->height);
  }
  else
  {
    w = _g->num[0]->width;
  }


  for (i=0 ; i<MAXPLAYERS ; i++)
  {
    x = DM_MATRIXX + DM_SPACINGX;

    if (_g->playeringame[i])
    {
      for (j=0 ; j<MAXPLAYERS ; j++)
      {
        if (_g->playeringame[j])
          WI_drawNum(x+w, y, dm_frags[i][j], 2);

        x += DM_SPACINGX;
      }
      WI_drawNum(DM_TOTALSX+w, y, dm_totals[i], 2);
    }
    y += WI_SPACINGY;
  }
}
//
// Note: The term "Netgame" means a coop game
//

// e6y
// 'short' => 'int' for cnt_kills, cnt_items and cnt_secret
//
// Original sources use 'int' type for cnt_kills instead of 'short'
// I don't know who have made change of type, but this change
// leads to desynch  if 'kills' percentage is more than 32767.
// Actually PrBoom will be in an infinite cycle at calculation of
// percentage if the player will not press <Use> for acceleration, because
// the condition (cnt_kills[0] >= (plrs[me].skills * 100) / wbs->maxkills)
// will be always false in this case.
//
// If you will kill 800 monsters on MAP30 on Ultra-Violence skill and
// will not press <Use>, vanilla will count up to 80000%, but PrBoom
// will be in infinite cycle of counting:
// (0, 1, 2, ..., 32766, 32767, -32768, -32767, ..., -1, 0, 1, ...)
// Negative numbers will not be displayed.

static int16_t *cnt_kills;
static int16_t *cnt_items;
static int16_t *cnt_secret;
static int16_t *cnt_frags;
static char    dofrags;
static char    ng_state;

// ====================================================================
// CPhipps - WI_endNetgameStats
// Purpose: Clean up coop game stats
// Args:    none
// Returns: void
//
static void WI_endNetgameStats(void)
{
  free(cnt_frags); cnt_frags = NULL;
  free(cnt_secret); cnt_secret = NULL;
  free(cnt_items); cnt_items = NULL;
  free(cnt_kills); cnt_kills = NULL;
}

// ====================================================================
// WI_initNetgameStats
// Purpose: Prepare for coop game stats
// Args:    none
// Returns: void
//
void WI_initNetgameStats(void)
{
  int i;

  _g->state = StatCount;
  _g->acceleratestage = 0;
  ng_state = 1;

  cnt_pause = TICRATE;

  // CPhipps - allocate these dynamically, blank with calloc
  cnt_kills = calloc(MAXPLAYERS, sizeof(*cnt_kills));
  cnt_items = calloc(MAXPLAYERS, sizeof(*cnt_items));
  cnt_secret= calloc(MAXPLAYERS, sizeof(*cnt_secret));
  cnt_frags = calloc(MAXPLAYERS, sizeof(*cnt_frags));

  for (i=0 ; i<MAXPLAYERS ; i++)
    if (_g->playeringame[i])
      dofrags += WI_fragSum(i);

  dofrags = !!dofrags; // set to true or false - did we have frags?

  WI_initAnimatedBack();
}


// ====================================================================
// WI_updateNetgameStats
// Purpose: Calculate coop stats as we display them with noise and fury
// Args:    none
// Returns: void
// Comment: This stuff sure is complicated for what it does
//
void WI_updateNetgameStats(void)
{
  int   i;
  int   fsum;

  uint8_t stillticking;

  WI_updateAnimatedBack();

  if (_g->acceleratestage && ng_state != 10)
  {
    _g->acceleratestage = 0;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (!_g->playeringame[i])
        continue;

      cnt_kills[i] = (_g->plrs[i].skills * 100) / _g->wbs->maxkills;
      cnt_items[i] = (_g->plrs[i].sitems * 100) / _g->wbs->maxitems;

      // killough 2/22/98: Make secrets = 100% if maxsecret = 0:
      cnt_secret[i] = _g->wbs->maxsecret ?
                      (_g->plrs[i].ssecret * 100) / _g->wbs->maxsecret : 100;
      if (dofrags)
        cnt_frags[i] = WI_fragSum(i);  // we had frags
    }
    S_StartSound(0, sfx_barexp);  // bang
    ng_state = 10;
  }

  if (ng_state == 2)
  {
    if (!(_g->bcnt&3))
      S_StartSound(0, sfx_pistol);  // pop

    stillticking = false;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (!_g->playeringame[i])
        continue;

      cnt_kills[i] += 2;

      if (cnt_kills[i] >= (_g->plrs[i].skills * 100) / _g->wbs->maxkills)
        cnt_kills[i] = (_g->plrs[i].skills * 100) / _g->wbs->maxkills;
      else
        stillticking = true; // still got stuff to tally
    }

    if (!stillticking)
    {
      S_StartSound(0, sfx_barexp);
      ng_state++;
    }
  }
  else if (ng_state == 4)
  {
    if (!(_g->bcnt&3))
      S_StartSound(0, sfx_pistol);

    stillticking = false;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (!_g->playeringame[i])
        continue;

      cnt_items[i] += 2;
      if (cnt_items[i] >= (_g->plrs[i].sitems * 100) / _g->wbs->maxitems)
        cnt_items[i] = (_g->plrs[i].sitems * 100) / _g->wbs->maxitems;
      else
        stillticking = true;
    }

    if (!stillticking)
    {
      S_StartSound(0, sfx_barexp);
      ng_state++;
    }
  }
  else if (ng_state == 6)
  {
    if (!(_g->bcnt&3))
      S_StartSound(0, sfx_pistol);

    stillticking = false;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (!_g->playeringame[i])
        continue;

      cnt_secret[i] += 2;

      if (cnt_secret[i] >= (_g->plrs[i].ssecret * 100) / _g->wbs->maxsecret)
        cnt_secret[i] = (_g->plrs[i].ssecret * 100) / _g->wbs->maxsecret;
      else
        stillticking = true;
    }

    if (!stillticking)
    {
      S_StartSound(0, sfx_barexp);
      ng_state += 1 + 2*!dofrags;
    }
  }
  else if (ng_state == 8)
  {
    if (!(_g->bcnt&3))
      S_StartSound(0, sfx_pistol);

    stillticking = false;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
      if (!_g->playeringame[i])
        continue;

      cnt_frags[i] += 1;

      if (cnt_frags[i] >= (fsum = WI_fragSum(i)))
        cnt_frags[i] = fsum;
      else
        stillticking = true;
    }

    if (!stillticking)
    {
      S_StartSound(0, sfx_pldeth);
      ng_state++;
    }
  }
  else if (ng_state == 10)
  {
    if (_g->acceleratestage)
    {
      S_StartSound(0, sfx_sgcock);
      if ( _g->gamemode == commercial )
        WI_initNoState();
      else
        WI_initShowNextLoc();
    }
  }
  else if (ng_state & 1)
  {
    if (!--cnt_pause)
    {
      ng_state++;
      cnt_pause = TICRATE;
    }
  }
}


// ====================================================================
// WI_drawNetgameStats
// Purpose: Put the coop stats on the screen
// Args:    none
// Returns: void
//
// proff/nicolas 09/20/98 -- changed for hi-res
// CPhipps - patch drawing updated
void WI_drawNetgameStats(void)
{
  int   i;
  int   x;
  int   y;

  int   pwidth = R_NumPatchWidth (percent_lump);
  int   fwidth = R_NumPatchHeight(facebackp_lumps[0]);

  WI_slamBackground();

  // draw animated background
  WI_drawAnimatedBack();

  WI_drawLF();

  // draw stat titles (top line)
  V_DrawNumPatch(NG_STATSX + NG_SPACINGX - R_NumPatchWidth(kills_lump),
     NG_STATSY, FB, kills_lump, CR_DEFAULT, VPT_STRETCH);

  V_DrawNumPatch(NG_STATSX + 2 * NG_SPACINGX - R_NumPatchWidth(items_lump),
     NG_STATSY, FB, items_lump, CR_DEFAULT, VPT_STRETCH);

  V_DrawNumPatch(NG_STATSX + 3 * NG_SPACINGX - R_NumPatchWidth(secret_lump),
     NG_STATSY, FB, secret_lump, CR_DEFAULT, VPT_STRETCH);

  if (dofrags)
    V_DrawNumPatch(NG_STATSX  + 4 * NG_SPACINGX - R_NumPatchWidth(frags_lump),
       NG_STATSY, FB, frags_lump, CR_DEFAULT, VPT_STRETCH);

  // draw stats
  y = NG_STATSY + R_NumPatchHeight(kills_lump);

  for (i=0 ; i<MAXPLAYERS ; i++)
  {
    //int trans = playernumtotrans[i];
    if (!_g->playeringame[i])
      continue;

    x = NG_STATSX;
    V_DrawNumPatch(x - fwidth, y, FB, facebackp_lumps[i],
       i ? CR_LIMIT+i : CR_DEFAULT,
       VPT_STRETCH | (i ? VPT_TRANS : 0));

    if (i == _g->me)
      V_DrawNumPatch(x-fwidth, y, FB, star_lump, CR_DEFAULT, VPT_STRETCH);

    x += NG_SPACINGX;
    if (cnt_kills)
      WI_drawPercent(x - pwidth, y + 10, cnt_kills[i]);
    x += NG_SPACINGX;
    if (cnt_items)
      WI_drawPercent(x - pwidth, y + 10, cnt_items[i]);
    x += NG_SPACINGX;
    if (cnt_secret)
      WI_drawPercent(x-pwidth, y + 10, cnt_secret[i]);
    x += NG_SPACINGX;

    if (dofrags && cnt_frags)
      WI_drawNum(x, y+10, cnt_frags[i], -1);

    y += WI_SPACINGY;
  }

  if (y <= SP_TIMEY)
    // cph - show times in coop on the entering screen
    WI_drawTimeStats(_g->plrs[_g->me].stime / TICRATE, _g->wbs->totaltimes / TICRATE, _g->wbs->partime / TICRATE);
}
// WI_initStats
// Purpose: Get ready for single player stats
// Args:    none
// Returns: void
// Comment: Seems like we could do all these stats in a more generic
//          set of routines that weren't duplicated for dm, coop, sp
//

void WI_initStats(void)
{
    _g->state = StatCount;
    _g->acceleratestage = 0;
    _g->sp_state = 1;

    // CPhipps - allocate (awful code, I know, but saves changing it all) and initialise
    *(cnt_kills = malloc(sizeof(*cnt_kills))) = -1;
    *(cnt_items = malloc(sizeof(*cnt_items))) = -1;
    *(cnt_secret= malloc(sizeof(*cnt_secret))) = -1;

    _g->cnt_time = _g->cnt_par = _g->cnt_total_time = -1;
    _g->cnt_pause = TICRATE;
    WI_initAnimatedBack();

}

// ====================================================================
// WI_updateStats
// Purpose: Calculate solo stats
// Args:    none
// Returns: void
//
void WI_updateStats(void)
{
  WI_updateAnimatedBack();

    if (_g->acceleratestage && _g->sp_state != 10)
    {
        _g->acceleratestage = 0;
        cnt_kills[0] = (_g->plrs[_g->me].skills * 100) / _g->wbs->maxkills;
        cnt_items[0] = (_g->plrs[_g->me].sitems * 100) / _g->wbs->maxitems;

        // killough 2/22/98: Make secrets = 100% if maxsecret = 0:
        cnt_secret[0] = (
            _g->wbs->maxsecret ? (_g->plrs[_g->me].ssecret * 100) / _g->wbs->maxsecret : 100);

        _g->cnt_total_time = _g->wbs->totaltimes / TICRATE;
        _g->cnt_time = _g->plrs[_g->me].stime / TICRATE;
        _g->cnt_par = _g->wbs->partime / TICRATE;
        S_StartSound(0, sfx_barexp);
        _g->sp_state = 10;
    }

    if (_g->sp_state == 2)
    {
        cnt_kills[0] += 2;

        if (!(_g->bcnt & 3))
            S_StartSound(0, sfx_pistol);

        if (cnt_kills[0] >= (_g->plrs[_g->me].skills * 100) / _g->wbs->maxkills)
        {
            cnt_kills[0] = (_g->plrs[_g->me].skills * 100) / _g->wbs->maxkills;
            S_StartSound(0, sfx_barexp);
            _g->sp_state++;
        }
    }
    else if (_g->sp_state == 4)
    {
        cnt_items[0] += 2;

        if (!(_g->bcnt & 3))
            S_StartSound(0, sfx_pistol);

        if (cnt_items[0] >= (_g->plrs[_g->me].sitems * 100) / _g->wbs->maxitems)
        {
            cnt_items[0] = (_g->plrs[_g->me].sitems * 100) / _g->wbs->maxitems;
            S_StartSound(0, sfx_barexp);
            _g->sp_state++;
        }
    }
    else if (_g->sp_state == 6)
    {
        cnt_secret[0] += 2;

        if (!(_g->bcnt & 3))
            S_StartSound(0, sfx_pistol);

        // killough 2/22/98: Make secrets = 100% if maxsecret = 0:
        if (cnt_secret[0] >= (
            _g->wbs->maxsecret ? (_g->plrs[_g->me].ssecret * 100) / _g->wbs->maxsecret : 100))
        {
            cnt_secret[0] = (
                _g->wbs->maxsecret ? (_g->plrs[_g->me].ssecret * 100) / _g->wbs->maxsecret : 100);
            S_StartSound(0, sfx_barexp);
            _g->sp_state++;
        }
    }
    else if (_g->sp_state == 8)
    {
        if (!(_g->bcnt & 3))
            S_StartSound(0, sfx_pistol);

        _g->cnt_time += 3;

        if (_g->cnt_time >= _g->plrs[_g->me].stime / TICRATE)
            _g->cnt_time = _g->plrs[_g->me].stime / TICRATE;

        _g->cnt_total_time += 3;

        if (_g->cnt_total_time >= _g->wbs->totaltimes / TICRATE)
            _g->cnt_total_time = _g->wbs->totaltimes / TICRATE;

        _g->cnt_par += 3;

        if (_g->cnt_par >= _g->wbs->partime / TICRATE)
        {
            _g->cnt_par = _g->wbs->partime / TICRATE;

            if ((_g->cnt_time >= _g->plrs[_g->me].stime / TICRATE) && (_g->cnt_total_time >= _g->wbs->totaltimes / TICRATE))
            {
                S_StartSound(0, sfx_barexp);
                _g->sp_state++;
            }
        }
    }
    else if (_g->sp_state == 10)
    {
        if (_g->acceleratestage)
        {
            S_StartSound(0, sfx_sgcock);

            if (_g->gamemode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if (_g->sp_state & 1)
    {
        if (!--_g->cnt_pause)
        {
            _g->sp_state++;
            _g->cnt_pause = TICRATE;
        }
    }
}

// ====================================================================
// WI_drawStats
// Purpose: Put the solo stats on the screen
// Args:    none
// Returns: void
//
// proff/nicolas 09/20/98 -- changed for hi-res
// CPhipps - patch drawing updated
void WI_drawStats(void)
{
    // line height
    int lh;
    if (isOnExternalFlash(&_g->num[0]->height))
    {
        lh = (3 * extMemFlashGetShortFromAddress(&_g->num[0]->height)) / 2;
    }
    else
    {
        lh = (3 * _g->num[0]->height) / 2;
    }

    WI_slamBackground();

  // draw animated background
    WI_drawAnimatedBack();
    WI_drawLF();
    V_DrawNumPatch(SP_STATSX, SP_STATSY, FB, kills_lump, CR_DEFAULT, VPT_STRETCH);
    if (cnt_kills)
        WI_drawPercent(320 - SP_STATSX, SP_STATSY, cnt_kills[0]);

    V_DrawNumPatch(SP_STATSX, SP_STATSY + lh, FB, items_lump, CR_DEFAULT, VPT_STRETCH);

    if (cnt_items)
        WI_drawPercent(320 - SP_STATSX, SP_STATSY + lh, cnt_items[0]);

    V_DrawNumPatch(SP_STATSX, SP_STATSY + 2 * lh, FB, sp_secret_lump, CR_DEFAULT, VPT_STRETCH);

    if (cnt_secret)
        WI_drawPercent(320 - SP_STATSX, SP_STATSY + 2 * lh, cnt_secret[0]);

    WI_drawTimeStats(_g->cnt_time, _g->cnt_total_time, _g->cnt_par);
}

// ====================================================================
// WI_checkForAccelerate
// Purpose: See if the player has hit either the attack or use key
//          or mouse button.  If so we set acceleratestage to 1 and
//          all those display routines above jump right to the end.
// Args:    none
// Returns: void
//
void WI_checkForAccelerate(void)
{
  int   i;
  player_t  *player;
  // check for button presses to skip delays
  for (i = 0, player = _g->players ; i < MAXPLAYERS ; i++, player++)
  {
    if (_g->playeringame[i])
    {
      if (player->cmd.buttons & BT_ATTACK)
      {
        if (!player->attackdown)
          _g->acceleratestage = 1;
        player->attackdown = true;
      }
      else
        player->attackdown = false;

      if (player->cmd.buttons & BT_USE)
      {
        if (!player->usedown)
          _g->acceleratestage = 1;
        player->usedown = true;
      }
      else
        player->usedown = false;
    }
  }
}

// ====================================================================
// WI_Ticker
// Purpose: Do various updates every gametic, for stats, animation,
//          checking that intermission music is running, etc.
// Args:    none
// Returns: void
//
void WI_Ticker(void)
{
    // counter for general background animation
    _g->bcnt++;

    if (_g->bcnt == 1)
    {
        // intermission music
        if (_g->gamemode == commercial)
            S_ChangeMusic(mus_dm2int, true);
        else
            S_ChangeMusic(mus_inter, true);
    }

    WI_checkForAccelerate();

    switch (_g->state)
    {
 case StatCount:
         if (_g->deathmatch)
         {
           WI_updateDeathmatchStats();
         }
         else if (_g->netgame)
         {
           WI_updateNetgameStats();
         }
         else
         {
           WI_updateStats();
         }
         break;

        case ShowNextLoc:
            WI_updateShowNextLoc();
            break;

        case NoState:
            WI_updateNoState();
            break;
    }
}

/* ====================================================================
 * WI_loadData
 * Purpose: Initialize intermission data such as background graphics,
 *          patches, map names, etc.
 * Args:    none
 * Returns: void
 *
 * CPhipps - modified for new wad lump handling.
 *         - no longer preload most graphics, other funcs can use
 *           them by name
 */

static void WI_loadData(void)
{
    int i, j;
    wi_anim_t *a;
    const const_wi_anim_t *ca;
    char name[9];  // limited to 8 characters

    if (_g->gamemode != commercial)
    {
      if (_g->wbs->epsd < 3)
      {
        for (j = 0; j < NUMANIMS[_g->wbs->epsd];j++)
        {
          a = &ram_anims[j];
          ca = &anims[_g->wbs->epsd][j];
          for (i = 0; i < ca->nanims; i++)
          {
            // MONDO HACK!
            if (_g->wbs->epsd != 1 || j != 8)
            {
              // animations
              snprintf(name, sizeof(name), "WIA%d%.2d%.2d", _g->wbs->epsd, j, i);
              a->p_num[i] = W_GetNumForName(name);
            }
            else
            {
              // HACK ALERT!
              snprintf(name, sizeof(name), "WIA%d%.2d%.2d", 1, 4, i);
              a->p_num[i] =  W_GetNumForName(name);
            }
          }
        }
      }
    }
}

// ====================================================================
// WI_Drawer
// Purpose: Call the appropriate stats drawing routine depending on
//          what kind of game is being played (DM, coop, solo)
// Args:    none
// Returns: void
//
// next-hack: added back netgame stats.
void WI_Drawer(void)
{

    switch (_g->state)
    {
        case StatCount:
           if (_g->deathmatch)
             WI_drawDeathmatchStats();
           else if (_g->netgame)
             WI_drawNetgameStats();
           else
             WI_drawStats();
           break;

        case ShowNextLoc:
            WI_drawShowNextLoc();
            break;

        case NoState:
            WI_drawNoState();
            break;
    }
}

// ====================================================================
// WI_initVariables
// Purpose: Initialize the intermission information structure
//          Note: wbstartstruct_t is defined in d_player.h
// Args:    wbstartstruct -- pointer to the structure with the data
// Returns: void
//
void WI_initVariables(wbstartstruct_t *wbstartstruct)
{

    _g->wbs = wbstartstruct;

    _g->acceleratestage = 0;
    _g->cnt = _g->bcnt = 0;
    _g->me = _g->wbs->pnum;
    _g->plrs = _g->wbs->plyr;

    if (!_g->wbs->maxkills)
        _g->wbs->maxkills = 1;  // probably only useful in MAP30

    if (!_g->wbs->maxitems)
        _g->wbs->maxitems = 1;

    if (_g->gamemode != retail)
        if (_g->wbs->epsd > 2)
            _g->wbs->epsd -= 3;
}

// ====================================================================
// WI_Start
// Purpose: Call the various init routines
//          Note: wbstartstruct_t is defined in d_player.h
// Args:    wbstartstruct -- pointer to the structure with the
//          intermission data
// Returns: void
//
void WI_Start(wbstartstruct_t *wbstartstruct)
{

    WI_initVariables(wbstartstruct);
    // next-hack: this is called in WI_Init()
    WI_loadData();
    if (_g->deathmatch)
      WI_initDeathmatchStats();
    else if (_g->netgame)
      WI_initNetgameStats();
    else
      WI_initStats();
}
