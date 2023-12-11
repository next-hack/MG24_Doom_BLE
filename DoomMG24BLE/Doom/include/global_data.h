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
 * global data collected by doomhack.
 * Modified by next-hack, still a lot of optimization is required.
 */
#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include "doomstat.h"
#include "doomdef.h"
#include "m_fixed.h"
#include "am_map.h"
#include "g_game.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "r_defs.h"
#include "i_sound.h"
#include "m_menu.h"
#include "p_spec.h"
#include "p_enemy.h"
#include "p_map.h"
#include "p_maputl.h"

#include "p_mobj.h"
#include "p_tick.h"

#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_things.h"

#include "s_sound.h"

#include "st_lib.h"
#include "st_stuff.h"

#include "v_video.h"

#include "w_wad.h"

#include "wi_stuff.h"
#include <stdint.h>

extern short draw_starty, draw_stopy;
extern const patch_t **hu_font;
#if STATIC_INTERCEPTS
extern intercept_t intercepts[MAXINTERCEPTS];
extern boolean interceptIsALine[MAXINTERCEPTS];
#else
extern intercept_t *intercepts;
extern boolean *interceptIsALine;
#endif

extern intercept_t *intercept_p;
// 2021/04/01 next-hack: still a lot to optimize here.
typedef struct globals_t
{
#ifndef GLOBAL_DEFS_H
#define GLOBAL_DEFS_H
    int16_t firstPatchLumpNum;
    int16_t lastPatchLumpNum;
    //
    uint32_t *lumpSizes; // pointer to lump sizes
    // static objects can read the position from flash!
    full_static_mobj_xy_and_type_t *full_static_mobj_xy_and_type_values;
//******************************************************************************
//am_map.c
//******************************************************************************
    byte automapmode; // Mode that the automap is in

// constant to 0...
// location of window on screen
// int  f_x; = 0
// int  f_y; = 0

// size of window on screen
// int  f_w; = SCREENWIDTH_PHYSICAL
// int  f_h; =  SCREENHEIGHT-ST_SCALED_HEIGHT

    mpoint_t m_paninc;    // how far the window pans each tic (map coords)

    fixed_t m_x, m_y;     // LL x,y window location on the map (map coords)
    fixed_t m_x2, m_y2;   // UR x,y window location on the map (map coords)

//
// width/height of window on map (map coords)
//
    fixed_t m_w;
    fixed_t m_h;

// based on level size
    fixed_t min_x;
    fixed_t min_y;
    fixed_t max_x;
    fixed_t max_y;

//fixed_t  max_w;          // max_x-min_x,
//fixed_t  max_h;          // max_y-min_y

    fixed_t min_scale_mtof; // used to tell when to stop zooming out
    fixed_t max_scale_mtof; // used to tell when to stop zooming in

// old location used by the Follower routine
    mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
    fixed_t scale_mtof;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
    fixed_t scale_ftom;

//int lastlevel, lastepisode;
    uint8_t lastlevel, lastepisode;

    boolean stopped;

    fixed_t mtof_zoommul; // how far the window zooms each tic (map coords)
    fixed_t ftom_zoommul; // how far the window zooms each tic (fb coords)

//******************************************************************************
//d_client.c
//******************************************************************************


    // gametic is the tic about to (or currently being) run
    // maketic is the tic that hasn't had control made for it yet
    // recvtic is the latest tic received from the server.
    //
    // a gametic cannot be run until ticcmds are received for it
    // from all players.
    //
    ticcmd_t netcmds[MAXPLAYERS][BACKUPTICS];
    // The index of the next tic to be made (with a call to BuildTiccmd).
    int maketic;
    int lastmadetic;
    int remotetic;  //tic expected from the remote

    // The number of tics that have been run (using RunTic) so far.

    int gametic;

//******************************************************************************
//d_main.c
//******************************************************************************

    short pagetic;
    short pagelump; // CPhipps - const

    unsigned int fps_timebefore;
    unsigned int fps_frames;
    unsigned short fps_framerate;

// wipegamestate can be set to -1 to force a wipe on the next draw
    signed char wipegamestate;
    signed char oldgamestate;

    byte demosequence;         // killough 5/2/98: made static

    boolean singletics; // debug flag to cancel adaptiveness
    boolean advancedemo;
    boolean server;

    byte gamma;

    boolean fps_show;
    boolean gameStarted;

//******************************************************************************
//doomstat.c
//******************************************************************************

// Game Mode - identify IWAD as shareware, retail etc.
    byte gamemode;
    byte gamemission;

//******************************************************************************
//f_finale.c
//******************************************************************************

    int castnum;
    int casttics;
    const state_t *caststate;
    int castframes;
    int castonmelee;

    const char *finaletext; // cph -
    const char *finaleflat; // made static const
    short finalecount; // made static

// Stage of animation:
//  0 = text, 1 = art screen, 2 = character cast
    byte finalestage; // cph -

    byte midstage;                 // whether we're in "mid-stage"
    byte laststage;

    boolean castattacking;
    boolean castdeath;

//******************************************************************************
//f_wipe.c
//******************************************************************************

    int wipe_tick;

//******************************************************************************
//g_game.c
//******************************************************************************

    const byte *demobuffer; /* cph - only used for playback */
    int demolength; // check for overrun (missing DEMOMARKER)

    const byte *demo_p;

    wbstartstruct_t wminfo;               // parms for world map / intermission

    player_t players[MAXPLAYERS];

    int starttime;     // for comparative timing purposes
    int totalleveltimes;      // CPhipps - total time for all completed levels
    int longtics;
    int basetic; /* killough 9/29/98: for demo sync */
    short totalkills, totallive, totalitems, totalsecret;    // for intermission
    short totalstatic;

    byte gameaction;
    byte gamestate;
    byte gameskill;

    uint8_t gameepisode;
    uint8_t gamemap;

    uint8_t demover;
    boolean         deathmatch;    // only if started as net death
    boolean       nomonsters;
    boolean     coop_spawns;
    boolean items_respawn;
    boolean         netgame;       // only true if packets are broadcast


// CPhipps - made lots of key/button state vars static
    //boolean gamekeydown[NUMKEYS];
    unsigned short gamekeydown;
    byte turnheld;       // for accelerative turning

// Game events info
    byte prevgamestate;

    byte d_skill;
    uint8_t d_episode;
    uint8_t d_map;

    byte savegameslot;         // Slot to load if gameaction == ga_loadgame

    boolean secretexit;

    boolean respawnmonsters;
    boolean paused;
// CPhipps - moved *_loadgame vars here

    boolean usergame;      // ok to save / end game
    boolean timingdemo;    // if true, exit with report on completion
    boolean playeringame[MAXPLAYERS];
    boolean demoplayback;
    boolean singledemo;           // quit after playing a demo from cmdline
    boolean haswolflevels;           // jff 4/18/98 wolf levels present
    uint8_t consoleplayer; // player taking events and displaying
    uint8_t displayplayer; // view being displayed
//******************************************************************************
//hu_stuff.c
//******************************************************************************

// widgets
    hu_textline_t w_title;
    hu_stext_t w_message;
    uint8_t message_counter;

    boolean message_on;
    boolean message_dontfuckwithme;
    boolean headsupactive;

//******************************************************************************
//i_audio.c
//******************************************************************************

//int lasttimereply;
    unsigned int basetime;

//******************************************************************************
//i_video.c
//******************************************************************************

    unsigned short *current_palette;
//const unsigned char* palette_lump;
    uint8_t newpal;

//******************************************************************************
//m_cheat.c
//******************************************************************************

    unsigned int cheat_buffer;

//******************************************************************************
//m_menu.c
//******************************************************************************

//
// defaulted values
//
    boolean showMessages;    // Show messages has default, 0 = off, 1 = on

    boolean alwaysRun;

    boolean messageToPrint;  // 1 = message to be printed

    uint8_t messageLastMenuActive;

    uint8_t saveSlot;        // which slot to save in
// old save description before edit

    uint8_t epi;

    uint8_t itemOn;           // menu item skull is on (for Big Font menus)
    uint8_t skullAnimCounter; // skull animation counter
    uint8_t whichSkull;       // which skull to draw (he blinks)

    boolean menuactive;    // The menus are up
    boolean messageNeedsInput; // timed message = no input from user
    boolean waitingForClients;    // if we are host and waiting for clients.

//******************************************************************************
//m_random.c
//******************************************************************************

    uint8_t rndindex;
    uint8_t prndindex;

//******************************************************************************
//p_ceiling.c
//******************************************************************************

// the list of ceilings moving currently, including crushers
    ceilinglist_t *activeceilings;

//******************************************************************************
//p_enemy.c
//******************************************************************************

    fixed_t dropoff_deltax, dropoff_deltay, floorz;

    mobj_t *corpsehit;
    fixed_t viletryx;
    fixed_t viletryy;

// killough 2/7/98: Remove limit on icon landings:
    mobj_t **braintargets;
    short numbraintargets_alloc;
    short numbraintargets;

    brain_t brain;   // killough 3/26/98: global state of boss brain

//******************************************************************************
//p_map.c
//******************************************************************************

    /* killough 8/2/98: for more intelligent autoaiming */
    uint_64_t aim_flags_mask;

    mobj_t *tmthing;
    fixed_t tmx;
    fixed_t tmy;

    int pe_x; // Pain Elemental position for Lost Soul checks // phares
    int pe_y; // Pain Elemental position for Lost Soul checks // phares
    int ls_x; // Lost Soul position for Lost Soul checks      // phares
    int ls_y; // Lost Soul position for Lost Soul checks      // phares

// The tm* items are used to hold information globally, usually for
// line or object intersection checking

    fixed_t tmbbox[4];  // bounding box for line intersection checks

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls

    const line_t *ceilingline;
    const line_t *blockline; /* killough 8/11/98: blocking linedef */
    const line_t *floorline; /* killough 8/1/98: Highest touched floor */
    int tmunstuck; /* killough 8/1/98: whether to allow unsticking */

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid

// 1/11/98 killough: removed limit on special lines crossed
    const line_t *spechit[4];                // new code -- killough

// Temporary holder for thing_sectorlist threads
#if USE_MSECNODE
    msecnode_t *sector_list;                             // phares 3/16/98
#endif
    /* killough 8/2/98: make variables static */
    fixed_t bestslidefrac;
    const line_t *bestslideline;
    mobj_t *slidemo;
    fixed_t tmxmove;
    fixed_t tmymove;

    mobj_t *linetarget; // who got hit (or NULL)
    mobj_t *shootthing;

// Height if not aiming up or down
    fixed_t shootz;

    int la_damage;
    fixed_t attackrange;

    fixed_t aimslope;

// slopes to top and bottom of target
// killough 4/20/98: make static instead of using ones in p_sight.c

    fixed_t topslope;
    fixed_t bottomslope;

    mobj_t *bombsource, *bombspot;
    int bombdamage;

    mobj_t *usething;

    short numspechit;

    fixed16_t tmfloorz16;   // floor you'd hit if free to fall
    fixed16_t tmceilingz16; // ceiling of sector you're in
    fixed16_t tmdropoffz16; // dropoff on other side of line you're crossing


// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
    boolean floatok;

    /* killough 11/98: if "felldown" true, object was pushed down ledge */
    boolean felldown;

    boolean crushchange, nofit;

    boolean telefrag; /* killough 8/9/98: whether to telefrag at exit */

//******************************************************************************
//p_maputl.c
//******************************************************************************

    fixed_t opentop;
    fixed_t openbottom;
    fixed_t openrange;
    fixed_t lowfloor;

// moved front and back outside P-LineOpening and changed    // phares 3/7/98
// them to these so we can pick up the new friction value
// in PIT_CheckLine()
    sector_t *openfrontsector; // made global                    // phares
    sector_t *openbacksector;  // made global

    divline_t trace;

//******************************************************************************
//p_mobj.c
//******************************************************************************

//******************************************************************************
//p_plats.c
//******************************************************************************

//******************************************************************************
//p_pspr.c
//******************************************************************************

    fixed_t bulletslope;

//******************************************************************************
//p_setup.c
//******************************************************************************

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
    const vertex_t *vertexes;
    const seg_t *segs;
    // next-hack split ram sectors and sectors.
    sector_t *sectors;
    ramSector_t *ramsectors;
    subsector_t *subsectors;
    short numvertexes;

    short numsegs;

    short numsectors;

    short numsubsectors;

    short numsides;
    short numlines;

    const line_t *lines;
#if OLD_VALIDCOUNT

    linedata_t *linedata;
#else

    uint32_t *lineSectorChecked;       // next-hack 2022/03/16: bitfield instead of validcount.
    uint32_t *lineIsSpecial;       
    uint32_t *lineIsMapped;
    uint32_t *lineStairDirection;
#endif
    side_t *sides;
    byte *textureoffsets; // 2021-02-13 next-hack: added this to avoid storing the whole side structure to ram.
// 2021-08-07 next-hack: to support full DOOM we need uint16_t and not only bytes. We still have ram
// 2022-02-05 next-hack: actually there are typically few special lines ( TODO : CONFIRM!), therefore, while it is true that switch textures shall be uint16_t, their index can stay to 8 bit value! 
    uint8_t *linesChangeableTextureIndex; // 2021-03-13: forgot that switches have changeable textures...
    uint16_t *switch_texture_top;
    uint16_t *switch_texture_mid;
    uint16_t *switch_texture_bot;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.

    short bmapwidth, bmapheight;  // size in mapblocks

// killough 3/1/98: remove blockmap limit internally:
    const short *blockmap;              // was short -- killough

// offsets in blockmap are from here
    const short *blockmaplump;          // was short -- killough

    fixed_t bmaporgx, bmaporgy;     // origin of block map

//mobj_t    **blocklinks;           // for thing chains
    unsigned short *blocklinks_sptrs;
//
// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without the special effect, this could
// be used as a PVS lookup as well.
//

//int rejectlump;// cph - store reject lump num if cached
    const byte *rejectmatrix; // cph - const*

// Maintain single and multi player starting spots.
    mapthing_t playerstarts[MAXPLAYERS];

//******************************************************************************
//p_sight.c
//******************************************************************************

//******************************************************************************
//p_switch.c
//******************************************************************************
    button_t buttonlist[MAXBUTTONS];

    short numswitches;                           // killough

//******************************************************************************
//p_tick.c
//******************************************************************************

    int leveltime; // tics in game play for par

// killough 8/29/98: we maintain several separate threads, each containing
// a special class of thinkers, to allow more efficient searches.
 //   thinker_t thinkerclasscap[th_all + 1];

//******************************************************************************
//r_bsp.c
//******************************************************************************

//******************************************************************************
//r_data.c
//******************************************************************************
// TODO: we might want to optimize this...
    char tex_lookup_last_name[8];    // added full string

    short firstflat, numflats;
    short firstspritelump, lastspritelump, numspritelumps;
    short numtextures;

//Store last lookup and return that if they match.
    short tex_lookup_last_num;
    boolean tex_lookup_valid;

//******************************************************************************
//p_user.c
//******************************************************************************

    boolean onground; // whether player is on ground or in air

//******************************************************************************
//r_draw.c
//******************************************************************************

    uint8_t fuzzpos;

//******************************************************************************
//r_draw.c
//******************************************************************************
// next-hack: all validcounts can be unsigned short
#if OLD_VALIDCOUNT
    unsigned short validcount;         // increment every time a check is made
#endif
    player_t *viewplayer;

// killough 3/20/98, 4/4/98: end dynamic colormaps

//******************************************************************************
//r_patch.c
//******************************************************************************

//******************************************************************************
//r_plane.c
//******************************************************************************

    //visplane_t *visplanes[MAXVISPLANES];   // killough
    //visplane_t *freetail;                  // killough
    //visplane_t **freehead;     // killough
    // next-hack: using short pointers, will allow using short pointers on visplane_t as well.
    unsigned short visplanes_sptr[MAXVISPLANES];   
    unsigned short freetail_sptr;                
    unsigned short *freehead_psptr;            // pointer to short pointer

// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1

// killough 2/8/98: make variables static

//******************************************************************************
//r_segs.c
//******************************************************************************

//
// regular wall
//
    drawseg_t drawsegs[MAXDRAWSEGS]; 
    short *lastopening;

//******************************************************************************
//r_sky.c
//******************************************************************************

//
// sky mapping
//
    short skyflatnum;
    short skytexture;

//******************************************************************************
//r_things.c
//******************************************************************************

// variables used to look up and range check thing_t sprites patches

//spritedef_t *sprites;
//int numsprites;

// why do we need this one?
//spriteframe_t sprtemp[MAX_SPRITE_FRAMES];
    short maxframe;		// nh
    vissprite_t vissprites[MAXVISSPRITES]; 
    uint8_t vissprite_indexes[MAXVISSPRITES * 2]; // 2021-02-13 next-hack bytes are smaller...

//******************************************************************************
//st_stuff.c
//******************************************************************************

  // main player in game
  player_t *plyr;

// health widget
    st_percent_t st_health;

// ready-weapon widget
    st_number_t w_ready;


// weapon ownership widgets
    st_multicon_t w_arms[6];

// face status widget
    st_multicon_t w_faces;

// keycard widgets
    st_multicon_t w_keyboxes[3];

// ammo widgets
    st_number_t w_ammo[4];

// max ammo widgets
    st_number_t w_maxammo[4];

// armor widget
    st_percent_t st_armor;

// used to use appopriately pained face
    short st_oldhealth;

// used for evil grin
    boolean oldweaponsowned[NUMWEAPONS];

    // count until face changes
    short st_facecount;

// current face index, used by w_faces
    short st_faceindex;

// holds key-type for each key box on bar
    short keyboxes[3];

// a random number per tick
    uint8_t st_randomnumber;

    uint8_t st_palette;

    uint8_t st_needrefresh;

//******************************************************************************
//v_video.c
//******************************************************************************

    screeninfo_t screens[NUM_SCREENS];

//******************************************************************************
//wi_stuff.c
//******************************************************************************

    // specifies current state
    stateenum_t state;

// contains information passed into intermission
    wbstartstruct_t *wbs;

    wbplayerstruct_t *plrs;  // wbs->plyr[]

    int cnt_time;
    int cnt_total_time;
    int cnt_par;
    int cnt_pause;

// 0-9 graphic
// TODO OPTIMIZE
    const patch_t *num[10];

// used for general timing
    uint8_t cnt;

// used for timing of background animation
    uint8_t bcnt;
//
    uint8_t sp_state;
// wbs->pnum
        uint8_t me;

//******************************************************************************
//w_wad.c
//******************************************************************************

    short numlumps;         // killough

    boolean snl_pointeron;
// used to accelerate or skip a stage
    boolean acceleratestage;           // killough 3/28/98: made global
// whether left-side main status bar is active
    boolean st_statusbaron;

//******************************************************************************
//s_sound.c
//******************************************************************************

// the set of channels available
    channel_t *channels;

// These are not used, but should be (menu).
// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
    unsigned char snd_SfxVolume;  // int

// Maximum volume of music. Useless so far.
    unsigned char snd_MusicVolume; // int

// music currently being played
    unsigned char mus_playing;

//jff 3/17/98 to keep track of last IDMUS specified music num
    signed char idmusnum;
    uint8_t allocatedVisplanes;
// whether songs are mus_paused
    boolean mus_paused;
    boolean darkerPage;

//******************************************************************************
#endif // GLOBAL_DEFS_H

} globals_t;


void InitGlobals();

extern globals_t *_g;

#endif // GLOBAL_DATA_H
