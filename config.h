/* FAT bootloader

   Initial code from mikrocontroller.net, additional code and Makefile system
   from sd2iec.

*/

#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_HARDWARE_VARIANT 4

#if CONFIG_HARDWARE_VARIANT==1
/* SD Card supply voltage - choose the one appropiate to your board */
/* #  define SD_SUPPLY_VOLTAGE (1L<<15)  / * 2.7V - 2.8V */
/* #  define SD_SUPPLY_VOLTAGE (1L<<16)  / * 2.8V - 2.9V */
/* #  define SD_SUPPLY_VOLTAGE (1L<<17)  / * 2.9V - 3.0V */
#  define SD_SUPPLY_VOLTAGE (1L<<18)  /* 3.0V - 3.1V */
/* #  define SD_SUPPLY_VOLTAGE (1L<<19)  / * 3.1V - 3.2V */
/* #  define SD_SUPPLY_VOLTAGE (1L<<20)  / * 3.2V - 3.3V */
/* #  define SD_SUPPLY_VOLTAGE (1L<<21)  / * 3.3V - 3.4V */
/* #  define SD_SUPPLY_VOLTAGE (1L<<22)  / * 3.4V - 3.5V */
/* #  define SD_SUPPLY_VOLTAGE (1L<<23)  / * 3.5V - 3.6V */


#elif CONFIG_HARDWARE_VARIANT==2
/* Hardware configuration: Shadowolf 1 */
#  define SD_SUPPLY_VOLTAGE     (1L<<18)
#define USE_FLASH_LED
#define FLASH_LED_PORT PORTC
#define FLASH_LED_DDR DDRC
#define FLASH_LED_PIN PC0
#define FLASH_LED_POLARITY 1

#define USE_ALIVE_LED
#define ALIVE_LED_PORT PORTC
#define ALIVE_LED_DDR DDRC
#define ALIVE_LED_PIN PC1

#define USE_FAT12
/* #define USE_FAT32 */
  
#define INIT_RETRIES 10


#elif CONFIG_HARDWARE_VARIANT == 3
/* Hardware configuration: LarsP */
#  define SD_SUPPLY_VOLTAGE     (1L<<21)


#elif CONFIG_HARDWARE_VARIANT == 4
/* Hardware configuration: uIEC */
#define USE_FLASH_LED
#define FLASH_LED_PORT PORTG
#define FLASH_LED_DDR DDRG
#define FLASH_LED_PIN PG4
#define FLASH_LED_POLARITY 0

#define USE_ALIVE_LED
#define ALIVE_LED_PORT PORTG
#define ALIVE_LED_DDR DDRG
#define ALIVE_LED_PIN PG4

#define USE_FAT12
#define USE_FAT32
  
#define INIT_RETRIES 1


#elif CONFIG_HARDWARE_VARIANT==5
/* Hardware configuration: Shadowolf 2 aka sd2iec 1.x */
#  define SD_SUPPLY_VOLTAGE     (1L<<18)
#define USE_FLASH_LED
#define FLASH_LED_PORT PORTC
#define FLASH_LED_DDR DDRC
#define FLASH_LED_PIN PC0
#define FLASH_LED_POLARITY 1

#define USE_ALIVE_LED
#define ALIVE_LED_PORT PORTC
#define ALIVE_LED_DDR DDRC
#define ALIVE_LED_PIN PC1

#define USE_FAT12
/* #define USE_FAT32 */
  
#define INIT_RETRIES 10


#elif CONFIG_HARDWARE_VARIANT == 6
/* Hardware configuration: NKC MMC2IEC */
#  define SD_SUPPLY_VOLTAGE     (1L<<21)


#else
#  error "CONFIG_HARDWARE_VARIANT is unset or set to an unknown value."
#endif



/* ---------------- End of user-configurable options ---------------- */

#endif
