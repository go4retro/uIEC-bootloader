/*-------------------------------------------------
	 mmc_lib.h C-header file for AVRGCC
	------------------------------------
	Interface for MultiMediaCard via SPI
	
	Author:
		Stefan Seegel
		dahamm@gmx.net
		ICQ UIN 14964408
		
	Thanks to:
		Ulrich Radig for his code
		Simon Lehmayr for good ideas
		ape for timing improovment
		Andreas Schwarz for the great forum
		
	Version:
		2.0 / 10.05.2005
	
	History:
		2.0 / 10.05.2005:
			- splited read & write commands in start..., read/write & stop...
			- Improved protocol handling for more supported cards
			- Improved description in header file
			- Added different SPI speeds for initialization, read and write
			- Supports SD cards
			- Added macros for CID register access
	
		1.2 / 11.08.2004:
			- Changed "mmc_send_byte()" to inline
			- Changed parameter "count" of "mmc_read_sector()" to unsigned short (16 bit)
			
		1.1 / 29.07.2004:
			-Removed typedef struct for CSD
			-Added different init-clock rate
			
		1.0 / 28.07.2004:
			-First release
	
---------------------------------------------------*/

//Port & Pin definitions. Be sure to use a pin of the same port as SPI for CS (Chip Select) !
//Settings below are recommended for a MEGA16/MEGA32
#define MMC_PORT PORTB
#define MMC_DDR DDRB

#define SPI_MISO	PB6		//DataOut of MMC 
#define SPI_MOSI	PB5		//DataIn of  MMC
#define SPI_CLK  	PB7		//Clock of MMC
#define MMC_CS		PB4		//ChipSelect of MMC

//Clockrate while initialisation / reading / writing
#define SPI_INIT_CLOCK 1<<SPR1 | 1<<SPR0
#define SPI_READ_CLOCK 0<<SPR1 | 0<<SPR0
#define SPI_WRITE_CLOCK 1<<SPR1 | 0<<SPR0

#define MMC_CMD0_RETRIES 15

//MMC Commandos
#define MMC_GO_IDLE_STATE 0
#define MMC_SEND_OP_COND 1
#define MMC_SEND_CSD	9
#define MMC_SEND_CID 10
#define MMC_SET_BLOCKLEN 16
#define MMC_READ_SINGLE_BLOCK 17
#define MMC_WRITE_BLOCK 24

// Result Codes
#define MMC_OK 				0
#define MMC_INIT 1
#define MMC_CMD0_TIMEOUT 2
#define MMC_NOSTARTBYTE	3
#define MMC_CMDERROR	4
#define MMC_OP_COND_TIMEOUT		5
