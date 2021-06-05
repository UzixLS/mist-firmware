// config.c

#include <stdio.h>
#include <string.h>

#include "errors.h"
#include "hardware.h"
#include "mmc.h"
#include "boot.h"
#include "fat_compat.h"
#include "osd.h"
#include "fpga.h"
#include "fdd.h"
#include "hdd.h"
#include "firmware.h"
#include "menu.h"
#include "config.h"
#include "user_io.h"
#include "usb/usb.h"
#include "misc_cfg.h"

configTYPE config;
extern char s[FF_LFN_BUF + 1];
char configfilename[12];
char DebugMode=0;
unsigned char romkey[3072];


// TODO fix SPIN macros all over the place!
#define SPIN() asm volatile ( "mov r0, r0\n\t" \
                              "mov r0, r0\n\t" \
                              "mov r0, r0\n\t" \
                              "mov r0, r0")

static void ClearKickstartMirrorE0(void)
{
  spi_osd_cmd32le_cont(OSD_CMD_WR, 0x00e00000);
  for (int i = 0; i < (0x80000 / 4); i++) {
    SPI(0x00);
    SPI(0x00);
    SPIN(); SPIN(); SPIN(); SPIN();
    SPI(0x00);
    SPI(0x00);
    SPIN(); SPIN(); SPIN(); SPIN();
  }
  DisableOsd();
  SPIN(); SPIN(); SPIN(); SPIN();
}

static void ClearVectorTable(void)
{
  spi_osd_cmd32le_cont(OSD_CMD_WR, 0x00000000);
  for (int i = 0; i < 256; i++) {
    SPI(0x00);
    SPI(0x00);
    SPIN(); SPIN(); SPIN(); SPIN();
    SPI(0x00);
    SPI(0x00);
    SPIN(); SPIN(); SPIN(); SPIN();
  }
  DisableOsd();
  SPIN(); SPIN(); SPIN(); SPIN();
}

//// UploadKickstart() ////
char UploadKickstart(char *name)
{
  FSIZE_t keysize=0;
  UINT br;
  FIL romfile, keyfile;

  ChangeDirectoryName("/");

  BootPrint("Checking for Amiga Forever key file:");
  if(FileOpenCompat(&keyfile,"ROM     KEY", FA_READ) == FR_OK) {
    keysize=f_size(&keyfile);
    if(keysize<sizeof(romkey)) {
      f_read(&keyfile, romkey, keysize, &br);
      BootPrint("Loaded Amiga Forever key file");
    } else {
      BootPrint("Amiga Forever keyfile is too large!");
    }
    f_close(&keyfile);
  }
  BootPrint("Loading file: ");
  BootPrint(name);

  if (f_open(&romfile, name, FA_READ) == FR_OK) {
    if (f_size(&romfile) == 0x100000) {
      // 1MB Kickstart ROM
      BootPrint("Uploading 1MB Kickstart ...");
      SendFileV2(&romfile, NULL, 0, 0xe00000, f_size(&romfile)>>10);
      SendFileV2(&romfile, NULL, 0, 0xf80000, f_size(&romfile)>>10);
      ClearVectorTable();
      f_close(&romfile);
      return(1);
    } else if(f_size(&romfile) == 0x80000) {
      // 512KB Kickstart ROM
      BootPrint("Uploading 512KB Kickstart ...");
      if (minimig_v1()) {
        PrepareBootUpload(0xF8, 0x08);
        SendFile(&romfile);
      } else {
        SendFileV2(&romfile, NULL, 0, 0xf80000, f_size(&romfile)>>9);
        f_rewind(&romfile);
        SendFileV2(&romfile, NULL, 0, 0xe00000, f_size(&romfile)>>9);
        ClearVectorTable();
      }
      f_close(&romfile);
      return(1);
    } else if ((f_size(&romfile) == 0x8000b) && keysize) {
      // 512KB Kickstart ROM
      BootPrint("Uploading 512 KB Kickstart (Probably Amiga Forever encrypted...)");
      if (minimig_v1()) {
        PrepareBootUpload(0xF8, 0x08);
        SendFileEncrypted(&romfile,romkey,keysize);
      } else {
        SendFileV2(&romfile, romkey, keysize, 0xf80000, f_size(&romfile)>>9);
        f_rewind(&romfile);
        SendFileV2(&romfile, romkey, keysize, 0xe00000, f_size(&romfile)>>9);
        ClearVectorTable();
      }
      f_close(&romfile);
      return(1);
    } else if (f_size(&romfile) == 0x40000) {
      // 256KB Kickstart ROM
      BootPrint("Uploading 256 KB Kickstart...");
      if (minimig_v1()) {
        PrepareBootUpload(0xF8, 0x04);
        SendFile(&romfile);
      } else {
        SendFileV2(&romfile, NULL, 0, 0xf80000, f_size(&romfile)>>9);
        f_rewind(&romfile);
        SendFileV2(&romfile, NULL, 0, 0xfc0000, f_size(&romfile)>>9);
        ClearVectorTable();
        ClearKickstartMirrorE0();
      }
      f_close(&romfile);
      return(1);
    } else if ((f_size(&romfile) == 0x4000b) && keysize) {
      // 256KB Kickstart ROM
      BootPrint("Uploading 256 KB Kickstart (Probably Amiga Forever encrypted...");
      if (minimig_v1()) {
        PrepareBootUpload(0xF8, 0x04);
        SendFileEncrypted(&romfile,romkey,keysize);
      } else {
        SendFileV2(&romfile, romkey, keysize, 0xf80000, f_size(&romfile)>>9);
        f_rewind(&romfile);
        SendFileV2(&romfile, romkey, keysize, 0xfc0000, f_size(&romfile)>>9);
        ClearVectorTable();
        ClearKickstartMirrorE0();
      }
      f_close(&romfile);
      return(1);
    } else {
      f_close(&romfile);
      BootPrint("Unsupported ROM file size!");
    }
  } else {
    siprintf(s, "No \"%s\" file!", name);
    BootPrint(s);
  }
  return(0);
}


//// UploadActionReplay() ////
char UploadActionReplay()
{
  FIL romfile;

  if(minimig_v1()) {
    if (FileOpenCompat(&romfile, "AR3     ROM", FA_READ) == FR_OK) {
      if (f_size(&romfile) == 0x40000) {
        // 256 KB Action Replay 3 ROM
        BootPrint("\nUploading Action Replay ROM...");
        PrepareBootUpload(0x40, 0x04);
        SendFile(&romfile);
        ClearMemory(0x440000, 0x40000);
        f_close(&romfile);
        return(1);
      } else {
        BootPrint("\nUnsupported AR3.ROM file size!!!");
        /* FatalError(6); */
        f_close(&romfile);
        return(0);
      }
    }
  } else {
    if (FileOpenCompat(&romfile, "HRTMON  ROM", FA_READ)== FR_OK) {
      int adr, data;
      puts("Uploading HRTmon ROM... ");
      SendFileV2(&romfile, NULL, 0, 0xa10000, (f_size(&romfile)+511)>>9);
      // HRTmon config
      adr = 0xa10000 + 20;
      spi_osd_cmd32le_cont(OSD_CMD_WR, adr);
      data = 0x00800000; // mon_size, 4 bytes
      SPI((data>>24)&0xff); SPI((data>>16)&0xff); SPIN(); SPIN(); SPIN(); SPIN(); SPI((data>>8)&0xff); SPI((data>>0)&0xff);
      data = 0x00; // col0h, 1 byte
      SPI((data>>0)&0xff);
      data = 0x5a; // col0l, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0x0f; // col1h, 1 byte
      SPI((data>>0)&0xff);
      data = 0xff; // col1l, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0xff; // right, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0x00; // keyboard, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0xff; // key, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = config.enable_ide[0] ? 0xff : 0; // ide, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0xff; // a1200, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = config.chipset&CONFIG_AGA ? 0xff : 0; // aga, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0xff; // insert, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0x0f; // delay, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0xff; // lview, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0x00; // cd32, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = config.chipset&CONFIG_NTSC ? 1 : 0; // screenmode, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0xff; // novbr, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 0; // entered, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      data = 1; // hexmode, 1 byte
      SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      DisableOsd();
      SPIN(); SPIN(); SPIN(); SPIN();
      adr = 0xa10000 + 68;
      spi_osd_cmd32le_cont(OSD_CMD_WR, adr);
      data = ((config.memory&0x3) + 1) * 512 * 1024; // maxchip, 4 bytes TODO is this correct?
      SPI((data>>24)&0xff); SPI((data>>16)&0xff); SPIN(); SPIN(); SPIN(); SPIN(); SPI((data>>8)&0xff); SPI((data>>0)&0xff);
      SPIN(); SPIN(); SPIN(); SPIN();
      DisableOsd();
      SPIN(); SPIN(); SPIN(); SPIN();
      f_close(&romfile);
      return(1);
    } else {
      puts("\rhrtmon.rom not found!\r");
      return(0);
    }
  }
  return(0);
}


//// SetConfigurationFilename() ////
void SetConfigurationFilename(int config)
{
  if(config)
    siprintf(configfilename,"MINIMIG%dCFG",config);
  else
    strcpy(configfilename,"MINIMIG CFG");
}


//// ConfigurationExists() ////
unsigned char ConfigurationExists(char *filename)
{
  FIL file;
  if(!filename) {
    // use slot-based filename if none provided
    filename=configfilename;
  }
  if (FileOpenCompat(&file, filename, FA_READ) == FR_OK) {
    f_close(&file);
    return(1);
  }
  return(0);
}

static void ApplyConfiguration(char reloadkickstart);

//// LoadConfiguration() ////
unsigned char LoadConfiguration(char *filename, int printconfig)
{
  static const char config_id[] = "MNMGCFG0";
  char updatekickstart=0;
  char result=0;
  unsigned char key, i;
  FIL file;

  if(!filename) {
    // use slot-based filename if none provided
    filename=configfilename;
  }

  // load configuration data
  if (FileOpenCompat(&file, filename, FA_READ) == FR_OK) {
    BootPrint("Opened configuration file\n");
    iprintf("Configuration file size: %llu\r", f_size(&file));
    if (f_size(&file) == sizeof(config)) {
      FileReadBlock(&file, sector_buffer);
      configTYPE *tmpconf=(configTYPE *)&sector_buffer;
      // check file id and version
      if (strncmp(tmpconf->id, config_id, sizeof(config.id)) == 0) {
        // A few more sanity checks...
        if(tmpconf->floppy.drives<=4) {
          // If either the old config and new config have a different kickstart file,
          // or this is the first boot, we need to upload a kickstart image.
          if(strncmp(tmpconf->kickstart,config.kickstart,sizeof(config.kickstart))!=0) {
            updatekickstart=true;
          }
          memcpy((void*)&config, (void*)sector_buffer, sizeof(config));
          result=1; // We successfully loaded the config.
        } else {
          BootPrint("Config file sanity check failed!\n");
        }
      } else {
        BootPrint("Wrong configuration file format!\n");
      }
    } else {
      iprintf("Wrong configuration file size: %lu (expected: %lu)\r", f_size(&file), sizeof(config));
    }
    f_close(&file);
  }
  if(!result) {
    BootPrint("Can not open configuration file!\n");
    BootPrint("Setting config defaults\n");
    // set default configuration
    memset((void*)&config, 0, sizeof(config));  // Finally found default config bug - params were reversed!
    strncpy(config.id, config_id, sizeof(config.id));
    strncpy(config.kickstart, "KICK.ROM", sizeof(config.kickstart));
    config.memory = 0x15;
    config.cpu = 0;
    config.chipset = 0;
    config.floppy.speed=CONFIG_FLOPPY2X;
    config.floppy.drives=1;
    config.enable_ide[0]=0;
    config.enable_ide[1]=0;
    config.hardfile[0].enabled = 1;
    strncpy(config.hardfile[0].name, "HARDFILE", sizeof(config.hardfile[0].name));
    strncpy(config.hardfile[1].name, "HARDFILE", sizeof(config.hardfile[1].name));
    config.hardfile[1].enabled = 2;  // Default is access to entire SD card
    updatekickstart=true;
    BootPrint("Defaults set\n");
  }

  // print config to boot screen
  if (minimig_v2() && printconfig) {
    char cfg_str[81];
    siprintf(cfg_str, "CPU:     %s", config_cpu_msg[config.cpu & 0x03]); BootPrintEx(cfg_str);
    siprintf(cfg_str, "Chipset: %s", config_chipset_msg [(config.chipset >> 2) & (minimig_v1()?3:7)]); BootPrintEx(cfg_str);
    siprintf(cfg_str, "Memory:  CHIP: %s  FAST: %s  SLOW: %s%s", 
        config_memory_chip_msg[(config.memory >> 0) & 0x03],
        config_memory_fast_txt(),
        config_memory_slow_msg[(config.memory >> 2) & 0x03],
        minimig_cfg.kick1x_memory_detection_patch ? "  [Kick 1.x patch enabled]" : "");
    BootPrintEx(cfg_str);
  }

  // wait up to 3 seconds for keyboard to appear. If it appears wait another
  // two seconds for the user to press a key
  int8_t keyboard_present = 0;
  for(i=0;i<3;i++) {
    unsigned long to = GetTimer(1000);
    while(!CheckTimer(to))
      usb_poll();

    // check if keyboard just appeared
    if(!keyboard_present && hid_keyboard_present()) {
      // BootPrintEx("Press F1 for NTSC, F2 for PAL");
      keyboard_present = 1;
      i = 0;
    }
  }
  
  key = OsdGetCtrl();
  if (key == KEY_F1) {
    // BootPrintEx("Forcing NTSC video ...");
    // force NTSC mode if F1 pressed
    config.chipset |= CONFIG_NTSC;
  }

  if (key == KEY_F2) {
    // BootPrintEx("Forcing PAL video ...");
    // force PAL mode if F2 pressed
    config.chipset &= ~CONFIG_NTSC;
  }

  ApplyConfiguration(updatekickstart);

  return(result);
}


//// ApplyConfiguration() ////
static void ApplyConfiguration(char reloadkickstart)
{
  ConfigCPU(config.cpu);

  if(reloadkickstart) {
    if(minimig_v1()) {
      ConfigChipset(config.chipset | CONFIG_TURBO); // set CPU in turbo mode
      ConfigFloppy(1, CONFIG_FLOPPY2X); // set floppy speed
      OsdReset(RESET_BOOTLOADER);

      if (!UploadKickstart(config.kickstart)) {
        strcpy(config.kickstart, "KICK.ROM");
        if (!UploadKickstart(config.kickstart)) {
          strcpy(config.kickstart, "AROS.ROM");
          if (!UploadKickstart(config.kickstart)) {
            FatalError(6);
          }
        }
      }

      //if (!CheckButton() && !config.disable_ar3) {
        // load Action Replay
        UploadActionReplay();
      //}
    }
  } else {
    ConfigChipset(config.chipset);
    ConfigFloppy(config.floppy.drives, config.floppy.speed);
  }

  char idxfail = 0;

  for (int i = 0; i < HARDFILES; i++)
    hardfile[i] = &config.hardfile[i];

  // Whether or not we uploaded a kickstart image we now need to set various parameters from the config.
  for (int i = 0; i < HARDFILES; i++) {
    if(OpenHardfile(i)) {
      switch(hdf[i].type) {
        // Customise message for SD card acces
        case (HDF_FILE | HDF_SYNTHRDB):
          siprintf(s, "\nHardfile %d (with fake RDB): %s", i, hardfile[i]->name);
          break;
        case HDF_FILE:
          siprintf(s, "\nHardfile %d: %s", i, hardfile[i]->name);
          break;
        case HDF_CARD:
          siprintf(s, "\nHardfile %d: using entire SD card", i);
          break;
        default:
          siprintf(s, "\nHardfile %d: using SD card partition %d", i, hdf[i].type-HDF_CARD);  // Number from 1
          break;
      }
      BootPrint(s);
      siprintf(s, "CHS: %u.%u.%u", hdf[i].cylinders, hdf[i].heads, hdf[i].sectors);
      BootPrint(s);
      siprintf(s, "Size: %lu MB", ((((unsigned long) hdf[i].cylinders) * hdf[i].heads * hdf[i].sectors) >> 11));
      BootPrint(s);
      siprintf(s, "Offset: %ld", hdf[i].offset);
      BootPrint(s);
      if (hdf[i].type & HDF_FILE && !hdf[i].idxfile->file.cltbl) idxfail = 1;
    }
  }
  if (idxfail)
    BootPrintEx("Warning! Indexing failed for a hardfile, continuing without indices.");

  ConfigIDE(config.enable_ide[0],        config.hardfile[0].present && config.hardfile[0].enabled, config.hardfile[1].present && config.hardfile[1].enabled);
  ConfigIDE(config.enable_ide[1] | 0x02, config.hardfile[2].present && config.hardfile[2].enabled, config.hardfile[3].present && config.hardfile[3].enabled);

  siprintf(s, "CPU clock     : %s", config.chipset & 0x01 ? "turbo" : "normal");
  BootPrint(s);
  siprintf(s, "Chip RAM size : %s", config_memory_chip_msg[config.memory & 0x03]);
  BootPrint(s);
  siprintf(s, "Slow RAM size : %s", config_memory_slow_msg[config.memory >> 2 & 0x03]);
  BootPrint(s);
  siprintf(s, "Fast RAM size : %s", config_memory_fast_txt());
  BootPrint(s);

  siprintf(s, "Floppy drives : %u", config.floppy.drives + 1);
  BootPrint(s);
  siprintf(s, "Floppy speed  : %s", config.floppy.speed ? "fast": "normal");
  BootPrint(s);

  BootPrint("");

  siprintf(s, "\nA600 IDE HDC is %s/%s.", config.enable_ide[0] ? "enabled" : "disabled", config.enable_ide[1] ? "enabled" : "disabled");
  BootPrint(s);
  for (int i = 0; i < HARDFILES; i++) {
    siprintf(s, "%s %s HDD is %s.",
      (i & 0x02) ? "Secondary" : "Primary", (i & 0x01) ? "Slave" : "Master",
      config.hardfile[i].present ? config.hardfile[i].enabled ? "enabled" : "disabled" : "not present");
    BootPrint(s);
  }

#if 0
  if (cluster_size < 64) {
    BootPrint("\n***************************************************");
    BootPrint(  "*  It's recommended to reformat your memory card  *");
    BootPrint(  "*   using 32 KB clusters to improve performance   *");
    BootPrint(  "*           when using large hardfiles.           *");  // AMR
    BootPrint(  "***************************************************");
  }
  iprintf("Bootloading is complete.\r");
#endif

  BootPrint("\nExiting bootloader...");

  ConfigMemory(config.memory);
  ConfigCPU(config.cpu);
  ConfigAutofire(config.autofire);

  if(minimig_v1()) {
    MM1_ConfigFilter(config.filter.lores, config.filter.hires);
    MM1_ConfigScanlines(config.scanlines);

    if(reloadkickstart) {
      WaitTimer(5000);
      BootExit();
    } else {
      OsdReset(RESET_NORMAL);
    }

    ConfigChipset(config.chipset);
    ConfigFloppy(config.floppy.drives, config.floppy.speed);
  } else {
    ConfigVideo(config.filter.hires, config.filter.lores, config.scanlines);
    ConfigChipset(config.chipset);
    ConfigFloppy(config.floppy.drives, config.floppy.speed);

    if(reloadkickstart) {
      iprintf("Reloading kickstart ...\r");
      TIMER_wait(1000);
      EnableOsd();
      SPI(OSD_CMD_RST);
      rstval |= (SPI_RST_CPU | SPI_CPU_HLT);
      SPI(rstval);
      DisableOsd();
      SPIN(); SPIN(); SPIN(); SPIN();
      UploadActionReplay();
      if (!UploadKickstart(config.kickstart)) {
        strcpy(config.kickstart, "KICK.ROM");
        if (!UploadKickstart(config.kickstart)) {
          FatalError(6);
        }
      }
    }
    iprintf("Resetting ...\r");
    EnableOsd();
    SPI(OSD_CMD_RST);
    rstval |= (SPI_RST_USR | SPI_RST_CPU);
    SPI(rstval);
    DisableOsd();
    SPIN(); SPIN(); SPIN(); SPIN();
    EnableOsd();
    SPI(OSD_CMD_RST);
    rstval = 0;
    SPI(rstval);
    DisableOsd();
    SPIN(); SPIN(); SPIN(); SPIN();
  }
}


//// SaveConfiguration() ////
unsigned char SaveConfiguration(char *filename)
{
  FIL file;
  UINT bw;

  if(!filename) {
    // use slot-based filename if none provided
    filename=configfilename;
  }

  // save configuration data
  if (FileOpenCompat(&file, filename, FA_WRITE | FA_OPEN_ALWAYS) == FR_OK) {
    iprintf("Configuration file size: %llu (expected: %lu) \r", f_size(&file), sizeof(config));
    if (f_size(&file) != sizeof(config)) {
      //f_truncate(&file);
    }
    if (f_write(&file, &config, sizeof(config), &bw) == FR_OK) {
      f_close(&file);
      iprintf("File written successfully.\r");
      return(1);
    }
    iprintf("File write failed!\r");
    f_close(&file);
  }
  return(0);
}
