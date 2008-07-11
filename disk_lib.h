/*-------------------------------------------------
	 disk_lib.h C-header file for AVRGCC
	------------------------------------
	Interface definitions
	
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

// Result Codes
#define DISK_OK 				      0
#define DISK_INIT             1
#define DISK_TIMEOUT          2

extern uint8_t disk_initialize(void);
/*			
*		Call disk_initialize one time after a card has been connected to the µC's SPI bus!
*	
*		return values:
*			DISK_OK:				MMC initialized successfully
*			DISK_INIT:			Error while trying to reset MMC
*/

extern void disk_read(uint32_t adr);
/*
*		disk_read initializes the reading of a sector
*
*		Parameters:
*			adr: specifies address to be read from
*
*		Example Code:
*			unsigned char mmc_buf[512];
*			init_disk();	//Initializes disk
*			disk_read(1000);	//start reading sector 1000
*/
