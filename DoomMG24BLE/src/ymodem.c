/**
 * @brief Poor man's YMODEM by next-hack. Need serious cleanup
 *
 *  Copyright (C) 2021-2023 by Nicola Wrachien
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
 */
#include "printf.h"
#include "ymodem.h"
#include "main.h"
#include "display.h"
#include "graphics.h"
#include "sharedUsart.h"
#include "extMemory.h"
#include "em_eusart.h"
#include "em_cmu.h"
//
#define XMODEM_SOH 0x01
#define XMODEM_SOX 0x02 // for 1k packets
#define XMODEM_EOT 0x04
#define XMODEM_ETB 0x17
#define XMODEM_ACK 0x06
#define XMODEM_NAK 0x15
#define XMODEM_ETB 0x17
#define XMODEM_CAN 0x18
#define XMODEM_C 0x43
#define MAXRETRANS 16
#define ONE_SECOND_MS (1000UL)
//
extern uint8_t staticZone[]; // until Doom hasn't started, we can do everything we want on this buffer.
//
int getChar()
{
  int c = -1;
  if (EUSART0->STATUS & EUSART_STATUS_RXFL)
  {
    c = EUSART0->RXDATA;
  }
  return c;
}
//
int getCharWithTimeout(uint32_t timeout_ms)
{
    int c = -1;
    uint32_t timeNow = TICK_TIMER->CNT;
    while (((uint32_t) TICK_TIMER->CNT - timeNow < (timeout_ms * TIMER_TICKS_PER_MILLISECOND)) && c == -1)
    {
        c = getChar();
    }
    return c;
}

//
static bool isCrcValid(uint8_t * buffer, int length)
{
  // compute CRC. We use hardware CRC functionality for this one.
  for (int i = 0; i < length; i++)
      GPCRC->INPUTDATABYTE = buffer[i];
  uint16_t data = GPCRC->DATA;
  if (data == 0)
  {
      return true;
  }
  else
  {
      displayPrintln(1, "CRC ERROR");
      displayPrintln(1, "r 0x%02x%02x c 0x%04x", buffer[length - 1], buffer[length - 2], data);
      return false;
  }}
//
static int str2int(uint8_t * str)
{
    int n = 0;
    while (*str != 0)
    {
        n = n * 10 + *str - '0';
        str++;
    }
    return n;
}
void checkAndErase64k(uint32_t * erasedTo, uint32_t startAddress, uint32_t programSize)
{
    uint32_t endAddress = startAddress + programSize;
    if (endAddress > *erasedTo)
    {
        extMemErase(endAddress & ~(0x10000 - 1 ), 0x10000);
        *erasedTo = (endAddress & ~0xFFFF) + 0x10000;
    }
}
//
static void initGPCRC()
{
   CMU_ClockEnable(cmuClock_GPCRC, true);
   GPCRC->CTRL = GPCRC_CTRL_AUTOINIT | GPCRC_CTRL_POLYSEL_CRC16 | GPCRC_CTRL_BITREVERSE_REVERSED;
   GPCRC->INIT = 0;
   GPCRC->POLY = __RBIT(0x10210000);
   GPCRC->EN = GPCRC_EN_EN;
   GPCRC->CMD = GPCRC_CMD_INIT;
}
//
int ymodemReceive(uint32_t address)
{
    uint32_t erasedTo = 0;
    bool firstPacket = true;
    uint8_t erased = false;
    unsigned char * packet = &staticZone[1]; // this is because packet[3] will be word aligned
    unsigned char sendChar = XMODEM_C;       // character to send
    unsigned char packetNumber = 1;
    int sizeOfPacket = 0;
    int c;
    int fileLength = 0xFFFFFF; // 16MB
    int bytesProgrammed = 0;
    int packetRetry = MAXRETRANS;
    bool startReceive;
    //
    initGPCRC();
    //
    SET_VCOM_MODE();
    printf("Waiting for X or YMODEM transmission\r\n");
    SET_VCOM_MODE();
    while (1)
    {
        startReceive = false;
        for (int retry = 0; retry < 16; retry++)
        {
            // if we need to send a char, send it
            if (sendChar)
            {
              usart_putchar(sendChar);
            }
            // did we get a character within 1 s?
            if ((c = getCharWithTimeout(3 * ONE_SECOND_MS)) >= 0)
            {
                switch (c)
                {
                    case XMODEM_SOH:
                        startReceive = true;
                        sizeOfPacket = 1 + 2 + 128 + 2;
                        break;
                    case XMODEM_SOX: // 1k modem support
                        startReceive = true;
                        sizeOfPacket = 1 + 2 + 1024 + 2;
                        break;
                    case XMODEM_ETB:
                    case XMODEM_EOT:
                        usart_putchar(XMODEM_ACK);
                        //
                        usart_putchar(XMODEM_ACK);
                        usart_putchar(XMODEM_ACK);
                        usart_putchar(XMODEM_ACK);
                        displayPrintln(1, "File received, end of transmission.\r\n");
                        return 0;
                        break;
                    case XMODEM_CAN:
                      displayPrintln(1,"Cancelled, rebooting\r\n");
                        return 1;
                        break;
                    default:
                    {
                        int s = 0;
                        while (getCharWithTimeout(ONE_SECOND_MS) >= 0)
                            s++;
                        displayPrintln(1, "Bogus char %d, skipped remaining %d\r\n", c, s);
                        usart_putchar(XMODEM_NAK);
                        break;
                    }
                }
                if (startReceive)
                {
                    break;
                }
            }
        }
        if (startReceive)
        {
            // did we need to start from 0
            bool valid = true;
            packet[0] = c; // SOH or SOX
            sendChar = 0;
            for (int i = 1; i < sizeOfPacket; i++)
            {
                c = getCharWithTimeout(ONE_SECOND_MS);
                if (c < 0)
                {
                  displayPrintln(1, "Timeout on packet %d, after %d bytes\r\n", packetNumber, i);
                    valid = false;
                    break;
                }
                packet[i] = c;
            }
            uint8_t error = 0;
            // check packet number field
            valid = valid && (packet[1] == (unsigned char)(~packet[2]));
            if (!valid && !error)
            {
              displayPrintln(1, "Error on packet number %d. Got: %x, negated: %x\r\n", packetNumber, (unsigned char)packet[1], (unsigned char)packet[2]);
                error = 2;
            }
            // check if we are aligned
            valid = valid && ((packet[1] == packetNumber) || (!erased && (packet[1] == 0)));
            if (!valid && !error)
            {
                error = 3;
            }
            // check CRC
            valid = valid && isCrcValid(&packet[3], sizeOfPacket - 3);
            if (!valid && !error)
            {
                error = 4;
            }
            if (valid)
            {
                if (!erased) // this also means packet 0, which contains file name and size
                {
                    if (packet[1] == 0) // special case to support y modem
                    {
                        packetNumber = 0;
                        sizeOfPacket = 5; // so that this will result in a 0 bytesToProgram
                        // skip file name
                        uint8_t * p = &packet[3];
                        while (*p++ != 0)
                            ;
                        // now get file size string
                        int idx = 0;
                        while (p[idx] != ' ' && idx < 8) // 8 digit size is 100M
                            idx++;
                        // found space.
                        p[idx] = 0;
                        fileLength = str2int(p);
                        displayPrintln(1, "File Length %d\r\n", fileLength);
                        sendChar = XMODEM_C; // required for YMODEM
                    }
                    erased = true;
                }
                //
                int bytesToProgram = sizeOfPacket - 5; // remove Start of header, packet number (2), and CRC (2)

                if (bytesToProgram > (fileLength - bytesProgrammed))
                {
                    bytesToProgram = fileLength - bytesProgrammed;
                }
                if (packetNumber % 64 == 0)
                {
                    displayPrintln(1, "Prg %d %d%%\r\n", bytesProgrammed, 100 * bytesProgrammed / fileLength);
                }
                if (bytesToProgram > 0)
                {
                    SET_FLASH_MODE();
                    checkAndErase64k(&erasedTo, address, bytesToProgram);
                    extMemProgram(address, &packet[3], (bytesToProgram + 3) & ~3);
                    address += bytesToProgram;
                    bytesProgrammed += bytesToProgram;
                    SET_VCOM_MODE();
                }
                //
                packetNumber++;
                // TODO: check overflow
                packetRetry = MAXRETRANS + 1;
                if (firstPacket)
                {
                    firstPacket = 0;
                    int c;
                    // flush bogus chars
                    do
                    {

                        c = getCharWithTimeout(10000);
                    } while (-1 != c);
                }
                usart_putchar(XMODEM_ACK);
            }
            else
            {
                // before nacking we need to purge the line
                while (getCharWithTimeout(ONE_SECOND_MS) >= 0)
                    ;
                displayPrintln(1, "Ymodem Error %d on packet %d, will recover\r\n", error, packetNumber);
                usart_putchar(XMODEM_NAK);
            }
            if (--packetRetry <= 0)
            {
                usart_putchar(XMODEM_CAN);
                usart_putchar(XMODEM_CAN);
                usart_putchar(XMODEM_CAN);
                displayPrintln(1, "Too many packet errors, resetting\r\n");
                return 3;
            }
        }
        else
        {
          displayPrintln(1, "No start of frame \r\n");
        }
    }
    return 2;
}
