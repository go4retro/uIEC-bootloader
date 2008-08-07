/* sd2iec - SD/MMC to Commodore serial bus interface/controller
   Copyright (C) 2007,2008  Ingo Korb <ingo@akana.de>

   Inspiration and low-level SD/MMC access based on code from MMC2IEC
     by Lars Pontoppidan et al., see sdcard.c|h and config.h.

   FAT filesystem access based on code from ChaN and Jim Brain, see ff.c|h.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


   sdcard.c: SD/MMC access routines

   Extended, optimized and cleaned version of code from MMC2IEC,
   original copyright header follows:

//
// Title        : SD/MMC Card driver
// Author       : Lars Pontoppidan, Aske Olsson, Pascal Dufour,
// Date         : Jan. 2006
// Version      : 0.42
// Target MCU   : Atmel AVR Series
//
// CREDITS:
// This module is developed as part of a project at the technical univerisity of
// Denmark, DTU.
//
// DESCRIPTION:
// This SD card driver implements the fundamental communication with a SD card.
// The driver is confirmed working on 8 MHz and 14.7456 MHz AtMega32 and has
// been tested successfully with a large number of different SD and MMC cards.
//
// DISCLAIMER:
// The author is in no way responsible for any problems or damage caused by
// using this code. Use at your own risk.
//
// LICENSE:
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//

*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>
#include "config.h"
#include "spi.h"
#include "crc7.h"
#include "fat.h"
#include "disk_lib.h"

#ifndef TRUE
#define TRUE -1
#endif
#ifndef FALSE
#define FALSE 0
#endif


// SD/MMC commands
#define GO_IDLE_STATE           0
#define SEND_OP_COND            1
#define SWITCH_FUNC             6
#define SEND_IF_COND            8
#define SEND_CSD                9
#define SEND_CID               10
#define STOP_TRANSMISSION      12
#define SEND_STATUS            13
#define SET_BLOCKLEN           16
#define READ_SINGLE_BLOCK      17
#define READ_MULTIPLE_BLOCK    18
#define WRITE_BLOCK            24
#define WRITE_MULTIPLE_BLOCK   25
#define PROGRAM_CSD            27
#define SET_WRITE_PROT         28
#define CLR_WRITE_PROT         29
#define SEND_WRITE_PROT        30
#define ERASE_WR_BLK_STAR_ADDR 32
#define ERASE_WR_BLK_END_ADDR  33
#define ERASE                  38
#define LOCK_UNLOCK            42
#define APP_CMD                55
#define GEN_CMD                56
#define READ_OCR               58
#define CRC_ON_OFF             59

// SD ACMDs
#define SD_STATUS                 13
#define SD_SEND_NUM_WR_BLOCKS     22
#define SD_SET_WR_BLK_ERASE_COUNT 23
#define SD_SEND_OP_COND           41
#define SD_SET_CLR_CARD_DETECT    42
#define SD_SEND_SCR               51

// R1 status bits
#define STATUS_IN_IDLE          1
#define STATUS_ERASE_RESET      2
#define STATUS_ILLEGAL_COMMAND  4
#define STATUS_CRC_ERROR        8
#define STATUS_ERASE_SEQ_ERROR 16
#define STATUS_ADDRESS_ERROR   32
#define STATUS_PARAMETER_ERROR 64


#ifdef CONFIG_SDHC_SUPPORT
static uint8_t isSDHC;
#else
#define isSDHC 0
#endif

// JLB testing.  2784 without crc checking, 2898 with....
//#define crc7update(a,y) 0

static uint8_t sdResponse(uint8_t expected)
{
  unsigned short count = 0x0FFF;

  while ((spiTransferByte(0xFF) != expected) && count )
    count--;

  // If count didn't run out, return success
  return (count != 0);
}

static void deselectCard(void) {
  // Send 8 clock cycles
  SPI_SS_HIGH();
  spiTransferByte(0xff);
}

/**
 * sendCommand - send a command to the SD card
 * @card     : card number to be accessed
 * @command  : command to be sent
 * @parameter: parameter to be sent
 * @deselect : Flags if the card should be deselected afterwards
 *
 * This function calculates the correct CRC7 for the command and
 * parameter and transmits all of it to the SD card. If requested
 * the card will be deselected afterwards.
 */
static int sendCommand(const uint8_t  command,
                       const uint32_t parameter,
                       const uint8_t  deselect) {
  union {
    uint32_t l;
    uint8_t  c[4];
  } long2char;

  uint8_t  i,crc,errorcount;
  uint16_t counter;

  long2char.l = parameter;
  crc = crc7update(0  , 0x40+command);
  crc = crc7update(crc, long2char.c[3]);
  crc = crc7update(crc, long2char.c[2]);
  crc = crc7update(crc, long2char.c[1]);
  crc = crc7update(crc, long2char.c[0]);
  crc = (crc << 1) | 1;

  errorcount = 0;
  while (errorcount < CONFIG_SD_AUTO_RETRIES) {
    // Select card
    SPI_SS_LOW();
    // Transfer command
    spiTransferByte(0x40+command);
    spiTransferLong(parameter);
    spiTransferByte(crc);

    // Wait for a valid response
    counter = 0;
    do {
      i = spiTransferByte(0xff);
      counter++;
    } while (i & 0x80 && counter < 0x1000);

    // Check for CRC error
    // can't reliably retry unless deselect is allowed
    if (deselect && (i & STATUS_CRC_ERROR)) {
      deselectCard();
      errorcount++;
      continue;
    }

    if (deselect) deselectCard();
    break;
  }

  return i;
}

#ifdef CONFIG_SDHC_SUPPORT
// Extended init sequence for SDHC support
static uint8_t extendedInit(void) {
  uint8_t  i;
  uint32_t answer;

  // Send CMD8: SEND_IF_COND
  //   0b000110101010 == 2.7-3.6V supply, check pattern 0xAA
  i = sendCommand(SEND_IF_COND, 0b000110101010, 0);
  if (i > 1) {
    // Card returned an error, ok (MMC oder SD1.x) but not SDHC
    deselectCard();
    return TRUE;
  }

  // No error, continue SDHC initialization
  answer = spiTransferLong(0);
  deselectCard();

  if (((answer >> 8) & 0x0f) != 0b0001) {
    // Card didn't accept our voltage specification
    return FALSE;
  }

  // Verify echo-back of check pattern
  if ((answer & 0xff) != 0b10101010) {
    // Check pattern mismatch, working but not SD2.0 compliant
    // The specs say we should not use the card, but let's try anyway.
    return TRUE;
  }

  return TRUE;
}
#endif

// SD common initialisation
static void sdInit(void) {
  uint8_t i;
  uint16_t counter;

  counter = 0xffff;
  do {
    // Prepare for ACMD, send CMD55: APP_CMD
    i = sendCommand(APP_CMD, 0, 1);
    if (i > 1) {
      // Command not accepted, could be MMC
      return;
    }

    // Send ACMD41: SD_SEND_OP_COND
    //   1L<<30 == Host has High Capacity Support
    i = sendCommand(SD_SEND_OP_COND, 1L<<30, 1);
    // Repeat while card card accepts command but isn't ready
  } while (i == 1 && --counter > 0);

  // Ignore failures, there is at least one Sandisk MMC card
  // that accepts CMD55, but not ACMD41.
}

uint8_t disk_initialize(void) {
  uint8_t  i;
  uint16_t counter;
  uint32_t answer;

  spiInit();
#ifdef CONFIG_SDHC_SUPPORT
  isSDHC   = FALSE;
#endif

  SPI_SS_HIGH();

  // Send 80 clks
  for (i=0; i<10; i++) {
    spiTransferByte(0xFF);
  }

  // Reset card
  i = sendCommand(GO_IDLE_STATE, 0, 1);
  if (i != 1) {
    return DISK_INIT;
  }

#ifdef CONFIG_SDHC_SUPPORT
  if (!extendedInit())
    return DISK_INIT;
#endif

  sdInit();

  counter = 0xffff;
  // According to the spec READ_OCR should work at this point
  // without retries. One of my Sandisk-cards thinks otherwise.
  do {
    // Send CMD58: READ_OCR
    i = sendCommand(READ_OCR, 0, 0);
    if (i > 1)
      deselectCard();
  } while (i > 1 && counter-- > 0);

  if (counter > 0) {
    answer = spiTransferLong(0);

    // See if the card likes our supply voltage
    if (!(answer & SD_SUPPLY_VOLTAGE)) {
      // The code isn't set up to completely ignore the card,
      // but at least report it as nonworking
      deselectCard();
      return DISK_INIT;
    }

#ifdef CONFIG_SDHC_SUPPORT
    // See what card we've got
    if (answer & 0x40000000) {
      isSDHC = TRUE;
    }
#endif
  }

  // Keep sending CMD1 (SEND_OP_COND) command until zero response
  counter = 0xffff;
  do {
    i = sendCommand(SEND_OP_COND, 1L<<30, 1);
    counter--;
  } while (i != 0 && counter > 0);

  if (counter==0) {
    return DISK_INIT;
  }


  // Send MMC CMD16(SET_BLOCKLEN) to 512 bytes
  i = sendCommand(SET_BLOCKLEN, 512, 1);
  if (i != 0) {
    return FALSE;
  }

  // Thats it!
  return DISK_OK;
}


/**
 * disk_read - reads sectors from the SD card to buffer
 * @drv   : drive (unused)
 * @buffer: pointer to the buffer
 * @sector: first sector to be read
 * @count : number of sectors to be read
 *
 * This function reads count sectors from the SD card starting
 * at sector to buffer. Returns RES_ERROR if an error occured or
 * RES_OK if successful. Up to SD_AUTO_RETRIES will be made if
 * the calculated data CRC does not match the one sent by the
 * card. If there were errors during the command transmission
 * disk_state will be set to DISK_ERROR and no retries are made.
 */
void disk_read(uint32_t sector) {
  uint8_t res,tmp,errorcount;
  unsigned char *buf = fat_buf;

  errorcount = 0;
  while (errorcount < CONFIG_SD_AUTO_RETRIES) {
    res = sendCommand(READ_SINGLE_BLOCK, (isSDHC?(sector<<9):sector), 0);

    if (res != 0) {
      SPI_SS_HIGH();
      return;
    }

    // Wait for data token
    if (!sdResponse(0xFE)) {
      SPI_SS_HIGH();
      return;
    }

    uint16_t i;

    // Get data
    // Initiate data exchange over SPI
    SPDR = 0xff;

    for (i=0; i<512; i++) {
      // Wait until data has been received
      loop_until_bit_is_set(SPSR, SPIF);
      tmp = SPDR;
      // Transmit the next byte while we store the current one
      SPDR = 0xff;

      *(buf++) = tmp;
    }
    // Wait until the first CRC byte is received
    loop_until_bit_is_set(SPSR, SPIF);

    // Check CRC
    spiTransferByte(0xff);

    break;
  }
  deselectCard();

  return;
}



