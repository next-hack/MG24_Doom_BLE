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
 *  System interface for sound.
 *  next-hack: added support for pwm and DAC sound.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "z_zone.h"

#include "m_swap.h"
#include "i_sound.h"
#include "m_misc.h"
#include "w_wad.h"
#include "lprintf.h"
#include "s_sound.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"

#include "d_main.h"

#include "m_fixed.h"
#include "i_sound.h"
#include "global_data.h"
#include "audio.h"

void I_UpdateSoundParams ( int channel,  int volume,  int separation)
{
#if STEREO_AUDIO
  int   rightvol;
  int   leftvol;

  // Separation, that is, orientation/stereo.
   //  range is: 1 - 256
   separation += 1;

   // Per left/right channel.
   //  x^2 seperation,
   //  adjust volume properly.
   leftvol = volume - ((volume * separation * separation) >> 16);
   separation = separation - 257;
   rightvol = volume - ((volume * separation * separation) >> 16);

   // Sanity check, clamp volume.
   if (rightvol < 0 || rightvol > 127)
     I_Error("rightvol out of bounds");

   if (leftvol < 0 || leftvol > 127)
     I_Error("leftvol out of bounds");

   // Get the proper lookup table piece
   //  for this volume level???
   soundChannels[channel].volumeLeft= leftvol;
   soundChannels[channel].volumeRight = rightvol;
  #else
  soundChannels[channel].volumeLeft = volume;
#endif
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int I_StartSound(int id, int channel, int vol, int sep)
{

    if ((channel < 0) || (channel >= MAX_CHANNELS))
        return -1;
    // 2021/05/15 next-hack
    //
    I_UpdateSoundParams(channel, vol, sep);
    //
    soundChannels[channel].lastAudioBufferIdx = 0xFFFF;
    soundChannels[channel].offset = 0;
    soundChannels[channel].sfxIdx = id;
    return channel;
}

//static SDL_AudioSpec audio;

void I_InitSound(void)
{

    if (!nomusicparm)
        I_InitMusic();

    // Finished initialization.
    lprintf(LO_INFO, "I_InitSound: sound ready");
}

void I_InitMusic(void)
{
}

void I_PlaySong(int handle, int looping)
{
    (void) looping;
    setMusic(handle);
    OPL_InitRegisters();
    initMusic();
    if (handle == mus_None)
        return;

}

void I_PauseSong(int handle)
{
  (void) handle;
}

void I_ResumeSong(int handle)
{
  (void) handle;
}

void I_StopSong(int handle)
{
  (void) handle;
}

void I_SetMusicVolume(int volume)
{
  I_OPL_SetMusicVolume(volume * 8);
  (void) volume;
}
