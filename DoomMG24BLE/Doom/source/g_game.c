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
 *  02111-1307, USA.59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:  none
 *  The original Doom description was none, basically because this file
 *  has everything. This ties up the game logic, linking the menu and
 *  input code to the underlying game by creating & respawning players,
 *  building game tics, calling the underlying thing logic.
 *  next-hack: added support for reading from external spi.
 *             modified save/load game functions, to use the displaybuffer
 *             as temporary buffer :) NOTE: display buffer is used also as
 *             stack in almost all the function calls here, but it is huge
 *             and it is used starting from its last element.
 *
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomstat.h"
#include "d_net.h"
#include "f_finale.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "p_setup.h"
#include "p_tick.h"
#include "p_map.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_draw.h"
#include "p_map.h"
#include "s_sound.h"
#include "dstrings.h"
#include "sounds.h"
#include "r_data.h"
#include "r_sky.h"
#include "p_inter.h"
#include "g_game.h"
#include "lprintf.h"
#include "i_main.h"
#include "i_system.h"
#include "i_video.h"
#include "global_data.h"
#include "z_bmalloc.h"

#include "extMemory.h"
#include "main.h"
#include "graphics.h"
//
#include "i_memory.h"
#if HAS_NETWORK
  #include "doom_ble.h"
#endif


#define BODYQUESIZE 8     // was 32, but 32 mobjs take a lot of bytes.

uint16_t  bodyque_sptr[BODYQUESIZE];
uint8_t   bodyqueslot = 0;

#if USE_MSECNODE
DECLARE_BLOCK_MEMORY_ALLOC_ZONE(secnodezone);
#endif
//
// controls (have defaults)
//

const int key_right = KEYD_RIGHT;
const int key_left = KEYD_LEFT;
const int key_up = KEYD_UP;
const int key_down = KEYD_DOWN;
const int key_menu_right = KEYD_RIGHT;                          // phares 3/7/98
const int key_menu_left = KEYD_LEFT;                                    //     |
const int key_menu_up = KEYD_UP;                                        //     V
const int key_menu_down = KEYD_DOWN;
const int key_menu_escape = KEYD_MENU;                                  //     |
const int key_menu_enter = KEYD_FIRE;                           // phares 3/7/98
const int key_strafeleft = KEYD_SL;
const int key_straferight = KEYD_SR;
//Match Doom II GBA retail controls ~ Kippykip
const int key_fire = KEYD_FIRE;
const int key_use = KEYD_USE;
const int key_weapon_down = KEYD_CHGWDOWN;
const int key_weapon_up = KEYD_CHGW;
const int key_speed = KEYD_SPEED;
const int key_escape = KEYD_MENU;                           // phares 4/13/98
const int key_enter = KEYD_FIRE;
const int key_map_right = KEYD_RIGHT;
const int key_map_left = KEYD_LEFT;
const int key_map_up = KEYD_UP;
const int key_map_down = KEYD_DOWN;
const int key_map = KEYD_MAP1;
const int key_map_follow = KEYD_USE;
const int key_map_zoomin = KEYD_SL;
const int key_map_zoomout = KEYD_SR;

#define SAVEGAME_ADDRESS ((extMemGetSize() - 2 * 4096) | EXT_FLASH_BASE)
#define SETTINGS_ADDRESS ((extMemGetSize() - 4 * 4096) | EXT_FLASH_BASE)

// phares

#define MAXPLMOVE   (forwardmove[1])
#define TURBOTHRESHOLD  0x32
#define SLOWTURNTICS  6
#define QUICKREVERSE (short)32768 // 180 degree reverse                    // phares

static const fixed_t forwardmove[2] =
{ 0x19, 0x32 };
static const fixed_t sidemove[2] =
{ 0x18, 0x28 };
static const fixed_t angleturn[3] =
{ 640, 1280, 320 };  // + slow turn

static void G_DoSaveGame(boolean menu);
static const byte* G_ReadDemoHeader(const byte *demo_p, size_t size, boolean failonerror);

typedef struct doom_save_data_t
{
    int save_present;
    skill_t gameskill;
    int gameepisode;
    int gamemap;
    int totalleveltimes;
    int alwaysRun;
    int gamma;

    int weaponowned[NUMWEAPONS];
    int ammo[NUMAMMO];
    int maxammo[NUMAMMO];
} doom_save_data_t;

typedef struct doom_save_settings_t
{
    unsigned int cookie;
    unsigned int alwaysRun;
    unsigned int gamma;
    unsigned int showMessages;
    unsigned int musicVolume;
    unsigned int soundVolume;
    unsigned char playerName[MAX_HOST_NAME_LENGTH];
} doom_save_settings_t;

const unsigned int settings_cookie = 0xbaddead2;

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
static inline signed char fudgef(signed char b)
{
    static int c;
    if (!b || _g->longtics)
        return b;
    if (++c & 0x1f)
        return b;
    b |= 1;
    if (b > 2)
        b -= 2;
    return b;
}

static inline signed short fudgea(signed short b)
{
    if (!b || !_g->longtics)
        return b;
    b |= 1;
    if (b > 2)
        b -= 2;
    return b;
}

void G_BuildTiccmd(ticcmd_t *cmd)
{
    int speed;
    int tspeed;
    int forward;
    int side;
    int newweapon;                                          // phares
    /* cphipps - remove needless I_BaseTiccmd call, just set the ticcmd to zero */
    memset(cmd, 0, sizeof *cmd);

    //Use button negates the always run setting.
    //speed = (_g->gamekeydown[key_use] ^ _g->alwaysRun);
    speed = ((_g->gamekeydown >> key_use) & 1) ^ _g->alwaysRun;

    
    forward = side = 0;

    // use two stage accelerative turning
    // on the keyboard and joystick
//    if (_g->gamekeydown[key_right] || _g->gamekeydown[key_left])
    if (((_g->gamekeydown >> key_right) & 1) || ( (_g->gamekeydown >> key_left) & 1 ))
    {
        _g->turnheld++;
        // next-hack: prevent overflow if one keeps the button pressed for long
        // this allows using a byte.
        if (_g->turnheld > SLOWTURNTICS)
        {
            _g->turnheld = SLOWTURNTICS;
        }
    }
    else
        _g->turnheld = 0;

    if (_g->turnheld < SLOWTURNTICS)
        tspeed = 2;             // slow turn
    else
        tspeed = speed;

    // let movement keys cancel each other out

    if (_g->gamekeydown & (1 << key_right))
        cmd->angleturn -= angleturn[tspeed];
    if (_g->gamekeydown & (1 << key_left))
        cmd->angleturn += angleturn[tspeed];

    if (_g->gamekeydown & (1 << key_up))
        forward += forwardmove[speed];
    if (_g->gamekeydown & (1 << key_down))
        forward -= forwardmove[speed];

    if (_g->gamekeydown & (1 << key_straferight))
        side += sidemove[speed];

    if (_g->gamekeydown & (1 << key_strafeleft))
        side -= sidemove[speed];

    if (_g->gamekeydown & (1 << key_fire))
        cmd->buttons |= BT_ATTACK;

    if (_g->gamekeydown & (1 << key_use))
    {
        cmd->buttons |= BT_USE;
    }

    // Toggle between the top 2 favorite weapons.                   // phares
    // If not currently aiming one of these, switch to              // phares
    // the favorite. Only switch if you possess the weapon.         // phares

    // killough 3/22/98:
    //
    // Perform automatic weapons switch here rather than in p_pspr.c,
    // except in demo_compatibility mode.
    //
    // killough 3/26/98, 4/2/98: fix autoswitch when no weapons are left

    if (_g->gamekeydown & (1 << key_weapon_up))
    {
        newweapon = P_WeaponCycleUp(&_g->players[_g->consoleplayer]);
        //side -= sidemove[speed]; //Hack cancel strafe.
    }
    else if (_g->gamekeydown & (1 << key_weapon_down))
    {
        newweapon = P_WeaponCycleDown(&_g->players[_g->consoleplayer]);
        //side += sidemove[speed]; //Hack cancel strafe.
    }
    else if (!demo_compatibility && (_g->players[_g->consoleplayer].attackdown && !P_CheckAmmo(&_g->players[_g->consoleplayer])))
        newweapon = P_SwitchWeapon(&_g->players[_g->consoleplayer]);           // phares
    else
    {                                 // phares 02/26/98: Added gamemode checks
        newweapon = wp_nochange;

        // killough 3/22/98: For network and demo consistency with the
        // new weapons preferences, we must do the weapons switches here
        // instead of in p_user.c. But for old demos we must do it in
        // p_user.c according to the old rules. Therefore demo_compatibility
        // determines where the weapons switch is made.

        // killough 2/8/98:
        // Allow user to switch to fist even if they have chainsaw.
        // Switch to fist or chainsaw based on preferences.
        // Switch to shotgun or SSG based on preferences.

        {
            const player_t *player = &_g->players[_g->consoleplayer];

            // only select chainsaw from '1' if it's owned, it's
            // not already in use, and the player prefers it or
            // the fist is already in use, or the player does not
            // have the berserker strength.

            if (newweapon == wp_fist && player->weaponowned[wp_chainsaw] && player->readyweapon != wp_chainsaw && (player->readyweapon == wp_fist || !player->powers[pw_strength] || P_WeaponPreferred(wp_chainsaw, wp_fist)))
                newweapon = wp_chainsaw;

            // Select SSG from '3' only if it's owned and the player
            // does not have a shotgun, or if the shotgun is already
            // in use, or if the SSG is not already in use and the
            // player prefers it.

            if (newweapon == wp_shotgun && _g->gamemode == commercial && player->weaponowned[wp_supershotgun] && (!player->weaponowned[wp_shotgun] || player->readyweapon == wp_shotgun || (player->readyweapon != wp_supershotgun && P_WeaponPreferred(wp_supershotgun, wp_shotgun))))
                newweapon = wp_supershotgun;
        }
        // killough 2/8/98, 3/22/98 -- end of weapon selection changes
    }

    if (newweapon != wp_nochange)
    {
        cmd->buttons |= BT_CHANGE;
        cmd->buttons |= newweapon << BT_WEAPONSHIFT;
    }

    if (forward > MAXPLMOVE)
        forward = MAXPLMOVE;
    else if (forward < -MAXPLMOVE)
        forward = -MAXPLMOVE;
    if (side > MAXPLMOVE)
        side = MAXPLMOVE;
    else if (side < -MAXPLMOVE)
        side = -MAXPLMOVE;

    cmd->forwardmove += fudgef((signed char) forward);
    cmd->sidemove += side;
    cmd->angleturn = fudgea(cmd->angleturn);

}


//
// G_DoLoadLevel
//

static void G_DoLoadLevel(void)
{

    /* cph 2006/07/31 - took out unused levelstarttic variable */

    if (_g->wipegamestate == GS_LEVEL)
        _g->wipegamestate = -1;             // force a wipe

    _g->gamestate = GS_LEVEL;

    for (int i=0 ; i < MAXPLAYERS ; i++)
    {
      if (_g->playeringame[i] && _g->players[i].playerstate == PST_DEAD)
        _g->players[i].playerstate = PST_REBORN;
      memset (_g->players[i].frags,0,sizeof(_g->players[i].frags));
    }
    // initialize the msecnode_t freelist.                     phares 3/25/98
    // any nodes in the freelist are gone by now, cleared
    // by Z_FreeTags() when the previous level ended or player
    // died.
#if USE_MSECNODE
    NULL_BLOCK_MEMORY_ALLOC_ZONE(secnodezone);
#endif
    NULL_BLOCK_MEMORY_ALLOC_ZONE(mobjzone);
    NULL_BLOCK_MEMORY_ALLOC_ZONE(static_mobjzone);
    NULL_BLOCK_MEMORY_ALLOC_ZONE(dropped_mobj_xy_zone);


    P_SetupLevel(_g->gameepisode, _g->gamemap, 0, _g->gameskill);

    _g->gameaction = ga_nothing;

    // clear cmd building stuff
    //memset(_g->gamekeydown, 0, sizeof(_g->gamekeydown));
    _g->gamekeydown = 0;

    _g->paused = false;

    // killough 5/13/98: in case netdemo has consoleplayer other than green
    ST_Start();
    HU_Start();
}

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//

boolean G_Responder(event_t *ev)
{
    // any other key pops up menu if in demos
    //
    // killough 8/2/98: enable automap in -timedemo demos
    //
    // killough 9/29/98: make any key pop up menu regardless of
    // which kind of demo, and allow other events during playback

    if (_g->gameaction == ga_nothing && (_g->demoplayback || _g->gamestate == GS_DEMOSCREEN))
    {

        // killough 10/98:
        // Don't pop up menu, if paused in middle
        // of demo playback, or if automap active.
        // Don't suck up keys, which may be cheats

        return _g->gamestate == GS_DEMOSCREEN && !(_g->paused & 2) && !(_g->automapmode & am_active) && ((ev->type == ev_keydown) || (ev->type == ev_mouse && ev->data1) || (ev->type == ev_joystick && ev->data1)) ? M_StartControlPanel(), true : false;
    }

    if (_g->gamestate == GS_FINALE && F_Responder(ev))
        return true;  // finale ate the event

    switch (ev->type)
    {
        case ev_keydown:

            if (ev->data1 < NUMKEYS)
                _g->gamekeydown |= 1 << ev->data1;
            return true;    // eat key down events

        case ev_keyup:
            if (ev->data1 < NUMKEYS)
                _g->gamekeydown &= ~ (1 << ev->data1);
            return false;   // always let key up events filter down

        default:
            break;
    }
    return false;
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//

void G_Ticker(void)
{
    int i;

    P_MapStart();
    // do player reborns if needed
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        if (_g->playeringame[i] && _g->players[i].playerstate == PST_REBORN)
        {
            G_DoReborn (i);
        }
    }
    P_MapEnd();

    // do things to change the game state
    while (_g->gameaction != ga_nothing)
    {
      switch (_g->gameaction)
      {
        case ga_loadlevel:
          // force players to be initialized on level reload
          for (i=0 ; i<MAXPLAYERS ; i++)
            _g->players[i].playerstate = PST_REBORN;
          G_DoLoadLevel ();
          break;
        case ga_newgame:
          G_DoNewGame ();
          break;
        case ga_loadgame:
          G_DoLoadGame ();
          break;
        case ga_savegame:
          G_DoSaveGame (false);
          break;
        case ga_playdemo:
          G_DoPlayDemo ();
          break;
        case ga_completed:
          G_DoCompleted ();
          break;
        case ga_victory:
          F_StartFinale ();
          break;
        case ga_worlddone:
          G_DoWorldDone ();
          break;
        case ga_nothing:
          break;
      }
    }
    //
    if (_g->netgame)    // 2023-13-03 next-hack temporary fix
    {
      _g->basetic = 0;
    }
    //

    if ((_g->paused & 2) || (!_g->demoplayback && _g->menuactive && !_g->netgame))
    {
        _g->basetic++;  // For revenant tracers and RNG -- we must maintain sync
    }
    else
    {
#if !HAS_NETWORK
        if (_g->playeringame[_g->consoleplayer])
        {
          int buf = _g->gametic % BACKUPTICS;

          ticcmd_t *cmd = &_g->players[_g->consoleplayer].cmd;

            memcpy(cmd, &_g->netcmds[_g->consoleplayer][buf], sizeof *cmd);

            if (_g->demoplayback)
                G_ReadDemoTiccmd(cmd);
        }
#else
        // get commands, check consistancy, and build new consistancy check
        int buf = _g->gametic % BACKUPTICS;
        for (i = 0 ; i < MAXPLAYERS ; i++)
        {
           if (_g->playeringame[i])
           {
               ticcmd_t *cmd = &_g->players[i].cmd;

               memcpy(cmd, &_g->netcmds[i][buf], sizeof *cmd);

               if (_g->demoplayback)
                   G_ReadDemoTiccmd(cmd);
#if CHECK_CONSISTANCY
// code from prboom, will probably restore
               if (netgame && !netdemo && !(gametic%ticdup) )
               {
                 if (gametic > BACKUPTICS  && consistancy[i][buf] != cmd->consistancy)
                     I_Error("G_Ticker: Consistency failure (%i should be %i)",
                 cmd->consistancy, consistancy[i][buf]);
                 if (players[i].mo)
                     consistancy[i][buf] = players[i].mo->x;
                 else
                     consistancy[i][buf] = 0; // killough 2/14/98
                }
             }
#endif

          }
       }
#endif
    }

    // cph - if the gamestate changed, we may need to clean up the old gamestate
    if (_g->gamestate != _g->prevgamestate)
    {
        if (_g->prevgamestate == GS_INTERMISSION)
        {
            WI_End();
        }
        _g->prevgamestate = _g->gamestate;
    }

    // e6y
    // do nothing if a pause has been pressed during playback
    // pausing during intermission can cause desynchs without that
    if ((_g->paused & 2) && _g->gamestate != GS_LEVEL)
        return;

    // do main actions
    switch (_g->gamestate)
    {
        case GS_LEVEL:
            P_Ticker();
            ST_Ticker();
            AM_Ticker();
            HU_Ticker();
            break;

        case GS_INTERMISSION:
            WI_Ticker();
            break;

        case GS_FINALE:
            F_Ticker();
            break;

        case GS_DEMOSCREEN:
            D_PageTicker();
            break;
    }
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//

static void G_PlayerFinishLevel(int player)
{
    player_t *p = &_g->players[player];
    memset(p->powers, 0, sizeof p->powers);
    memset(p->cards, 0, sizeof p->cards);
    p->mo = NULL;           // cph - this is allocated PU_LEVEL so it's gone
    p->extralight = 0;      // cancel gun flashes
    p->fixedcolormap = 0;   // cancel ir gogles
    p->damagecount = 0;     // no palette changes
    p->bonuscount = 0;
}

//
// G_CheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//
boolean G_CheckSpot (int   playernum,   mapthing_t* mthing )
{
    fixed_t   x;
    fixed_t   y;
    subsector_t*  ss;
    mobj_t*   mo;
    int     i;

    if (!_g->players[playernum].mo)
    {
  // first spawn of level, before corpses
  for (i=0 ; i<playernum ; i++)
      if (_g->players[i].mo->x == mthing->x << FRACBITS
    && _g->players[i].mo->y == mthing->y << FRACBITS)
    return false;
  return true;
    }

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (!P_CheckPosition (_g->players[playernum].mo, x, y) )
      return false;

    // flush an old corpse if needed
    if (bodyqueslot >= BODYQUESIZE)
      P_RemoveMobj (getLongPtr(bodyque_sptr[bodyqueslot %BODYQUESIZE]));
    bodyque_sptr[bodyqueslot%BODYQUESIZE] = getShortPtr(_g->players[playernum].mo);
    bodyqueslot++;

    // spawn a teleport fog
    ss = R_PointInSubsector (x,y);


    // The code in the released source looks like this:
    //
    //    an = ( ANG45 * (((unsigned int) mthing->angle)/45) )
    //         >> ANGLETOFINESHIFT;
    //    mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an]
    //                     , ss->sector->floorheight
    //                     , MT_TFOG);
    //
    // But 'an' can be a signed value in the DOS version. This means that
    // we get a negative index and the lookups into finecosine/finesine
    // end up dereferencing values in finetangent[].
    // A player spawning on a deathmatch start facing directly west spawns
    // "silently" with no spawn fog. Emulate this.
    //
    // This code is imported from PrBoom+.

    {
        fixed_t xa, ya;
        signed int an;

        // This calculation overflows in Vanilla Doom, but here we deliberately
        // avoid integer overflow as it is undefined behavior, so the value of
        // 'an' will always be positive.
        an = (ANG45 >> ANGLETOFINESHIFT) * ((signed int) mthing->angle / 45);

        switch (an)
        {
            case 4096:  // -4096:
                xa = finetangent(2048);    // finecosine[-4096]
                ya = finetangent(0);       // finesine[-4096]
                break;
            case 5120:  // -3072:
                xa = finetangent(3072);    // finecosine[-3072]
                ya = finetangent(1024);    // finesine[-3072]
                break;
            case 6144:  // -2048:
                xa = finesine(0);          // finecosine[-2048]
                ya = finetangent(2048);    // finesine[-2048]
                break;
            case 7168:  // -1024:
                xa = finesine(1024);       // finecosine[-1024]
                ya = finetangent(3072);    // finesine[-1024]
                break;
            case 0:
            case 1024:
            case 2048:
            case 3072:
                xa = finecosine(an);
                ya = finesine(an);
                break;
            default:
                I_Error("G_CheckSpot: unexpected angle %d\n", an);
                xa = ya = 0;
                break;
        }
        mo = P_SpawnMobj(x + 20 * xa, y + 20 * ya,
                         FIXED16_TO_FIXED32(_g->ramsectors[ss->sector_num].floorheight16), MT_TFOG);
    }

    if (_g->players[_g->consoleplayer].viewz != 1)
      S_StartSound (mo, sfx_telept);  // don't start sound on first frame

    return true;
}


//
// G_DeathMatchSpawnPlayer
// Spawns a player at one of the random death match spots
// called at level load and each death
//

void G_DeathMatchSpawnPlayer (int playernum)
{
    int             i, j;
    int       selections;

    selections = deathmatch_p - deathmatchstarts;
    if (selections < 4)
       I_Error ("Only %i deathmatch spots, 4 required", selections);

    for (j = 0 ; j < 20 ; j++)
    {
      i = P_Random() % selections;
      if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
      {
          deathmatchstarts[i].type = playernum + 1;
          P_SpawnPlayer (playernum, &deathmatchstarts[i]);
          return;
      }
    }
    // no good spot, so the player will probably get stuck
    P_SpawnPlayer (playernum, &_g->playerstarts[playernum]);
}

//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//

void G_PlayerReborn(int player)
{
    player_t* p;
    int   i;
    int   frags[MAXPLAYERS];
    int   killcount;
    int   itemcount;
    int   secretcount;

    memcpy (frags,_g->players[player].frags,sizeof(frags));
    killcount = _g->players[player].killcount;
    itemcount = _g->players[player].itemcount;
    secretcount = _g->players[player].secretcount;

    p = &_g->players[player];
    memset (p, 0, sizeof(*p));

    memcpy (_g->players[player].frags, frags, sizeof(_g->players[player].frags));
    _g->players[player].killcount = killcount;
    _g->players[player].itemcount = itemcount;
    _g->players[player].secretcount = secretcount;

    p->usedown = p->attackdown = true;  // don't do anything immediately
    p->playerstate = PST_LIVE;
    p->health = initial_health;     // Use dehacked value
    p->readyweapon = p->pendingweapon = wp_pistol;
    p->weaponowned[wp_fist] = true;
    p->weaponowned[wp_pistol] = true;
    p->ammo[am_clip] = initial_bullets;

    for (i=0 ; i<NUMAMMO ; i++)
      p->maxammo[i] = maxammo[i];
}

//
// G_DoReborn
//

void G_DoReborn (int playernum)
{
  if (!_g->netgame)
    _g->gameaction = ga_loadlevel;      // reload the level from scratch
  else
    {                               // respawn at the start
      int i;

      // first dissasociate the corpse
      _g->players[playernum].mo->player_n = 0xFF;
// BUG! Color won't be correct
      // spawn at random spot if in death match
      if (_g->deathmatch)
        {
          G_DeathMatchSpawnPlayer (playernum);
          return;
        }

      if (G_CheckSpot (playernum, &_g->playerstarts[playernum]) )
        {
          P_SpawnPlayer (playernum, &_g->playerstarts[playernum]);
          return;
        }

      // try to spawn at one of the other players spots
      for (i=0 ; i<MAXPLAYERS ; i++)
        {
          if (G_CheckSpot (playernum, &_g->playerstarts[i]) )
            {
              P_SpawnPlayer (playernum, &_g->playerstarts[i]);
              return;
            }
          // he's going to be inside something.  Too bad.
        }
      P_SpawnPlayer (playernum, &_g->playerstarts[playernum]);
    }
}

// DOOM Par Times
const int pars[4][10] =
{
    { 0 },
    { 0, 30, 75, 120, 90, 165, 180, 180, 30, 165 },
    { 0, 90, 90, 90, 120, 90, 360, 240, 30, 170 },
    { 0, 90, 45, 90, 150, 90, 90, 165, 30, 135 } 
};

// DOOM II Par Times
const int cpars[32] =
{ 
    30, 90, 120, 120, 90, 150, 120, 120, 270, 90,  //  1-10
    210, 150, 150, 150, 210, 150, 420, 150, 210, 150,  // 11-20
    240, 150, 180, 150, 150, 300, 330, 420, 300, 180,  // 21-30
    120, 30          // 31-32
};

void G_ExitLevel(void)
{
    // 2021/05/19: next hack workaround for repeating sound before going to the intermission screen
    S_Stop();
    _g->secretexit = false;
    _g->gameaction = ga_completed;
}

// Here's for the german edition.
// IF NO WOLF3D LEVELS, NO SECRET EXIT!

void G_SecretExitLevel(void)
{
    // 2021/05/19: next hack workaround for repeating sound before going to the intermission screen
    S_Stop();
    if (_g->gamemode != commercial || _g->haswolflevels)
        _g->secretexit = true;
    else
        _g->secretexit = false;
    _g->gameaction = ga_completed;
}

//
// G_DoCompleted
//

void G_DoCompleted(void)
{
    _g->gameaction = ga_nothing;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
      if (_g->playeringame[i])
      {
        G_PlayerFinishLevel(i);        // take away cards and stuff
      }
    }
    if (_g->automapmode & am_active)
        AM_Stop();

    if (_g->gamemode != commercial) // kilough 2/7/98
        switch (_g->gamemap)
        {
            // cph - Remove ExM8 special case, so it gets summary screen displayed
            case 9:
              for (int i = 0 ; i < MAXPLAYERS ; i++)
              {
                _g->players[i].didsecret = true;
              }
              break;
        }

    _g->wminfo.didsecret = _g->players[_g->consoleplayer].didsecret;
    _g->wminfo.epsd = _g->gameepisode - 1;
    _g->wminfo.last = _g->gamemap - 1;

    // wminfo.next is 0 biased, unlike gamemap
    if (_g->gamemode == commercial)
    {
        if (_g->secretexit)
            switch (_g->gamemap)
            {
                case 15:
                    _g->wminfo.next = 30;
                    break;
                case 31:
                    _g->wminfo.next = 31;
                    break;
            }
        else
            switch (_g->gamemap)
            {
                case 31:
                case 32:
                    _g->wminfo.next = 15;
                    break;
                default:
                    _g->wminfo.next = _g->gamemap;
            }
    }
    else
    {
        if (_g->secretexit)
            _g->wminfo.next = 8;  // go to secret level
        else if (_g->gamemap == 9)
        {
            // returning from secret level
            switch (_g->gameepisode)
            {
                case 1:
                    _g->wminfo.next = 3;
                    break;
                case 2:
                    _g->wminfo.next = 5;
                    break;
                case 3:
                    _g->wminfo.next = 6;
                    break;
                case 4:
                    _g->wminfo.next = 2;
                    break;
            }
        }
        else
            _g->wminfo.next = _g->gamemap;          // go to next level
    }

    _g->wminfo.maxkills = _g->totalkills;
    _g->wminfo.maxitems = _g->totalitems;
    _g->wminfo.maxsecret = _g->totalsecret;
    _g->wminfo.pnum = _g->consoleplayer;
    _g->wminfo.maxfrags = 0;

    if (_g->gamemode == commercial)
        _g->wminfo.partime = TICRATE * cpars[_g->gamemap - 1];
    else
        _g->wminfo.partime = TICRATE * pars[_g->gameepisode][_g->gamemap];

    for (int i = 0 ; i < MAXPLAYERS ; i++)
    {
      _g->wminfo.plyr[i].in = _g->playeringame[i];
      _g->wminfo.plyr[i].skills = _g->players[i].killcount;
      _g->wminfo.plyr[i].sitems = _g->players[i].itemcount;
      _g->wminfo.plyr[i].ssecret = _g->players[i].secretcount;
      _g->wminfo.plyr[i].stime = _g->leveltime;
      memcpy (_g->wminfo.plyr[i].frags, _g->players[i].frags,
               sizeof(_g->wminfo.plyr[i].frags));
    }

    /* cph - modified so that only whole seconds are added to the totalleveltimes
     *  value; so our total is compatible with the "naive" total of just adding
     *  the times in seconds shown for each level. Also means our total time
     *  will agree with Compet-n.
     */
    _g->wminfo.totaltimes = (_g->totalleveltimes += (_g->leveltime - _g->leveltime % 35));

    _g->gamestate = GS_INTERMISSION;
    _g->automapmode &= ~am_active;

    // lmpwatch.pl engine-side demo testing support
    // print "FINISHED: <mapname>" when the player exits the current map
    if (nodrawers && (_g->demoplayback || _g->timingdemo))
    {
        if (_g->gamemode == commercial)
            lprintf(LO_INFO, "FINISHED: MAP%02d\n", _g->gamemap);
        else
            lprintf(LO_INFO, "FINISHED: E%dM%d\n", _g->gameepisode, _g->gamemap);
    }

    WI_Start(&_g->wminfo);
}

//
// G_WorldDone
//

void G_WorldDone(void)
{
    _g->gameaction = ga_worlddone;

    if (_g->secretexit)
        _g->players[_g->consoleplayer].didsecret = true;

    if (_g->gamemode == commercial)
    {
        switch (_g->gamemap)
        {
            case 15:
            case 31:
                if (!_g->secretexit)
                {
                    break;
                }
                else
                {
                    F_StartFinale();
                }
                break;
            case 6:
            case 11:
            case 20:
            case 30:
                F_StartFinale();
                break;
        }
    }
    else if (_g->gamemap == 8)
        _g->gameaction = ga_victory; // cph - after ExM8 summary screen, show victory stuff
}

void G_DoWorldDone(void)
{
    _g->idmusnum = -1;        //jff 3/17/98 allow new level's music to be loaded
    _g->gamestate = GS_LEVEL;
    _g->gamemap = _g->wminfo.next + 1;
    G_DoLoadLevel();
    _g->gameaction = ga_nothing;
}

// killough 2/28/98: A ridiculously large number
// of players, the most you'll ever need in a demo
// or savegame. This is used to prevent problems, in
// case more players in a game are supported later.

#define MIN_MAXPLAYERS 4

//
// killough 5/15/98: add forced loadgames, which allow user to override checks
//

void G_ForcedLoadGame(void)
{
    // CPhipps - net loadgames are always forced, so we only reach here
    //  in single player
    _g->gameaction = ga_loadgame;
}
//
// 2021/05/02 next-hack: cheap itoa.
//
int str2num(const char *str)
{
    int num = 0;
    int d;
    while ((d = *str))
    {
        d -= '0';
        if (d < 0 || d > 9)
            return 0;
        num = 10 * num + d;
        str++;
    }
    return num;
}

//
// Update the strings displayed in the load-save menu.
//
void G_UpdateSaveGameStrings()
{
    unsigned int savebuffersize = sizeof(doom_save_data_t) * 8;

    // 2021/05/09 next-hack: display buffer is used here also as a stack
    // but it is huge (15kB) and, as stack, its last elements are used first.
    // Therefore we can "safely" use it as a temporary work buffer.
    // Yes, this is a bad hack, but it works :)
    byte *loadbuffer = (byte*) displayData.displayFrameBuffer[displayData.workingBuffer];

    extMemSetCurrentAddress(SAVEGAME_ADDRESS);
    extMemGetDataFromCurrentAddress(loadbuffer, savebuffersize);
    //
    doom_save_data_t *saveslots = (doom_save_data_t*) loadbuffer;

    for (int i = 0; i < 8; i++)
    {
        if (saveslots[i].save_present != 1)
        {
            strcpy(savegamestrings[i], "EMPTY");
        }
        else
        {
            if (_g->gamemode == commercial)
            {
                snprintf(savegamestrings[i], sizeof(savegamestrings[i]), "MAP %d", saveslots[i].gamemap);
            }
            else
            {
                strcpy(savegamestrings[i], "ExMy");

                savegamestrings[i][1] = '0' + saveslots[i].gameepisode;
                savegamestrings[i][3] = '0' + saveslots[i].gamemap;
            }
        }
    }
}

// killough 3/16/98: add slot info
// killough 5/15/98: add command-line
void G_LoadGame(int slot, boolean command)
{
    (void) command;
    _g->savegameslot = slot;
    _g->demoplayback = false;

    G_DoLoadGame();
}

// CPhipps - size of version header
#define VERSIONSIZE   16

void G_DoLoadGame()
{
    unsigned int savebuffersize = sizeof(doom_save_data_t) * 8;

    // 2021/05/09 next-hack: display buffer is used here also as a stack
    // but it is huge (15kB) and, as stack, its last elements are used first.
    // Therefore we can "safely" use it as a temporary work buffer.
    // Yes, this is a bad hack, but it works :)
    byte *loadbuffer = (byte*) displayData.displayFrameBuffer[displayData.workingBuffer];
    // load buffer
    extMemSetCurrentAddress(SAVEGAME_ADDRESS);
    extMemGetDataFromCurrentAddress(loadbuffer, savebuffersize);

    doom_save_data_t *saveslots = (doom_save_data_t*) loadbuffer;

    doom_save_data_t *savedata = &saveslots[_g->savegameslot];

    if (savedata->save_present != 1)
        return;

    _g->gameskill = savedata->gameskill;
    _g->gameepisode = savedata->gameepisode;
    _g->gamemap = savedata->gamemap;
    _g->alwaysRun = savedata->alwaysRun;
    _g->gamma = savedata->gamma;

    printf("InitNew()\r\n");
    G_InitNew(_g->gameskill, _g->gameepisode, _g->gamemap);

    _g->totalleveltimes = savedata->totalleveltimes;
    for (int i = 0; i < NUMWEAPONS; i++)
    {
        _g->players[_g->consoleplayer].weaponowned[i] = savedata->weaponowned[i];
    }
    for (int i = 0; i < NUMAMMO; i++)
    {
        _g->players[_g->consoleplayer].ammo[i] = savedata->ammo[i];
        _g->players[_g->consoleplayer].maxammo[i] = savedata->maxammo[i];
    }
    //If stored maxammo is more than no backpack ammo, player had a backpack.
    if (_g->players[_g->consoleplayer].maxammo[am_clip] > maxammo[am_clip])
        _g->players[_g->consoleplayer].backpack = true;
}

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//

void G_SaveGame(int slot, const char *description)
{
    (void) description;
    _g->savegameslot = slot;
    G_DoSaveGame(true);
}

static void G_DoSaveGame(boolean menu)
{
    (void) menu;
    unsigned int savebuffersize = sizeof(doom_save_data_t) * 8;

    // 2021/05/09 next-hack: display buffer is used here also as a stack
    // but it is huge (15kB) and, as stack, its last elements are used first.
    // Therefore we can "safely" use it as a temporary work buffer.
    // Yes, this is a bad hack, but it works :)
    byte *savebuffer = (byte*) displayData.displayFrameBuffer[displayData.workingBuffer];
    // read old data
    extMemSetCurrentAddress(SAVEGAME_ADDRESS);
    extMemGetDataFromCurrentAddress(savebuffer, savebuffersize);
    //
    doom_save_data_t *saveslots = (doom_save_data_t*) savebuffer;
    doom_save_data_t *savedata = &saveslots[_g->savegameslot];
    // update
    savedata->save_present = 1;
    savedata->gameskill = _g->gameskill;
    savedata->gameepisode = _g->gameepisode;
    savedata->gamemap = _g->gamemap;
    savedata->totalleveltimes = _g->totalleveltimes;
    savedata->alwaysRun = _g->alwaysRun;
    savedata->gamma = _g->gamma;
    //
    for (int i = 0; i < NUMWEAPONS; i++)
    {
        savedata->weaponowned[i] = _g->players[_g->consoleplayer].weaponowned[i];
    }
    for (int i = 0; i < NUMAMMO; i++)
    {
        savedata->ammo[i] = _g->players[_g->consoleplayer].ammo[i];
        savedata->maxammo[i] = _g->players[_g->consoleplayer].maxammo[i];
    }
    //
    extMemWrite(SAVEGAME_ADDRESS, savebuffer, savebuffersize);

//    Z_Free(savebuffer);

    _g->players[_g->consoleplayer].message = GGSAVED;

    G_UpdateSaveGameStrings();
}

void G_SaveSettings()
{
    doom_save_settings_t settings;
    settings.cookie = settings_cookie;
    settings.gamma = _g->gamma;
    settings.alwaysRun = _g->alwaysRun;
    settings.showMessages = _g->showMessages;
    settings.musicVolume = _g->snd_MusicVolume;
    settings.soundVolume = _g->snd_SfxVolume;
#if HAS_NETWORK
    memcpy(settings.playerName, localPlayerName, sizeof(settings.playerName));
#endif
    extMemWrite(SETTINGS_ADDRESS, &settings, sizeof(settings));
}

void G_LoadSettings()
{
    doom_save_settings_t *settings = (doom_save_settings_t*) displayData.displayFrameBuffer[displayData.workingBuffer];
    memset(settings, 0, sizeof(*settings));
    //
    extMemSetCurrentAddress(SETTINGS_ADDRESS);
    extMemGetDataFromCurrentAddress(settings, sizeof(*settings));
    //
    if (settings->cookie == settings_cookie)
    {
        _g->gamma = (settings->gamma >= MAX_GAMMA) ? MAX_GAMMA : settings->gamma;
        _g->alwaysRun = (settings->alwaysRun > 0) ? 1 : 0;

        _g->showMessages = (settings->showMessages > 0) ? 1 : 0;

        _g->snd_SfxVolume = (settings->soundVolume > 15) ? 15 : settings->soundVolume;
        _g->snd_MusicVolume = (settings->musicVolume > 15) ? 15 : settings->musicVolume;

        V_SetPalette(0);
#if HAS_NETWORK
        memcpy(localPlayerName, settings->playerName,  sizeof(localPlayerName));
        // let's check if is valid.
        // conditions: must have at least one char, and all chars shall be either 0 or between ! and Z
        boolean valid = true;
        if (!(localPlayerName[0] >= '!' && localPlayerName[0] <= 'Z'))
        {
          valid = false;
        }
        for (int i = 1; i < sizeof(localPlayerName) && valid; i++)
        {
          char c = localPlayerName[i];
          if (0 == c) // null terminator admissible from char 1
          {
            break;
          }
          if ( !(c >= '!' && c <= 'Z'))
          {
            valid = false;
          }
        }
        // if not valid, reset to "?"
        if (!valid)
        {
          localPlayerName[0] = '?';
          localPlayerName[1] = '0';
        }
        //
#endif
        S_SetSfxVolume(_g->snd_SfxVolume);
        S_SetMusicVolume(_g->snd_MusicVolume);
    }
}

void G_DeferedInitNew(skill_t skill, int episode, int map)
{
    _g->d_skill = skill;
    _g->d_episode = episode;
    _g->d_map = map;
    _g->gameaction = ga_newgame;
}

// killough 3/1/98: function to reload all the default parameter
// settings before a new game begins

void G_ReloadDefaults(void)
{
    // killough 3/1/98: Initialize options based on config file
    // (allows functions above to load different values for demos
    // and savegames without messing up defaults).
    _g->demoplayback = false;
    _g->singledemo = false;         // killough 9/29/98: don't stop after 1 demo
}

void G_DoNewGame(void)
{
    G_ReloadDefaults();            // killough 3/1/98
    // here we can choose the map for testing
    _g->d_map = START_MAP;
    G_InitNew(_g->d_skill, _g->d_episode, _g->d_map);
    _g->gameaction = ga_nothing;

    //jff 4/26/98 wake up the status bar in case were coming out of a DM demo
    ST_Start();
}

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//

void G_InitNew(skill_t skill, int episode, int map)
{
    if (_g->paused)
    {
        _g->paused = false;
        S_ResumeSound();
    }

    if (skill > sk_nightmare)
        skill = sk_nightmare;

    if (episode < 1)
        episode = 1;

    if (_g->gamemode == retail)
    {
        if (episode > 4)
            episode = 4;
    }
    else if (_g->gamemode == shareware)
    {
        if (episode > 1)
            episode = 1; // only start episode 1 on shareware
    }
    else if (episode > 3)
        episode = 3;

    if (map < 1)
        map = 1;
    if (map > 9 && _g->gamemode != commercial)
        map = 9;

    M_ClearRandom();

    _g->respawnmonsters = skill == sk_nightmare;

    for (int i = 0 ; i < MAXPLAYERS ; i++)
    {
       _g->players[i].playerstate = PST_REBORN;
    }
    _g->usergame = true;                // will be set false if a demo
    _g->paused = false;
    _g->automapmode &= ~am_active;
    _g->gameepisode = episode;
    _g->gamemap = map;
    _g->gameskill = skill;

    _g->totalleveltimes = 0; // cph
    printf("\r\n\r\nDo load Level\r\n\r\n");
    G_DoLoadLevel();
}

//
// DEMO RECORDING
//

#define DEMOMARKER    0x80

void G_ReadDemoTiccmd(ticcmd_t *cmd)
{
#if DEMO_SUPPORT
    unsigned char at; // e6y: tasdoom stuff
    //
#define NEW_READ 1
#if NEW_READ
    uint8_t demoBuffer[5];
    extMemSetCurrentAddress((uint32_t) _g->demo_p);
    extMemGetDataFromCurrentAddress(demoBuffer, sizeof(demoBuffer));
    if (demoBuffer[0] == DEMOMARKER)
        G_CheckDemoStatus();      // end of demo data stream
    else if (_g->demoplayback && _g->demo_p + (_g->longtics ? 5 : 4) > _g->demobuffer + _g->demolength)
    {
        lprintf(LO_WARN, "G_ReadDemoTiccmd: missing DEMOMARKER\n");
        G_CheckDemoStatus();
    }
    else
    {
        cmd->forwardmove = ((signed char) demoBuffer[0]);
        cmd->sidemove = ((signed char) demoBuffer[1]);
        if (!_g->longtics)
        {
            cmd->angleturn = ((unsigned char) (at = demoBuffer[2])) << 8;
            cmd->buttons = (unsigned char) demoBuffer[3];

        }
        else
        {
            unsigned int lowbyte = (unsigned char) demoBuffer[2];
            cmd->angleturn = (((signed int) (demoBuffer[3])) << 8) + lowbyte;
            cmd->buttons = (unsigned char) demoBuffer[4];
        }
        demodbgprintf("MR: %02X GT: %d. fm %x, sm %x, at %x, bt %x, X: %x, Y: %x, Mx: %x, My: %x, angle: %x\r\n",_g->prndindex, _g->gametic, cmd->forwardmove, cmd->sidemove, cmd->angleturn, cmd->buttons, _g->player.mo->x, _g->player.mo->y, _g->player.mo->momx, _g->player.mo->momy, _g->player.mo->angle16 << 16);
        _g->demo_p += _g->longtics ? 5 : 4;
    }
#else
    if (extMemGetByteFromAddress(_g->demo_p) == DEMOMARKER)
        G_CheckDemoStatus();      // end of demo data stream
    else if (_g->demoplayback && _g->demo_p + (_g->longtics ? 5 : 4) > _g->demobuffer + _g->demolength)
    {
        lprintf(LO_WARN, "G_ReadDemoTiccmd: missing DEMOMARKER\n");
        G_CheckDemoStatus();
    }
    else
    {
        cmd->forwardmove = ((signed char) extMemGetByteFromAddress(_g->demo_p++));
        cmd->sidemove = ((signed char) extMemGetByteFromAddress(_g->demo_p++));
        if (!_g->longtics)
        {
            cmd->angleturn = ((unsigned char) (at = extMemGetByteFromAddress(_g->demo_p++))) << 8;
        }
        else
        {
            unsigned int lowbyte = (unsigned char) extMemGetByteFromAddress(_g->demo_p++);
            cmd->angleturn = (((signed int) (extMemGetByteFromAddress(_g->demo_p++))) << 8) + lowbyte;
        }
        cmd->buttons = (unsigned char) extMemGetByteFromAddress(_g->demo_p++);
        demodbgprintf("MR: %02X GT: %d. fm %x, sm %x, at %x, bt %x, X: %x, Y: %x, Mx: %x, My: %x, angle: %x\r\n",_g->prndindex, _g->gametic, cmd->forwardmove, cmd->sidemove, cmd->angleturn, cmd->buttons, _g->player.mo->x, _g->player.mo->y, _g->player.mo->momx, _g->player.mo->momy, _g->player.mo->angle16 << 16);
    }
#endif
    #endif
}

/* Same, but read instead of write
 * cph - const byte*'s
 */

const byte* G_ReadOptions(const byte *demo_p)
{
    const byte *target = demo_p + GAME_OPTION_SIZE;

    return target;
}

//
// G_PlayDemo
//

static const char *defdemoname;

void G_DeferedPlayDemo(const char *name)
{
#if DEMO_SUPPORT
    defdemoname = name;
    _g->gameaction = ga_playdemo;
#endif
}

static int demolumpnum = -1;

//e6y: Check for overrun
static boolean CheckForOverrun(const byte *start_p, const byte *current_p, size_t maxsize, size_t size, boolean failonerror)
{
#if DEMO_SUPPORT

    size_t pos = current_p - start_p;
    if (pos + size > maxsize)
    {
        if (failonerror)
            I_Error("G_ReadDemoHeader: wrong demo header\n");
        else
            return true;
    }
#endif
    return false;
}

static const byte* G_ReadDemoHeader(const byte *demo_p, size_t size, boolean failonerror)
{
#if DEMO_SUPPORT
    skill_t skill;
    int episode, map;

    // e6y
    // The local variable should be used instead of demobuffer,
    // because demobuffer can be uninitialized
    const byte *header_p = demo_p;

    _g->basetic = _g->gametic;  // killough 9/29/98

    // killough 2/22/98, 2/28/98: autodetect old demos and act accordingly.
    // Old demos turn on demo_compatibility => compatibility; new demos load
    // compatibility flag, and other flags as well, as a part of the demo.

    //e6y: check for overrun
    if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
        return NULL;

    _g->demover = extMemGetByteFromAddress(demo_p++);
    _g->longtics = 0;

    // e6y
    // Handling of unrecognized demo formats
    // Versions up to 1.2 use a 7-byte header - first byte is a skill level.
    // Versions after 1.2 use a 13-byte header - first byte is a demoversion.
    // BOOM's demoversion starts from 200
    if (!((_g->demover <= 4) || (_g->demover >= 104 && _g->demover <= 111) || (_g->demover >= 200 && _g->demover <= 214)))
    {
        I_Error("G_ReadDemoHeader: Unknown demo format %d.", _g->demover);
    }

    if (_g->demover < 200)     // Autodetect old demos
    {
        if (_g->demover >= 111)
            _g->longtics = 1;

        // killough 3/2/98: force these variables to be 0 in demo_compatibility

        // killough 3/6/98: rearrange to fix savegame bugs (moved fastparm,
        // respawnparm, nomonsters flags to G_LoadOptions()/G_SaveOptions())

        if ((skill = _g->demover) >= 100)      // For demos from versions >= 1.4
        {
            //e6y: check for overrun
            if (CheckForOverrun(header_p, demo_p, size, 8, failonerror))
                return NULL;

            skill = extMemGetByteFromAddress(demo_p++);
            episode = extMemGetByteFromAddress(demo_p++);
            map = extMemGetByteFromAddress(demo_p++);
            demo_p++;
            demo_p++;
            demo_p++;
            demo_p++;
            demo_p++;
        }
        else
        {
            //e6y: check for overrun
            if (CheckForOverrun(header_p, demo_p, size, 2, failonerror))
                return NULL;

            episode = extMemGetByteFromAddress(demo_p++);
            map = extMemGetByteFromAddress(demo_p++);
        }

    }
    else    // new versions of demos
    {
        demo_p += 6;               // skip signature;
        switch (_g->demover)
        {
            case 200: /* BOOM */
            case 201:
                //e6y: check for overrun
                if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
                {
                    return NULL;
                }
                break;
            case 202:
                //e6y: check for overrun
                if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
                {
                    return NULL;
                }
                break;
            case 203:
                /* LxDoom or MBF - determine from signature
                 * cph - load compatibility level */
                switch (*(header_p + 2))
                {
                    case 'B': /* LxDoom */
                        /* cph - DEMOSYNC - LxDoom demos recorded in compatibility modes support dropped */
                        break;
                    case 'M':
                        demo_p++;
                        break;
                }
                break;
            case 210:
                demo_p++;
                break;
            case 211:
                demo_p++;
                break;
            case 212:
                demo_p++;
                break;
            case 213:
                demo_p++;
                break;
            case 214:
                _g->longtics = 1;
                demo_p++;
                break;
        }
        //e6y: check for overrun
        if (CheckForOverrun(header_p, demo_p, size, 5, failonerror))
            return NULL;

        skill = extMemGetByteFromAddress(demo_p++);
        episode = extMemGetByteFromAddress(demo_p++);
        map = extMemGetByteFromAddress(demo_p++);
        demo_p++;
        demo_p++;

        //e6y: check for overrun
        if (CheckForOverrun(header_p, demo_p, size, GAME_OPTION_SIZE, failonerror))
            return NULL;

        demo_p = G_ReadOptions(demo_p);  // killough 3/1/98: Read game options

        if (_g->demover == 200)    // killough 6/3/98: partially fix v2.00 demos
            demo_p += 256 - GAME_OPTION_SIZE;
    }

    //e6y: check for overrun
    if (CheckForOverrun(header_p, demo_p, size, MAXPLAYERS, failonerror))
    {
        return NULL;
    }
    for (int i = 0; i < MAXPLAYERS; i++)
    {
      _g->playeringame[i] = extMemGetByteFromAddress(demo_p++);
      _g->players[i].cheats = 0;
    }
    if (_g->gameaction != ga_loadgame)
    { /* killough 12/98: support -loadgame */

        G_InitNew(skill, episode, map);
    }
    return demo_p;
    #else
    return NULL;
    #endif
}

void G_DoPlayDemo(void)
{
#if DEMO_SUPPORT

    char basename[9];

    ExtractFileBase(defdemoname, basename);           // killough
    basename[8] = 0;

    /* cph - store lump number for unlocking later */
    demolumpnum = W_GetNumForName(basename);
    _g->demobuffer = W_CacheLumpNum(demolumpnum);
    _g->demolength = W_LumpLength(demolumpnum);

    _g->demo_p = G_ReadDemoHeader(_g->demobuffer, _g->demolength, true);

    _g->gameaction = ga_nothing;
    _g->usergame = false;

    _g->demoplayback = true;

    _g->starttime = I_GetTime();
    #endif
}

/* G_CheckDemoStatus
 *
 * Called after a death or level completion to allow demos to be cleaned up
 * Returns true if a new demo loop action will take place
 */
boolean G_CheckDemoStatus(void)
{
#if DEMO_SUPPORT

    if (_g->timingdemo)
    {
        int endtime = I_GetTime();
        // killough -- added fps information and made it work for longer demos:
        unsigned realtics = endtime - _g->starttime;
        unsigned int fps = (unsigned) _g->gametic * 100 * TICRATE / realtics;
        printf("Timed %u gametics in %u realtics = %u.%02u frames per second", (unsigned) _g->gametic, realtics, fps / 100, fps % 100);

    }

    if (_g->demoplayback)
    {
//        if (_g->singledemo)
 //           exit(0);  // killough

        if (demolumpnum != -1)
        {
            demolumpnum = -1;
        }
        G_ReloadDefaults();    // killough 3/1/98
        D_AdvanceDemo();
        return true;
    }
    #endif
    return false;
}

