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
 *      Fixed point arithemtics, implementation.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __M_FIXED__
#define __M_FIXED__

#include "config.h"
#include "doomtype.h"
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
// to count divisions per frame
extern int divisions;
/*
 * Fixed point, 32bit as 16.16.
 */

#define FRACBITS 16
#define FRACUNIT (1<<FRACBITS)

#define FRACBITS16 3
// required for z momentum
#define FRACBITS_MOMZ 8       
//
typedef int fixed_t;
typedef short fixed16_t;
typedef short fixed_momz_t;           // this is for MOMZ
#define FRAC16_MIN (sizeof (fixed16_t) == 2 ? SHRT_MIN : INT_MIN)
#define FRAC16_MAX (sizeof (fixed16_t) == 2 ? SHRT_MAX : INT_MAX)
//
#define FIXED16_TO_FIXED32(f) ((fixed_t)(((uint32_t) f) << (FRACBITS - FRACBITS16)))
#define FIXED32_TO_FIXED16(f) ((f) >> (FRACBITS - FRACBITS16))
//
#define FIXED_MOMZ_TO_FIXED32(f) ((f) << (FRACBITS - FRACBITS_MOMZ))
#define FIXED32_TO_FIXED_MOMZ(f) (((f) + (1 << (FRACBITS - FRACBITS_MOMZ - 1)) ) >> (FRACBITS - FRACBITS_MOMZ))
//
#define FIXED16_TO_FIXED_MOMZ(f) ((f) << (FRACBITS_MOMZ - FRACBITS16))
#define FIXED_MOMZ_TO_FIXED16(f) ((f) >> (FRACBITS_MOMZ - FRACBITS16))
//
#define FIXED_Z_TO_FIXED32(f) FIXED_MOMZ_TO_FIXED32(f)  // momentum and z have same precision
#define FIXED32_TO_FIXED_Z(f) FIXED32_TO_FIXED_MOMZ(f)
//
#define FIXED16_TO_FIXED_Z(f) FIXED16_TO_FIXED_MOMZ(f)
#define FIXED_Z_TO_FIXED16(f) FIXED_MOMZ_TO_FIXED16(f)
//
#define FIXED_MOMZ_TO_FIXED_Z(f) (f)
#define FIXED_Z_TO_FIXED_MOMZ(f) (f)
/*
 * Absolute Value
 *
 * killough 5/10/98: In djgpp, use inlined assembly for performance
 * killough 9/05/98: better code seems to be gotten from using inlined C
 * next-hack: standard library implements this function in 2 asm instructions.
 */

inline static int CONSTFUNC D_abs(fixed_t x)
{
    return abs(x);
    /*fixed_t _t = (x), _s;
    _s = _t >> (8 * sizeof _t - 1);
    return (_t ^ _s) - _s;*/
}

/*
 * Fixed Point Multiplication
 */

/* CPhipps - made __inline__ to inline, as specified in the gcc docs
 * Also made const */

fixed_t CONSTFUNC FixedMul(fixed_t a, fixed_t b);

/*
 * Fixed Point Division
 */

/* CPhipps - made __inline__ to inline, as specified in the gcc docs
 * Also made const */

inline static fixed_t CONSTFUNC FixedDiv(fixed_t a, fixed_t b)
{

    divisions++;
//    return ((unsigned)D_abs(a)>>14) >= (unsigned)D_abs(b) ? ((a^b)>>31) ^ INT_MAX : (fixed_t)(((int_64_t) a << FRACBITS) / b);
    // this is much faster as we have a FPU..
//    float r = ((float) a) / b;
//    return (fixed_t) (65536 * r);
    return ((unsigned)D_abs(a)>>14) >= (unsigned)D_abs(b) ? ((a^b)>>31) ^ INT_MAX : (fixed_t)(((int_64_t) a << FRACBITS) / b);

}

/* CPhipps -
 * FixedMod - returns a % b, guaranteeing 0<=a<b
 * (notice that the C standard for % does not guarantee this)
 */

inline static fixed_t CONSTFUNC FixedMod(fixed_t a, fixed_t b)
{
    divisions++;

    if (!a)
        return 0;

    if (b & (b - 1))
    {
        fixed_t r = a % b;
        return ((r < 0) ? r + b : r);
    }
    else
        return (a & (b - 1));
}

//Approx Reciprocal of v
inline static CONSTFUNC fixed_t FixedReciprocal(fixed_t v)
{
    divisions++;
    fixed_t result;
    // CORTEX M4 and M33 have FPU. Let's use it (we can't afford 64k of flash for reciprocals!) .
    if (0 == v)
        return 0;   // we should return infinity, but the table returned 0
    float r;
    r = 65536.0f / v;
    result = (fixed_t) (r * 65536.0f);
    return result;
}
//Count leading zeros. 16bit.
inline static CONSTFUNC unsigned int clz16(unsigned int x)
{
    return __builtin_clz(x);
}

//Approx fixed point divide of a/b using reciprocal. -> a * (1/b).
inline static CONSTFUNC fixed_t FixedApproxDiv(fixed_t a, fixed_t b)
{
    divisions++;

    float r = ((float) a) / b;
    return (fixed_t) (65536.0f * r);
//  return  FixedMul(a, FixedReciprocal(b));
}

#endif
