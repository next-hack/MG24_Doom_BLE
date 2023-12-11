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
 * This is designed to be a fast allocator for small, regularly used block sizes
 *
 * next-hack: modified to have low memory-overhead.
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomtype.h"
#include "z_zone.h"
#include "z_bmalloc.h"
#include "lprintf.h"
#include "i_memory.h"


__inline static void* getelem(memblock_t *p, size_t n, struct block_memory_alloc_s *pzone)
{
    return ((byte*) p + pzone->size * n + sizeof(memblock_t));
}

__inline static PUREFUNC int iselem(memblock_t *pool, const void *p, struct block_memory_alloc_s *pzone)
{
    int dif = (const char*) p - (const char*) pool;
    // remove memblock size
    dif -= sizeof(memblock_t);
    if (dif < 0)
        return -1;
    dif /= pzone->size;
    return (((size_t) dif >= pzone->perpool) ? -1 : dif);
}

enum
{
    unused_block = 0, used_block = 1
};
static void setBlockUsageStatus(memblock_t *pool, int n, int state)
{
    if (state == unused_block)
    {
        pool->allocated &= ~(1 << n);

    }
    else
    {
        pool->allocated |= (1 << n);
    }
}
static int checkForBlockUsageStatus(memblock_t *block, int state, struct block_memory_alloc_s *pzone)
{
    for (int i = 0; i < pzone->perpool; i++)
    {
        int bit = block->allocated & (1 << i);
        if ((state && bit) || (state == 0 && bit == 0))
        {
            return i;
        }
    }
    // not found
    return -1;
}
memblock_t * Z_Pool_Allocate(int size, int n)
{
    ZONE_ENTER_CRITICAL();
    uint8_t *m = Z_Calloc(n, size, PU_POOL, NULL);
    // get to the real memblock
    memblock_t * pool = (memblock_t*) (m - sizeof(memblock_t));
    // 
    pool->allocated = 0;
    //pool->poolObjectWordSize = size / 4;
    pool->nextPool_sptr = 0;
    ZONE_EXIT_CRITICAL();
    return pool;
}
void* Z_BMalloc(struct block_memory_alloc_s *pzone)
{
    ZONE_ENTER_CRITICAL();
    uint16_t *pool_spptr = &pzone->firstpool_sptr;
    while (*pool_spptr != 0)
    {
        memblock_t *pool = getLongPtr(*pool_spptr);
        int n = checkForBlockUsageStatus(pool, unused_block, pzone); // Scan for unused marker
        if (n != -1)
        {
            setBlockUsageStatus(pool, n, used_block);
            void * r = getelem(pool, n, pzone);
            ZONE_EXIT_CRITICAL();
            return r;
        }
        else
            pool_spptr = &pool->nextPool_sptr;
    }
    // found null. Need to allocate new one

    // Nothing available, must allocate a new pool
    memblock_t *newpool;
    // CPhipps: Allocate new memory, initialised to 0
    newpool = Z_Pool_Allocate(pzone->size, pzone->perpool);
    //
    *pool_spptr = getShortPtr(newpool);

    // Return element 0 from this pool to satisfy the request
    setBlockUsageStatus(newpool, 0, used_block);
    void *r =getelem(newpool, 0, pzone);
    ZONE_EXIT_CRITICAL();
    return r;
}

boolean Z_BFree(struct block_memory_alloc_s *pzone, void *p)
{
//    register bmalpool_t **pool = (bmalpool_t**) &(pzone->firstpool);
    //printf("Z_Bfree\r\n");
    ZONE_ENTER_CRITICAL();
    uint16_t *pool_spptr = &pzone->firstpool_sptr;
    //
    while (*pool_spptr != 0)
    {
        memblock_t *pool = getLongPtr(*pool_spptr);
        int n = iselem(pool, p, pzone);
        if (n >= 0)
        {
            setBlockUsageStatus(pool, n, unused_block);
            int m = checkForBlockUsageStatus(pool, used_block, pzone);
            //printf("Element was %d, now first unused is %d\r\n", n, m);
            if (-1 == m)
            {
                // Block is all unused, can be freed
                *pool_spptr = pool->nextPool_sptr;
                //printf("---->>>> ZBfree made Z_Free\r\n");
                // look at the mess! 
                Z_Free((uint8_t*) pool + sizeof(memblock_t));
            }
            ZONE_EXIT_CRITICAL();
            return true;
        }
        else
        {
            pool_spptr = &pool->nextPool_sptr;
            //printf("ZBmall: element is NOT part\r\n");
        }
    }
    ZONE_EXIT_CRITICAL();
    return false;
}
