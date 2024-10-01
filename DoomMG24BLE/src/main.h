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
 *  Description: main.h header file. Use this to configure the hardware
 *  as you wish
 */
#ifndef MAIN_H
#define MAIN_H
#include <stdbool.h>
#include "macros.h"
#include "sl_system_process_action.h"
// Test configuration
#define DISABLE_CACHING_TEXTURE_TO_FLASH  0    // non 0 is only for debug and to test how bad we can get if too many textures are uncached
#define TEST_DISABLE_ASYNCH_LOAD          0    // non 0 is to check how slow the rendering will be if data is not drawn asynchronously.
//
// Feature config
#define FAST_CPU_SMALL_FLASH              0       // if we have fast cpu but small flash, we can reduce math table sizes
#define CORRECT_TABLE_ERROR               1       // mandatory for demo compatibility. Will correct errors due to small math tables
//
#define CACHE_ALL_COLORMAP_TO_RAM         0       // if enabled this wastes 8.25 more kB, but will improve a little bit the performance
//
#define DEMO_SUPPORT                      1
#define DOOM2SUPPORT                      1
//
#define HAS_NETWORK                       1
//
#define DEBUG_SETUP                       0
#if DEBUG_SETUP
    #define SHOW_FPS_IN_HUD               true
    #define SHOW_FPS_IN_AMMO              true
    #define TIME_DEMO                     NULL // "demo3"
    #define NO_DEMO_LOOP                  true
    #define START_MAP                     1
#else
    #define SHOW_FPS_IN_HUD               true
    #define SHOW_FPS_IN_AMMO              false
    #define TIME_DEMO                     NULL // "demo3"
    #define NO_DEMO_LOOP                  true
    #define START_MAP                     1
#endif
#define WAD_FILE_NAME                     "WAD.WAD" // this is the name on SD card
//
// Board-independent configs
//
#define FPCLK                             80000000U
//
// Serial Output configuration
//
#define DEBUG_OUT_PRINTF                  1
//
#define UART_OVERSAMPLE                   16U
#define UART_BAUDRATE                     115200U
//
// Timer config
#define TICK_TIMER_FREQUENCY_HZ           (10000000UL)    // 10MHz
#define TIMER_TICKS_PER_MILLISECOND       (TICK_TIMER_FREQUENCY_HZ / 1000UL)
//
// TIMERS
// Free running timer, used for delays and timings
#define TICK_TIMER_NUMBER                 0
// Audio timer used for PWM if required. Using Timer 1 because can be routed anywhere, unlike 2, 3, 4
#define AUDIO_PWM_TIMER_NUMBER            1
#define AUDIO_SAMPLE_TIMER_NUMBER         2

#define FIRST_SPI_NUMBER                  0
#define SECOND_SPI_NUMBER                 1
#define FIRST_SPI_USART                   CAT(EUSART, FIRST_SPI_NUMBER)
#define SECOND_SPI_USART                  CAT(EUSART, SECOND_SPI_NUMBER)

// LDMA and PRS
#define FIRST_SPI_LDMA_CH                 (0)
#define SECOND_SPI_LDMA_CH                (FIRST_SPI_LDMA_CH + 1)
#define INTERLEAVED_SPI_PRS_CH            8     // DO NOT USE PRS 7, it's for BT
#define DISPLAY_LDMA_CH                   (SECOND_SPI_LDMA_CH + 3)
//
#define AUDIO_DMA_CHANNEL_L               (DISPLAY_LDMA_CH + 1)
#define AUDIO_DMA_CHANNEL_R               (AUDIO_DMA_CHANNEL_L + 1)
//
#define ADC_USES_DMA 1
//
#define IADC_LDMA_CH                      (AUDIO_DMA_CHANNEL_R + 1)
//
#define ENABLE_MUSIC                      1
#define MUSIC_NUM_SAMPLES                 1024
//
// Actual timer definitions.
#define TICK_TIMER                        CAT(TIMER, TICK_TIMER_NUMBER)
#define AUDIO_SAMPLE_TIMER                CAT(TIMER, AUDIO_SAMPLE_TIMER_NUMBER)
#define AUDIO_PWM_TIMER                   CAT(TIMER, AUDIO_PWM_TIMER_NUMBER)
//
#endif
