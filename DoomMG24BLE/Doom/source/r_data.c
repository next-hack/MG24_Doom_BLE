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
 *      Preparation of data for rendering,
 *      generation of lookups, caching, retrieval by name.
 *
 *  next-hack: code for caching texture and texture data to flash.
 *             modified load texture to support caching to flash.
 *             texture heights array stored to flash to save some hundreds bytes.
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "w_wad.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_sky.h"
#include "i_system.h"
#include "r_things.h"
#include "p_tick.h"
#include "lprintf.h"  // jff 08/03/98 - declaration of lprintf
#include "p_tick.h"

#include "global_data.h"
#include "extMemory.h"
//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//

//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//

typedef struct
{
    short originx;
    short originy;
    short patch;
    short stepdir;         // unused in Doom but might be used in Phase 2 Boom
    short colormap;        // unused in Doom but might be used in Phase 2 Boom
} PACKEDATTR mappatch_t;

typedef struct
{
    char name[8];
    char pad2[4];      // unused
    short width;
    short height;
    char pad[4];       // unused in Doom but might be used in Boom Phase 2
    short patchcount;
    mappatch_t patches[1];
} PACKEDATTR maptexture_t;
//
// this to suppress compiler warnings, as strupr is not always implemented.
//
static char* strupr(char *string)
{
    int i = 0;
    while (string[i] != 0)
    {
        if (string[i] >= 'a' && string[i] <= 'z')
        {
            string[i] = string[i] - 'a' + 'A';
        }
        i++;
    }
    return string;
}
/**
 * This will fill the textureheight array. This must be called when textureheight is in RAM,
 * i.e. only in the init function!
 */
static const texture_t* R_LoadTexture(int texture_num, boolean cacheToFlash, uint32_t *spaceRequired)
{
    uint32_t spaceReq = 0;
    if (spaceRequired == NULL)
        spaceRequired = &spaceReq;
    const byte *pnames = W_CacheLumpName("PNAMES");

    //Skip to list of names.
    pnames += 4;

    const int *maptex1, *maptex2;
    int numtextures1, numtextures2;
    const int *directory1, *directory2;

    maptex1 = W_CacheLumpName("TEXTURE1");
    extMemSetCurrentAddress((uint32_t) maptex1);
    extMemGetDataFromCurrentAddress(&numtextures1, sizeof(numtextures1));
    //numtextures1 = *maptex1;
    directory1 = maptex1 + 1;

    if (W_CheckNumForName("TEXTURE2") != -1)
    {
        maptex2 = W_CacheLumpName("TEXTURE2");
        extMemSetCurrentAddress((uint32_t) maptex2);
        extMemGetDataFromCurrentAddress(&numtextures2, sizeof(numtextures2));
        directory2 = maptex2 + 1;
    }
    else
    {
        maptex2 = NULL;
        numtextures2 = 0;
        directory2 = NULL;
    }

    int offset = 0;
    const int *maptex = maptex1;
    if (texture_num < numtextures1)
    {

        //offset = directory1[texture_num];
        extMemSetCurrentAddress((uint32_t) directory1 + texture_num * sizeof(directory1[0]));
        extMemGetDataFromCurrentAddress(&offset, sizeof(offset));
    }
    else if (maptex2 && ((texture_num - numtextures1) < numtextures2))
    {
        maptex = maptex2;
        //offset = directory2[texture_num-numtextures1];
        extMemSetCurrentAddress((uint32_t) directory2 + (texture_num - numtextures1) * sizeof(directory2[0]));
        extMemGetDataFromCurrentAddress(&offset, sizeof(offset));
    }
    else
    {
        I_Error("R_LoadTexture: Texture %d not in range.", texture_num);
    }
    maptexture_t *mtexture_addr = (maptexture_t*) ((const byte*) maptex + offset);
    maptexture_t mtexture;
    extMemSetCurrentAddress((uint32_t) mtexture_addr);
    extMemGetDataFromCurrentAddress(&mtexture, sizeof(mtexture));
    texture_t *texture;
    if (cacheToFlash)
    {
        texture = Z_Malloc(sizeof(texture_t) + sizeof(texpatch_t) * (mtexture.patchcount - 1), PU_STATIC, NULL);
    }
    else
    {
        texture = Z_Malloc(sizeof(texture_t) + sizeof(texpatch_t) * (mtexture.patchcount - 1), PU_LEVEL, NULL);
        printf("Texture not cached to flash. Blocking!\r\n");
        while(1);
    }
    texture->width = mtexture.width;
    texture->height = mtexture.height;
    // texture->patchcount = mtexture.patchcount; // 2022/02/01 nexthack: removed, multipatches are converter to single one by the wad converter
    //  texture->name = mtexture->name;  // 2021/03/06: next-hack removed, useless

    texpatch_t *patch = texture->patches;
    const mappatch_t *mpatch_addr = mtexture_addr->patches;
    //
    // texture->overlapped = 0;   // 2022/02/01 nexthack: removed, multipatches are converter to single one by the wad converter
    //for (int j = 0; j < texture->patchcount; j++, mpatch_addr++, patch++)
    //{
        mappatch_t mpatch;
        extMemSetCurrentAddress((uint32_t) mpatch_addr);
        extMemGetDataFromCurrentAddress(&mpatch, sizeof(mpatch));
        //patch->originx = mpatch.originx;         // 2022/02/01 nexthack: removed, multipatches are converter to single one by the wad converter
        //patch->originy = mpatch.originy;         // 2022/02/01 nexthack: removed, multipatches are converter to single one by the wad converter
        char pname[8];
        char mpatchpname[8];
        extMemSetCurrentAddress((uint32_t) &pnames[mpatch.patch * 8]);
        extMemGetDataFromCurrentAddress(mpatchpname, sizeof(mpatchpname));
        strncpy(pname, (const char*) mpatchpname, 8);
        int lumpNum = W_GetNumForName(pname);
        filelump_t fl;
        getFileLumpByNum(lumpNum, &fl);
        // will the texture fit in flash?
#if DISABLE_CACHING_TEXTURE_TO_FLASH
#warning Caching textures to flash disabled!
#else

        if (getUserFlashRegionRemainingSpace() >= *spaceRequired + fl.size)
        {
            patch->patch = getAddressOrCacheLumpNum(lumpNum, true, FLASH_LEVEL_REGION);
        }
        else
#endif
        {
            patch->patch = (const patch_t*) W_CacheLumpNum(lumpNum);
        }
    //}
     // 2022/02/01 nexthack: removed, multipatches are converted to single one by the wad converter
    // Removed below, it is done in R_FillTextureHeightData()
    //textureheight[texture_num] = texture->height<<FRACBITS;
    if (cacheToFlash)
    {
        *spaceRequired = *spaceRequired - (sizeof(const texture_t) + sizeof(const texpatch_t) * (mtexture.patchcount - 1));
        textures[texture_num] = writeBufferToFlashRegion(texture, sizeof(const texture_t) + sizeof(const texpatch_t) * (mtexture.patchcount - 1), FLASH_LEVEL_REGION, true);
        Z_Free(texture);
        return textures[texture_num];
    }
    else
    {
        textures[texture_num] = texture;
        return texture;
    }
}

const texture_t* R_GetTexture(int texture, boolean cacheToFlash, uint32_t *spaceRequired)
{
    if (texture >= _g->numtextures)
        return NULL;

    if (textures[texture])
        return textures[texture];

    const texture_t *t = R_LoadTexture(texture, cacheToFlash, spaceRequired);

    textures[texture] = t;

    return t;
}

static int R_GetTextureNumForName(const char *tex_name)
{
    const int *maptex1, *maptex2;
    int numtextures1;
    const int *directory1, *directory2;

    //Convert name to uppercase for comparison.
    char tex_name_upper[9];

    strncpy(tex_name_upper, tex_name, 8);
    tex_name_upper[8] = 0; //Ensure null terminated.

    strupr(tex_name_upper);

    //if(_g->tex_lookup_last_name && (!strncmp(_g->tex_lookup_last_name, tex_name_upper, 8)))
    if (_g->tex_lookup_valid && (!strncmp((char*__restrict) _g->tex_lookup_last_name, tex_name_upper, 8)))
    {
        return _g->tex_lookup_last_num;
    }

    maptex1 = W_CacheLumpName("TEXTURE1");
    extMemSetCurrentAddress((uint32_t) maptex1);
    //numtextures1 = *maptex1;
    extMemGetDataFromCurrentAddress(&numtextures1, sizeof(numtextures1));
    directory1 = maptex1 + 1;

    if (W_CheckNumForName("TEXTURE2") != -1)
    {
        maptex2 = W_CacheLumpName("TEXTURE2");
        directory2 = maptex2 + 1;
    }
    else
    {
        maptex2 = NULL;
        directory2 = NULL;
    }

    const int *directory = directory1;
    const int *maptex = maptex1;

    for (int i = 0; i < _g->numtextures; i++, directory++)
    {
        if (i == numtextures1)
        {
            // Start looking in second texture file.
            maptex = maptex2;
            directory = directory2;
        }

        int offset; //= *directory;
        extMemSetCurrentAddress((uint32_t) directory);
        extMemGetDataFromCurrentAddress(&offset, sizeof(offset));
        const maptexture_t *mtexture = (const maptexture_t*) ((const byte*) maptex + offset);
        extMemSetCurrentAddress((uint32_t) &mtexture->name);
        char texturename[8];
        extMemGetDataFromCurrentAddress(texturename, sizeof(texturename));
        if (!strncmp(tex_name_upper, texturename, 8))
        {
            strncpy((char*__restrict) _g->tex_lookup_last_name, texturename, 8);
            _g->tex_lookup_valid = true;
            _g->tex_lookup_last_num = i;
            return i;
        }
    }
    return -1;
}

int R_LoadTextureByName(const char *tex_name, boolean cacheToFlash)
{
    if (tex_name[0] == '-')
        return NO_TEXTURE;

    int tnum = R_GetTextureNumForName(tex_name);

    if (tnum == -1)
    {
        printf("texture name: %s not found.\n", tex_name);
        return NO_TEXTURE;
    }

    R_GetTexture(tnum, cacheToFlash, NULL);

    return tnum;
}

//
// R_InitTextures
// Initializes the texture list
//  with the textures from the world map.
//

static void R_InitTextures()
{
    const int *mtex1 = W_CacheLumpName("TEXTURE1");
    extMemSetCurrentAddress((uint32_t) mtex1);
    int numtextures1 = 0;
    extMemGetDataFromCurrentAddress(&numtextures1, sizeof(numtextures1));
    //
    int numtextures2 = 0;
    if (W_CheckNumForName("TEXTURE2") != -1)
    {
        const int *mtex2 = W_CacheLumpName("TEXTURE2");
        extMemSetCurrentAddress((uint32_t) mtex2);
        extMemGetDataFromCurrentAddress(&numtextures2, sizeof(numtextures2));
    }
    printf("Numtextures %d %d add %x\r\n", numtextures1, numtextures2, mtex1);
    _g->numtextures = numtextures1 + numtextures2;
    p_wad_immutable_flash_data->numtextures = _g->numtextures;
    //
//    textureheight = Z_Malloc(_g->numtextures * sizeof *textureheight, PU_STATIC, 0);
//    memset(textureheight, 0, _g->numtextures * sizeof *textureheight);

    texturetranslation = Z_Malloc((_g->numtextures + 1) * sizeof *texturetranslation, PU_STATIC, 0);

    for (int i = 0; i < _g->numtextures; i++)
    {
        // next-hack: typically a texture animation is texture_num + anim index. Therefore, we simply store the offset.
        texturetranslation[i] = 0; // was i, now offset i 0.
    }
    // get texture height, and store them in flash;
//    R_FillTextureHeightData();
//    p_wad_immutable_flash_data->textureheight = writeBufferToFlashRegion(textureheight, _g->numtextures * sizeof *textureheight, FLASH_IMMUTABLE_REGION, true);
//    Z_Free(textureheight);
//    textureheight = p_wad_immutable_flash_data->textureheight;
}
//
// R_InitFlats
//
static void R_InitFlats(void)
{
    int i;

    _g->firstflat = W_GetNumForName("F_START") + 1;
    int lastflat = W_GetNumForName("F_END") - 1;
    _g->numflats = lastflat - _g->firstflat + 1;

    // Create translation table for global animation.
    // killough 4/9/98: make column offsets 32-bit;
    // clean up malloc-ing to use sizeof

    flattranslation = Z_Malloc((_g->numflats + 1) * sizeof(*flattranslation), PU_STATIC, 0);

    for (i = 0; i < _g->numflats; i++)
        flattranslation[i] = 0;     // next-hack: now flats are stored as offset to the original index
}

//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
// so the sprite does not need to be cached completely
// just for having the header info ready during rendering.
//
static void R_InitSpriteLumps(void)
{
    _g->firstspritelump = W_GetNumForName("S_START") + 1;
    _g->lastspritelump = W_GetNumForName("S_END") - 1;
    _g->numspritelumps = _g->lastspritelump - _g->firstspritelump + 1;
}

//
// R_InitColormaps
//
/*
 void R_InitColormaps (void)
 {
 int lump = W_GetNumForName("COLORMAP");
 colormaps = W_CacheLumpNum(lump);
 }
 */
//
// R_InitData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData(void)
{
    lprintf(LO_INFO, "Textures");
    R_InitTextures();
    lprintf(LO_INFO, "Flats");
    R_InitFlats();
    lprintf(LO_INFO, "Sprites");
    R_InitSpriteLumps();
    //lprintf(LO_INFO, "Colormaps");
    //R_InitColormaps();                    // killough 3/20/98
}

//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
// killough 4/17/98: changed to use ns_flats namespace
//

int R_FlatNumForName(const char *name)    // killough -- const added
{
    int i = W_CheckNumForName(name);

    if (i == -1)
        I_Error("R_FlatNumForName: %.8s not found", name);
    return i - _g->firstflat;
}

//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
// Rewritten by Lee Killough to use hash table for fast lookup. Considerably
// reduces the time needed to start new levels. See w_wad.c for comments on
// the hashing algorithm, which is also used for lump searches.
//
// killough 1/21/98, 1/31/98
//

int PUREFUNC R_CheckTextureNumForName(const char *name)
{
    // "NoTexture" marker.
    if (name[0] == '-')
        return 0;

    return R_GetTextureNumForName(name);
}
