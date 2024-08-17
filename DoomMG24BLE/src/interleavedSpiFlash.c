/**
 * Interleaved SPI flash implementation. Two SPI Flash IC are accessed
 * simultaneously to increase thoughput (typically 8MB @ 40 MHz)
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
 */
#include "interleavedSpiFlash.h"
#include "em_cmu.h"
#include "em_ldma.h"
#include "em_prs.h"
#include "main.h"
#include "boards.h"
#include "delay.h"
#include "printf.h"
//
#define SPI_FLASH_WRITE_ENABLE_CMD              0x06
#define SPI_FLASH_PAGE_PROGRAM_CMD              0x02
#define SPI_FLASH_STATUS_REGISTER_READ_CMD      0x05
#define SPI_FLASH_CHIP_ERASE                    0xC7
#define SPI_FLASH_SECTOR_ERASE                  0x20
#define SPI_FLASH_BLOCK32K_ERASE                0x52
#define SPI_FLASH_BLOCK64K_ERASE                0xD8
#define SPI_FLASH_MFG_ID                        0x90
#define SPI_FLASH_READ_DATA                     0x03
#define SPI_FLASH_STATUS_REGISTER_BUSY          1
#define SPI_FLASH_PAGE_SIZE                     256
// ID for common flash sizes
#define ID_4M                                   0x15
#define ID_8M                                   0x16
#define ID_16M                                  0x17
//
static void interleavedSpiSet8BitDataOperation(void);
static void interleavedSpiSet16BitDataOperation(void);
static void interleavedSpiFlashWaitBusy();
static void interleavedSpiFlashWriteEnable();

//
uint32_t flashSize = 0;
interleavedSpiData_t interleavedSpiData = {0};
//
uint8_t *currentSpiAddress;

static  LDMA_Descriptor_t ldma_DualRX_SPI_EUSART0_descriptors[] =
{
  {
    .xfer =
    {
      .structType   = ldmaCtrlStructTypeXfer,
      .structReq    = 0,
      .xferCnt      = 31,                    // 2048 is the maximum tx size
      .byteSwap     = 0,
      .blockSize    = ldmaCtrlBlockSizeUnit4,
      .doneIfs      = 0,                       // no ISR on done
      .reqMode      = ldmaCtrlReqModeBlock,       //
      .decLoopCnt   = 0,                       // do not enable loop
      .ignoreSrec   = 1,
      .srcInc       = ldmaCtrlSrcIncNone,
      .size         = ldmaCtrlSizeHalf,
      .dstInc       = ldmaCtrlDstIncTwo,  // modified
      .srcAddrMode  = ldmaCtrlSrcAddrModeAbs,
      .dstAddrMode  = ldmaCtrlDstAddrModeAbs,
      .srcAddr      = (uint32_t)(&EUSART0->RXDATA),
      .dstAddr      = (uint32_t) (&interleavedSpiData.rxBuffer),
      .linkMode     = ldmaLinkModeAbs,
      .link         = 0,
      .linkAddr     = (uint32_t) 0
    }

  },
};
//
static LDMA_Descriptor_t ldma_DualRX_SPI_EUSART1_descriptors[] =
{
  {
    .xfer =
    {
      .structType   = ldmaCtrlStructTypeXfer,
      .structReq    = 0,
      .xferCnt      = 31,                    // 2048 is the maximum tx size
      .byteSwap     = 0,
      .blockSize    = ldmaCtrlBlockSizeUnit4,
      .doneIfs      = 0,                       // no ISR on done
      .reqMode      = ldmaCtrlReqModeBlock,       //
      .decLoopCnt   = 0,                       // do not enable loop
      .ignoreSrec   = 1,
      .srcInc       = ldmaCtrlSrcIncNone,
      .size         = ldmaCtrlSizeHalf,
      .dstInc       = ldmaCtrlDstIncTwo,  // modified
      .srcAddrMode  = ldmaCtrlSrcAddrModeAbs,
      .dstAddrMode  = ldmaCtrlDstAddrModeAbs,
      .srcAddr      = (uint32_t)(&EUSART1->RXDATA),
      .dstAddr      =  ((uint32_t) (&interleavedSpiData.rxBuffer)) + 2,
      .linkMode     = ldmaLinkModeAbs,
      .link         = 0,
      .linkAddr     = (uint32_t) 0
    }
  },
};
//
void interleavedSpiFlashDmaInit(void)
{
  LDMA->EN = LDMA_EN_EN;
  // initially disabled
  LDMA->CHDIS = (3 << FIRST_SPI_LDMA_CH);
  // this just configure DMA descriptors.
  // common config
  uint16_t size = 32;
  for (int i = FIRST_SPI_LDMA_CH; i < FIRST_SPI_LDMA_CH + 2; i++)
  {
    LDMAXBAR->CH[i].REQSEL = (i == FIRST_SPI_LDMA_CH) ? ldmaPeripheralSignal_EUSART0_RXFL : ldmaPeripheralSignal_EUSART1_RXFL;
    LDMA->CH[i].LOOP = 0;
    LDMA->CH[i].CFG = (ldmaCfgArbSlotsAs1 << _LDMA_CH_CFG_ARBSLOTS_SHIFT)
                         | (ldmaCfgSrcIncSignPos << _LDMA_CH_CFG_SRCINCSIGN_SHIFT)
                         | (ldmaCfgDstIncSignPos << _LDMA_CH_CFG_DSTINCSIGN_SHIFT);
    LDMA->CH[i].CTRL =  LDMA_CH_CTRL_DSTMODE_ABSOLUTE |
                        LDMA_CH_CTRL_SRCMODE_ABSOLUTE |
                        LDMA_CH_CTRL_DSTINC_TWO |
                        LDMA_CH_CTRL_SIZE_HALFWORD |
                        LDMA_CH_CTRL_SRCINC_NONE |
                        LDMA_CH_CTRL_IGNORESREQ |
                        LDMA_CH_CTRL_REQMODE_BLOCK |
                        LDMA_CH_CTRL_BLOCKSIZE_UNIT4 |
                        ((size / 4) <<_LDMA_CH_CTRL_XFERCNT_SHIFT) |
                        LDMA_CH_CTRL_STRUCTTYPE_TRANSFER;
    LDMA->CH[i].LINK = 0;
    LDMA->CH[i].SRC = (i == FIRST_SPI_LDMA_CH) ? (uint32_t)(&FIRST_SPI_USART->RXDATA) : (uint32_t)(&SECOND_SPI_USART->RXDATA);
    LDMA->CH[i].DST = (i == FIRST_SPI_LDMA_CH) ? (uint32_t)(&interleavedSpiData.rxBuffer[0]) : (uint32_t)(&interleavedSpiData.rxBuffer[2]);
  }
}
void interleavedSpiFlashPrsInit(void)
{
  EUSART0->TRIGCTRL = EUSART_TRIGCTRL_TXTEN | EUSART_TRIGCTRL_AUTOTXTEN | EUSART_TRIGCTRL_RXTEN;
  //while (EUSART0->SYNCBUSY);
  EUSART1->TRIGCTRL = EUSART_TRIGCTRL_TXTEN | EUSART_TRIGCTRL_AUTOTXTEN | EUSART_TRIGCTRL_RXTEN;
  //while (EUSART1->SYNCBUSY);
  PRS_ConnectConsumer(INTERLEAVED_SPI_PRS_CH, prsTypeAsync, prsConsumerEUSART0_TRIGGER);
  PRS_ConnectConsumer(INTERLEAVED_SPI_PRS_CH, prsTypeAsync, prsConsumerEUSART1_TRIGGER);
}
int interleavedSpiFlashGetSize(void)
{
    if (0 == flashSize)
    {
        uint8_t oldMode =   interleavedSpiData.mode;
        if (oldMode)
        {
          interleavedSpiSet8BitDataOperation();
        }
        FLASH_NCS_LOW();
        interleavedSpiReadSameData(SPI_FLASH_MFG_ID);
        interleavedSpiReadSameData(0);
        interleavedSpiReadSameData(0);
        interleavedSpiReadSameData(0);
        interleavedSpiReadSameData(0);
        interleavedSpiTxRxData_t id;
        id = interleavedSpiReadSameData(0);
        uint32_t firstFlashSize = 4096 * 1024 << (id.firstData - ID_4M);
        uint32_t secondFlashSize = 4096 * 1024 << (id.secondData - ID_4M);
        FLASH_NCS_HIGH();
        flashSize = firstFlashSize < secondFlashSize ? 2 * firstFlashSize : 2 * secondFlashSize;
        if (oldMode)
        {
          interleavedSpiSet16BitDataOperation();
        }
    }
    return flashSize;
}

void interleavedSpiFlashChipErase(void)
{
    uint8_t oldMode =   interleavedSpiData.mode;
    if (oldMode)
    {
      interleavedSpiSet8BitDataOperation();
    }
    FLASH_NCS_HIGH();
    interleavedSpiFlashWriteEnable();
    FLASH_NCS_LOW();
    interleavedSpiReadSameData(SPI_FLASH_CHIP_ERASE);
    FLASH_NCS_HIGH();
    interleavedSpiFlashWaitBusy();
    if (oldMode)
    {
      interleavedSpiSet16BitDataOperation();
    }
}
// note: this will erase TWO sectors.
void interleavedSpiFlashEraseTwoSectors(uint32_t logicalAddress)
{
    uint32_t address = logicalAddress >> 1;
    uint8_t oldMode =   interleavedSpiData.mode;
    if (oldMode)
    {
      interleavedSpiSet8BitDataOperation();
    }
    FLASH_NCS_HIGH();
    interleavedSpiFlashWriteEnable();
    FLASH_NCS_LOW();
    // command and address
    interleavedSpiReadSameData(SPI_FLASH_SECTOR_ERASE);
    interleavedSpiReadSameData(address >> 16);
    interleavedSpiReadSameData(address >> 8);
    interleavedSpiReadSameData(address);
    //
    FLASH_NCS_HIGH();
    interleavedSpiFlashWaitBusy();
    if (oldMode)
    {
      interleavedSpiSet16BitDataOperation();
    }
}
/**
 * @brief erases 2-64kB sectors, one per chip, at the specified logical address.
 * This means that each chip will have a 64-kB sector, located at address / 2 eased
 * @param address (logical)
 */
void interleavedSpiFlashEraseTwo64kBlocks(uint32_t logicalAddress)
{
  uint32_t address = logicalAddress >> 1;
  uint8_t oldMode =   interleavedSpiData.mode;
  if (oldMode)
  {
    interleavedSpiSet8BitDataOperation();
  }
  FLASH_NCS_HIGH();
  interleavedSpiFlashWriteEnable();
  FLASH_NCS_LOW();
  // command and address
  interleavedSpiReadSameData(SPI_FLASH_BLOCK64K_ERASE);
  interleavedSpiReadSameData(address >> 16);
  interleavedSpiReadSameData(address >> 8);
  interleavedSpiReadSameData(address);
  //
  FLASH_NCS_HIGH();
  interleavedSpiFlashWaitBusy();
  if (oldMode)
  {
    interleavedSpiSet16BitDataOperation();
  }
}
/**
 * @brief erases 2-32kB sectors, one per chip, at the specified logical address.
 * This means that each chip will have a 64-kB sector, located at address / 2 eased
 * @param address (logical)
 */
void interleavedSpiFlashEraseTwo32kBlocks(uint32_t logicalAddress)
{
  uint32_t address = logicalAddress >> 1;
  uint8_t oldMode =   interleavedSpiData.mode;
  if (oldMode)
  {
    interleavedSpiSet8BitDataOperation();
  }
  FLASH_NCS_HIGH();
  interleavedSpiFlashWriteEnable();
  FLASH_NCS_LOW();
  // command and address
  interleavedSpiReadSameData(SPI_FLASH_BLOCK32K_ERASE);
  interleavedSpiReadSameData(address >> 16);
  interleavedSpiReadSameData(address >> 8);
  interleavedSpiReadSameData(address);
  //
  FLASH_NCS_HIGH();
  interleavedSpiFlashWaitBusy();
  if (oldMode)
  {
    interleavedSpiSet16BitDataOperation();
  }
}

void interleavedSpiFlashDriveStrength(void)
{
    // set enable write of volatile SR
    FLASH_NCS_LOW();
    interleavedSpiReadSameData(0x50);        // write enable volatile sr
    FLASH_NCS_HIGH();
    FLASH_NCS_LOW();
    interleavedSpiReadSameData(0x11);        // SR
    interleavedSpiReadSameData(0x0);
    FLASH_NCS_HIGH();
    interleavedSpiFlashWaitBusy();
}

static void interleavedSpiFlashWaitBusy(void)
{
    interleavedSpiTxRxData_t result;
    FLASH_NCS_LOW();
    result = interleavedSpiReadSameData(SPI_FLASH_STATUS_REGISTER_READ_CMD);
    do
    {
        result = interleavedSpiReadSameData(0xFF);
        if (result.firstData == 0xFF || result.secondData == 0xFF)
        {
          FLASH_NCS_HIGH();
          delay(1);
          FLASH_NCS_LOW();
          result = interleavedSpiReadSameData(SPI_FLASH_STATUS_REGISTER_READ_CMD);
          result = interleavedSpiReadSameData(0xFF);

        }
    } while ((result.firstData & SPI_FLASH_STATUS_REGISTER_BUSY) || (result.secondData & SPI_FLASH_STATUS_REGISTER_BUSY));
    FLASH_NCS_HIGH();

}
static void interleavedSpiFlashWriteEnable(void)
{
    FLASH_NCS_LOW();
    interleavedSpiReadSameData(SPI_FLASH_WRITE_ENABLE_CMD);
    FLASH_NCS_HIGH();

}
void interleavedSpiFlashRestoreDataMode(void)
{
  if (interleavedSpiFlashGetDataMode())
  {
    interleavedSpiSet16BitDataOperation();
  }
  else
  {
    interleavedSpiSet8BitDataOperation();
  }
}
int interleavedSpiFlashGetDataMode(void)
{
  return interleavedSpiData.mode;
}
/**
 * @brief program the two spi flash, by interleaving halfwords.
 * Note that to increase DMA throughput, we write the even 16-bit halfwords to flash 0, and odd 16-bit halfwords to flash 1
 * @param address. Must be multiple of 4.
 * @param buffer
 * @param size. Must be multiple of 4.
 */
void interleavedSpiFlashProgram(uint32_t address, uint8_t *buffer, uint32_t size)
{
    if (!size)
        return;
    uint8_t oldMode =   interleavedSpiData.mode;
    if (oldMode)
    {
      interleavedSpiSet8BitDataOperation();
    }
    uint32_t written = 0;
    uint32_t i = 0;
    address = address & ~3 & SPI_ADDRESS_MASK;    // we can only program at word boundaries
    while (written < size)
    {
        FLASH_NCS_HIGH();
        interleavedSpiFlashWriteEnable();
        FLASH_NCS_LOW();
        interleavedSpiReadSameData(SPI_FLASH_PAGE_PROGRAM_CMD);
        // there are two flash ICs, so the address we must set is divided by two
        uint32_t flashAddress = address >> 1;
        interleavedSpiReadSameData(flashAddress >> 16);
        interleavedSpiReadSameData(flashAddress >> 8);
        interleavedSpiReadSameData(flashAddress);
        // we can only program within one page at once.

        uint32_t maxWrite = 2 * SPI_FLASH_PAGE_SIZE - (address & (2 * SPI_FLASH_PAGE_SIZE - 1));
        for (i = 0; i + written < size && i < maxWrite; i += 4)
        {
            // program four bytes
            interleavedSpiRead(buffer[i + 1], buffer[i + 3]);
            interleavedSpiRead(buffer[i], buffer[i + 2]);
        }
        FLASH_NCS_HIGH();
        interleavedSpiFlashWaitBusy();
        written += i;
        address += i;
        buffer += i;
    }
    FLASH_NCS_HIGH();
    if (oldMode)
    {
      interleavedSpiSet16BitDataOperation();
    }

}

/**
 * @brief Set address for next read operation and starts reading n bytes to the specified buffer.
 * @param address: flash address we need to read
 * @param bufferAddress: where we want to store our data. Can't be null. The first 8 bytes will be
 * @param maxBlockSize maximum block size we expect to read afterwards
 * @return the flash address itself
 * @note the device shall be already in dualSpi mode
 * @note read is word aligned (both in size and start address)
 * @note: the buffer shall be  8 + maxBlockSize bytes large, and maxblocksize must be multiple of 8.
 */
uint32_t interleavedSpiFlashStartRead(uint32_t address, void *bufferAddress, uint32_t maxBlockSize)
{
      FIRST_SPI_USART->EN = 0;
      SECOND_SPI_USART->EN = 0;
      // Disable dma channels
      LDMA->CHDIS = (3 << FIRST_SPI_LDMA_CH);
      // put async PRS level to 0, to disable any continuous transmission
      // and to allow pulse triggers to work correctly
      PRS->ASYNC_SWLEVEL = 0; //(1 << DUAL_SPI_PRS_CH);
      // round size to nearest word boundary
      uint32_t size = ((address & 3) + maxBlockSize + 3) / 4 + 1;
      //
      ldma_DualRX_SPI_EUSART0_descriptors[0].xfer.xferCnt = size;
      ldma_DualRX_SPI_EUSART1_descriptors[0].xfer.xferCnt = size;
      //
      ldma_DualRX_SPI_EUSART0_descriptors[0].xfer.dstAddr = (uint32_t) bufferAddress;
      ldma_DualRX_SPI_EUSART1_descriptors[0].xfer.dstAddr = 2 + (uint32_t) bufferAddress;
      //LDMA->CH[0].LINK = (uint32_t) &ldma_DualRX_SPI_EUSART0_descriptors;
      //LDMA->CH[1].LINK = (uint32_t) &ldma_DualRX_SPI_EUSART1_descriptors;
      // Pulse on CS
      GPIO->P_SET[FLASH_NCS_PORT].DOUT = 1 << FLASH_NCS_PIN;
//      while(LDMA->CHSTATUS &  (3 << FIRST_SPI_LDMA_CH));
      while(LDMA->CHBUSY &  (3 << FIRST_SPI_LDMA_CH));
      LDMA->CHDONE_CLR = (3 << FIRST_SPI_LDMA_CH);
      LDMA->LINKLOAD = (3 << FIRST_SPI_LDMA_CH);
      __asm volatile ("cpsid i\r\n");
      GPIO->P_CLR[FLASH_NCS_PORT].DOUT = 1 << FLASH_NCS_PIN;
      // By the time we are here, the eusarts are disabled. The following check might not be required
#define BETTER_SAFETY_ON_EUSARTS 1
#if BETTER_SAFETY_ON_EUSARTS
#if FIRST_SPI_NUMBER == 0
      while(FIRST_SPI_USART->EN & 2);
#else
      while(SECOND_SPI_USART->EN & 2);
#endif
#endif
      FIRST_SPI_USART->EN = 1;
      SECOND_SPI_USART->EN = 1;
      // select flash
      // enable transmitters
      PRS->ASYNC_SWLEVEL = (1 << INTERLEAVED_SPI_PRS_CH);      // wait till add data have been transmitted
      // send data. Note we do not check, but address will be at most 24 bit
      FIRST_SPI_USART->TXDATA = (SPI_FLASH_READ_DATA << 8) | (address >> 17);
      SECOND_SPI_USART->TXDATA = (SPI_FLASH_READ_DATA << 8) | (address >> 17);
      //
      uint32_t flashAddress = ((address & ~3) >> 1);
      //flashAddress = (flashAddress >> 8) | (flashAddress << 8);
      //__asm volatile ("REV16 %0, %0\n\t" : "+r" (flashAddress));
      FIRST_SPI_USART->TXDATA = flashAddress;
      SECOND_SPI_USART->TXDATA = flashAddress;
      // re enable back the reception just before we start receiving the actual data.
      __asm volatile ("cpsie i\r\n");
      return 8 + (uint32_t) bufferAddress;
}
static void interleavedSpiSet16BitDataOperation()
{
  interleavedSpiData.mode = 1;
  FIRST_SPI_USART->EN = 0;
  SECOND_SPI_USART->EN = 0;
  ldma_DualRX_SPI_EUSART1_descriptors[0].xfer.linkAddr = ((uint32_t) &ldma_DualRX_SPI_EUSART1_descriptors) >> 2;
  ldma_DualRX_SPI_EUSART0_descriptors[0].xfer.linkAddr = ((uint32_t) &ldma_DualRX_SPI_EUSART0_descriptors) >> 2;
  //
  LDMA->CH[FIRST_SPI_LDMA_CH].LINK = (uint32_t) &ldma_DualRX_SPI_EUSART0_descriptors;
  LDMA->CH[SECOND_SPI_LDMA_CH].LINK = (uint32_t) &ldma_DualRX_SPI_EUSART1_descriptors;
  while (FIRST_SPI_USART->EN & 2);
  while (SECOND_SPI_USART->EN & 2);
  FIRST_SPI_USART->FRAMECFG = eusartDataBits16;
  SECOND_SPI_USART->FRAMECFG = eusartDataBits16;
  FIRST_SPI_USART->EN = 1;
  SECOND_SPI_USART->EN = 1;
}
//
static void interleavedSpiSet8BitDataOperation(void)
{

  // Disable dma channels
  LDMA->CHDIS = (3 << FIRST_SPI_LDMA_CH);
  // put async PRS level to 0, to disable any continuous transmission
  // and to allow pulse triggers to work correctly
  PRS->ASYNC_SWLEVEL_CLR = (1 << INTERLEAVED_SPI_PRS_CH);

  interleavedSpiData.mode = 0;
  FIRST_SPI_USART->EN = 0;
  SECOND_SPI_USART->EN = 0;
  while (FIRST_SPI_USART->EN & 2);
  while (SECOND_SPI_USART->EN & 2);
  FIRST_SPI_USART->FRAMECFG = eusartDataBits8;
  SECOND_SPI_USART->FRAMECFG = eusartDataBits8;
  FIRST_SPI_USART->EN = 1;
  SECOND_SPI_USART->EN = 1;
}
  //
uint8_t readTestBuffer[64];

/**
 * @brief Initializes Spi IC for interleaved operation.
 * @note PRS, LDMA and EUSART0-1 clocks must be initialized before this function
 */
void interleavedSpiFlashInit(void)
{
  // set up maximum recommended slew rate of all ports
  for (int i = gpioPortA; i <= gpioPortD; i++)
    GPIO_SlewrateSet(i, 6, 6);
  // deselect flash
  FLASH_NCS_HIGH();
  // set pin modes
  GPIO_PinModeSet(FLASH_NCS_PORT, FLASH_NCS_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(FIRST_FLASH_SPI_MOSI_PORT, FIRST_FLASH_SPI_MOSI_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(FIRST_FLASH_SPI_MISO_PORT, FIRST_FLASH_SPI_MISO_PIN, gpioModeInputPull, 1);
  GPIO_PinModeSet(FIRST_FLASH_SPI_CLK_PORT, FIRST_FLASH_SPI_CLK_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(SECOND_FLASH_SPI_MOSI_PORT, SECOND_FLASH_SPI_MOSI_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(SECOND_FLASH_SPI_MISO_PORT, SECOND_FLASH_SPI_MISO_PIN, gpioModeInputPull, 1);
  GPIO_PinModeSet(SECOND_FLASH_SPI_CLK_PORT, SECOND_FLASH_SPI_CLK_PIN, gpioModePushPull, 1);
  // Connect EUSART to GPIOs
  //
  // first usart
  GPIO->EUSARTROUTE[FIRST_SPI_NUMBER].TXROUTE = ((uint32_t) FIRST_FLASH_SPI_MOSI_PORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT) | (FIRST_FLASH_SPI_MOSI_PIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[FIRST_SPI_NUMBER].RXROUTE = ((uint32_t) FIRST_FLASH_SPI_MISO_PORT << _GPIO_EUSART_RXROUTE_PORT_SHIFT) | (FIRST_FLASH_SPI_MISO_PIN << _GPIO_EUSART_RXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[FIRST_SPI_NUMBER].SCLKROUTE = ((uint32_t) FIRST_FLASH_SPI_CLK_PORT << _GPIO_EUSART_SCLKROUTE_PORT_SHIFT) | (FIRST_FLASH_SPI_CLK_PIN << _GPIO_EUSART_SCLKROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[FIRST_SPI_NUMBER].ROUTEEN = GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_RXPEN | GPIO_EUSART_ROUTEEN_SCLKPEN;
  // second usart
  GPIO->EUSARTROUTE[SECOND_SPI_NUMBER].TXROUTE = ((uint32_t) SECOND_FLASH_SPI_MOSI_PORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT) | (SECOND_FLASH_SPI_MOSI_PIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[SECOND_SPI_NUMBER].RXROUTE = ((uint32_t) SECOND_FLASH_SPI_MISO_PORT << _GPIO_EUSART_RXROUTE_PORT_SHIFT) | (SECOND_FLASH_SPI_MISO_PIN << _GPIO_EUSART_RXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[SECOND_SPI_NUMBER].SCLKROUTE = ((uint32_t) SECOND_FLASH_SPI_CLK_PORT << _GPIO_EUSART_SCLKROUTE_PORT_SHIFT) | (SECOND_FLASH_SPI_CLK_PIN << _GPIO_EUSART_SCLKROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[SECOND_SPI_NUMBER].ROUTEEN = GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_RXPEN | GPIO_EUSART_ROUTEEN_SCLKPEN;
  // Configure both EUSART
  EUSART_TypeDef * const eusarts [] = {EUSART1, EUSART0, EUSART0, EUSART1}; // 2 times. It proved to be working under overclock...
  for (unsigned int i = 0; i < sizeof (eusarts) / sizeof(eusarts[0]); i++)
  {
    EUSART_TypeDef  * const eusart = eusarts[i];
    // disable usart
    if (eusart->EN)
    {
      eusart->EN = 0;
      while(eusart->EN & EUSART_EN_DISABLING);    // wait till disabled
    }
    eusart->CLKDIV = 0;
    eusart->CFG1 = eusartRxFiFoWatermark4Frame | eusartTxFiFoWatermark15Frame;
    eusart->CFG2 = _EUSART_CFG2_MASTER_MASK | (40 << 24); // 1/0.5 MHz with/without OC
    eusart->FRAMECFG = eusartDataBits8; // will be 16 for dual SPI
    eusart->CFG0 = _EUSART_CFG0_SYNC_SYNC | _EUSART_CFG0_MSBF_MASK;
    // Configure frame format
    // Finally enable the Rx and/or Tx channel (as specified).
    eusart->EN = EUSART_EN_EN;

    while (eusart->SYNCBUSY & (_EUSART_SYNCBUSY_RXEN_MASK | _EUSART_SYNCBUSY_TXEN_MASK)); // Wait for low frequency register synchronization.
    eusart->CMD = EUSART_CMD_TXDIS |  EUSART_CMD_CLEARTX | EUSART_CMD_RXDIS;
    //while (eusart->SYNCBUSY & (_EUSART_SYNCBUSY_RXEN_MASK | _EUSART_SYNCBUSY_TXEN_MASK));
    //while (~eusart->STATUS & (_EUSART_STATUS_RXIDLE_MASK | _EUSART_STATUS_TXIDLE_MASK));
    while (eusart->SYNCBUSY);

  }
  // enable DMA and PRS
  interleavedSpiFlashDmaInit();
  interleavedSpiFlashPrsInit();
  // Set both chips output to high strength
  delay(100);
  interleavedSpiFlashDriveStrength();
  delay(100);
  // go to fast mode
  FIRST_SPI_USART->EN = 0;
  while (FIRST_SPI_USART->EN & EUSART_EN_DISABLING);
  FIRST_SPI_USART->CFG2 = _EUSART_CFG2_MASTER_MASK | (HIGH_SPEED_EUSART_DIVISOR << 24)  ; // 40/20 MHz with/without OC
  FIRST_SPI_USART->EN = EUSART_EN_EN;
  //
  SECOND_SPI_USART->EN = 0;
  while (SECOND_SPI_USART->EN & EUSART_EN_DISABLING);
  SECOND_SPI_USART->CFG2 = _EUSART_CFG2_MASTER_MASK | (HIGH_SPEED_EUSART_DIVISOR << 24);
  SECOND_SPI_USART->EN = EUSART_EN_EN;
  //
  interleavedSpiFlashGetSize();
  interleavedSpiSet16BitDataOperation();
}
//
