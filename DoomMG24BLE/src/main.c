/***************************************************************************//**
 * @file main.c
 * @brief main function for Doom on EFR32xg24 MCUs
 *
 *  Copyright (C) 2022-2023 by Nicola Wrachien
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
 *******************************************************************************
 ******************************************************************************/
// BIG NOTE!!! This project is configured for a 20 dBm part, to support
// the Sparkfun Thing Plus Matter board.
// Unfortunately, this means BLE won't work on 10 dBm parts.
// If you have a 10 dBm part, then you'll have to configure the to a 10 dBm part
// (e.g. MGM240PB22VNA).
// Note: conversely, starting from GSDK 4.3 if you configure for 10 dBm,
// it won't work on a 20 dBm part.
// To configure, double click on DoomMG24BLE.slcp, select Oveview on top of
// the window, then on target and tools settings press "Change Target/SDK/Generators".
// Finally select the correct part number and click save.
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
// NOTE: check if we can cache ST nums. This might save about 100us or more, probably 0.5 ms.
#include "sl_component_catalog.h"
#include "sl_system_init.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  #include "sl_power_manager.h"
#endif
#if defined(SL_CATALOG_KERNEL_PRESENT)
  #include "sl_system_kernel.h"
  #include "app_task_init.h"
#else // SL_CATALOG_KERNEL_PRESENT
  #include "sl_system_process_action.h"
#endif // SL_CATALOG_KERNEL_PRESENT
#include "sl_event_handler.h"
#include "keyboard.h"
#include "delay.h"
#include "sharedUsart.h"
#include "main.h"
#include "z_zone.h"
#include "global_data.h"
#include "audio.h"
#include "ymodem.h"
#include "d_main.h"
#include "extMemory.h"
#include "extMemory.h"
#include "em_cmu.h"
#include "boards.h"
#include "graphics.h"
#include "doom_ble.h"
#include "diskio.h"
#define TEST_YMODEM                           0
#define KEY_COMBINATION_FOR_WAD_UPLOAD        (KEY_ALT | KEY_FIRE | KEY_UP | KEY_DOWN)
extern uint8_t staticZone[];
// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * Main function
 *****************************************************************************/
int main(void)
{
  Z_Init();
  // Initialize Silicon Labs device, system, service(s) and protocol stack(s).
  // Note that if the kernel is present, processing task(s) will be created by
  // this call.
  // Note: system sl_system_init() call was split to allow using the malloc wrapper
  sl_platform_init();
  sl_driver_init();
  sl_service_init();
  //
  // Enable clocks
  CMU_ClockDivSet(cmuClock_PCLK, 1);
  const uint32_t clocks[] =
  {
      cmuClock_EUSART0, cmuClock_EUSART1, cmuClock_PRS, cmuClock_LDMA,
      cmuClock_LDMAXBAR, cmuClock_SYSCFG, cmuClock_USART0, CAT(cmuClock_TIMER, TICK_TIMER_NUMBER),
      cmuClock_GPIO,
  };
  for (size_t i = 0; i < sizeof (clocks) / sizeof(clocks[0]); i++)
  {
    CMU_ClockEnable(clocks[i], true);
  }
  //
  // Enable Timer 1 for generic delay
  TICK_TIMER->CFG = TIMER_CFG_PRESC_DIV8;
  TICK_TIMER->EN = TIMER_EN_EN;
  TICK_TIMER->TOP = 0xFFFFFFFF;
  TICK_TIMER->CMD = TIMER_CMD_START;
  //
#ifdef VCOM_ENABLE_PORT
  GPIO_PinModeSet(VCOM_ENABLE_PORT, VCOM_ENABLE_PIN, gpioModePushPull, 1);
#endif
  GPIO_PinModeSet(VCOM_TX_PORT, VCOM_TX_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(VCOM_RX_PORT, VCOM_RX_PIN, gpioModePushPull, 1);
  //
  displayInit();
  initGraphics();
  displayPrintln(0, "Doom on EFR32MG24 by Nicola Wrachien");
  displayPrintln(1, "Build date %s", __DATE__);
  displayPrintln(1, "Build time %s", __TIME__);
  //
  // enable VCOM
  //
  // measure refresh time!
  //
  uint32_t oldTime = TICK_TIMER->CNT;
  startDisplayRefresh(0);
  startDisplayRefresh(0);
  oldTime = TICK_TIMER->CNT - oldTime;
  displayPrintln(1, "Frame refresh time %d us!", oldTime / 10);
  displayPrintln(1, "Memzone size %d bytes.", getStaticZoneSize());
  displayPrintln(1, "Code Size: %d bytes", FLASH_CODE_SIZE);
  displayPrintln(1, "Trying to read external flash...");
  delay(500);
  extMemInit();
  displayPrintln(1, "SPI Flash Size: %d MB.", extMemGetSize() / 1048576);
  displayPrintln(1, "HFXO: %d Hz", SystemHFXOClockGet());
  displayPrintln(1, "HCLK: %d Hz", SystemHCLKGet());
  FLASH_NCS_HIGH();
  displayPrintln(1, "");
  displayPrintln(1, "Press ALT & FIRE & UP & DOWN");
  displayPrintln(1, "to start WAD installation.");
  delay(2000);
  FLASH_NCS_LOW();
  SET_FLASH_MODE();
#if 0   // for testing
  #define BSIZE 1024
  for (int i = 0; i < 1; i++)
  {
    memset(interleavedSpiData.rxBuffer, 0xaa, sizeof(interleavedSpiData.rxBuffer));

    interleavedSpiFlashStartRead(4 + i * BSIZE, &interleavedSpiData.rxBuffer[0], BSIZE);
    interleavedSpiWaitDma();
    for (int y = 0; y < BSIZE/16; y++)
    {
      printf("\r\n%08x:  ", i * BSIZE  + y * 16);
      for (int x = 0; x < 16; x++)
      {
        uint8_t c = interleavedSpiData.rxBuffer[8 + y * 16 + x];
        printf("%02x", c);
      }
      printf(" : ");
      for (int x = 0; x < 16; x++)
      {
        char c = interleavedSpiData.rxBuffer[8 + y * 16 + x];
        if ( c >= 32  && c <= 127)
        {
          printf("%c", c);
        }
        else
        {
          printf(".");
        }
      }
    }
  }
#endif
  // Let's check if we shall go in ymodem mode
  initKeyboard();
  uint8_t c;
  getKeys(&c);

  displayPrintln(1, "Key Pressed: %x", c);
  //
  //
#if !TEST_YMODEM
  if ((c & KEY_COMBINATION_FOR_WAD_UPLOAD) == (KEY_COMBINATION_FOR_WAD_UPLOAD))
#endif
  {
    // let's first try to mount SD
    bool sdUploadSuccessful = false;
#if HAS_SD
      // let's first check if there is a SD card on the disk
      displayPrintln(1, "Trying to init SD CARD.");
      FATFS *fs = (void*)staticZone;
      uint32_t stat =  f_mount(fs, "", 1);
      printf("f_mount() %d\r\n", stat);
      if (stat == 0)
      {
        displayPrintln(1, "SD Card init successful!");
        displayPrintln(1, "Opening " WAD_FILE_NAME "...");

        FIL *fil = (void*) &staticZone[sizeof(*fs)];
        stat = f_open(fil, "0:" WAD_FILE_NAME, FA_READ);
        int32_t fsize = fil->obj.objsize;
        printf("f_open() %d, size %d\r\n", stat, fsize);
        if (stat == 0)
        {

          displayPrintln(1, "Success! size: %d bytes", fsize);
          if (fsize > 0)
          {
            uint32_t address = WAD_ADDRESS;
            displayPrintln(1, "Erasing flash, please wait");
            displayPrintln(1, "(this might take up to 100 seconds)");
            SET_FLASH_MODE();
            extMemEraseAll();
            displayPrintln(1, "Programming, please wait");
            while (fsize > 0)
            {
              UINT br = 0;
              uint32_t  btr = fsize;
              uint32_t maxbtr = 40960;
              if (btr > maxbtr)
              {
                btr = maxbtr;
              }
              uint8_t *buffer = &staticZone[sizeof(FIL) + sizeof (FATFS)];
              stat = f_read(fil, buffer , btr, &br);
              if (!br || stat)
              {
                displayPrintln(1, "Error, read 0 bytes, stat %d", stat);
                break;
              }
              extMemProgram(address, buffer, br);
              fsize -= br;
              address += br;
              displayPrintln(1, "%d bytes remaining...", fsize);
            }
            displayPrintln(1, "WAD copy success!");
            sdUploadSuccessful = true;
          }
          else
          {
            displayPrintln(1, "Error, non-positve size!", fsize);
          }
        }
        else
        {
          displayPrintln(1, "Can't open file WAD.WAD!");
        }
      }
      else
      {
        displayPrintln(1, "Can't open SD card.");
      }

#endif
      if (!sdUploadSuccessful)
      {
        displayPrintln(1, "Begin YMODEM Wad Upload");
        if (0 == ymodemReceive(WAD_ADDRESS))
        {
            displayPrintln(1, "Wad Upload successful.");
        }
        else
        {
            displayPrintln(1, "YMODEM Error.");
        }
      }
      displayPrintln(1, "Reset in 2 seconds!");
      delay(2000);
      NVIC_SystemReset();
  }
  InitGlobals();
  //
  #if HAS_NETWORK
  // Init BLE
    sl_stack_init();
  #endif
  //
  SET_FLASH_MODE();
  //
  D_DoomMain();
}
// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
