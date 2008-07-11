#include <avr/io.h>
#include <string.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>
#include "fat1216.h"
#include "config.h"
#include "uart.h"
#include <stdlib.h>
#include <avr/wdt.h>

/*#define USE_FLASH_LED
#define FLASH_LED_PORT PORTC
#define FLASH_LED_DDR DDRC
#define FLASH_LED_PIN PC0
#define FLASH_LED_POLARITY 1

#define USE_ALIVE_LED
#define ALIVE_LED_PORT PORTC
#define ALIVE_LED_DDR DDRC
#define ALIVE_LED_PIN PC1
*/

#define USE_FLASH_LED
#define FLASH_LED_PORT PORTG
#define FLASH_LED_DDR DDRG
#define FLASH_LED_PIN PG4
#define FLASH_LED_POLARITY 0

#define USE_ALIVE_LED
#define ALIVE_LED_PORT PORTG
#define ALIVE_LED_DDR DDRG
#define ALIVE_LED_PIN PG4


typedef struct
{
	uint32_t dev_id;
	uint16_t app_version;
	uint16_t crc;
} bootldrinfo_t;


uint16_t startcluster;
uint16_t updatecluster; //is set when update is available
bootldrinfo_t current_bootldrinfo;

void (*app_start)(void) = 0x0000;


/* Make sure the watchdog is disabled as soon as possible    */
/* Copy this code to your bootloader if you use one and your */
/* MCU doesn't disable the WDT after reset!                  */
void get_mcusr(void) \
      __attribute__((naked)) \
      __attribute__((section(".init3")));
void get_mcusr(void)
{
  MCUSR = 0;
  wdt_disable();
}


static inline uint8_t crc_file(void)
{
	uint16_t filesector;
	uint16_t index;
	uint16_t flash_crc = 0xFFFF;
	
	for (filesector = 0; filesector < (FLASHEND - BOOTLDRSIZE + 1) / 512; filesector++)
	{
		#ifdef USE_FLASH_LED
		FLASH_LED_PORT ^= 1<<FLASH_LED_PIN;
		#endif
		
		fat1216_readfilesector(startcluster, filesector);
	
     	for (index=0; index < 512; index++)
     	{
			flash_crc = _crc_ccitt_update(flash_crc, fat_buf[index]);
		}
	}
	
	//LED on
	#ifdef USE_FLASH_LED
	#if FLASH_LED_POLARITY
		FLASH_LED_PORT &= ~(1<<FLASH_LED_PIN);
	#else
		FLASH_LED_PORT |= 1<<FLASH_LED_PIN;
	#endif
	#endif
	
	return flash_crc; // result is ZERO when CRC is okay
}


static inline void check_file(void)
{
	//Check filesize
	
	if (filesize != FLASHEND - BOOTLDRSIZE + 1)
		return;

	bootldrinfo_t *file_bootldrinfo;
	fat1216_readfilesector(startcluster, (FLASHEND - BOOTLDRSIZE + 1) / 512 - 1);
	
	file_bootldrinfo =  (bootldrinfo_t*) (uint8_t*) (fat_buf + (FLASHEND - BOOTLDRSIZE - sizeof(bootldrinfo_t) + 1) % 512);
	
	//Check DEVID
	if (file_bootldrinfo->dev_id != DEVID)
		return;
	
	uart_putc('!');
	//Check application version
	if (file_bootldrinfo->app_version <= current_bootldrinfo.app_version && 
	    file_bootldrinfo->app_version   != 0 &&
	    current_bootldrinfo.app_version != 0)
		return;
	
  uart_putc('@');
	// If development version in flash and in file,
        // check for different crc
	if (current_bootldrinfo.app_version == 0 &&
	    file_bootldrinfo->app_version   == 0 &&
	    current_bootldrinfo.crc == file_bootldrinfo->crc)
	        return;

  uart_putc('#');
	// check CRC of file
	if(crc_file() != 0)
	 return;
	
	uart_puthex((uint8_t)(file_bootldrinfo->app_version>>8));
  uart_puthex((uint8_t)(file_bootldrinfo->app_version));
  uart_putc('$');
  uart_puthex((uint8_t)(current_bootldrinfo.app_version>>8));
  uart_puthex((uint8_t)(current_bootldrinfo.app_version));
	current_bootldrinfo.app_version = file_bootldrinfo->app_version;
	updatecluster = startcluster;
	
}


static inline void flash_update(void)
{
	uint16_t filesector, j;
	uint8_t i;
	uint16_t *lpword;
	uint32_t adr;
	
	for (filesector = 0; filesector < (FLASHEND - BOOTLDRSIZE + 1) / 512; filesector++)
	{
		#ifdef USE_FLASH_LED
		FLASH_LED_PORT ^= 1<<FLASH_LED_PIN;
		#endif
		
		lpword = (uint16_t*) fat_buf;
		fat1216_readfilesector(updatecluster, filesector);
	
		for (i=0; i<(512 / SPM_PAGESIZE); i++)
		{
			adr = (filesector * 512UL) + i * SPM_PAGESIZE;
			boot_page_erase(adr);
			while (boot_rww_busy())
				boot_rww_enable();
			
			for (j=0; j<SPM_PAGESIZE; j+=2)
				boot_page_fill(adr + j, *lpword++);

			boot_page_write(adr);
			while (boot_rww_busy())
				boot_rww_enable();
		}
	}
	
	//LED on
	#ifdef USE_FLASH_LED
	#if FLASH_LED_POLARITY
		FLASH_LED_PORT &= ~(1<<FLASH_LED_PIN);
	#else
		FLASH_LED_PORT |= 1<<FLASH_LED_PIN;
	#endif
	#endif
}

int main(void)
{
	uint16_t i;
  uint16_t flash_crc;
  uint32_t adr;
  
	
	init_serial();
	//LED On
	#ifdef USE_FLASH_LED
	FLASH_LED_DDR |= 1<<FLASH_LED_PIN;
	#if !FLASH_LED_POLARITY
	FLASH_LED_PORT |= 1<<FLASH_LED_PIN;
	#endif
	#endif
	
	#ifdef USE_ALIVE_LED
	ALIVE_LED_DDR |= 1<<ALIVE_LED_PIN;
	ALIVE_LED_PORT |= 1<<ALIVE_LED_PIN;
	#endif
	
  //memcpy_P(&current_bootldrinfo, (uint8_t*) FLASHEND - BOOTLDRSIZE - sizeof(bootldrinfo_t) + 1, sizeof(bootldrinfo_t));
	for(i = 0;i < sizeof(bootldrinfo_t);i++) {
	  ((uint8_t*)&current_bootldrinfo)[i] = pgm_read_byte_far(FLASHEND - BOOTLDRSIZE - sizeof(bootldrinfo_t) + 1 + i);
	}

  uart_puthex((uint8_t)(current_bootldrinfo.crc>>8));
  uart_puthex((uint8_t)(current_bootldrinfo.crc));
  uart_putc('+');
  uart_puthex((uint8_t)(current_bootldrinfo.app_version>>8));
  uart_puthex((uint8_t)(current_bootldrinfo.app_version));
	
	if (current_bootldrinfo.app_version == 0xFFFF) {
	  uart_putc('-');
		current_bootldrinfo.app_version = 0;    //application not flashed yet
	} else {
	  for (adr=0,flash_crc = 0xFFFF; adr<FLASHEND - BOOTLDRSIZE + 1; adr++)
	     flash_crc = _crc_ccitt_update(flash_crc, pgm_read_byte_far(adr));
	     
	   if (flash_crc)
	     current_bootldrinfo.app_version = 0; //bad app code, reflash
	}
	
	
	if (fat1216_init() == 0) {
		for (i=0; i<512; i++)	{
#ifdef USE_FLASH_LED
		  FLASH_LED_PORT ^= 1<<FLASH_LED_PIN;
#endif

			startcluster = fat1216_readRootDirEntry(i);
			
			if (startcluster == 0xFFFF)
				continue;

			check_file();
		}
#ifdef USE_FLASH_LED
#if FLASH_LED_POLARITY
		FLASH_LED_PORT &= ~(1<<FLASH_LED_PIN);
#else
		FLASH_LED_PORT |= 1<<FLASH_LED_PIN;
#endif
#endif
		
		if (updatecluster)
			flash_update();
	}

	uart_putc('*');
	for (adr=0,flash_crc = 0xFFFF; adr<FLASHEND - BOOTLDRSIZE + 1; adr++)
		flash_crc = _crc_ccitt_update(flash_crc, pgm_read_byte_far(adr));
		
	if (flash_crc == 0)	{
		//Led off
		#ifdef USE_FLASH_LED
		FLASH_LED_DDR = 0x00;
		#endif
	  uart_putc('g');
		app_start();
	}
	
//	while (1);
  uart_putc('G');
    app_start();

}
