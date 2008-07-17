#include <util/delay.h>
#include <avr/io.h>
#include "config.h"
#include "disk_lib.h"
#include "ata.h"
#include "fat.h"

#define FALSE 0
#define TRUE 1

#define ATA_WRITE_CMD(cmd) { ata_write_reg(ATA_REG_CMD,cmd); }

/* Yes, this is a very inaccurate delay mechanism, but this interface only
 * uses it for timeout that will cause a fatal error, so it should be fine.
 * About the only downside is a possible additional delay if a drive is not
 * present on the bus.
 */

#define DELAY_VALUE(ms) ((double)F_CPU/40000UL * ms)
#define ATA_INIT_TIMEOUT 31000  /* 31 sec */

/*-----------------------------------------------------------------------*/
/* Read an ATA register                                                  */
/*-----------------------------------------------------------------------*/

static uint8_t ata_read_reg(uint8_t reg) {
  uint8_t data;

  ATA_PORT_CTRL_OUT = reg;
  ATA_PORT_CTRL_OUT &= (uint8_t)~ATA_PIN_RD;
  ATA_PORT_CTRL_OUT &= (uint8_t)~ATA_PIN_RD;
  ATA_PORT_CTRL_OUT &= (uint8_t)~ATA_PIN_RD;
  data = ATA_PORT_DATA_LO_IN;
  ATA_PORT_CTRL_OUT |= ATA_PIN_RD;
  return data;
}


/*-----------------------------------------------------------------------*/
/* Write a byte to an ATA register                                       */
/*-----------------------------------------------------------------------*/

static void ata_write_reg(uint8_t reg, uint8_t data) {
  ATA_PORT_DATA_LO_DDR = 0xff;            /* bring to output */
  ATA_PORT_DATA_LO_OUT = data;
  ATA_PORT_CTRL_OUT = reg;
  ATA_PORT_CTRL_OUT &= (uint8_t)~ATA_PIN_WR;
  ATA_PORT_CTRL_OUT &= (uint8_t)~ATA_PIN_WR; /* delay */
  ATA_PORT_CTRL_OUT |= ATA_PIN_WR;
  ATA_PORT_DATA_LO_OUT = 0xff;
  ATA_PORT_DATA_LO_DDR = 0x00;            /* bring to input */
}


/*-----------------------------------------------------------------------*/
/* Wait for Data Ready                                                   */
/*-----------------------------------------------------------------------*/

static uint8_t ata_wait_data(void) {
  uint8_t s;
  uint32_t i = DELAY_VALUE(1000);
  do {
    if(!--i) return FALSE;
    s=ata_read_reg(ATA_REG_STATUS);
  } while((s & (ATA_STATUS_BSY | ATA_STATUS_DRQ)) != ATA_STATUS_DRQ && !(s & ATA_STATUS_ERR));
  //} while((s&ATA_STATUS_BSY)!= 0 && (s&ATA_STATUS_ERR)==0 && (s & (ATA_STATUS_DRDY | ATA_STATUS_DRQ)) != (ATA_STATUS_DRDY | ATA_STATUS_DRQ));
  if(s & ATA_STATUS_ERR)
    return FALSE;

  ata_read_reg(ATA_REG_ALTSTAT);
  return TRUE;
}


/*-----------------------------------------------------------------------*/
/* Wait for Busy Low                                                     */
/*-----------------------------------------------------------------------*/

static uint8_t ata_wait_busy(void) {
  uint8_t s;
  uint32_t i = DELAY_VALUE(1000);
  do {
    if(!--i) return FALSE;
    s=ata_read_reg(ATA_REG_STATUS);
  } while(s & ATA_STATUS_BSY);
  if (s & ATA_STATUS_ERR) return FALSE;
  return TRUE;
}


static void ata_select_sector(uint32_t sector) {
  ata_write_reg(ATA_REG_COUNT, 1);
  ata_write_reg(ATA_REG_LBA0, (uint8_t)sector);
  ata_write_reg(ATA_REG_LBA1, (uint8_t)(sector >> 8));
  ata_write_reg(ATA_REG_LBA2, (uint8_t)(sector >> 16));
  ata_write_reg(ATA_REG_LBA3, ((uint8_t)(sector >> 24) & 0x0F)
                              | ATA_LBA3_LBA
                              | ATA_DEV_MASTER);
}


/*-----------------------------------------------------------------------*/
/* Read a part of data block                                             */
/*-----------------------------------------------------------------------*/

static void ata_read_part (uint8_t *buff, uint8_t ofs, uint8_t count) {
  uint8_t c = 0, dl, dh;

  ATA_PORT_CTRL_OUT = ATA_REG_DATA;          /* Select Data register */
  do {
    ATA_PORT_CTRL_OUT &= (uint8_t)~ATA_PIN_RD;  /* IORD = L */
    ATA_PORT_CTRL_OUT &= (uint8_t)~ATA_PIN_RD;  /* delay */
    dl = ATA_PORT_DATA_LO_IN;                /* Read even data */
    dh = ATA_PORT_DATA_HI_IN;                /* Read odd data */
    ATA_PORT_CTRL_OUT |= ATA_PIN_RD;         /* IORD = H */
    if (count && (c >= ofs)) {               /* Pick up a part of block */
      *buff++ = dl;
      *buff++ = dh;
      count--;
    }
  } while (++c);
  //ata_read_reg(ATA_REG_ALTSTAT);
  ata_read_reg(ATA_REG_STATUS);
}


uint8_t disk_initialize(void)
{
  /* Initialize the ATA control port */
  ATA_PORT_CTRL_OUT = 0xff;
  ATA_PORT_CTRL_DDR = 0xff;

  uint8_t data[2];
  uint32_t i = DELAY_VALUE(ATA_INIT_TIMEOUT);
  
  // we need to set the drive.
  ata_write_reg (ATA_REG_LBA3, ATA_LBA3_LBA | ATA_DEV_MASTER);
  do {
    if (!--i) goto di_error;
  } while ((ata_read_reg(ATA_REG_STATUS) & (ATA_STATUS_BSY | ATA_STATUS_DRDY)) == ATA_STATUS_BSY);

  ata_write_reg(ATA_REG_DEVCTRL, ATA_DEVCTRL_SRST | ATA_DEVCTRL_NIEN);  /* Software reset */
  _delay_ms(20);
  ata_write_reg(ATA_REG_DEVCTRL, ATA_DEVCTRL_NIEN);
  _delay_ms(20);
  i = DELAY_VALUE(ATA_INIT_TIMEOUT);
  do {
    if (!--i) goto di_error;
  } while ((ata_read_reg(ATA_REG_STATUS) & (ATA_STATUS_DRDY|ATA_STATUS_BSY)) != ATA_STATUS_DRDY);

  ata_write_reg (ATA_REG_FEATURES, 3); /* set PIO mode 0 */
  ata_write_reg (ATA_REG_COUNT, 1);
  ATA_WRITE_CMD (ATA_CMD_SETFEATURES);
  if (!ata_wait_busy()) goto di_error; /* Wait cmd ready */
  ATA_WRITE_CMD(ATA_CMD_IDENTIFY);
  if(!ata_wait_data()) goto di_error;
  ata_read_part(data, 49, 1);
  if(!(data[1] & 0x02)) goto di_error; /* No LBA support */

  return DISK_OK;

di_error:
  ATA_PORT_DATA_HI_OUT = 0xff; // signal we did not find the disk, don't try again.
	return(DISK_INIT);
}

void disk_read(uint32_t adr) {
  /* Issue Read Sector(s) command */
  ata_select_sector(adr);
  ATA_WRITE_CMD(ATA_CMD_READ);

  unsigned char *buf = fat_buf;
  uint8_t c, iord_l, iord_h;
  iord_h = ATA_REG_DATA;
  iord_l = ATA_REG_DATA & (uint8_t)~ATA_PIN_RD;
  if (!ata_wait_data()) return; /* Wait data ready */
  ATA_PORT_CTRL_OUT = ATA_REG_DATA;
  c = 0;
  do {
    ATA_PORT_CTRL_OUT = iord_l;       /* IORD = L */
    ATA_PORT_CTRL_OUT = iord_l;       /* delay */
    ATA_PORT_CTRL_OUT = iord_l;       /* delay */
    ATA_PORT_CTRL_OUT = iord_l;       /* delay */
    ATA_PORT_CTRL_OUT = iord_l;       /* delay */
    *buf++ = ATA_PORT_DATA_LO_IN;     /* Get even data */
    *buf++ = ATA_PORT_DATA_HI_IN;     /* Get odd data */
    ATA_PORT_CTRL_OUT = iord_h;       /* IORD = H */
    ATA_PORT_CTRL_OUT = iord_h;       /* delay */
    ATA_PORT_CTRL_OUT = iord_h;       /* delay */
    ATA_PORT_CTRL_OUT = iord_h;       /* delay */
  } while (++c);

  //ata_read_reg(ATA_REG_ALTSTAT);
  ata_read_reg(ATA_REG_STATUS);

  return;
}

