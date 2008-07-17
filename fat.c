#include <avr/io.h>
#include <string.h>
#include "disk_lib.h"
#include "config.h"
#include "fat.h"

uint8_t fat_buf[512];
uint32_t filesize;
uint32_t filestart;

static uint16_t   RootDirRegionStartSec;  // technically, 32 bits, but probably OK here
static uint32_t   DataRegionStartSec;
static uint16_t   RootDirRegionInfo;      // technically, 32 bits, but probably OK here
static uint8_t    SectorsPerCluster;
static uint16_t   FATRegionStartSec;      // technically, 32 bits, but probably OK here
static parttype_t part_type;

static sector_t currentfatsector;

uint8_t fat_init(void)
{
	mbr_t *mbr = (mbr_t*) fat_buf;
	vbr_t *vbr = (vbr_t*) fat_buf;
	
	uint8_t init = 1, i;
	
	for (i=0; (i<INIT_RETRIES) && (init != 0); i++)
		init = disk_initialize();
	
	if (init != DISK_OK)
		return ERR_INIT;
		
  FATRegionStartSec = 0;
  part_type = PARTTYPE_NONE;
  do {
    //Load VBR
    disk_read(FATRegionStartSec);
    
    if (mbr->sector.magic != 0xAA55)
      return ERR_INVALID_SIG;

  //Check VBR
#ifdef USE_FAT32
    if (vbr->bsFileSysType32[0] == 'F' && vbr->bsFileSysType32[3] == '3') {
      part_type = PARTTYPE_FAT32;
      
      FATRegionStartSec       += vbr->bsRsvdSecCnt;
      if(!vbr->bsNrSeProFAT16)
        RootDirRegionStartSec = FATRegionStartSec + (vbr->bsNumFATs * vbr->bsNumSecPerFAT32); 
      else  
        RootDirRegionStartSec = FATRegionStartSec + (vbr->bsNumFATs * vbr->bsNrSeProFAT16);  
      DataRegionStartSec      = RootDirRegionStartSec;
      RootDirRegionInfo       = (uint16_t)(vbr->bsRootDir);
  
    } else
#endif  
    if ((vbr->bsFileSysType[0] == 'F') && (vbr->bsFileSysType[3] == '1')) {
#ifdef USE_FAT12
    if (vbr->bsFileSysType[4] == '2')
      part_type = PARTTYPE_FAT12;
    else
#endif
      if (vbr->bsFileSysType[4] == '6')
        part_type = PARTTYPE_FAT16;
    }
    
    //Try sector 0 as MBR
    if(!part_type) {
      if(FATRegionStartSec)
        return ERR_INVALID_SIG;
      FATRegionStartSec = mbr->sector.partition[0].sectorOffset;
    }
  } while(!part_type);
	
	SectorsPerCluster  			= vbr->bsSecPerClus;		// 8
	
#ifdef USE_FAT32
	if(part_type != PARTTYPE_FAT32) 
#endif
	// Calculation Algorithms
	{
	  FATRegionStartSec			+= vbr->bsRsvdSecCnt;						// 6
  	RootDirRegionStartSec	= FATRegionStartSec + (vbr->bsNumFATs * vbr->bsNrSeProFAT16);		// 496	
  	RootDirRegionInfo		 	= vbr->bsRootEntCnt / 16;						// 32
  	DataRegionStartSec 		= RootDirRegionStartSec + RootDirRegionInfo;	// 528
  }
	return 0;
}

uint8_t fat_readRootDirEntry(uint16_t entry_num) {
	direntry_t *dir; //Zeiger auf einen Verzeichniseintrag
	
#ifdef USE_FAT32
	if(part_type == PARTTYPE_FAT32) {
    uint32_t dirsector = entry_num / 16;
    if(fat_readfilesector(RootDirRegionInfo,dirsector))
      return ERR_ENDOFDIR;
	}
	else
#endif
	{
	  if ((entry_num / 16) >= RootDirRegionInfo)
	    return ERR_ENDOFDIR; //End of root dir region reached!

	  //uint32_t dirsector = RootDirRegionStartSec + entry_num * sizeof(direntry_t) / 512;
	  uint32_t dirsector = RootDirRegionStartSec + entry_num / 16;
	  disk_read(dirsector);
  }
  entry_num %= 512 / sizeof(direntry_t);
	

	dir = (direntry_t *) fat_buf + entry_num;

	if (dir->name[0] == 0)
	  return ERR_ENDOFDIR;
	if((dir->name[0] == 0xE5) || (dir->fstclust == 0))
		return ERR_DELETED_ENTRY;
		
	filesize = dir->filesize;
	filestart = dir->fstclust;
#ifdef USE_FAT32
	if(part_type == PARTTYPE_FAT32)
	  filestart |= ((uint32_t)(dir->filesize_hi)<<16);
#endif
	return ERR_OK;
}

static void load_fat_sector(sector_t sec)
{
	if (currentfatsector != sec)	{
		disk_read(FATRegionStartSec + sec);
		currentfatsector = sec;
	}
}


uint8_t fat_readfilesector(sector_t startcluster, sector_t filesector)
{
	sector_t clusteroffset;
	uint8_t temp, secoffset;
	uint32_t templong;
	
	fatsector_t *fatsector = (fatsector_t*) fat_buf;
	
	clusteroffset = filesector; // seit uint16_t filesector gehts nun doch ;)
	temp = SectorsPerCluster >> 1;
	while(temp)
	{
		clusteroffset >>= 1;
		temp >>= 1;
	}

	secoffset = (uint8_t)filesector & (SectorsPerCluster-1); // SectorsPerCluster is always power of 2 !
	
#ifdef USE_FAT32
	currentfatsector = 0xFFFFFFFF;
#else
	currentfatsector = 0xFFFF;
#endif
	
	while (clusteroffset)
	{
		if (part_type == PARTTYPE_FAT16)
		{//FAT16
			load_fat_sector(startcluster / 256);
			startcluster = fatsector->fat16_entries[startcluster % 256];
		}
		
		#ifdef USE_FAT12
		if (part_type == PARTTYPE_FAT12)
		{//FAT12
			uint32_t fat12_bytenum;
			uint8_t clust1, clust2;
	
			fat12_bytenum = (startcluster * 3 / 2);
			
			load_fat_sector(fat12_bytenum / 512);
			clust1 = fatsector->fat_bytes[fat12_bytenum % 512];
			fat12_bytenum++;
			load_fat_sector(fat12_bytenum / 512);
			clust2 = fatsector->fat_bytes[fat12_bytenum % 512];
		
			if (startcluster & 0x01)
				startcluster = ((clust1 & 0xF0) >> 4) | (clust2 << 4);
			else
				startcluster = clust1 | ((clust2 & 0x0F) << 8);
		}
		#endif
#ifdef USE_FAT32
    if (part_type == PARTTYPE_FAT32)
    {//FAT32
      load_fat_sector(startcluster / 128);
      startcluster = fatsector->fat32_entries[startcluster % 128];
    }
#endif    
		
		clusteroffset--;
	}
	
	if (startcluster < 2)
		return ERR_NO_DATA; //Free/reserved Sector
	
#ifdef USE_FAT32
  if (part_type == PARTTYPE_FAT32 && (startcluster & 0xFFFFFF0) == 0xFFFFFF0)
    return ERR_NO_DATA; //Reserved / bad / last cluster
  else if (part_type == PARTTYPE_FAT16)
#endif
	if ((startcluster & 0xFFF0) == 0xFFF0) //FAT16 hat nur 16Bit Clusternummern
		return ERR_NO_DATA; //Reserved / bad / last cluster

	templong = startcluster - 2;
	temp = SectorsPerCluster >> 1;
	while(temp)
	{
		templong <<= 1;
		temp >>= 1;
	}
		
	disk_read(templong + DataRegionStartSec + secoffset);
	return ERR_OK;
}
