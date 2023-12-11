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
 *  Do all the WAD I/O, get map description,
 *  set up initial state and misc. LUTs.
 *
 * next-hack: heavily modified load functions to support caching of WAD-wise
 * and level-wise constant data to flash, saving tons of RAM.
 *-----------------------------------------------------------------------------*/

#include <math.h>

#include "doomstat.h"
#include "m_bbox.h"
#include "g_game.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_things.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "lprintf.h" //jff 10/6/98 for debug outputs
#include "v_video.h"
#include "doomstat.h"

#include "global_data.h"
#include "r_sky.h"
#include "extMemory.h"
#include "audio.h"
#include "utility_functions.h"
extern patch_t * cachedColumnOffsetDataPatch;
mapthing_t  deathmatchstarts[MAX_DM_STARTS];
mapthing_t* deathmatch_p;
extern uint8_t     iquehead;
extern uint8_t     iquetail;
//
// P_LoadVertexes
//
// killough 5/3/98: reformatted, cleaned up
//
static void P_LoadVertexes(int lump)
{
    // TODO: remove _g->vertexes and _g->numvertexes and use the values in p_wad_level_flash_data instead
    // Determine number of lumps:
    //  total lump length / vertex record length.
    _g->numvertexes = W_LumpLength(lump) / sizeof(vertex_t);
    p_wad_level_flash_data->numvertex = W_LumpLength(lump) / sizeof(vertex_t);
    // Allocate zone memory for buffer.
    p_wad_level_flash_data->vertexes = writeLumpToFlashRegion(lump, FLASH_LEVEL_REGION, true);
    _g->vertexes = p_wad_level_flash_data->vertexes;

}

//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadSegs(int lump)
{
    // TODO: remove _g->numsegs and _g->segs and use the values in p_wad_level_flash_data instead
    _g->numsegs = W_LumpLength(lump) / sizeof(seg_t);
    p_wad_level_flash_data->numsegs = W_LumpLength(lump) / sizeof(seg_t);
    p_wad_level_flash_data->segs = writeLumpToFlashRegion(lump, FLASH_LEVEL_REGION, true);
    _g->segs = (const seg_t*) p_wad_level_flash_data->segs;
//    if (!_g->numsegs)
//      I_Error("P_LoadSegs: no segs in level");
}

//
// P_LoadSubsectors
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadSubsectors(int lump)
{
    /* cph 2006/07/29 - make data a const mapsubsector_t *, so the loop below is simpler & gives no constness warnings */
    const mapsubsector_t *data;
    int i;

    _g->numsubsectors = W_LumpLength(lump) / sizeof(mapsubsector_t);
    // 2021-11-18 next-hack: this is going to be cached on the flash
    _g->subsectors = Z_Calloc(_g->numsubsectors, sizeof(subsector_t), PU_STATIC, 0);
    data = (const mapsubsector_t*) W_CacheLumpNum(lump);

    if ((!data) || (!_g->numsubsectors))
        I_Error("P_LoadSubsectors: no subsectors in level");

    extMemSetCurrentAddress((uint32_t) data);
    for (i = 0; i < _g->numsubsectors; i++)
    {
        mapsubsector_t subsector;
        extMemGetDataFromCurrentAddress(&subsector, sizeof(subsector));
        //_g->subsectors[i].numlines  = (unsigned short)SHORT(data[i].numsegs );
        //_g->subsectors[i].firstline = (unsigned short)SHORT(data[i].firstseg);
        _g->subsectors[i].numlines = (unsigned short) SHORT(subsector.numsegs);
        _g->subsectors[i].firstline = (unsigned short) SHORT(subsector.firstseg);
    }
}

//
// P_LoadSectors
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadSectors(int lump)
{
    const byte *data; // cph - const*
    int i;

    _g->numsectors = W_LumpLength(lump) / sizeof(mapsector_t);
    _g->sectors = Z_Calloc(_g->numsectors, sizeof(sector_t), PU_STATIC, 0);
    _g->ramsectors = Z_Calloc(_g->numsectors, sizeof(ramSector_t), PU_LEVEL, 0);

    data = W_CacheLumpNum(lump); // cph - wad lump handling updated

    extMemSetCurrentAddress((uint32_t) data);
#define TEST_HEIGHTS 0
#if TEST_HEIGHTS
    int fhm =0, chm =0;
#endif
    for (i = 0; i < _g->numsectors; i++)
    {
        sector_t *ss = _g->sectors + i;
        ramSector_t *rs = _g->ramsectors + i;

        // next-hack: keep track of sector number. Easier to handle then.
        ss->sectorNumber = i;
        // we must set the address each time, as it is changed by R_FlatNumForName
        extMemSetCurrentAddress((uint32_t) data + i * sizeof(mapsector_t));
        mapsector_t ms;
        extMemGetDataFromCurrentAddress(&ms, sizeof(mapsector_t));
        rs->floorheight16 = FIXED32_TO_FIXED16(SHORT(ms.floorheight) << FRACBITS);
        rs->ceilingheight16 = FIXED32_TO_FIXED16(SHORT(ms.ceilingheight) << FRACBITS);
        // this will change the spi address.
        rs->floorpic = R_FlatNumForName(ms.floorpic);
        ss->ceilingpic = R_FlatNumForName(ms.ceilingpic);

        rs->lightlevel = SHORT(ms.lightlevel);
        rs->special = SHORT(ms.special);
        // next-hack: changed oldspecial to a single bit
        rs->wasSecret = (SHORT(ms.special) == 9 || (SHORT(ms.special) & SECRET_MASK));
        ss->tag = SHORT(ms.tag);

        rs->thinglist_sptr = 0;
#if USE_MSECNODE
        rs->touching_thinglist_sptr = 0;            // phares 3/14/98
#endif
#if TEST_HEIGHTS
        if (fhm > rs->floorheight16)
        {
            fhm = rs->floorheight16;
        }
        if (chm < rs->ceilingheight16)
        {
            chm = rs->ceilingheight16;
        }
        printf("Sector %d, heights (f, c): %d, %d, max (f, c) %d, %d\r\n",
        i, 
        rs->floorheight16, 
        rs->ceilingheight16,
        fhm,
        chm); 
#endif
    }
}

//
// P_LoadNodes
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadNodes(int lump)
{
    // TODO: remove numnodes and nodes and use p_wad_level
    numnodes = W_LumpLength(lump) / sizeof(mapnode_t);
    p_wad_level_flash_data->numnodes = W_LumpLength(lump) / sizeof(mapnode_t);
    //
    p_wad_level_flash_data->nodes = writeLumpToFlashRegion(lump, FLASH_LEVEL_REGION, true);
    nodes = p_wad_level_flash_data->nodes; // cph - wad lump handling updated

    if ((!nodes) || (!numnodes))
    {
        // allow trivial maps
        if (_g->numsubsectors == 1)
            lprintf(LO_INFO, "P_LoadNodes: trivial map (no nodes, one subsector)\n");
        else
            I_Error("P_LoadNodes: no nodes in level");
    }
}

/*
 * P_LoadThings
 *
 * killough 5/3/98: reformatted, cleaned up
 * cph 2001/07/07 - don't write into the lump cache, especially non-idepotent
 * changes like byte order reversals. Take a copy to edit.
 */

static void P_LoadThings(int lump)
{
    int i, numthings = W_LumpLength(lump) / sizeof(mapthing_t);
    const mapthing_t *data = W_CacheLumpNum(lump);
//
    if ((!data) || (!numthings))
        I_Error("P_LoadThings: no things in level");
    printf("Num things %d\r\n", numthings);
    _g->totalstatic = 0;
    // first get the number of static things
    extMemSetCurrentAddress((uint32_t) data);
    for (i = 0; i < numthings; i++)
    {

        // mapthing_t mt = data[i];
        mapthing_t mt;
        extMemGetDataFromCurrentAddress(&mt, sizeof(mt));
        mt.type = SHORT(mt.type);

        //
        if (!P_IsDoomnumAllowed(mt.type))
            continue;
        // is it a static ? count it
        int type = P_FindDoomedNum(mt.type);
        if (type == NUMMOBJTYPES)
        {
            continue;
        }
        if (mobjinfo[type].flags & MF_STATIC)
        {
            _g->totalstatic++;
        }
    }
    // create temporary buffer for static xy positions
    _g->full_static_mobj_xy_and_type_values = Z_Calloc(_g->totalstatic, sizeof(full_static_mobj_xy_and_type_t), PU_STATIC, NULL);
    int numpos = _g->totalstatic;
    // reset static stuff.
    _g->totalstatic = 0;
    extMemSetCurrentAddress((uint32_t) data);
    for (i = 0; i < numthings; i++)
    {

        // mapthing_t mt = data[i];
        mapthing_t mt;
        extMemGetDataFromCurrentAddress(&mt, sizeof(mt));
        mt.x = SHORT(mt.x);
        mt.y = SHORT(mt.y);
        mt.angle = SHORT(mt.angle);
        mt.type = SHORT(mt.type);
        mt.options = SHORT(mt.options);

        if (!P_IsDoomnumAllowed(mt.type))
            continue;
        // Do spawn all other stuff.
        P_SpawnMapThing(&mt);
    }
    printf("Num static things %d\r\n", _g->totalstatic);
    // save the xy static position buffer in flash. 
    void *tmp = _g->full_static_mobj_xy_and_type_values;
    // note: we store numpos elements and not _g->xy_static_pos, otherwise we need to rewrite everything even if we modified skill.
    _g->full_static_mobj_xy_and_type_values =  writeBufferToFlashRegion(tmp, numpos * sizeof(*_g->full_static_mobj_xy_and_type_values), FLASH_LEVEL_REGION, true);
    // free precious RAM
    Z_Free(tmp);
}

//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//        ^^^
// ??? killough ???
// Does this mean secrets used to be linedef-based, rather than sector-based?
//
// killough 4/4/98: split into two functions, to allow sidedef overloading
//
// killough 5/3/98: reformatted, cleaned up

static void P_LoadLineDefs(int lump)
{
    int i;
    // TODO: remove _g->numlines and _g->lines and use the values in p_wad_level_flash_data instead
    _g->numlines = W_LumpLength(lump) / sizeof(line_t);
    p_wad_level_flash_data->numlines = W_LumpLength(lump) / sizeof(line_t);
    p_wad_level_flash_data->lines = writeLumpToFlashRegion(lump, FLASH_LEVEL_REGION, true);
    _g->lines = p_wad_level_flash_data->lines;
#if OLD_VALID_COUNT
    _g->linedata = Z_Calloc(_g->numlines, sizeof(linedata_t), PU_LEVEL, 0);


    for (i = 0; i < _g->numlines; i++)
    {
        _g->linedata[i].special = _g->lines[i].const_special ? 1 : 0;
    }
#else
    // note: there are always more lines than sectors.
    _g->lineSectorChecked = Z_Calloc((_g->numlines + 31) / 32, sizeof(uint32_t), PU_LEVEL, 0);
    _g->lineIsSpecial = Z_Calloc((_g->numlines + 31) / 32, sizeof(uint32_t), PU_LEVEL, 0);
    _g->lineIsMapped = Z_Calloc((_g->numlines + 31) / 32, sizeof(uint32_t), PU_LEVEL, 0);
    _g->lineStairDirection = Z_Calloc((_g->numlines + 31) / 32, sizeof(uint32_t), PU_LEVEL, 0);
    for (i = 0; i < _g->numlines; i++)
    {
        setBitMask32Value( _g->lineIsSpecial, i, _g->lines[i].const_special ? 1 : 0);
    }

#endif
}

//
// P_LoadSideDefs
// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up

static void P_LoadSideDefs(int lump)
{
    // 2021-02-13 next-hack
    // It is stupid to copy everything to ram. therefore we use everything which is in ROM
    // except texture offset, which needs to be copied to ram (it might change).
    // In this way, we save 12 bytes for each side def.
    // now we use only ROM. It is up to the wad to have correct data.
    p_wad_level_flash_data->numsides = W_LumpLength(lump) / sizeof(mapsidedef_t);
    _g->numsides = p_wad_level_flash_data->numsides;
    _g->textureoffsets = Z_Calloc(_g->numsides, sizeof(*_g->textureoffsets), PU_LEVEL, 0);
    // TODO: remove _g->numsides and _g->sides and use the values in p_wad_level_flash_data instead.
    p_wad_level_flash_data->sides = writeLumpToFlashRegion(lump, FLASH_LEVEL_REGION, true);
    _g->sides = (side_t*) p_wad_level_flash_data->sides;
    int i;
    int n = 0, ma = 0, mi = 0;
    for (i = 0; i < _g->numsides; i++)
    {
        register side_t *sd = _g->sides + i;
        _g->textureoffsets[i] = sd->textureoffset;
        // nh: to count how much we can squeeze
        if (n < sd->midtexture)
            n = sd->midtexture;
        if (n < sd->toptexture)
            n = sd->toptexture;
        if (n < sd->bottomtexture)
            n = sd->bottomtexture;
        if (ma < sd->textureoffset)
            ma = sd->textureoffset;
        if (mi > sd->textureoffset)
            mi = sd->textureoffset;
    }
    // 2021-03-13: forgot that switches have changeable textures...
    // is a line special? Then we must reserve a memory location to store its texture numbers
    // first count how many specials we have
    int l = 0;
    for (i = 0; i < _g->numlines; i++)
    {
        if (_g->lines[i].const_special)
        {
            l++;
        }
    }
    // then allocate spaces
    // note, this buffer is stored in flash and later freed.
    _g->linesChangeableTextureIndex = Z_Calloc(_g->numlines, sizeof(*_g->linesChangeableTextureIndex), PU_STATIC, 0);
    // TODO: we could use only two arrays. Top and bot. In fact at most you can change the top/bottom or mid. Not all the 3 at the same time.
    _g->switch_texture_top = Z_Calloc(l, sizeof(*_g->switch_texture_top), PU_LEVEL, 0);
    _g->switch_texture_mid = Z_Calloc(l, sizeof(*_g->switch_texture_mid), PU_LEVEL, 0);
    _g->switch_texture_bot = Z_Calloc(l, sizeof(*_g->switch_texture_bot), PU_LEVEL, 0);
    l = 0;
    for (i = 0; i < _g->numlines; i++)
    {
        if (_g->lines[i].const_special)
        {
            // set the changeableTextureIndex
            _g->linesChangeableTextureIndex[i] = l;
            int side0num = _g->lines[i].sidenum[0];
            side_t *sd = _g->sides + side0num;
            // set texture index
            _g->switch_texture_top[l] = sd->toptexture;
            _g->switch_texture_mid[l] = sd->midtexture;
            _g->switch_texture_bot[l] = sd->bottomtexture;
            // point to next slot
            l++;
        }
    }
    // save the index buffer in flash. 
    uint8_t *tmp = _g->linesChangeableTextureIndex;
    _g->linesChangeableTextureIndex =  writeBufferToFlashRegion(tmp, _g->numlines * sizeof(*_g->linesChangeableTextureIndex), FLASH_LEVEL_REGION, true);
    // free precious RAM
    Z_Free(tmp);
    //
    printf("Number of textures %d, max offset %d, min offset %d. Special %d\r\n", n, ma, mi, l);
}
static void P_LoadSideDefsTextures()
{
    // 2021-03-23 next-hack
    // This function is called after we stored all the important level data. Here we try to cache (in flash) as many textures as possible
    // _g->sides, _g->numsides shall be already initialized by P_LoadSideDefs.
    int i;
    // reserve space for the texture array
    uint32_t size = _g->numtextures * sizeof(*textures);
    uint8_t *texMustBeLoaded = Z_Calloc(sizeof(*texMustBeLoaded), _g->numtextures, PU_STATIC, NULL);
    for (i = 0; i < _g->numsides; i++)
    {
        // TODO: instead of loading finding everytime TEXTUREx lumps by name we could cache it
        // it would save some time.
        register side_t *sd = _g->sides + i;
        if (!texMustBeLoaded[sd->midtexture])
        {
            size += getTextureStructSize(sd->midtexture);
            texMustBeLoaded[sd->midtexture] = 1;
            // 2023/12/03 next-hack: oops, forgot about animated textures!
            size += markAnimTextures(sd->midtexture, texMustBeLoaded) * getTextureStructSize(sd->midtexture);
        }
        if (!texMustBeLoaded[sd->bottomtexture])
        {
            size +=  getTextureStructSize(sd->bottomtexture);
            texMustBeLoaded[sd->bottomtexture] = 1;
            // 2023/12/03 next-hack: oops, forgot about animated textures!
            size += markAnimTextures(sd->bottomtexture, texMustBeLoaded) * getTextureStructSize(sd->midtexture);
        }
        if (!texMustBeLoaded[sd->toptexture])
        {
            size += getTextureStructSize(sd->toptexture);
            texMustBeLoaded[sd->toptexture] = 1;
            // 2023/12/03 next-hack: oops, forgot about animated textures!
            size += markAnimTextures(sd->toptexture, texMustBeLoaded) * getTextureStructSize(sd->midtexture);
        }
        // oops we forgot the anims
    }
    // next-hack: this extra step will increase load time but will allow to cut down some RAM
    // scan al the lines for special ones, to search for switch textures.
    for (i = 0; i < _g->numlines; i++)
    {
        // only count special lines
        if (_g->lines[i].const_special)
        {
            // get the texture number.
            int ttop = _g->switch_texture_top[_g->linesChangeableTextureIndex[i]];
            int tmid = _g->switch_texture_mid[_g->linesChangeableTextureIndex[i]];
            int tbot = _g->switch_texture_bot[_g->linesChangeableTextureIndex[i]];            
            // find the texture corresponds to a switch texture
            for (int j = 0; j < _g->numswitches * 2; j++)
            {
                int switchTex = p_wad_immutable_flash_data->switchlist[j];
                if ((ttop == switchTex || tmid == switchTex || tbot == switchTex))
                {
                    // found a switch. We must load it and its opposite state
                    if (!texMustBeLoaded[switchTex])
                     {
                        size += getTextureStructSize(switchTex);
                        texMustBeLoaded[switchTex] = 1;
                     }
                    // load the texture corresponding to the other state as well, if not yet loaded
                    switchTex = p_wad_immutable_flash_data->switchlist[j ^ 1];
                    if (!texMustBeLoaded[switchTex])
                     {
                        size += getTextureStructSize(switchTex);
                        texMustBeLoaded[switchTex] = 1;
                     }
                }
            }
        }
        // TODO: instead of loading finding everytime TEXTUREx lumps by name we could cache it
    }
    printf(">>>Size before %d\r\n", size);
 /*   for (i = 0; i < _g->numsides; i++)
    {
        register side_t *sd = _g->sides + i;
        R_GetTexture(sd->midtexture, true, &size);
        R_GetTexture(sd->toptexture, true, &size);
        R_GetTexture(sd->bottomtexture, true, &size);
    }*/
    for (int i = 0; i < _g->numtextures; i++)
    {
        if (texMustBeLoaded[i])
        {
            R_GetTexture(i, true, &size);
        }
    }
    //
    //
    // now the textures array is all filled. No more change will be made. Let's put it in rom.
    //  free RAM buffer and store it to flas instead
    void *ptr = textures;
    textures = writeBufferToFlashRegion(textures, _g->numtextures * sizeof(*textures), FLASH_LEVEL_REGION, true);
    Z_Free(ptr);
    size -= _g->numtextures * sizeof(*textures);
    printf(">>>Size after %d\r\n", size);
    Z_Free(texMustBeLoaded);

}

//
// jff 10/6/98
// New code added to speed up calculation of internal blockmap
// Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
//

#define blkshift 7               /* places to shift rel position for cell num */
#define blkmask ((1<<blkshift)-1)/* mask for rel position within cell */
#define blkmargin 0              /* size guardband around map used */
// jff 10/8/98 use guardband>0
// jff 10/12/98 0 ok with + 1 in rows,cols

typedef struct linelist_t        // type used to list lines in each block
{
    long num;
    struct linelist_t *next;
} linelist_t;

//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit,
// though current algorithm is brute-force and unoptimal.
//

static void P_LoadBlockMap(int lump)
{

    // TODO: remove  _g->blockmaplump and use p_wad_level_flash_data->blockmaplump instead.
    p_wad_level_flash_data->blockmaplump = writeLumpToFlashRegion(lump, FLASH_LEVEL_REGION, true);
    _g->blockmaplump = p_wad_level_flash_data->blockmaplump;

    _g->bmaporgx = _g->blockmaplump[0] << FRACBITS;
    _g->bmaporgy = _g->blockmaplump[1] << FRACBITS;
    _g->bmapwidth = _g->blockmaplump[2];
    _g->bmapheight = _g->blockmaplump[3];

    // clear out mobj chains - CPhipps - use calloc
    _g->blocklinks_sptrs = Z_Calloc(_g->bmapwidth * _g->bmapheight, sizeof(*_g->blocklinks_sptrs), PU_LEVEL, 0);

    _g->blockmap = _g->blockmaplump + 4;
}

//
// P_LoadReject - load the reject table, padding it if it is too short
// totallines must be the number returned by P_GroupLines()
// an underflow will be padded with zeroes, or a doom.exe z_zone header
// 
// this function incorporates e6y's RejectOverrunAddInt code:
// e6y: REJECT overrun emulation code
// It's emulated successfully if the size of overflow no more than 16 bytes.
// No more desync on teeth-32.wad\teeth-32.lmp.
// http://www.doomworld.com/vb/showthread.php?s=&threadid=35214

static void P_LoadReject(int lumpnum)
{
    int lump = lumpnum + ML_REJECT;
    p_wad_level_flash_data->rejectmatrix = writeLumpToFlashRegion(lump, FLASH_LEVEL_REGION, true);
    _g->rejectmatrix = p_wad_level_flash_data->rejectmatrix;
}

//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
// killough 5/3/98: reformatted, cleaned up
// cph 18/8/99: rewritten to avoid O(numlines * numsectors) section
// It makes things more complicated, but saves seconds on big levels
// figgi 09/18/00 -- adapted for gl-nodes

// cph - convenient sub-function
#if PACKED_ADDRESS
static void P_AddLineToSector(const line_t *li, sector_t *sector)
{
    //sector->lines[sector->linecount++] = li;
    getSectorLines(sector)[sector->linecount++] = li; // getPackedAddress(li);
}
static int P_GroupLines(void)
{
    register const line_t *li;
    register sector_t *sector;
    int i, j, total = _g->numlines;

    // figgi
    for (i = 0; i < _g->numsubsectors; i++)
    {
        const seg_t *seg = &_g->segs[_g->subsectors[i].firstline];
        // 2021-11-18 next-hack: subsectors in flash.
        //_g->subsectors[i].sector = NULL;
        _g->subsectors[i].sector_num = MAX_SECTOR_NUM;
        for (j = 0; j < _g->subsectors[i].numlines; j++)
        {
            if (seg->sidenum != NO_INDEX)
            {
//                _g->subsectors[i].sector = &_g->sectors[_g->sides[seg->sidenum].sector_num];
                _g->subsectors[i].sector_num = _g->sides[seg->sidenum].sector_num;
                break;
            }
            seg++;
        }
//        if (_g->subsectors[i].sector == NULL)
          if (MAX_SECTOR_NUM == _g->subsectors[i].sector_num)
            I_Error("P_GroupLines: Subsector a part of no sector!\n");
    }
    // count number of lines in each sector
    for (i = 0, li = _g->lines; i < _g->numlines; i++, li++)
    {
        LN_FRONTSECTOR(li)->linecount++;
        if (LN_BACKSECTOR(li) && LN_BACKSECTOR(li) != LN_FRONTSECTOR(li))
        {
            LN_BACKSECTOR(li)->linecount++;
            total++;
        }
    }
    // now we can write the subsector on the flash, and free it.
    void *ssptr = _g->subsectors;
    _g->subsectors = writeBufferToFlashRegion(_g->subsectors, _g->numsubsectors * sizeof(subsector_t), FLASH_LEVEL_REGION, true);
    // free subsector temporary buffer
    Z_Free(ssptr);
// now we have the total number of lines.
    line_t **linebuffer = Z_Malloc(total * sizeof(line_t*), PU_STATIC, 0);
    line_t **lineBufferArray = linebuffer;
    {  // allocate line tables for each sector

//        packedAddress_t *linebuffer_ppptr = Z_Malloc(total*sizeof(packedAddress_t), PU_LEVEL, 0);
        // e6y: REJECT overrun emulation code
        // moved to P_LoadReject
        int lineIndex = 0;
        for (i = 0, sector = _g->sectors; i < _g->numsectors; i++, sector++)
        {
            sector->lines_ppptr = getPackedAddress(linebuffer);
            linebuffer += sector->linecount;
            sector->linecount = 0;
        }

    }
    // Enter those lines
    for (i = 0, li = _g->lines; i < _g->numlines; i++, li++)
    {
        P_AddLineToSector(li, LN_FRONTSECTOR(li));
        if (LN_BACKSECTOR(li) && LN_BACKSECTOR(li) != LN_FRONTSECTOR(li))
            P_AddLineToSector(li, LN_BACKSECTOR(li));
    }
    // store buffer to flash
    line_t **flashArrayAddress = writeBufferToFlashRegion(lineBufferArray, total * sizeof(line_t*), FLASH_LEVEL_REGION, true);
    // now we need to update again all the pointers
    for (i = 0, sector = _g->sectors; i < _g->numsectors; i++, sector++)
    {
        sector->lines_ppptr = getPackedAddress(flashArrayAddress);
        flashArrayAddress += sector->linecount;
    }
    // now we can free the buffer
    Z_Free(lineBufferArray);
    // calculate the box that contains each sector.
    // sound will be generated from the center of this box.
    for (i = 0, sector = _g->sectors; i < _g->numsectors; i++, sector++)
    {
        fixed_t bbox[4];
        M_ClearBox(bbox);

        for (int l = 0; l < sector->linecount; l++)
        {
            const line_t *pline = getSectorLineByIndex(sector, l);

            M_AddToBox(bbox, pline->v1.x, pline->v1.y);
            M_AddToBox(bbox, pline->v2.x, pline->v2.y);
        }
        sector->soundorg.x = bbox[BOXRIGHT] / 2 + bbox[BOXLEFT] / 2;
        sector->soundorg.y = bbox[BOXTOP] / 2 + bbox[BOXBOTTOM] / 2;
    }

    return total; // this value is needed by the reject overrun emulation code
}

#else
short *sectorLineIndexes;

static void P_AddLineIndexToSector(unsigned int li, sector_t *sector)
{
  sectorLineIndexes[sector->lineStartIndex + sector->linecount++] = li ;
 }
// modified to return totallines (needed by P_LoadReject)
static int P_GroupLines(void)
{
    register const line_t *li;
    register sector_t *sector;
    int i, j, total = _g->numlines;

    // figgi
    for (i = 0; i < _g->numsubsectors; i++)
    {
        const seg_t *seg = &_g->segs[_g->subsectors[i].firstline];
        // 2021-11-18 next-hack: subsectors in flash.
        //_g->subsectors[i].sector = NULL;
        _g->subsectors[i].sector_num = MAX_SECTOR_NUM;
        for (j = 0; j < _g->subsectors[i].numlines; j++)
        {
            if (seg->sidenum != NO_INDEX)
            {
                _g->subsectors[i].sector_num = _g->sides[seg->sidenum].sector_num;
                break;
            }
            seg++;
        }
        if (MAX_SECTOR_NUM == _g->subsectors[i].sector_num)
            I_Error("P_GroupLines: Subsector a part of no sector!\n");
    }
    // count number of lines in each sector
    for (i = 0, li = _g->lines; i < _g->numlines; i++, li++)
    {
        LN_FRONTSECTOR(li)->linecount++;
        if (LN_BACKSECTOR(li) && LN_BACKSECTOR(li) != LN_FRONTSECTOR(li))
        {
            LN_BACKSECTOR(li)->linecount++;
            total++;
        }
    }
    // now we can write the subsector on the flash, and free it.
    void *ssptr = _g->subsectors;
    _g->subsectors = writeBufferToFlashRegion(_g->subsectors, _g->numsubsectors * sizeof(subsector_t), FLASH_LEVEL_REGION, true);
    // free subsector temporary buffer
    Z_Free(ssptr);
// now we have the total number of lines.
//    line_t **linebuffer = Z_Malloc(total * sizeof(line_t*), PU_STATIC, 0);
//    line_t **lineBufferArray = linebuffer;
    sectorLineIndexes = Z_Malloc(total * sizeof(short), PU_STATIC, 0);

    {  // allocate line tables for each sector
        // e6y: REJECT overrun emulation code
        // moved to P_LoadReject
        int lineIndex = 0;
        for (i = 0, sector = _g->sectors; i < _g->numsectors; i++, sector++)
        {
            sector->lineStartIndex = lineIndex; 
            lineIndex += sector->linecount;
            sector->linecount = 0;
        }
    }
    // now scan each line and add it to the sector(s) it belongs
    for (i = 0, li = _g->lines; i < _g->numlines; i++, li++)
    {
        P_AddLineIndexToSector(i, LN_FRONTSECTOR(li));
        if (LN_BACKSECTOR(li) && LN_BACKSECTOR(li) != LN_FRONTSECTOR(li))
            P_AddLineIndexToSector(i, LN_BACKSECTOR(li));
    }

    // store buffer to flash
    short *flashArrayAddress = writeBufferToFlashRegion(sectorLineIndexes, total * sizeof(short), FLASH_LEVEL_REGION, true);
    // now we need to update again all the pointers
#if PACKED_ADDRESS
    for (i = 0, sector = _g->sectors; i < _g->numsectors; i++, sector++)
    {
        sector->lines_ppptr = getPackedAddress(flashArrayAddress);
        flashArrayAddress += sector->linecount;
    }
#endif
    // now we can free the buffer
    Z_Free(sectorLineIndexes);
    sectorLineIndexes = flashArrayAddress;
    // calculate the box that contains each sector.
    // sound will be generated from the center of this box.
    for (i = 0, sector = _g->sectors; i < _g->numsectors; i++, sector++)
    {
        fixed_t bbox[4];
        M_ClearBox(bbox);

        for (int l = 0; l < sector->linecount; l++)
        {
            const line_t *pline = getSectorLineByIndex(sector, l);
  //          printf("total %d, l = %d, s = %d, linecout = %d, lineStartIndex = %d, pline = 0x%08x, lineindex = %d\r\n",total, l, i, sector->linecount, sector->lineStartIndex, pline, sectorLineIndexes[sector->lineStartIndex + l]);
            M_AddToBox(bbox, pline->v1.x, pline->v1.y);
            M_AddToBox(bbox, pline->v2.x, pline->v2.y);
        }
#if !USE_MSECNODE
        sector->sbbox[BOXRIGHT] = bbox[BOXRIGHT] >> FRACBITS;
        sector->sbbox[BOXLEFT] = bbox[BOXLEFT] >> FRACBITS;
        sector->sbbox[BOXTOP] = bbox[BOXTOP] >> FRACBITS;
        sector->sbbox[BOXBOTTOM] = bbox[BOXBOTTOM] >> FRACBITS;
#else
        sector->soundorg.x = bbox[BOXRIGHT] / 2 + bbox[BOXLEFT] / 2;
        sector->soundorg.y = bbox[BOXTOP] / 2 + bbox[BOXBOTTOM] / 2;
#endif
    }

    return total; // this value is needed by the reject overrun emulation code
}
#endif
void P_FreeLevelData()
{
    R_ResetPlanes();

    Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1);

    Z_Free(_g->braintargets);
    _g->braintargets = NULL;
    _g->numbraintargets_alloc = _g->numbraintargets = 0;
}


//
// P_SetupLevel
//
// killough 5/3/98: reformatted, cleaned up
void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{

    (void) playermask;      // why was it used ?
    static uint8_t oldSkill = -1;
    //
    cachedColumnOffsetDataPatch = NULL;
    // every boot, we need to refresh level data, to be sure that corrupted data
    // due to  resets or power down wont screw up everything.
    static boolean levelDataAlreadyInitialized = false;
    char lumpname[9];
    int lumpnum;
    printf("Setup level\r\n");
    _g->allocatedVisplanes = 0;
    _g->totallive = _g->totalkills = _g->totalitems = _g->totalsecret = 0;
    _g->wminfo.maxfrags = 0;

    _g->wminfo.partime = 180;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
      _g->players[i].killcount = _g->players[i].secretcount = _g->players[i].itemcount = 0;
    }

    // Initial height of PointOfView will be set by player think.
    _g->players[_g->consoleplayer].viewz = 1;

    // Make sure all sounds are stopped before Z_FreeTags.
    S_Start();
    P_FreeLevelData();

    // now that we have freed level data, we can allocate new temporary buffers.
    initLumpPtrTable();

    // let's check if we are changing level
    boolean differentLevel = false;
    if (levelDataAlreadyInitialized) // && (uint32_t) p_wad_immutable_flash_data->levelData > (uint32_t) p_wad_immutable_flash_data && p_wad_immutable_flash_data->levelData < FLASH_SIZE)
    {
        if (p_wad_level_flash_data->map != map || p_wad_level_flash_data->episode != episode || false == levelDataAlreadyInitialized || oldSkill != skill)
        {
            differentLevel = true;
        }
    }
    else
    {
        differentLevel = true;
    }
    oldSkill = skill;
    // allocate RAM where texture data shall be stored.
    if (differentLevel)
    {
        textures = Z_Calloc(_g->numtextures, sizeof *textures, PU_STATIC, 0);
    }
    /*  if (false == levelDataAlreadyInitialized || p_wad_level_flash_data->map != map || p_wad_level_flash_data->episode != episode || false == levelDataAlreadyInitialized)
     {
     differentLevel = true;
     }*/
    p_wad_level_flash_data = initLevelFlashRegion();
    p_wad_level_flash_data->map = map;
    p_wad_level_flash_data->episode = episode;
    printf("p_wad_level_flash_data: 0x%08x\r\n", p_wad_level_flash_data);
    //Load the sky texture.
    // Set the sky map.
    // First thing, we have a dummy sky texture name,
    //  a flat. The data is in the WAD only because
    //  we look for an actual index, instead of simply
    //  setting one.

    _g->skyflatnum = R_FlatNumForName( SKYFLATNAME);

    P_InitThinkers();

    // find map name
    if (_g->gamemode == commercial)
    {
        sprintf(lumpname, "MAP%02d", map);         // killough 1/24/98: simplify
    }
    else
    {
        sprintf(lumpname, "E%dM%d", episode, map); // killough 1/24/98: simplify
    }

    lumpnum = W_GetNumForName(lumpname);

    _g->leveltime = 0;
    _g->totallive = 0;

    printf("P_LoadVertexes\r\n");
    // vertex in flash
    P_LoadVertexes(lumpnum + ML_VERTEXES);
    // sectors: in RAM
    printf("P_LoadSectors\r\n");
    P_LoadSectors(lumpnum + ML_SECTORS);
    printf("P_LoadLineDefs\r\n");
    // Line Defs, stored in flash, but some info in RAM
    P_LoadLineDefs(lumpnum + ML_LINEDEFS);
    // Side defs in flash but some info in RAM
    printf("P_LoadSideDefs\r\n");
    P_LoadSideDefs(lumpnum + ML_SIDEDEFS);
    printf("P_LoadBlockMap\r\n");
    // block ram in MAP
    P_LoadBlockMap(lumpnum + ML_BLOCKMAP);
    printf("P_LoadSubsectors\r\n");
    // subsectors in ram
    P_LoadSubsectors(lumpnum + ML_SSECTORS);
    printf("P_LoadNodes\r\n");
    // nodes in flash
    P_LoadNodes(lumpnum + ML_NODES); // 0k
    printf("P_LoadSegs\r\n");
    // segs in flash
    P_LoadSegs(lumpnum + ML_SEGS);  // 0k
    // line in ram
    printf("P_GroupLines\r\n");
    P_GroupLines();
    // next-hack now _g->sectors can be saved in flash and the coresponding RAM buffer can be freed
    sector_t *p = _g->sectors;
    _g->sectors = writeBufferToFlashRegion(_g->sectors, _g->numsectors * sizeof(sector_t), FLASH_LEVEL_REGION, true);
    // free sector temporary buffer
    Z_Free(p);
    printf("P_LoadReject\r\n");
    // reject loading and underflow padding separated out into new function
    // P_GroupLines modified to return a number the underflow padding needs
    // P_grouplines in falash
    P_LoadReject(lumpnum);  // 0k


    // Note: you don't need to clear player queue slots --
    // a much simpler fix is in g_game.c -- killough 10/98

    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;

    /* cph - reset all multiplayer starts */
    memset(_g->playerstarts, 0, sizeof(_g->playerstarts));

    for (int i = 0; i < MAXPLAYERS; i++)
        _g->players[i].mo = NULL;

    P_MapStart();

    P_LoadThings(lumpnum + ML_THINGS);

    // if deathmatch, randomly spawn the active players
    if (_g->deathmatch)
    {
      for (int i = 0; i < MAXPLAYERS; i++)
      {
        if (_g->playeringame[i])
        {
          _g->players[i].mo = NULL; // not needed? - done before P_LoadThings
          G_DeathMatchSpawnPlayer(i);
        }
      }
    }
    else // if !deathmatch, check all necessary player starts actually exist
    {
      for (int i = 0; i < MAXPLAYERS; i++)
      {
        if (_g->playeringame[i] && !_g->players[i].mo)
        {
          I_Error("P_SetupLevel: missing player %d start\n", i+1);
        }
      }
    }


    // killough 3/26/98: Spawn icon landings:
    if (_g->gamemode == commercial)
        P_SpawnBrainTargets();

    // clear special respawning que
    iquehead = iquetail = 0;

    // set up world state
    P_SpawnSpecials(); // 

    P_MapEnd();

    // caching, different level
    if (differentLevel)
    {
        lprintf(LO_INFO, "P_InitPicAnims");
        P_InitPicAnims();

        // now cache sky
        // DOOM determines the sky texture to be used
        // depending on the current episode, and the game version.
        if (_g->gamemode == commercial)
        // || gamemode == pack_tnt   //jff 3/27/98 sorry guys pack_tnt,pack_plut
        // || gamemode == pack_plut) //aren't gamemodes, this was matching retail
        {
            _g->skytexture = R_LoadTextureByName("SKY3", true);
            if (_g->gamemap < 12)
                _g->skytexture = R_LoadTextureByName("SKY1", true);
            else if (_g->gamemap < 21)
                _g->skytexture = R_LoadTextureByName("SKY2", true);
        }
        else
            //jff 3/27/98 and lets not forget about DOOM and Ultimate DOOM huh?
            switch (_g->gameepisode)
            {
                case 1:
                    _g->skytexture = R_LoadTextureByName("SKY1", true);
                    break;
                case 2:
                    _g->skytexture = R_LoadTextureByName("SKY2", true);
                    break;
                case 3:
                    _g->skytexture = R_LoadTextureByName("SKY3", true);
                    break;
                case 4: // Special Edition sky
                    _g->skytexture = R_LoadTextureByName("SKY4", true);
                    break;
            } //jff 3/27/98 end sky setting fix
        printf(">>>Current level data length before flats: %d\r\n",p_wad_level_flash_data->dataLength);

        // cache all the flats.
        for (int j = 0; j < _g->numsectors; j++)
        {
            int picnum;
#if PRINT_ADDRESSES
            void *addr;
            picnum = _g->sectors[j].ceilingpic + _g->firstflat;
            addr = getAddressOrCacheLumpNum(picnum, true, FLASH_LEVEL_REGION);
            printf("Cache C flat num %d 0x%02x\r\n",picnum, (uint32_t) addr);
            picnum = _g->sectors[j].floorpic + _g->firstflat;
            addr = getAddressOrCacheLumpNum(picnum, true, FLASH_LEVEL_REGION);
            printf("Cache F flat num %d 0x%02x\r\n",picnum, (uint32_t) addr);
#else
            picnum = _g->sectors[j].ceilingpic + _g->firstflat;
            getAddressOrCacheLumpNum(picnum, true, FLASH_LEVEL_REGION);
            picnum = _g->ramsectors[j].floorpic + _g->firstflat;
            getAddressOrCacheLumpNum(picnum, true, FLASH_LEVEL_REGION);
#endif
        }
        printf(">>>Current level data length after flats: %d\r\n",p_wad_level_flash_data->dataLength);
        printf(">>>Current immutable data length: %d\r\n", p_wad_immutable_flash_data->immutableDataLength);
        printf(">>>Current level address: %d\r\n", p_wad_immutable_flash_data->levelData);

        // cache as many texture as possible; Note: this will put textures[] array in flash and free the previously allocated buffer
        P_LoadSideDefsTextures();
    }
    // now cache every pointer so that in game we do not need to save to flash.
    for (int i = 0; i < _g->numlumps; i++)
    {
        getAddressOrCacheLumpNum(i, false, FLASH_LEVEL_REGION);
    }
    p_wad_level_flash_data->lumpAddressTable = storeLumpArrayToFlash(differentLevel);
    p_wad_level_flash_data = storeLevelDataHeader(differentLevel); // this will also free old p_wad_level_flash_data in RAM.
    //
    printf("p_wad_level_flash_data: 0x%08x\r\n", p_wad_level_flash_data);
    //
    levelDataAlreadyInitialized = true;
}

//
// P_Init
//
void P_Init(void)
{
    lprintf(LO_INFO, "P_InitSwitchList");
    P_InitSwitchList();
// moved to setup level
//    lprintf(LO_INFO, "P_InitPicAnims");
//    P_InitPicAnims();

    lprintf(LO_INFO, "R_InitSprites");
    R_InitSprites(sprnames);
}
