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
 *      Zone Memory Allocation, perhaps NeXT ObjectiveC inspired.
 *      Remark: this was the only stuff that, according
 *       to John Carmack, might have been useful for
 *       Quake.
 *
 * Rewritten by Lee Killough, though, since it was not efficient enough.
 *
 * Restored back to its original form by Nicola Wrachien and optimized,
 * as it was not memory efficient enough (see zone.c).
 *
 *---------------------------------------------------------------------*/

#ifndef __Z_ZONE__
#define __Z_ZONE__

#ifndef __GNUC__
#define __attribute__(x)
#endif

// Include system definitions so that prototypes become
// active before macro replacements below are in effect.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "printf.h"
extern int32_t zone_critical;
extern uint8_t zone_critical_isr_mask;
static inline void ZONE_ENTER_CRITICAL(void)
{
  if (zone_critical == 0)
  {
    zone_critical_isr_mask = __get_PRIMASK();
    __set_PRIMASK(1);
  }
  zone_critical++;
}
static inline void ZONE_EXIT_CRITICAL(void)
{
  zone_critical--;
  if (zone_critical == 0)
  {
    __set_PRIMASK(zone_critical_isr_mask);
  }
  if (zone_critical < 0)
  {
    printf("Error!!! blocking");
    while(1);
  }
}
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// ZONE MEMORY
typedef struct memblock
{
    unsigned int next_sptr:16;
    unsigned int prev_sptr:16;
    union 
    {
        unsigned short user_spptr;  // short pointer to pointer.
        unsigned short nextPool_sptr;
    };
    unsigned short allocated:14;    // bitfield: allocated
   // unsigned short poolObjectWordSize:6;        // number of 32 bit words for each object. Max 63 * 4 bytes.
    unsigned short tag :2;
} memblock_t;

// PU - purge tags.

enum
{
    PU_FREE, PU_STATIC, /*PU_SOUND, PU_MUSIC,*/PU_LEVEL, PU_POOL, /*PU_CACHE,*/
    /* Must always be last -- killough */PU_MAX
};
#define PU_LEVSPEC PU_LEVEL
#define PU_PURGELEVEL PU_MAX        /* First purgable tag's level */

#define DA(x,y) 
#define DAC(x,y)

void* (Z_Malloc2)(uint32_t size, int tag, void **ptr, const char *sz DA(const char *, int));
void (Z_Free)(void *ptr DA(const char *, int));
void (Z_FreeTags)(int lowtag, int hightag DA(const char *, int));
void (Z_ChangeTag)(void *ptr, int tag DA(const char *, int));
void (Z_Init)(void);
void Z_Close(void);
void* (Z_Calloc)(size_t n, size_t n2, int tag, void **user DA(const char *, int));
void* (Z_Realloc)(void *p, size_t n, int tag, void **user DA(const char *, int));
char* (Z_Strdup)(const char *s, int tag, void **user DA(const char *, int));
void (Z_CheckHeap)(DAC(const char *,int)); // killough 3/22/98: add file/line info
void Z_DumpHistory(char*);
uint32_t getStaticZoneSize(void);

#define Z_Malloc(n, tag, u) Z_Malloc2(n, tag, u, TOSTRING(n) )
/* cphipps 2001/11/18 -
 * If we're using memory mapped file access to WADs, we won't need to maintain
 * our own heap. So we *could* let "normal" malloc users use the libc malloc
 * directly, for efficiency. Except we do need a wrapper to handle out of memory
 * errors... damn, ok, we'll leave it for now.
 */
#ifndef HAVE_LIBDMALLOC
// Remove all definitions before including system definitions
//
#undef malloc
#undef free
#undef realloc
#undef calloc
#undef strdup

#define malloc(n)          Z_Malloc(n,PU_STATIC,0)
#define free(p)            Z_Free(p)
#define realloc(p,n)       Z_Realloc(p,n,PU_STATIC,0)
#define calloc(n1,n2)      Z_Calloc(n1,n2,PU_STATIC,0)
#define strdup(s)          Z_Strdup(s,PU_STATIC,0)

#endif

void Z_ZoneHistory(char*);

#endif
