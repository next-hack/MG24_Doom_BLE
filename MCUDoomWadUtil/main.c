/*
   MCUDoomWadutil by Nicola Wrachien.

   Based on doomhack's GBAWadutil
   Original source: https://github.com/doomhack/GbaWadUtil

   This command line utility allows to convert your wad file
   to a format convenient for MCU-based Doom ports.

   Usage:
   MCUDoomWadutil <inputwad> <outputwad>

   Revision History:
   0.1: first release
   0.2: multi patch textures.
   0.3: fix for multipatch textures
   0.4: additional wad data has version appended.
*/
#include <stdio.h>
#include <stdlib.h>
#include "wadfile.h"
#include "wadprocessor.h"
#define VERSION_MAJOR       0
#define VERSION_MINOR       4
#define MCU_DOOM_NAME       "mcudoom"
int main(int argc, char *argv[])
{
    wadfile_t wadfile;
    wadfile_t mcuwadfile;
    printf("MCUDoomWadutil by Nicola Wrachien V%d.%d\r\nOriginal source by doomhack.\r\n", VERSION_MAJOR, VERSION_MINOR);
    // process input
    if (argc != 3)
    {
        printf("Usage: %s <input wad> <output wad>\r\n", argv[0]);
        return 0;
    }
    char mcuWadFileName[32];
    snprintf(mcuWadFileName, sizeof(mcuWadFileName), "%s_%d_%d.wad", MCU_DOOM_NAME, VERSION_MAJOR, VERSION_MINOR);
    if (!loadWad(mcuWadFileName, &mcuwadfile))
    {
        printf("Error, %s must reside on the same directory of this program.\r\n", mcuWadFileName);
        return 1;
    }
    if (!loadWad(argv[1], &wadfile ))
    {
        printf("Cannot open %s\r\n", argv[1]);
        return 1;
    }
    // we merge first because we need to process the additional mcuwadfile patches.
    mergeWadFile(&wadfile, &mcuwadfile);
    processWad(&wadfile, false, true); // do not remove sound, convert patches
    //
    if (saveWad(argv[2], &wadfile, 'I'))
    {
        printf("Saved %s.\r\n", argv[2]);
    }
    else
    {
        printf("Cannot save %s.\r\n", argv[2]);
    }
    return 0;
}
