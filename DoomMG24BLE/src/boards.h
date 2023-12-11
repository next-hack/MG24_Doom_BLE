/***************************************************************************//**
 * @file boards.h
 * @brief board definitions.
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
 *
 ******************************************************************************/
// This file contains board definitions
#ifndef SRC_BOARDS_H_
#define SRC_BOARDS_H_
#ifdef __cplusplus
extern "C" {
#endif
#include "macros.h"
#include "config_enum_values.h"
// Select board here
#define BOARD                               BOARD_SPARKDOOM // choose from: BOARD_SPARKDOOM or BOARD_WSTK_3
//
#if BOARD == BOARD_WSTK_1
  // Ext Memory Config
  #define EXT_MEMORY_TYPE                       EXT_MEMORY_SPI_INTERLEAVED
  //
  // Keyboard config
  //
  //
  #define KEYBOARD                              I2C_KEYBOARD
  //
  #define I2C_SCL_PIN_NUMBER                    0
  #define I2C_SCL_PORT_NUMBER                   gpioPortA
  #define I2C_SDA_PIN_NUMBER                    7
  #define I2C_SDA_PORT_NUMBER                   gpioPortC
  //
  #define EXPANDER_I2C_ADDRESS                  0x27
  // Key definitions
  #define KEY_ALT                               0x08
  #define KEY_FIRE                              0x04
  #define KEY_USE                               0x02
  #define KEY_CHGW                              0x01
  #define KEY_UP                                0x40
  #define KEY_DOWN                              0x20
  #define KEY_LEFT                              0x10
  #define KEY_RIGHT                             0x80
  //
  // External Flash configuration
  //
  // First flash
  #define FIRST_FLASH_SPI_MOSI_PORT             gpioPortA
  #define FIRST_FLASH_SPI_MISO_PORT             gpioPortA
  #define FIRST_FLASH_SPI_CLK_PORT              gpioPortA
  #define FIRST_FLASH_SPI_MOSI_PIN              5
  #define FIRST_FLASH_SPI_MISO_PIN              3
  #define FIRST_FLASH_SPI_CLK_PIN               7
  // Second Flash
  #define SECOND_FLASH_SPI_MOSI_PORT            gpioPortA
  #define SECOND_FLASH_SPI_MISO_PORT            gpioPortB
  #define SECOND_FLASH_SPI_CLK_PORT             gpioPortC
  #define SECOND_FLASH_SPI_MOSI_PIN             4
  #define SECOND_FLASH_SPI_MISO_PIN             2
  #define SECOND_FLASH_SPI_CLK_PIN              9
  // common chip select for both flash
  #define FLASH_NCS_PORT                        gpioPortA
  #define FLASH_NCS_PIN                         6
  //
  // display config
  //
  // screen size
  #define CENTER_SCREEN                         1
  #define SCREEN_WIDTH                          320 //240
  #define SCREEN_HEIGHT                         240 //240
  #define DISPLAY_INVERT_XY                     1
  //
  //
  #define GAMMA_CORRECTION                      1      // This will create an almost linear gamma.
  #define GAMMA_SETTINGS                        0xf0, 0x7, 0xf, 0x0, 0x0, 0x30, 0x28, 0x54, 0x56, 0x3e, 0x1e, 0x1f, 0x37, 0x38   // used if GAMMA_CORRECTION is 1
  //
  #define DISPLAY_USES_RESET_INSTEAD_OF_NCS     1 // for displays without CS...
  //
  #define DISPLAY_DC_PORT                       gpioPortD
  #define DISPLAY_DC_PIN                        5
  //
  #define DISPLAY_RES_PORT                      gpioPortC
  #define DISPLAY_RES_PIN                       1
  // Comment NCS pin if not connected!
  //#define DISPLAY_NCS_PORT                      // not used
  //#define DISPLAY_NCS_PIN                       // not used
  //
  #define DISPLAY_SCK_PORT                      gpioPortC
  #define DISPLAY_SCK_PIN                       0
  //
  #define DISPLAY_MOSI_PORT                     gpioPortB
  #define DISPLAY_MOSI_PIN                      4
  // Virtual com port
  #define VCOM_ENABLE_PORT                      gpioPortB
  #define VCOM_ENABLE_PIN                       0
  //
  #define VCOM_TX_PORT                          gpioPortA
  #define VCOM_TX_PIN                           8
  //
  #define VCOM_RX_PORT                          gpioPortA
  #define VCOM_RX_PIN                           9
  // audio config
  #define STEREO_AUDIO                          1
  #define AUDIO_MODE                            DAC_AUDIO_MODE
  //
  #define AUDIO_PORT_L                          (gpioPortB)
  #define AUDIO_PIN_L                           3
  //
  #define AUDIO_PORT_R                          (gpioPortD)
  #define AUDIO_PIN_R                           4
#elif BOARD == BOARD_WSTK_2
  // Ext Memory Config
  #define EXT_MEMORY_TYPE                       EXT_MEMORY_SPI_INTERLEAVED
  //
  // Keyboard config
  //
  //
  #define KEYBOARD                              SPI74165_KEYBOARD
  //
  // Key definitions
  #define KEY_ALT                               0x80
  #define KEY_FIRE                              0x20
  #define KEY_USE                               0x40
  #define KEY_CHGW                              0x10
  #define KEY_UP                                0x01
  #define KEY_DOWN                              0x04
  #define KEY_LEFT                              0x02
  #define KEY_RIGHT                             0x08
  //
  #define SR_MISO_PORT                          gpioPortD
  #define SR_MISO_PIN                           3
  //
  #define SR_CLK_PORT                           gpioPortC
  #define SR_CLK_PIN                            6
  //
  #define SR_PL_PORT                            gpioPortC
  #define SR_PL_PIN                             8
  //
  // SD related stuff
  #define HAS_SD                                1
  #define SD_NCS_PORT                           gpioPortC
  #define SD_NCS_PIN                            1
  #define SD_MISO_PORT                          gpioPortC
  #define SD_MISO_PIN                           3


  // Not used i2c stuff
  // #define I2C_SCL_PIN_NUMBER                    // not used
  // #define I2C_SCL_PORT_NUMBER                   // not used
  // #define I2C_SDA_PIN_NUMBER                    // not used
  // #define I2C_SDA_PORT_NUMBER                   // not used
  // #define EXPANDER_I2C_ADDRESS                  // not used
  //
  // External Flash configuration
  //
  // First flash
  #define FIRST_FLASH_SPI_MOSI_PORT             gpioPortA
  #define FIRST_FLASH_SPI_MISO_PORT             gpioPortA
  #define FIRST_FLASH_SPI_CLK_PORT              gpioPortA
  #define FIRST_FLASH_SPI_MOSI_PIN              7
  #define FIRST_FLASH_SPI_MISO_PIN              3
  #define FIRST_FLASH_SPI_CLK_PIN               6
  // Second Flash
  #define SECOND_FLASH_SPI_MOSI_PORT            gpioPortB
  #define SECOND_FLASH_SPI_MISO_PORT            gpioPortD
  #define SECOND_FLASH_SPI_CLK_PORT             gpioPortD
  #define SECOND_FLASH_SPI_MOSI_PIN             4
  #define SECOND_FLASH_SPI_MISO_PIN             4
  #define SECOND_FLASH_SPI_CLK_PIN              5
  // common chip select for both flash
  #define FLASH_NCS_PORT                        gpioPortA
  #define FLASH_NCS_PIN                         5
  //
  // display config
  //
  // screen size
  #define CENTER_SCREEN                         1
  #define SCREEN_WIDTH                          320
  #define SCREEN_HEIGHT                         240 //240
  #define DISPLAY_INVERT_XY                     1
  //
  //
  #define GAMMA_CORRECTION                      1      // This will create an almost linear gamma.
  #define GAMMA_SETTINGS                        0xf0, 0x7, 0xf, 0x0, 0x0, 0x30, 0x28, 0x54, 0x56, 0x3e, 0x1e, 0x1f, 0x37, 0x38   // used if GAMMA_CORRECTION is 1
  //
  #define DISPLAY_USES_RESET_INSTEAD_OF_NCS     0 // for displays without CS...
  //
  #define DISPLAY_DC_PORT                       gpioPortA
  #define DISPLAY_DC_PIN                        0
  //
  #define DISPLAY_RES_PORT                      // not used
  #define DISPLAY_RES_PIN                       // not used
  //
  #define DISPLAY_NCS_PORT                      gpioPortA
  #define DISPLAY_NCS_PIN                       4
  //
  #define DISPLAY_SCK_PORT                      gpioPortC
  #define DISPLAY_SCK_PIN                       0
  //
  #define DISPLAY_MOSI_PORT                     gpioPortD
  #define DISPLAY_MOSI_PIN                      2
  // Virtual com port
  #define VCOM_ENABLE_PORT                      gpioPortB
  #define VCOM_ENABLE_PIN                       0
  //
  #define VCOM_TX_PORT                          gpioPortA
  #define VCOM_TX_PIN                           8
  //
  #define VCOM_RX_PORT                          gpioPortA
  #define VCOM_RX_PIN                           9
  // audio config
  #define STEREO_AUDIO                          1
  #define AUDIO_MODE                            DAC_AUDIO_MODE
  //
  #define AUDIO_PORT_L                          (gpioPortB)
  #define AUDIO_PIN_L                           2
  //
  #define AUDIO_PORT_R                          (gpioPortB)
  #define AUDIO_PIN_R                           3

#elif BOARD == BOARD_WSTK_3
  // Ext Memory Config
  #define EXT_MEMORY_TYPE                       EXT_MEMORY_SPI_INTERLEAVED

  //
  // Keyboard config
  //
  //
  #define KEYBOARD                              SPI74165_KEYBOARD
  //
  // Key definitions
  #define KEY_ALT                               0x80
  #define KEY_FIRE                              0x20
  #define KEY_USE                               0x40
  #define KEY_CHGW                              0x10
  #define KEY_UP                                0x01
  #define KEY_DOWN                              0x04
  #define KEY_LEFT                              0x02
  #define KEY_RIGHT                             0x08
  //
  #define SR_MISO_PORT                          gpioPortD
  #define SR_MISO_PIN                           3
  //
  #define SR_CLK_PORT                           gpioPortC
  #define SR_CLK_PIN                            6
  //
  #define SR_PL_PORT                            gpioPortC
  #define SR_PL_PIN                             8

  // SD related stuff
  #define HAS_SD                                1
  #define SD_NCS_PORT                           gpioPortA
  #define SD_NCS_PIN                            4
  #define SD_MISO_PORT                          gpioPortC
  #define SD_MISO_PIN                           1
  // Not used i2c stuff
  // #define I2C_SCL_PIN_NUMBER                    // not used
  // #define I2C_SCL_PORT_NUMBER                   // not used
  // #define I2C_SDA_PIN_NUMBER                    // not used
  // #define I2C_SDA_PORT_NUMBER                   // not used
  // #define EXPANDER_I2C_ADDRESS                  // not used
  //
  // External Flash configuration
  //
  // First flash
  #define FIRST_FLASH_SPI_MOSI_PORT             gpioPortA
  #define FIRST_FLASH_SPI_MISO_PORT             gpioPortA
  #define FIRST_FLASH_SPI_CLK_PORT              gpioPortA
  #define FIRST_FLASH_SPI_MOSI_PIN              7
  #define FIRST_FLASH_SPI_MISO_PIN              3
  #define FIRST_FLASH_SPI_CLK_PIN               6
  // Second Flash
  #define SECOND_FLASH_SPI_MOSI_PORT            gpioPortB
  #define SECOND_FLASH_SPI_MISO_PORT            gpioPortD
  #define SECOND_FLASH_SPI_CLK_PORT             gpioPortD
  #define SECOND_FLASH_SPI_MOSI_PIN             4
  #define SECOND_FLASH_SPI_MISO_PIN             4
  #define SECOND_FLASH_SPI_CLK_PIN              5
  // common chip select for both flash
  #define FLASH_NCS_PORT                        gpioPortA
  #define FLASH_NCS_PIN                         5
  //
  // display config
  //
  // screen size
  #define CENTER_SCREEN                         1
  #define SCREEN_WIDTH                          320
  #define SCREEN_HEIGHT                         240 //240
  #define DISPLAY_INVERT_XY                     1
  //
  //
  #define GAMMA_CORRECTION                      1      // This will create an almost linear gamma.
  #define GAMMA_SETTINGS                        0xf0, 0x7, 0xf, 0x0, 0x0, 0x30, 0x28, 0x54, 0x56, 0x3e, 0x1e, 0x1f, 0x37, 0x38   // used if GAMMA_CORRECTION is 1
  //
  #define DISPLAY_USES_RESET_INSTEAD_OF_NCS     0 // for displays without CS...
  //
  #define DISPLAY_DC_PORT                       gpioPortC//gpioPortA
  #define DISPLAY_DC_PIN                        0
  //
  #define DISPLAY_RES_PORT                      // not used
  #define DISPLAY_RES_PIN                       // not used
  //
  #define DISPLAY_NCS_PORT                      gpioPortC
  #define DISPLAY_NCS_PIN                       3
  //
  #define DISPLAY_SCK_PORT                      gpioPortA //gpioPortC
  #define DISPLAY_SCK_PIN                       0
  //
  #define DISPLAY_MOSI_PORT                     gpioPortD
  #define DISPLAY_MOSI_PIN                      2
  // Virtual com port
  #define VCOM_ENABLE_PORT                      gpioPortB
  #define VCOM_ENABLE_PIN                       0
  //
  #define VCOM_TX_PORT                          gpioPortA
  #define VCOM_TX_PIN                           8
  //
  #define VCOM_RX_PORT                          gpioPortA
  #define VCOM_RX_PIN                           9
  // audio config
  #define STEREO_AUDIO                          1
  #define AUDIO_MODE                            DAC_AUDIO_MODE
  //
  #define AUDIO_PORT_L                          (gpioPortB)
  #define AUDIO_PIN_L                           2
  //
  #define AUDIO_PORT_R                          (gpioPortB)
  #define AUDIO_PIN_R                           3
#elif BOARD == BOARD_SPARKDOOM
  // Ext Memory Config
  #define EXT_MEMORY_TYPE                       EXT_MEMORY_SPI_INTERLEAVED

  //
  // Keyboard config
  //
  //
  #define KEYBOARD                              SPI74165_KEYBOARD
  //
  // Key definitions
  #define KEY_ALT                               0x20
  #define KEY_FIRE                              0x80
  #define KEY_USE                               0x10
  #define KEY_CHGW                              0x08
  #define KEY_UP                                0x01
  #define KEY_DOWN                              0x40
  #define KEY_LEFT                              0x02
  #define KEY_RIGHT                             0x04
  //
  #define SR_MISO_PORT                          gpioPortD
  #define SR_MISO_PIN                           1
  //
  #define SR_CLK_PORT                           gpioPortD
  #define SR_CLK_PIN                            0
  //
  #define SR_PL_PORT                            gpioPortA
  #define SR_PL_PIN                             0

  // SD related stuff
  #define HAS_SD                                1
  #define SD_NCS_PORT                           gpioPortC
  #define SD_NCS_PIN                            1
  #define SD_MISO_PORT                          gpioPortC
  #define SD_MISO_PIN                           6
  // Not used i2c stuff
  // #define I2C_SCL_PIN_NUMBER                    // not used
  // #define I2C_SCL_PORT_NUMBER                   // not used
  // #define I2C_SDA_PIN_NUMBER                    // not used
  // #define I2C_SDA_PORT_NUMBER                   // not used
  // #define EXPANDER_I2C_ADDRESS                  // not used
  //
  // External Flash configuration
  //
  // First flash
  #define FIRST_FLASH_SPI_MOSI_PORT             gpioPortB
  #define FIRST_FLASH_SPI_MISO_PORT             gpioPortB
  #define FIRST_FLASH_SPI_CLK_PORT              gpioPortB
  #define FIRST_FLASH_SPI_MOSI_PIN              0
  #define FIRST_FLASH_SPI_MISO_PIN              2
  #define FIRST_FLASH_SPI_CLK_PIN               1
  // Second Flash
  #define SECOND_FLASH_SPI_MOSI_PORT            gpioPortC
  #define SECOND_FLASH_SPI_MISO_PORT            gpioPortA
  #define SECOND_FLASH_SPI_CLK_PORT             gpioPortC
  #define SECOND_FLASH_SPI_MOSI_PIN             5
  #define SECOND_FLASH_SPI_MISO_PIN             4
  #define SECOND_FLASH_SPI_CLK_PIN              4
  // common chip select for both flash
  #define FLASH_NCS_PORT                        gpioPortA
  #define FLASH_NCS_PIN                         0
  //
  // display config
  //
  // screen size
  #define CENTER_SCREEN                         1
  #define SCREEN_WIDTH                          320
  #define SCREEN_HEIGHT                         240 // 200 or 240
  #define DISPLAY_INVERT_XY                     1
  //
  //
  #define GAMMA_CORRECTION                      1      // This will create an almost linear gamma.
  #define GAMMA_SETTINGS                        0xf0, 0x7, 0xf, 0x0, 0x0, 0x30, 0x28, 0x54, 0x56, 0x3e, 0x1e, 0x1f, 0x37, 0x38   // used if GAMMA_CORRECTION is 1
  //
  #define DISPLAY_USES_RESET_INSTEAD_OF_NCS     0 // for displays without CS...
  //
  #define DISPLAY_DC_PORT                       gpioPortC
  #define DISPLAY_DC_PIN                        7
  //
  #define DISPLAY_RES_PORT                      // not used
  #define DISPLAY_RES_PIN                       // not used
  //
  #define DISPLAY_NCS_PORT                      gpioPortC
  #define DISPLAY_NCS_PIN                       0
  //
  #define DISPLAY_SCK_PORT                      gpioPortC
  #define DISPLAY_SCK_PIN                       2
  //
  #define DISPLAY_MOSI_PORT                     gpioPortC
  #define DISPLAY_MOSI_PIN                      3
  // Virtual com port is always on
  //#define VCOM_ENABLE_PORT
  //#define VCOM_ENABLE_PIN
  //
  #define VCOM_TX_PORT                          gpioPortA
  #define VCOM_TX_PIN                           5
  //
  #define VCOM_RX_PORT                          gpioPortA
  #define VCOM_RX_PIN                           6
  // audio config
  #define STEREO_AUDIO                          1
  #define AUDIO_MODE                            DAC_AUDIO_MODE
  //
  #define AUDIO_PORT_L                          (gpioPortD)
  #define AUDIO_PIN_L                           3
  //
  #define AUDIO_PORT_R                          (gpioPortD)
  #define AUDIO_PIN_R                           2

  #else
#error unsupported board
#endif
// common hardware-related macros and defines
#define FLASH_NCS_LOW()                   GPIO->P_CLR[FLASH_NCS_PORT].DOUT = (1 << FLASH_NCS_PIN)
#define FLASH_NCS_HIGH()                  GPIO->P_SET[FLASH_NCS_PORT].DOUT = (1 << FLASH_NCS_PIN)
//
#ifdef __cplusplus
}
#endif
#endif /* SRC_BOARDS_H_ */
