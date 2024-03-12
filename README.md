## MG24 Doom BLE
 Doom port to Silicon Labs' EFR32xG24 and MGM240x series microcontrollers and modules, with BLE multiplayer support.
 Coded by Nicola Wrachien.

The code is based on doomhack's GBA Doom Port (https://github.com/doomhack/GBADoom) with some Kippykip's additions, using the sources published on 2020. Thousands of changes have been made to fit to 256 kB of RAM, restore missing features, and enable multiplayer.

**Warning!** After you import the project on Simplicity Studio, you must "force project generation", so that the Gecko SDK files are copied on it. This is to comply with Silabs' licensing system. See "Building, developing, etc." below.

**12/03/2024 Update! Now peak 35 fps can be achieved also on 320 x 240 resolution thanks to partial screen refresh!**

**Features**

- Supports 320x200 and 320x240 resolution (the resolution can be changed at compile time)
- All the features of the original Doom are implemented:
  - full detail rendering, with Z-depth lighting.
  - 8-channel sound at 11025 Hz (samples with higher sample-rate are downsampled to 11025 Hz), mixed to 2 stereo channels.
  - Music with OPL2 emulation.
  - Monster behavior and sound propagation implemented.
  - Screen melt effect.
 - Extremely fast. Run almost always above 30 fps at 320x240, and even more at 320x200.
- Single player with save games and cheats support.
- Multiplayer features:
  - Each device can act as server or client.
  - Up to 4 players per game (server + 3 clients).
  - Game/player name can be edited using the menu.
  - Server can kick players in the lobby menu.
  - Clients can choose from up to 4 hosts.
  - Deathmatch or cooperative
  - Monster on/off.
  - Item respawn on/off
  - Time and frag limits.
  - Multiplayer over BLE.
  - Frame rate shown in the bottom-left corner (can be disabled with a compiler directive).
 - Additional features (with respect to vanilla Doom)
   - Player's weapon also is subjected to ambient light (as in PrBoom).
   - Background Doom title image becomes darker when Menus are turned on, to improve readability.
   - During networking games, kills messages are shown.
  - Tested with:
  - Shareware DOOM1.WAD.
  - Full Commercial DOOM.WAD
  - Partial Doom1.WAD v1.9 built-in demo support. Must be enabled by compiler directive.

**What's missing?**
- Only partial demo playback.
- Savegames do not store the whole status (might be implemented later), but only the current level and inventory.
- Need to be seriously debugged and cleaned up.

**Known issues**
- Multiplayer might be unstable in very crowded RF environments, i.e. joining might be difficult and 4-player game sometimes might be slow (from our tests, 3 player game is always fast, going at 30 fps).
- The display controller is driven via SPI and there is no with no vertical sync, so you can spot some artifacts due to sending new frame data, while the display is still refreshing the old frame.
- Only GSDK 4.3.2 is supported for now, with 10.2.1 GCC toolchain.

## Controls:
**Strafe:** alt + Left-Right
**Automap:** use + change weapon
**Menu:** alt + use
**Run** use + up-down


## Hardware:
In the "hardware" subdirectory you will find the project for the SparkFun Thing Plus Matter MGM240P board (SparkDoom subdir) and the WSTK adapter board .

## Content of this repository:
- MCUDoomWadUtil: source of the WAD converter (Code::Blocks project). Note that "mcudoom_0_4.wad" must be put in the same directory as the executable.
- DoomMG24BLE: the actual Doom port (Simplicity Studio 5.5 Project). **note!!!**: after you import the project with Simplicity Studio 5.5 project, you **must** "force generate" the project, so that the gecko sdk directory is recreated. This is because Silabs' GSDK has a different licensing, which requires you do download it from Simplicity Studio and accept the MSLA license.  Although you can download it from [Silicon Labs' github repo](https://github.com/SiliconLabs/gecko_sdk), using Simplicity Studio is strongly recommended, as only only the required files will be added.
- hardware: schematics.

## Building, developing, etc:
See https://next-hack.com/index.php/2023/12/10/multiplayer-doom-on-the-sparkfun-thing-plus-matter-board/

## License:
- Hardware: [Solderpad Hardware License version 2.1](https://solderpad.org/licenses/SHL-2.1)
- MCUDoomWadUtil: [GPLv2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html#SEC1)
- DoomMG24BLE
	- Original Doom code, modifications and its additions: GPLv2 or at your option later, as per original Doom/PrBoom sources.
	- [Marco Paland's printf](https://github.com/mpaland/printf): MIT License
	- Silicon Labs' GSDK (**note!** No GSDK files are provided due to its MSLA license, they must be downloaded from Simplicity Studio and restored to the project by forcing project generation): Silicon Lab's MSLA.
	- [Font8x8 By Daniel Hepper](https://github.com/dhepper/font8x8): Public Domain