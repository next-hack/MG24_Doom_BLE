/* Utilities functions for Doom port to microcontrollers by Nicola Wrachien
 * Copyright 2021-2022-2023 Nicola Wrachien
 */
#ifndef UTILITY_FUNCTIONS_H
#define UTILITY_FUNCTIONS_H
#include "global_data.h"
#include "p_mobj.h"
#include "r_main.h"
#include "i_memory.h"
extern globals_t *_g;
extern short *sectorLineIndexes;

static inline subsector_t* getMobjSubsector(mobj_t *pmobj)
{
    return &_g->subsectors[pmobj->subsector_num];
}
static inline subsector_t* setMobjSubesctor(mobj_t *pmobj, subsector_t *ss)
{
  // don't be fooled by the division. The subsector_t is 32 bytes, so the compiler will translate to a right shift by 2.
    pmobj->subsector_num = ((uint8_t*) ss - (uint8_t*)_g->subsectors) / sizeof(subsector_t);
    return ss;
}
static inline ramSector_t* getRamSector(const sector_t *ps)
{
    int index = ps->sectorNumber;
    return &_g->ramsectors[index];
}
static inline const struct line_s* getSectorLineByIndex(sector_t *psec, int i)
{
    return &_g->lines[sectorLineIndexes[psec->lineStartIndex + i]];
}
static inline mobj_t* getSectorThingList(sector_t *psec)
{
    return (mobj_t*) getLongPtr(getRamSector(psec)->thinglist_sptr);
}
static inline void* getSectorCeilingData(sector_t *psec)
{
    return (void*) getLongPtr(getRamSector(psec)->ceilingdata_sptr);
}
static inline void* getSectorFloorData(sector_t *psec)
{
    return (void*) getLongPtr(getRamSector(psec)->floordata_sptr);
}
/*
static inline sector_t* getSectorPtr(unsigned short shortPointer)
{
    return (sector_t*) getLongPtr(shortPointer);
}*/
#if USE_MSECNODE
static inline msecnode_t* getTouchingSectorList(mobj_t *pmobj)
{
    return (msecnode_t*) getLongPtr(pmobj->touching_sectorlist_sptr);
}
#endif
static inline mobj_t* getSectorSoundTarget(sector_t *ps)
{
    return (mobj_t*) getLongPtr(getRamSector(ps)->soundtarget_sptr);
}
static inline sector_t* getFloorMoveSector(floormove_t *p_fm)
{
    return &_g->sectors[p_fm->sector_number];
}
static inline ramSector_t* getFloorMoveRamSector(floormove_t *p_fm)
{
    return &_g->ramsectors[p_fm->sector_number];
}
static inline fixed_t getTextureHeight(short texture)
{
    return textures[texture]->height << FRACBITS;
}

static inline struct player_s* getMobjPlayer(mobj_t *pmobj)
{
    if (pmobj->type ==  MT_PLAYER)
    {
      // 2023-03-12 next-hack: to properly render player colors.
      if (pmobj->player_n < MAXPLAYERS)
        return &_g->players[pmobj->player_n];
      else
        return NULL;
    }
    else
        return NULL;
}
static inline uint32_t  getBitMask32(uint32_t *array, int line)
{
    return (array[line / 32] & (1 << (line % 32))) ? 1 : 0;
}
static inline void  setBitMask32(uint32_t *array, int line)
{
    array[line / 32] |= (1 << (line % 32));
}
static inline void  setBitMask32Value(uint32_t *array, int n, int value)
{
    if (value)
        array[n / 32] |= (1 << (n % 32));
    else
        array[n / 32] &= ~(1 << (n % 32));
}

static inline void  clrBitMask32(uint32_t *array, int line)
{
    array[line / 32] &= ~(1 << (line % 32));
}

static inline void clearArray32(uint32_t *array, int num)
{
    for (int i = 0; i < num; i++)
        *array++ = 0;
}
static inline void* getThinkerFunction(thinker_t *t)
{  
   return (void *) thinkerFunctions[t->function_idx];
}

static inline fixed_t getMobjX(mobj_t *pmobj)
{
    uint32_t flags = getMobjFlags(pmobj);
    if (!(flags & MF_STATIC))
    {
        // non static use x position in full mobj_t
        return pmobj->x;
    }
    else if (!(flags & MF_DROPPED))
    {
        // static not dropped? read from flash
        return _g->full_static_mobj_xy_and_type_values[pmobj->pos_index].x << FRACBITS; 
    }
    else
    {
        dropped_xy_t *dp = (dropped_xy_t*) getLongPtr(pmobj->dropped_xy_sptr);  
        return dp->x;
    }
} 
static inline fixed_t getMobjY(mobj_t *pmobj)
{
    uint32_t flags = getMobjFlags(pmobj);
    if (!(flags & MF_STATIC))
    {
        // non static use x position in full mobj_t
        return pmobj->y;
    }
    else if (!(flags & MF_DROPPED))
    {
        // static not dropped? read from flash
        return _g->full_static_mobj_xy_and_type_values[pmobj->pos_index].y << FRACBITS; 
    }
    else
    {
        dropped_xy_t *dp = (dropped_xy_t*) getLongPtr(pmobj->dropped_xy_sptr);  
        return dp->y;
    }
}
static inline fixed_t getMobjZ(mobj_t *pmobj)
{
  return FIXED_Z_TO_FIXED32(pmobj->zr);
}
// Arm specific.
static inline void fast32BytesCopy(void *dst, void *src, uint32_t numberOf32Bytes)
{
  register void *r0 __asm("r0") = dst;
  register void *r1 __asm("r1") = src;
  register uint32_t r2 __asm("r2") = numberOf32Bytes;
  __asm volatile
  (
    // copy 256 bytes = 32 32-bit words
    "PUSH {r0-r10}\n\t"
    "copyLoop%=:\r\n"
    "LDMIA r1!,{r3-r10}\n\t"
    "STMIA r0!, {r3-r10}\n\t"
    "SUBS r2, #1\n\t"
    "BNE copyLoop%=\n\t"
    "POP {r0-r10}\n\t"
      :
      : "l" (r0), "l" (r1), "l" (r2) : "cc"
  );
}

#endif
