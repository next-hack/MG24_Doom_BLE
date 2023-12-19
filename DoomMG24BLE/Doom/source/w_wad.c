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
 *      Handles WAD file header, directory, lump I/O.
 *
 * next-hack:
 * Added code to load data from external flash.
 * Added code to store/cache some wad data (And more) to internal flash.
 * This latter might be moved sooner or later to a better place.
 *-----------------------------------------------------------------------------
 */

// use config.h if autoconf made one -- josh
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>
#include "em_device.h"
#include "doomstat.h"
#include "d_net.h"
#include "doomtype.h"
#include "i_system.h"

#include "doom_iwad.h"

#include "w_wad.h"
#include "lprintf.h"

#include "global_data.h"
#include <stdint.h>
#include "extMemory.h"
#include "main.h"
#include "st_gfx.h"
#include "graphics.h"
#include "em_msc.h"
#define FLASH_ALIGNMENT 4
/* Flash organization
 * After xxk, 8k Aligned
 * (4-byte word alignment)
 * Wad CRC      -  used to determine, together with length, if the Wad has been changed.
 * Wad Length   -
 * Wad immutableData CRC  - To determine if the cache is ok, to speed up boot time.
 * wad immutableDataLength
 * Wad Number Of Lumps
 * Address to Lump Offset Table
 * Address to Lump Lengths Table
 * Number of Sprite
 * Address to Sprite Def table
 * Address to level table
 *
 * Lump Offset Table
 * Lump Offset Length
 *
 *
 *
 * Level Table Address: (origin 8kB alignd)
 * Cached addresses
 *
 */
#define getNumberOfLumps() (_g->numlumps)
uint32_t currentImmutableFlashOffset = 0;
uint32_t currentLevelFlashOffset = 0;
wad_immutable_flash_data_t *p_wad_immutable_flash_data;
wad_level_flash_data_t *p_wad_level_flash_data;
void **lumpPtrArray;
boolean lumpPtrArrayStoredInFlash = false;
void setCurrentImmutableFlashOffset(uint32_t offset)
{
    currentImmutableFlashOffset = offset;
}

void** storeLumpArrayToFlash(boolean levelChanged)
{
    lumpPtrArrayStoredInFlash = true;
    void **oldLumpPtrArrayAddress = lumpPtrArray;
    if (levelChanged)
    {
        lumpPtrArray = writeBufferToFlashRegion(lumpPtrArray, getNumberOfLumps() * sizeof(void*), FLASH_LEVEL_REGION, true);
        printf("changed, updated lumpAddressTable 0x%x\r\n", lumpPtrArray);
    }
    else
    {  // do not reload
        lumpPtrArray = p_wad_immutable_flash_data->levelData->lumpAddressTable;
        printf("level not changed, retrieving old lumpAddressTable 0x%x\r\n", lumpPtrArray);
    }
    Z_Free(oldLumpPtrArrayAddress);
    return lumpPtrArray;
}
#if CACHE_ALL_COLORMAP_TO_RAM
uint8_t ramColorMap[256 * 34]; // this gives only marginal gain: about 500 us per frame on average.
#endif



void initImmutableFlashRegion()
{
    // use a temporary region in RAM
    p_wad_immutable_flash_data = Z_Malloc(sizeof(*p_wad_immutable_flash_data), PU_STATIC, NULL);
    extMemSetCurrentAddress(EXT_FLASH_BASE);         // just to init SPI address
    // create temporary lumpptr
    lumpPtrArrayStoredInFlash = false;
    lumpPtrArray = NULL; // this will force all cache functions to look everytime from the external flash
    // get wad size
    currentImmutableFlashOffset = 0;
    extMemSetCurrentAddress((uint32_t) p_doom_iwad_len);
    extMemGetDataFromCurrentAddress(&p_wad_immutable_flash_data->wadSize, 4);
    extMemSetCurrentAddress((uint32_t) doom_iwad);
    // get wad header (includes also number of lumps and lump table offset)
    extMemGetDataFromCurrentAddress(&p_wad_immutable_flash_data->wadHeader, sizeof(p_wad_immutable_flash_data->wadHeader));
    //
    // set size of this structure
    p_wad_immutable_flash_data->immutableDataLength = sizeof(wad_immutable_flash_data_t);
    currentImmutableFlashOffset = sizeof(wad_immutable_flash_data_t);
    //
    cacheLumpNamesToFlash();
    // get the palette
    p_wad_immutable_flash_data->palette_lump = writeLumpToFlashRegion(W_GetNumForName("PLAYPAL"), FLASH_IMMUTABLE_REGION, true);
    // get the colormap
    p_wad_immutable_flash_data->colormaps = writeLumpToFlashRegion(W_GetNumForName("COLORMAP"), FLASH_IMMUTABLE_REGION, true);
    //
#if SCREENWIDTH == 320
    // if we are running on 320 x 200, then load STBAR. STBAR is actually a patch, so we need to draw it on a buffer
    // and then store it to flash.
    // we have a big buffer, let's write there.
    _g->screens[0].data = displayData.displayFrameBuffer[0];
    draw_stopy = SCREENHEIGHT - 1;
    draw_starty = 0;

    V_DrawPatchNoScale(0, 0 , W_CacheLumpName("STBAR"));
    gfx_stbar = writeBufferToFlashRegion(_g->screens[0].data, SCREENWIDTH * 32, FLASH_IMMUTABLE_REGION, true);
#endif
    //
    #if CACHE_ALL_COLORMAP_TO_RAM
        memcpy (ramColorMap, p_wad_immutable_flash_data->colormaps, sizeof (ramColorMap));
        p_wad_immutable_flash_data->colormaps = ramColorMap;
    #endif
}
wad_level_flash_data_t* initLevelFlashRegion()
{
    p_wad_level_flash_data = Z_Malloc(sizeof(*p_wad_level_flash_data), PU_STATIC, NULL);
    memset(p_wad_level_flash_data, 0, sizeof(*p_wad_level_flash_data));
    currentLevelFlashOffset = sizeof(*p_wad_level_flash_data);
    return p_wad_level_flash_data;
}
void initLumpPtrTable(void)
{
    printf("Init ram lump table\r\n");
    lumpPtrArray = Z_Malloc(getNumberOfLumps() * sizeof(void*), PU_STATIC, NULL);
    // init to 0xFF
    memset(lumpPtrArray, 0xFF, getNumberOfLumps() * sizeof(void*));
    //
    lumpPtrArrayStoredInFlash = false;
    // update - just for sake of completeness, but we actually won't use it - colormaps and playpal lumps
    lumpPtrArray[W_GetNumForName("COLORMAP")] = p_wad_immutable_flash_data->colormaps;
    lumpPtrArray[W_GetNumForName("PLAYPAL")] = p_wad_immutable_flash_data->palette_lump;
    //
}
void getFileLumpByNum(int n, filelump_t *fl)
{
    extMemSetCurrentAddress(WAD_ADDRESS + p_wad_immutable_flash_data->wadHeader.infotableofs + n * sizeof(filelump_t));
    extMemGetDataFromCurrentAddress(fl, sizeof(filelump_t));
}
// get how much space we have in flash, taking into account if we already stored the lump pointer table or not.
uint32_t getUserFlashRegionRemainingSpace()
{
    int size = FLASH_CACHE_REGION_SIZE - (currentLevelFlashOffset + ((currentImmutableFlashOffset + FLASH_BLOCK_SIZE - 1) & ~(FLASH_BLOCK_SIZE - 1)));
    // we need to take into account that we will need to store also the lumpPtrArray.
    if (lumpPtrArrayStoredInFlash == 0)
        size -= getNumberOfLumps() * sizeof(void*);
    if (size < 0)
        return 0;
    return (uint32_t) size;
}

__attribute__ ((section(".ram"), noinline)) void flashEraseCurrentPage()
{
    MSC->WRITECMD = MSC_WRITECMD_ERASEPAGE;
    while (!(MSC->IF & MSC_IF_ERASE));    // wait until erase operations is done
}
void flashErasePage(uint32_t *pageAddress)
{
    __disable_irq();
    MSC_ErasePage(pageAddress);
    __enable_irq();
}
void programFlashWord(uint32_t *address, uint32_t word)
{
    if (*address == 0xFFFFFFFF)
    {
        __disable_irq();
        MSC_WriteWord(address, &word, 4 );
        __enable_irq();
    }
}
/**
 * @brief Store a word to flash. It checks if erase must be triggered.
 *        In such a case, it will copy also the previous header values, and the
 *        data PAST the header region.
 * @param dest
 * @param word
 * @param flashRegion
 * @param isHeader
 */
#define PROCESS_ACTION_TIME_MICROSEC    100000UL
void storeWordToFlash(uint32_t *dest, uint32_t word, uint8_t flashRegion, boolean isHeader)
{
    static uint32_t lastProcessActionTime_us = 0;
    uint32_t timeNow = I_GetTimeMicrosecs();
    if (timeNow - lastProcessActionTime_us > PROCESS_ACTION_TIME_MICROSEC)
    {
      sl_system_process_action();
      lastProcessActionTime_us = I_GetTimeMicrosecs();
    }
    // first check if dest is compatible, i.e. is the same as source or it is erased
    if (!(*dest == word || *dest == 0xFFFFFFFF))
    {
        // we need to erase before writing.
        // if we are writing the header (immutable or level) we might need to copy the data AFTER the header
        // as well. However, typically this is not required, because if we change level, we are also going
        // to change the data of the first page, which contains the level data header. This would result
        // in all the words of the level data header to be automatically 0xFFFFFFFF if level changes.
        // This might not be true, however, for immutable flash data, when we change build config or WAD.
        uint32_t pageAddress = ((uint32_t) dest) & ~(FLASH_BLOCK_SIZE - 1); // page that will be erased
        uint32_t startCopyAddress = pageAddress;
        uint32_t stopCopyAddress = (uint32_t) dest; // copy up to the previous word.
        uint32_t *upperDataBuffer = NULL;
        uint32_t headerSize = (pageAddress == FLASH_IMMUTABLE_REGION_ADDRESS) ? sizeof(wad_immutable_flash_data_t) : sizeof(wad_level_flash_data_t);
        // if we are writing in the same page as header, and it is not a header, then we need to skip the first bytes that will be used by the header
        if (pageAddress == FLASH_IMMUTABLE_REGION_ADDRESS && !isHeader)
        {
            startCopyAddress = pageAddress + sizeof(wad_immutable_flash_data_t);
        }
        else if (flashRegion == FLASH_LEVEL_REGION && pageAddress == (uint32_t) p_wad_immutable_flash_data->levelData && !isHeader)
        {
            startCopyAddress = pageAddress + sizeof(wad_level_flash_data_t);
        }
        // now that we know from where to where (stop non inclusive) we need to copy, we can allocate enough buffer
        // note! addresses are 4-byte aligned!
        uint32_t *tmp = Z_Malloc((stopCopyAddress - startCopyAddress), PU_STATIC, NULL);
        for (uint32_t i = 0; i < (stopCopyAddress - startCopyAddress) / sizeof(uint32_t);
                i++)
        {
            tmp[i] = *((uint32_t*) startCopyAddress + i);
        }
        if (isHeader)
        {
          upperDataBuffer = Z_Malloc(FLASH_BLOCK_SIZE - headerSize, PU_STATIC, NULL);
          // copy old data on the page, after the header
          for (unsigned int i = headerSize / sizeof(uint32_t); i < FLASH_BLOCK_SIZE / sizeof (uint32_t); i++)
          {
            upperDataBuffer[i - headerSize / sizeof(uint32_t)] = *((uint32_t *) pageAddress + i);
          }
        }
        // erase
        printf("Erasing page 0x%08X Start Copy Addr 0x%08X Stop Copy Addr 0x%08X\r\n. ", pageAddress, startCopyAddress, stopCopyAddress);
        flashErasePage((uint32_t*) pageAddress);
        // write back everything
        for (uint32_t i = 0; i < (stopCopyAddress - startCopyAddress) / sizeof(uint32_t);
                i++)
        {
            programFlashWord(((uint32_t*) startCopyAddress + i), tmp[i]);
        }
        // was it a header? Then after it there might have been some data that should be restored!
        if (isHeader)
        {
          uint32_t *dst = (uint32_t*) pageAddress;
          for (uint32_t i = headerSize / sizeof(uint32_t); i < FLASH_BLOCK_SIZE / sizeof(uint32_t); i++)
          {
            programFlashWord(dst + i, upperDataBuffer[i - headerSize / sizeof(uint32_t)]);
          }
          // free the data buffer
          Z_Free(upperDataBuffer);
        }
        // free buffer
        Z_Free(tmp);
    }
    // write word
    if (!(*dest == word))
    {
        if (*dest == 0xFFFFFFFF)
            programFlashWord(dest, word);
        else
        {
            printf("trying to overwrite a non blank word: addr: 0x%08X old: 0x%08X new 0x%08X. Is Header: %d flash Region %d Blocking", dest, *dest, word, isHeader, flashRegion);
            while (1);
        }
    }
    //
}
/**
 * @brief write the buffer to the flash region
 */

void* writeBufferToFlashRegion(void *buffer, int size, uint8_t flashRegion, boolean updateSize)
{
    // todo check and eventually erase
    uint32_t *address;
    size = (size + FLASH_ALIGNMENT - 1) & ~(FLASH_ALIGNMENT - 1);
    if (flashRegion == FLASH_IMMUTABLE_REGION)
    {
        address = (uint32_t*) (FLASH_ADDRESS + currentImmutableFlashOffset);
    }
    else
    {
        address = (uint32_t*) (FLASH_ADDRESS + currentLevelFlashOffset + ((currentImmutableFlashOffset + FLASH_BLOCK_SIZE - 1) & ~(FLASH_BLOCK_SIZE - 1)));
    }
    uint32_t *s = buffer;
    uint32_t *d = address;
    for (int i = 0; i < size / 4; i++)
    {
        storeWordToFlash(d++, *s++, flashRegion, false);
    }
    if (flashRegion == FLASH_IMMUTABLE_REGION)
    {
        currentImmutableFlashOffset += size;
    }
    else
    {
        currentLevelFlashOffset += size;
    }

    if (updateSize)
    {
        if (flashRegion == FLASH_IMMUTABLE_REGION)
        {
            p_wad_immutable_flash_data->immutableDataLength += size;
        }
        else
        {
            p_wad_level_flash_data->dataLength += size;
        }

    }
    return address;
}

void updateLumpAddresses(int lump, void *address)
{
    if (lumpPtrArray)
    {
        if (lumpPtrArrayStoredInFlash)
        {
            printf("Updating lump %d, address 0x%08X\r\n", lump, address);
            if ((uint32_t) lumpPtrArray[lump] != 0xFFFFFFFF)
            {
                if (lumpPtrArray[lump] == address)
                    printf("Same lump address stored\r\n");
                else
                {
                    printf("Different lump address! old: 0x%08X new: 0x%08X. Blocking \r\n", lumpPtrArray[lump], address);
                    while (1);
                }
            }
            else
            {
                printf("Programming\r\n");
                programFlashWord((uint32_t*) &lumpPtrArray[lump], (uint32_t) address);
            }
            printf("Value now in Flash: 0x%08X\r\n", lumpPtrArray[lump]);
        }
        else
            lumpPtrArray[lump] = address;
    }
}
void* writeLumpToFlashRegion(int lumpnum, uint8_t flashRegion, boolean updateSize)
{
    uint32_t *address = 0;
    filelump_t fl;
    if (lumpnum == -1)
    {
      printf("Trying to store negative lump, blocking");
      while(1);
    }
    getFileLumpByNum(lumpnum, &fl);
    // adjust to boundary
    fl.size = (fl.size + FLASH_ALIGNMENT - 1) & ~(FLASH_ALIGNMENT - 1);

    // will it fit in flash?
    if (fl.size > getUserFlashRegionRemainingSpace() && flashRegion != FLASH_IMMUTABLE_REGION)
    {
        address = (uint32_t*) (WAD_ADDRESS + fl.filepos);
        updateLumpAddresses(lumpnum, address);
        return address;
    }
    extMemSetCurrentAddress(WAD_ADDRESS + fl.filepos);
    for (unsigned int i = 0; i < fl.size / 4; i++)
    {
        uint32_t data;
        extMemGetDataFromCurrentAddress(&data, 4);
        uint32_t *a = writeBufferToFlashRegion(&data, 4, flashRegion, updateSize);
        if (i == 0)
        {
            address = a;
        }
    }
    updateLumpAddresses(lumpnum, address);
    return address;
}
/*
 * @brief stores to flash the wad_immutable_flash_data_t.
 * This function shall be called after all immutable data have been stored.
 * This also updates the position of where level data will start
 * The function will take care of copying data on the same flash page, after
 * the wad_immutable_flash_data_t structure.
 * @return the address where it has been stored.
 */
void* storeImmutableDataHeader()
{
    // store header
    uint32_t *src = (uint32_t*) p_wad_immutable_flash_data;
    // update pointer to where the level data are going to be stored
    p_wad_immutable_flash_data->levelData = (wad_level_flash_data_t*) ((uint8_t*) FLASH_ADDRESS + ((p_wad_immutable_flash_data->immutableDataLength + FLASH_BLOCK_SIZE - 1) & ~(FLASH_BLOCK_SIZE - 1)));
    //
    for (unsigned int i = 0; i < sizeof(*p_wad_immutable_flash_data) / sizeof(uint32_t); i++)
    {
        storeWordToFlash((uint32_t*) FLASH_ADDRESS + i, src[i], FLASH_IMMUTABLE_REGION, true);
    }
    // free old immutable flash data temporary ram buffer, and point p_wad_immutable_flash_data to flash.
    Z_Free(p_wad_immutable_flash_data);
    p_wad_immutable_flash_data = (void*) FLASH_ADDRESS;
    return (uint32_t*) FLASH_ADDRESS;
}
/**
 * @brief This function stores the level flash data header.
 * This function should be called after all the level data have been stored.
 * This function will also free the temporary buffer for level data header, and \
 * will point p_wad_level_flash_data to internal flash.
 * @param levelChanged
 * @return the new address that should be assigned to p_wad_level_flash_data.
 */
void* storeLevelDataHeader(boolean levelChanged)
{
    if (levelChanged)
    {
        // store header
        uint32_t *src = (uint32_t*) p_wad_level_flash_data;
        for (unsigned int i = 0; i < sizeof(*p_wad_level_flash_data) / sizeof(uint32_t);
                i++)
        {
            storeWordToFlash((uint32_t*) p_wad_immutable_flash_data->levelData + i, src[i], FLASH_LEVEL_REGION, true);
        }
        printf("Storing level values\r\n");
    }
    else
    {
      printf("NOT Storing level values\r\n");
    }
    printf("Freeing p_wad_level_flash_data %0x\r\n", (uint32_t) p_wad_level_flash_data);
    Z_Free(p_wad_level_flash_data);
    p_wad_level_flash_data = p_wad_immutable_flash_data->levelData;
    return (uint32_t*) p_wad_level_flash_data;
}
//
// GLOBALS
//
void ExtractFileBase(const char *path, char *dest)
{
    const char *src = path + strlen(path) - 1;
    int length;

    // back up until a \ or the start
    while (src != path && src[-1] != ':' // killough 3/22/98: allow c:filename
    && *(src - 1) != '\\' && *(src - 1) != '/')
    {
        src--;
    }

    // copy up to eight characters
    memset(dest, 0, 8);
    length = 0;

    while ((*src) && (*src != '.') && (++length < 9))
    {
        *dest++ = toupper(*src);
        src++;  //nh fixed, it was *src++.
    }
    /* cph - length check removed, just truncate at 8 chars.
     * If there are 8 or more chars, we'll copy 8, and no zero termination
     */
}

//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
//
// Reload hack removed by Lee Killough
// CPhipps - source is an enum
//
// proff - changed using pointer to wadfile_info_t
static void W_AddFile()
{

    wadinfo_t header;
    extMemSetCurrentAddress((uint32_t) p_doom_iwad_len);
    uint32_t length;
    extMemGetDataFromCurrentAddress(&length, sizeof(length));
    extMemSetCurrentAddress((uint32_t) doom_iwad);
    if (length)
    {
        extMemGetDataFromCurrentAddress(&header, sizeof(header));
        if (strncmp(header.identification, "IWAD", 4))
            I_Error("W_AddFile: Wad file doesn't have IWAD id");

        _g->numlumps = header.numlumps;
    }
}

//Return -1 if not found.
//Set lump ptr if found.
int lumpByNameRequest;

static const filelump_t* PUREFUNC FindLumpByNum(int num)
{
    wadinfo_t header;
    if (num < 0)
        return NULL;
    extMemSetCurrentAddress((uint32_t) p_doom_iwad_len);
    uint32_t wadLength;
    extMemGetDataFromCurrentAddress(&wadLength, sizeof(uint32_t));
    if (wadLength)
    {
        extMemSetCurrentAddress((uint32_t) doom_iwad);
        extMemGetDataFromCurrentAddress(&header, sizeof(header));

        if (num >= header.numlumps)
        {
            return NULL;
        }
        return (filelump_t*) (WAD_ADDRESS + header.infotableofs + num * sizeof(filelump_t));
    }
    return NULL;
}

/**
 * @brief Given the wad, all the names are cached to flash.
 */
uint8_t *lumpNames;
void cacheLumpNamesToFlash(void)
{
  uint32_t wadsize;
  uint32_t infotableofs;
  const wadinfo_t *header = (const wadinfo_t*) WAD_ADDRESS;
  extMemSetCurrentAddress((uint32_t) p_doom_iwad_len);
  extMemGetDataFromCurrentAddress(&wadsize, sizeof(wadsize));
  //
  extMemSetCurrentAddress((uint32_t) &header->infotableofs);
  extMemGetDataFromCurrentAddress(&infotableofs, sizeof(infotableofs));
  //
  lumpNames = Z_Calloc(8, _g->numlumps, PU_STATIC, NULL);
  //
  for (int i = 0; i < _g->numlumps; i++)
  {
      extMemSetCurrentAddress(infotableofs + WAD_ADDRESS + sizeof(filelump_t) * i  + offsetof(filelump_t, name));
      extMemGetDataFromCurrentAddress(lumpNames + 8 * i, 8);

  }
  uint8_t *oldLumpNames = lumpNames;
  // store to flash
  lumpNames = writeBufferToFlashRegion(lumpNames, 8 * _g->numlumps, FLASH_IMMUTABLE_REGION, true);
  // free data
  Z_Free(oldLumpNames);
}
//
// W_CheckNumForName
// Returns -1 if name not found.
//
// Rewritten by Lee Killough to use hash table for performance. Significantly
// cuts down on time -- increases Doom performance over 300%. This is the
// single most important optimization of the original Doom sources, because
// lump name lookup is used so often, and the original Doom used a sequential
// search. For large wads with > 1000 lumps this meant an average of over
// 500 were probed during every search. Now the average is under 2 probes per
// search. There is no significant benefit to packing the names into longwords
// with this new hashing algorithm, because the work to do the packing is
// just as much work as simply doing the string comparisons with the new
// algorithm, which minimizes the expected number of comparisons to under 2.
//
// killough 4/17/98: add namespace parameter to prevent collisions
// between different resources such as flats, sprites, colormaps
//
// next-hack: linear search but decent enough speed even under menu (i.e. when names are actually used)
int PUREFUNC W_CheckNumForName(const char *name)
{
    int_64_t nameint = 0;
    strncpy((char*) &nameint, name, 8);
    for (int i = 0; i < _g->numlumps; i++)
    {
        int_64_t nameint2 = *(int_64_t *) (lumpNames + 8 * i);
        if (nameint == nameint2)
        {
            return i;
        }
    }
    return -1;
}

// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int PUREFUNC W_GetNumForName(const char *name)     // killough -- const added
{
    int i = W_CheckNumForName(name);

    if (i == -1)
        I_Error("W_GetNumForName: %.8s not found", name);

    return i;
}

const char* PUREFUNC W_GetNameForNum(int lump)
{
    const filelump_t *l = FindLumpByNum(lump);
    if (l)
    {
        return l->name;
    }

    return NULL;
}

// W_Init
// Loads each of the files in the wadfiles array.
// All files are optional, but at least one file
//  must be found.
// Files with a .wad extension are idlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file
//  does override all earlier ones.
//
// CPhipps - modified to use the new wadfiles array
//

void W_Init(void)
{
    // CPhipps - start with nothing

    _g->numlumps = 0;

    W_AddFile();

    //
    if (!_g->numlumps)
    {
        I_Error("W_Init: No files found, blocking");
        while(1);
    }
    //
}

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//

int PUREFUNC W_LumpLength(int lump)
{
    const filelump_t *l = FindLumpByNum(lump);

    if (l)
    {
        extMemSetCurrentAddress((uint32_t) &l->size);
        int size;
        extMemGetDataFromCurrentAddress(&size, sizeof(size));
        return size;
    }

    I_Error("W_LumpLength: %i >= numlumps", lump);

    return 0;
}

static const void* PUREFUNC W_GetLumpPtr(int lump)
{
    const filelump_t *l = FindLumpByNum(lump);

    if (l)
    {
        extMemSetCurrentAddress((uint32_t) &l->filepos);
        int filepos;
        extMemGetDataFromCurrentAddress(&filepos, sizeof(filepos));
        return (const uint8_t*) WAD_ADDRESS + filepos;
    }
    return NULL;
}
void* getAddressOrCacheLumpNum(int lump, boolean storeInFlash, uint8_t flashRegion)
{
//  printf("Get addr: %d 0x%08x 0x%08x Lump Array in Flash %s\r\n", lump, lumpPtrArray[lump], lumpPtrArray, lumpPtrArrayStoredInFlash ? "yes" : "no");
    if (lump > _g->numlumps)
    {
        printf("Attempt to load a non existent lump. %d Blocking\r\n", lump);
        while (1);
    }
    if (lumpPtrArray == NULL && !storeInFlash)
    {
        return (void*) W_GetLumpPtr(lump);
    }
    //
    if (lumpPtrArray[lump] != (void*) 0xFFFFFFFF && lumpPtrArray[lump])
        return lumpPtrArray[lump];

    void *address;
    if (storeInFlash)
    {
        printf("Storing lump %d in flash\r\n", lump);
        address = writeLumpToFlashRegion(lump, flashRegion, true);
    }
    else
    {
        address = (void*) W_GetLumpPtr(lump);
        updateLumpAddresses(lump, address);
    }
    return address;

}

const void* /*PUREFUNC*/W_CacheLumpNum(int lump)
{

    //return   W_GetLumpPtr(lump);
    // printf("Caching lump %d ... ", lump);
    void *addr = getAddressOrCacheLumpNum(lump, false, 0);
    // printf("cached to --> 0x%08x\r\n",  addr);
    return addr;
}
