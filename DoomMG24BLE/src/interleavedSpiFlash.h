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
#ifndef SRC_INTERLEAVEDSPIFLASHL_H_
#define SRC_INTERLEAVEDSPIFLASHL_H_

#include <stdint.h>
#include <stdbool.h>
#include "em_device.h"
#include "em_eusart.h"
#include "em_gpio.h"
#include "main.h"
//
#define HIGH_SPEED_EUSART_DIVISOR               1
#define SPI_ADDRESS_MASK                        0x1FFFFFF   // only 16+16 MB max supported
//
#define SPI_FLASH_SECTOR_SIZE                   4096
#define SPI_FLASH_32K_BLOCK_SIZE                (32 * 1024)
typedef struct
{
    uint32_t firstData;
    uint32_t secondData;
} interleavedSpiTxRxData_t;

static inline void interleavedSpiWaitDma()
{
  while ((LDMA->CHDONE & (3 << FIRST_SPI_LDMA_CH)) != (3 << FIRST_SPI_LDMA_CH));
}
/**
 * @brief Sends to both flash a data, and return the received values
 * @param firstData
 * @param secondData
 * @return data.
 * @note using 32 bit data because frame could be 16 bit and registers are 32 bit anyway.
 */
static inline interleavedSpiTxRxData_t interleavedSpiRead(uint32_t firstData, uint32_t secondData)
{
  interleavedSpiTxRxData_t ret;
  FIRST_SPI_USART->TXDATA = firstData;
  SECOND_SPI_USART->TXDATA = secondData;
  PRS->ASYNC_SWPULSE = (1 << INTERLEAVED_SPI_PRS_CH);
  while (!(FIRST_SPI_USART->STATUS & EUSART_STATUS_TXENS));
  while (!(FIRST_SPI_USART->STATUS & EUSART_STATUS_TXC));
  while (!(SECOND_SPI_USART->STATUS & EUSART_STATUS_TXENS));
  while (!(SECOND_SPI_USART->STATUS & EUSART_STATUS_TXC));
  ret.firstData = FIRST_SPI_USART->RXDATA;
  ret.secondData = SECOND_SPI_USART->RXDATA;
  FIRST_SPI_USART->CMD_SET = EUSART_CMD_TXDIS | EUSART_CMD_RXDIS;
  SECOND_SPI_USART->CMD_SET = EUSART_CMD_TXDIS | EUSART_CMD_RXDIS;
  while ((FIRST_SPI_USART->STATUS & EUSART_STATUS_TXENS));
  while ((SECOND_SPI_USART->STATUS & EUSART_STATUS_TXENS));
  return ret;
}
static inline interleavedSpiTxRxData_t interleavedSpiReadSameData(uint32_t data)
{
  interleavedSpiTxRxData_t ret;
  FIRST_SPI_USART->TXDATA = data;
  SECOND_SPI_USART->TXDATA = data;
  PRS->ASYNC_SWPULSE = (1 << INTERLEAVED_SPI_PRS_CH);
  while (!(FIRST_SPI_USART->STATUS & EUSART_STATUS_TXENS));
  while (!(FIRST_SPI_USART->STATUS & EUSART_STATUS_TXC));
  while (!(SECOND_SPI_USART->STATUS & EUSART_STATUS_TXENS));
  while (!(SECOND_SPI_USART->STATUS & EUSART_STATUS_TXC));
  ret.firstData = FIRST_SPI_USART->RXDATA;
  ret.secondData = SECOND_SPI_USART->RXDATA;
  FIRST_SPI_USART->CMD_SET = EUSART_CMD_TXDIS | EUSART_CMD_RXDIS;
  SECOND_SPI_USART->CMD_SET = EUSART_CMD_TXDIS | EUSART_CMD_RXDIS;
  while ((FIRST_SPI_USART->STATUS & EUSART_STATUS_TXENS));
  while ((SECOND_SPI_USART->STATUS & EUSART_STATUS_TXENS));
  return ret;
}
typedef struct
{
    uint32_t bufferStartAddress;  //address belonging to buffer. 4-byte aligned
    uint32_t currentReadIndex;      // current pointer
    uint32_t count;               // how many bytes we actually read.
    uint8_t rxBuffer[256 + 8];
    uint8_t padding[8];  // for unaligned accesses
    uint8_t mode;
} interleavedSpiData_t;
void interleavedSpiFlashInit();
void interleavedSpiFlashProgram(uint32_t address, uint8_t *buffer, uint32_t size);
uint32_t interleavedSpiFlashStartRead(uint32_t address, void *bufferAddress, uint32_t maxBlockSize);
void interleavedSpiFlashEraseTwoSectors(uint32_t logicalAddress);
void interleavedSpiFlashEraseTwo64kBlocks(uint32_t logicalAddress);
void interleavedSpiFlashEraseTwo32kBlocks(uint32_t logicalAddress);
int interleavedSpiFlashGetSize(void);
int interleavedSpiFlashGetDataMode(void);
void interleavedSpiFlashRestoreDataMode(void);
void interleavedSpiFlashChipErase(void);
extern interleavedSpiData_t interleavedSpiData;
#endif /* SRC_INTERLEAVEDSPIFLASHL_H_ */
