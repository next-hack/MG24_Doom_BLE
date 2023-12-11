/**
 *  Keyboard interface for Doom Port to the EFR32xG2x MCUs by Nicola Wrachien
 *
 *  Copyright (C) 2021-23 Nicola Wrachien (next-hack in the comments).
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
 * Description: Keyboard handling functions for different types
 * (i2c, shift register).
 */
#include "main.h"
#include "keyboard.h"
#include "printf.h"
#include "delay.h"
#include "em_gpio.h"
#include "em_i2c.h"
#include "em_cmu.h"


#if KEYBOARD == PARALLEL_KEYBOARD
#if 0
    const uint8_t pins[] =
    {
        PIN_KEY_UP,
        PIN_KEY_DOWN,
        PIN_KEY_LEFT,
        PIN_KEY_RIGHT,
        PIN_KEY_USE,
        PIN_KEY_CHGW,
        PIN_KEY_ALT,
        PIN_KEY_FIRE
    };
#endif
    const uint8_t keys[] = 
    {
        KEY_UP,
        KEY_DOWN,
        KEY_LEFT,
        KEY_RIGHT,
        KEY_USE,
        KEY_CHGW,
        KEY_ALT,
        KEY_FIRE,
    };
#endif
#if KEYBOARD == I2C_KEYBOARD
//
// for port expander
//
#define MCP23008_IODIR 0
#define MCP23008_IPOL 1
#define MCP23008_GPINTEN 2
#define MCP23008_DEFVAL 3
#define MCP23008_INTCON 4
#define MCP23008_IOCON 5
#define MCP23008_GPPU 6
#define MCP23008_INTF 7
#define MCP23008_INTCAP 8
#define MCP23008_GPIO 9
#define MCP23008_OLAT 10
//
I2C_TransferReturn_TypeDef i2cTransfer( I2C_TransferSeq_TypeDef *seq)
{
  I2C_TransferReturn_TypeDef rc = i2cTransferDone;
  uint32_t timeout = 10000000;

  /* Start a polled transfer */
  rc = I2C_TransferInit(I2C0, seq);
  while ((i2cTransferInProgress == rc) && (timeout--))
  {
    rc = I2C_Transfer(I2C0);
  }
  return rc;
}

void i2cSendAndWait(uint8_t addr, uint8_t value)
{
    uint8_t buf[2];
    I2C_TransferSeq_TypeDef seq =
    {
        .addr = EXPANDER_I2C_ADDRESS << 1,
        .flags = I2C_FLAG_WRITE
    };
    seq.buf[0].data = buf;
    seq.buf[0].len = 2;
    buf[0] = addr;
    buf[1] = value;
    i2cTransfer(&seq);
}
void updateI2cKeyboard(uint8_t *keys)
{
  uint8_t wrBuf;
  I2C_TransferSeq_TypeDef seq =
  {
      .addr = EXPANDER_I2C_ADDRESS << 1,
      .flags = I2C_FLAG_WRITE_READ,
  };
  wrBuf = MCP23008_GPIO;
  // write confi
  seq.buf[0].data = &wrBuf;
  seq.buf[0].len = 1;
  // read config: directly on keys value
  seq.buf[1].data = keys;
  seq.buf[1].len = 1;
  i2cTransfer(&seq);
}
void initI2cKeyboard()
{
  // enable i2c clk
  CMU_ClockEnable(cmuClock_I2C0, true);
  // configure GPIO
  GPIO_PinModeSet(I2C_SDA_PORT_NUMBER, I2C_SDA_PIN_NUMBER, gpioModeWiredAndPullUp, 1);
  GPIO_PinModeSet(I2C_SCL_PORT_NUMBER, I2C_SCL_PIN_NUMBER, gpioModeWiredAndPullUp, 1);
  // to avoid issue if the device is reset in between an i2c transaction.
  for (int i = 0; i < 9; i++)
  {
    GPIO_PinOutClear(I2C_SCL_PORT_NUMBER, I2C_SCL_PIN_NUMBER);
    delay(10);
    GPIO_PinOutSet(I2C_SCL_PORT_NUMBER, I2C_SCL_PIN_NUMBER);
    delay(10);
  }
  GPIO->I2CROUTE[0].SDAROUTE = (I2C_SDA_PORT_NUMBER << _GPIO_I2C_SDAROUTE_PORT_SHIFT) | (I2C_SDA_PIN_NUMBER << _GPIO_I2C_SDAROUTE_PIN_SHIFT);
  GPIO->I2CROUTE[0].SCLROUTE = (I2C_SCL_PORT_NUMBER << _GPIO_I2C_SCLROUTE_PORT_SHIFT) | (I2C_SCL_PIN_NUMBER << _GPIO_I2C_SCLROUTE_PIN_SHIFT);
  GPIO->I2CROUTE[0].ROUTEEN = GPIO_I2C_ROUTEEN_SCLPEN | GPIO_I2C_ROUTEEN_SDAPEN;
  I2C_Init_TypeDef init = I2C_INIT_DEFAULT;
  init.freq = 400000;
  I2C_Init(I2C0, &init);
  //

    // configure port expander
    // all input
    i2cSendAndWait(MCP23008_IODIR, 255);
    // all pull up
    i2cSendAndWait(MCP23008_GPPU, 255);
    // wait some time, because pull ups are quite weak and there is 100nF cap
    delay(10);
}
#endif
#if KEYBOARD == PARALLEL_KEYBOARD

void initParallelKeyboard()
{
 // error not implemented !
}
#endif
#if KEYBOARD == SPI74165_KEYBOARD
void initSpi74165Keyboard()
{
  //
  GPIO_PinModeSet(SR_MISO_PORT, SR_MISO_PIN, gpioModeInput, 1);
  GPIO_PinModeSet(SR_PL_PORT, SR_PL_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(SR_CLK_PORT, SR_CLK_PIN, gpioModePushPull, 1);
}
#endif
void initKeyboard()
{
#if KEYBOARD == PARALLEL_KEYBOARD
    initParallelKeyboard();
#elif KEYBOARD == I2C_KEYBOARD
    initI2cKeyboard();
#elif KEYBOARD == SPI74165_KEYBOARD
    initSpi74165Keyboard();
#else
#error You should have a keyboard to play Doom, right?
#endif
}

void getKeys(uint8_t * keys)
{
#if KEYBOARD == PARALLEL_KEYBOARD
    uint8_t buttons = 0;
    for (int i = 0; i < 8; i++)
    {
#if 0
      uint32_t pressed = !(ports[i]->IN & (1 << pins[i]));
        if (pressed)
        {
            buttons |= keys[i];
        }
#endif
    }
    *keys = buttons;

#elif KEYBOARD == I2C_KEYBOARD
    updateI2cKeyboard(keys);
    *keys = ~ *keys;
#elif KEYBOARD == SPI74165_KEYBOARD
    uint8_t buttons = 0;
    // pulse on parallel load
    // Note some chips require some delay. Repeating the instruction twice.
    // start with clock low
    GPIO->P_CLR[SR_CLK_PORT].DOUT = 1 << SR_CLK_PIN;
    // shift mode
    GPIO->P_SET[SR_PL_PORT].DOUT = 1 << SR_PL_PIN;
    GPIO->P_SET[SR_PL_PORT].DOUT = 1 << SR_PL_PIN;
    // Load
    GPIO->P_CLR[SR_PL_PORT].DOUT = 1 << SR_PL_PIN;
    GPIO->P_CLR[SR_PL_PORT].DOUT = 1 << SR_PL_PIN;
    // shift mode
    GPIO->P_SET[SR_PL_PORT].DOUT = 1 << SR_PL_PIN;
    GPIO->P_SET[SR_PL_PORT].DOUT = 1 << SR_PL_PIN;
    //
    for (int i = 0; i < 8; i++)
    {
      // read data (should be already there).
      uint8_t bit = (GPIO->P[SR_MISO_PORT].DIN >> SR_MISO_PIN) & 1;
      // clock high
      GPIO->P_SET[SR_CLK_PORT].DOUT = 1 << SR_CLK_PIN;
      GPIO->P_SET[SR_CLK_PORT].DOUT = 1 << SR_CLK_PIN;
      // clock low
      GPIO->P_CLR[SR_CLK_PORT].DOUT = 1 << SR_CLK_PIN;
      GPIO->P_CLR[SR_CLK_PORT].DOUT = 1 << SR_CLK_PIN;
      //
      buttons = buttons << 1;
      buttons |= bit;

    }
    *keys =  ~buttons;
#else
    *keys = 0;
    #warning define some keyboard!
#endif
}
