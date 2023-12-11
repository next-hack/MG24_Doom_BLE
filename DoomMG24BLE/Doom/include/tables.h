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
 * DESCRIPTION: (OUTDATED!)
 *      Lookup tables.
 *      Do not try to look them up :-).
 *      In the order of appearance:
 *
 *      int finetangent[4096]   - Tangens LUT.
 *       Should work with BAM fairly well (12 of 16bit,
 *      effectively, by shifting).
 *
 *      int finesine[10240]             - Sine lookup.
 *       Guess what, serves as cosine, too.
 *       Remarkable thing is, how to use BAMs with this?
 *
 *      int tantoangle[2049]    - ArcTan LUT,
 *        maps tan(angle) to angle fast. Gotta search.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __TABLES__
#define __TABLES__

#include "m_fixed.h"
#include "doomdef.h"
#include "main.h"
//
// Lighting constants.

#define LIGHTLEVELS       16
#define LIGHTSEGSHIFT      4
#define MAXLIGHTSCALE     48
#define LIGHTSCALESHIFT   12
#define MAXLIGHTZ        128
#define LIGHTZSHIFT       20

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.

#define NUMCOLORMAPS 32
//
#define TABULATED_VALUES  1 // use resolution-dependent tabulated values for some arrays.
#define FINEANGLES              8192
#define FINEMASK                (FINEANGLES-1)

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT        19

// Binary Angle Measument, BAM.
#define ANG45   0x20000000
#define ANG90   0x40000000
#define ANG180  0x80000000
#define ANG270  0xc0000000

#define FINEANGLE90 (FINEANGLES / 4)

#define SLOPERANGE 2048
#define SLOPEBITS    11
#define DBITS      (FRACBITS-SLOPEBITS)

#define SMALL_ANGLES 1
#if SMALL_ANGLES
    typedef unsigned short angle16_t;
    #define ANGLE16_TO_ANGLE32(a) ((unsigned int)((a) << 16))
    #define ANGLE32_TO_ANGLE16(a) ((unsigned short)((a) >> 16))
#else
    typedef unsigned int angle16_t;
    #define ANGLE16_TO_ANGLE32(a) ((a))
    #define ANGLE32_TO_ANGLE16(a) ((a))
#endif


// Load trig tables if needed
void R_LoadTrigTables(void);

// Effective size is 10240. // 2022/02/01 next-hack: better to save on flash... 
#if FAST_CPU_SMALL_FLASH
extern const uint16_t finesinetable[FINEANGLES / 4];
#if CORRECT_TABLE_ERROR
extern const uint8_t sineTableError[FINEANGLES / 4];
#endif
extern const int angleOffs[4];
#else
extern const fixed_t finesinetable[FINEANGLES + FINEANGLES / 4];
extern const fixed_t *const finecosinetable;

#endif
extern int trigOpsPerFrame;
// Re-use data, is just PI/2 phase shift.
static inline fixed_t finesine(unsigned int angle)
{
#if PROFILE_TRIGOPS
    trigOpsPerFrame++;
#endif
    #if FAST_CPU_SMALL_FLASH
        uint32_t errorAngle = angle;
        unsigned int index = angle / FINEANGLE90;
        int error = 0;
        #if CORRECT_TABLE_ERROR
            if (index)
                error = ((sineTableError[errorAngle / 4] >> ((errorAngle % 4 ) * 2)) & 3) - 1;
        #else
            error = 0;
        #endif
/*        switch (index)
        {
            case 0:  // 0...2047
                return (fixed_t) finesinetable[angle];  // no error here.
            case 1: // 2048...4095 --> 2047...0
                return (fixed_t) finesinetable[2 * FINEANGLE90 - 1 - angle ] + error;
            case 2: // 4096..6143 --> - sin(0...2047)
                return - ((fixed_t) finesinetable[angle - 2 * FINEANGLE90 ]) + error;
            case 3: // 6144..8191 --> - (sin(2047...0))
                return -((fixed_t) finesinetable[4 * FINEANGLE90 - 1 - angle]) + error;
        }*/
        angle += angleOffs[index];
        int signAngle = (index & 1) ? -1 : 1;
        int signSine = (index & 2) ? -1 : 1;
        return (fixed_t) signSine * finesinetable[signAngle * angle] + error;  
    #else
        return finesinetable[angle];
    #endif
}
static inline fixed_t finecosine( unsigned int angle)
{
 #if FAST_CPU_SMALL_FLASH
     #if CORRECT_TABLE_ERROR
        if (angle >=  FINEANGLES * 3 / 4)
        {
            uint32_t errorAngle = angle % (FINEANGLES / 4);
            int error = ((sineTableError[errorAngle / 4] >> ((errorAngle % 4 ) * 2)) & 3) - 1;
            return finesine((angle + (FINEANGLES / 4)) & (FINEANGLES - 1)) + error;
        }
        else
        {
            return finesine((angle + (FINEANGLES / 4)) & (FINEANGLES - 1));
        }

     #else
        return finesine((angle + (FINEANGLES / 4)) & (FINEANGLES - 1));
     #endif
 #else
    return finecosinetable[angle];
 #endif
}

#if !FAST_CPU_SMALL_FLASH
// Effective size is 4096.
extern const fixed_t finetangenttable[4096];
#endif
// Effective size is 2049;
// The +1 size is to handle the case when x==y without additional checking.
static inline fixed_t finetangent(unsigned int angle)
{
    #if FAST_CPU_SMALL_FLASH
        angle = (angle - FINEANGLE90) & (FINEANGLES - 1);
        return FixedApproxDiv(finesine(angle),finecosine(angle));
    #else
        return finetangenttable[angle];
    #endif
}

extern const angle_t tantoangle[2049];

#if TABULATED_VALUES
#if SCREENWIDTH < 256
    #define VIEWANGLE_TYPE byte
#else    
    #define VIEWANGLE_TYPE short
#endif
extern const VIEWANGLE_TYPE viewangletox[4096];
extern const angle_t xtoviewangle[SCREENWIDTH + 1];
extern const fixed_t yslope[SCREENHEIGHT];
extern const fixed_t distscale[SCREENWIDTH];
extern const uint8_t scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern const uint8_t zlight[LIGHTLEVELS][MAXLIGHTZ];
extern const unsigned char translationtables[3 * 256];
#else
extern int viewangletox[4096];
extern angle_t xtoviewangle[SCREENWIDTH + 1];
extern fixed_t yslope[SCREENHEIGHT];
extern fixed_t distscale[SCREENWIDTH];
#endif

extern const short screenheightarray[SCREENWIDTH_PHYSICAL];

extern const short negonearray[SCREENWIDTH_PHYSICAL];


#endif
