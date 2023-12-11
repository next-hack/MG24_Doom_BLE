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
 * next-hack: 16-bit pointers utility functions and memory definitions
 */

#ifndef DOOM_INCLUDE_I_MEMORY_H_
#define DOOM_INCLUDE_I_MEMORY_H_
#include <stdint.h>

#define RAM_PTR_BASE 0x20000000UL
#define EXT_FLASH_BASE 0x12000000
#define FLASH_PTR_BASE 0x08000000
//
#define FLASH_BLOCK_SIZE 8192
//
#define WAD_ADDRESS                           (EXT_FLASH_BASE + 4)
extern uint32_t __flashSize[];                // not really an array!
#define FLASH_CODE_SIZE                       ((uint32_t) __flashSize)
#if (FLASH_CODE_KB * 1024) & (FLASH_BLOCK_SIZE - 1)
#error FLASH_CODE_KB not multiple of 8kB
#endif
#define FLASH_ADDRESS                         (FLASH_PTR_BASE + FLASH_CODE_SIZE)
#define FLASH_IMMUTABLE_REGION_ADDRESS        FLASH_ADDRESS
#define FLASH_IMMUTABLE_REGION                0
#define FLASH_LEVEL_REGION                    1
#ifndef FLASH_SIZE
  #define FLASH_SIZE           (1536 * 1024)
#endif
#define FLASH_CACHE_REGION_SIZE ((FLASH_SIZE - FLASH_CODE_SIZE))

//
#define DEBUG_GETSHORTPTR 0
//
#define isOnExternalFlash(a) (((uint32_t) a & EXT_FLASH_BASE) == EXT_FLASH_BASE)
//
static inline void* getLongPtr(unsigned short shortPointer)
{ // Special case: short pointer being all 0 => NULL
    if (!shortPointer)
        return 0;
    return (void*) (((unsigned int) shortPointer << 2) | RAM_PTR_BASE);
}
static inline unsigned short getShortPtr(void *longPtr)
{
#if 0
    if ((uint32_t) longPtr & 0x2)
        printf("Error Long Ptr %p\r\n", longPtr);
#endif
    return ((unsigned int) longPtr) >> 2;
}
#endif /* DOOM_INCLUDE_I_MEMORY_H_ */
