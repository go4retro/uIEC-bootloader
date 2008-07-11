#include <avr/io.h>
#include "mmc_lib.h"
#include "disk_lib.h"
#include "fat1216.h"

#include <util/delay.h>

static unsigned char cmd[6];

#define MMCCS_LO MMC_PORT &= ~(1<<MMC_CS)	//MMC Chip Select -> Low (activate)
#define MMCCS_HI MMC_PORT |= 1<<MMC_CS; //MMC Chip Select -> High (deactivate);

inline static void spi_send_byte(uint8_t data)
{
	SPDR=data;
	loop_until_bit_is_set(SPSR, SPIF); // wait for byte transmitted...
}

static uint8_t send_cmd(void)
{
	uint8_t i;
	
	spi_send_byte(0xFF); //Dummy delay 8 clocks
	
	MMCCS_LO;
	
	_delay_loop_2(1500);

	for (i=0; i<6; i++)
	{//Send 6 Bytes
		spi_send_byte(cmd[i]);
	}
	
	uint8_t result;
	
	for(i=0; i<128; i++)
	{//waiting for response (!0xff)
		spi_send_byte(0xFF);
		
		result = SPDR;
		
		if ((result & 0x80) == 0)
			break;
	}
	
	return(result); // TimeOut !
}

uint8_t disk_initialize(void)
{
	SPCR = 0;
	MMC_DDR = 1<<SPI_CLK | 1<<SPI_MOSI | 1<<MMC_CS;	//MMC Chip Select -> Output
	MMC_DDR = 0;
	MMC_DDR = 1<<SPI_CLK | 1<<SPI_MOSI | 1<<MMC_CS;	//MMC Chip Select -> Output
	
	SPCR = 1<<SPE | 1<<MSTR | SPI_INIT_CLOCK; //SPI Enable, SPI Master Mode
	SPSR = 0;

	uint8_t i;

	i = 12;
	while (i)
	{//Pulse 80+ clocks to reset MMC
		spi_send_byte(0xFF);
		i--;
	}
	
	uint8_t res;

	cmd[0] = 0x40 + MMC_GO_IDLE_STATE;
	cmd[1] = 0x00; cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 0x95;
	cmd[5] = 0x95;
	
	for (i=0; i<MMC_CMD0_RETRIES; i++)
	{
		res = send_cmd(); //store result of reset command, should be 0x01
		
		MMCCS_HI;

		spi_send_byte(0xFF);
		if (res == 0x01)
			break;
	}
	
	if (i == MMC_CMD0_RETRIES)
		return DISK_TIMEOUT;
	
	if (res != 0x01) //Response R1 from MMC (0x01: IDLE, The card is in idle state and running the initializing process.)
		return(DISK_INIT);
	
	cmd[0]=0x40 + MMC_SEND_OP_COND;
	
	i=255;
	
	while (i)
	{
		if (send_cmd() == 0)
		{
			MMCCS_HI;
			spi_send_byte(0xFF);
			spi_send_byte(0xFF);
			return DISK_OK;
		}
		i--;
	}
	
	MMCCS_HI;
	
	return(DISK_TIMEOUT);
}

static uint8_t wait_start_byte(void)
{
	uint8_t i = 255;
	
	do
	{
		spi_send_byte(0xFF);
		if (SPDR == 0xFE)
			return MMC_OK;
		
		i--;
	} while (i);
	
	return MMC_NOSTARTBYTE;
}

void disk_read(uint32_t adr) {
	adr <<= 1;
	
	cmd[0] = 0x40 + MMC_READ_SINGLE_BLOCK;
	cmd[1] = (adr & 0x00FF0000) >> 0x10;
	cmd[2] = (adr & 0x0000FF00) >> 0x08;
	cmd[3] = (adr & 0x000000FF);
	cmd[4] = 0;
	
	SPCR = 1<<SPE | 1<<MSTR | SPI_READ_CLOCK; //SPI Enable, SPI Master Mode
	
	if (send_cmd() != 0x00)
	{
		MMCCS_HI;
		return; //wrong response!
	}
	
	if (wait_start_byte())
	{
		MMCCS_HI;
		return;
	}
	
	unsigned char *buf = fat_buf;
	unsigned short len = 512;
	while (len--)
	{
		spi_send_byte(0xFF);
		*(buf++) = SPDR;
	}

	//read 2 bytes CRC (not used);
	spi_send_byte(0xFF);
	spi_send_byte(0xFF);
	MMCCS_HI;
}
