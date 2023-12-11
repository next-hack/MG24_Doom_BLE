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
 *      Mission begin melt/wipe screen special effect.
 *      next-hack: the GBA doom port removed the screen melt effect, so the
 *      wipe has been reduced now to a stub function.
 *      I will probably remove all the functions and variables too sooner or later.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomdef.h"
#include "f_wipe.h"
#include "global_data.h"
#include "graphics.h"
#include "i_video.h"
#include "m_random.h"
#include "v_video.h"
#include "z_zone.h"
#include "r_main.h"
//
// SCREEN WIPE PACKAGE
//

int wipe_StartScreen(void)
{
    _g->wipe_tick = 0;
    return 0;
}

int wipe_EndScreen(void)
{
    _g->wipe_tick = 0;
    return 0;
}
// 2022/11/08 Added hack to have music and sound during wipe
extern maskedData_t maskedData;
void wipe_updateSound()
{
  __disable_irq();
         __asm volatile
         (
               "STR SP, [%[newStack]]\n\t"
               "MOV SP, %[newStack]\n\t"
               "SUB SP, #4\n\t"
               : : [newStack] "r" ((uint8_t*) &maskedData + sizeof(maskedData_t) - 4) : "memory"
         );
         __enable_irq();
          updateSound();
         __disable_irq();
         __asm volatile
         (
               "LDR SP, [%[newStack]]\n\t"
           : : [newStack] "r" ((volatile uint32_t) ( (uint8_t*) &maskedData + sizeof(maskedData_t) - 4)) : "memory"
         );
         __enable_irq();

}
// oh man, why aren't you commenting anything ?
// 2021-08-08 next-hack: commented and modified to use the dual buffer.
static int wipe_doMelt(int ticks, short *y_lookup)
{
    boolean done = true;
    int i;
    while (ticks--)
    {

        wipe_updateSound();
        for (i = 0; i < (SCREENWIDTH); i++)
        {
            if (y_lookup[i] < 0)
            {
                y_lookup[i]++;
                done = false;
                continue;
            }
            // scroll down columns, which are still visible
            if (y_lookup[i] < SCREENHEIGHT)
            {
                byte *s, *d;
                int j, dy;

                /* cph 2001/07/29 -
                 *  The original melt rate was 8 pixels/sec, i.e. 25 frames to melt
                 *  the whole screen, so make the melt rate depend on SCREENHEIGHT
                 *  so it takes no longer in high res
                 */
                dy = (y_lookup[i] < 16) ? y_lookup[i] + 1 : SCREENHEIGHT / 25;
                // At most dy shall be so that the column is shifted by SCREENHEIGHT (i.e. just
                // invisible)
                if (y_lookup[i] + dy >= SCREENHEIGHT)
                    dy = SCREENHEIGHT - y_lookup[i];
                s = &displayData.displayFrameBuffer[displayData.workingBuffer][i] +
                    ((SCREENHEIGHT - dy - 1) * SCREENPITCH);
                d = &displayData.displayFrameBuffer[displayData.workingBuffer][i] +
                    ((SCREENHEIGHT - 1) * SCREENPITCH);
                // scroll down the column. Of course we need to copy from the bottom... up to
                // SCREENHEIGHT - yLookup - dy
                for (j = SCREENHEIGHT - y_lookup[i] - dy; j; j--)
                {
                    *d = *s;
                    d += -SCREENPITCH;
                    s += -SCREENPITCH;
                }
                // copy new screen. We need to copy only between y_lookup and + dy y_lookup
                s = &displayData.displayFrameBuffer[1 - displayData.workingBuffer][i] +  y_lookup[i] * SCREENPITCH; 
                d = &displayData.displayFrameBuffer[displayData.workingBuffer][i] + y_lookup[i] * SCREENPITCH;
                for (j = 0 ; j < dy; j++)
                {
                    *d = *s;
                    d += SCREENPITCH;
                    s += SCREENPITCH;
                }
                y_lookup[i] += dy;
                done = false;
            }
        }
    }
    return done;
}

void wipe_initMelt(short *y_lookup)
{
    // setup initial column positions (y<0 => not ready to scroll yet)
    y_lookup[0] = -(M_Random() % 16);
    for (int i = 1; i < SCREENWIDTH; i++)
    {
        int r = (M_Random() % 3) - 1;
        y_lookup[i] = y_lookup[i - 1] + r;
        if (y_lookup[i] > 0)
            y_lookup[i] = 0;
        else if (y_lookup[i] == -16)
            y_lookup[i] = -15;
    }
}


int wipe_ScreenWipe(int ticks, short *y_lookup)
{
    // do a piece of wipe-in
    return wipe_doMelt(ticks, y_lookup);
}
