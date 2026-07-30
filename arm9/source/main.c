/*---------------------------------------------------------------------------------
  Tuna-viDS -- AVI + Xvid + MP3 player for Nintendo DS

  Copyright(C) 2007-2008 Michael "Chishm" Chisholm

  This program is free software ; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation ; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY ; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program ; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
---------------------------------------------------------------------------------*/
#include <errno.h>
#include <fat.h>
#include <malloc.h>
#include <nds.h>
#include <nds/arm9/console.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nds/arm9/dldi.h>
#include <stdio.h>

#include "controls.h"
#include "ipc9.h"
#include "player.h"
#include "version.h"
#include "video.h"


const char* clutTxtPath = "fat:/_nds/colorLut/currentSetting.txt";
u16* colorTable = NULL;

// const char *DEFAULTFILE = "/tuna-vids.avi";
const char *DEFAULTFILE = "fat:/tuna-vids.avi";
const char *DEFAULTFILEDSI = "fat:/tuna-vids_dsi.avi";

static bool fatInitSuccess = false;


const DISC_INTERFACE *dldiGet(void) {
	if(io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA)sysSetCartOwner(BUS_OWNER_ARM9);
	if(io_dldi_data->ioInterface.features & FEATURE_SLOT_NDS)sysSetCardOwner(BUS_OWNER_ARM9);
	return &io_dldi_data->ioInterface;
}

void DisplayConsole() {
	iprintf("\n");
	iprintf("     Loading please wait...     \n");
	iprintf("\n");
    iprintf(" Tuna-viDS v" VERSION_STRING "\n");
    iprintf("\n");
    iprintf(" AVI + Xvid + MP3 player by\n");
    iprintf(" Michael Chisholm (Chishm)\n");
    iprintf("\n");
    iprintf(" See documentation for a \n");
    iprintf(" full list of credits.\n");	
}

int exitProgram(void) {
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if (!keysHeld())break;
	}
	while (1) {
		swiWaitForVBlank();
		scanKeys();
		if (keysDown() != 0)break;
	}
	systemShutDown();
	return -1;
}


int main(int argc, const char* argv[])
{
    FILE* aviFile = NULL;
    const char* aviFileName;

    // Exception handling, just in case
    defaultExceptionHandler();

    // Give 128KiB of VRAM to the ARM7
    vramSetBankD(VRAM_D_ARM7_0x06000000);

    consoleSetup();

    powerOn(POWER_ALL_2D);
    lcdMainOnTop();

	sysSetCardOwner(BUS_OWNER_ARM9);

    // File set up
	if (isDSiMode()) {
		fatInitSuccess = fatMountSimple("fat", dldiGet());
	} else {
		fatInitSuccess = fatInitDefault();
	}
	
	DisplayConsole();
	
	if (!fatInitSuccess) {
		consoleClear();
        iprintf(" Failed to init FAT\n");
        return exitProgram();
    }

    // Video set up
    vramSetBankA(VRAM_A_LCD);
    vramSetBankB(VRAM_B_LCD);
    vramSetBankC(VRAM_C_LCD);

	if (access(clutTxtPath, F_OK) == 0) {
		// Load color LUT
		char lutName[128] = {0};
		FILE* file = fopen(clutTxtPath, "rb");
		fread(lutName, 1, 128, file);
		fclose(file);

		char colorLutPath[256];
		sprintf(colorLutPath, "/_nds/colorLut/%s.lut", lutName);

		if (access(colorLutPath, F_OK) == 0) {
			file = fopen(colorLutPath, "rb");
			fseek(file, 0, SEEK_END);
			off_t fsize = ftell(file);
			fseek(file, 0, SEEK_SET);

			if ((fsize >= 0x10000 && fsize < 0x20000) || fsize == 0x20000) {
				colorTable = (u16*)malloc(0x10000);
				fread(colorTable, 1, 0x10000, file);

				vramSetBankE(VRAM_E_LCD);
				vramcpy(VRAM_E, colorTable, 0x10000);
				free(colorTable);
				colorTable = VRAM_E;

				for (int i = 0; i < 256; i++) {
					BG_PALETTE_SUB[i] = colorTable[BG_PALETTE_SUB[i] % 0x8000];
				}
			}
			fclose(file);
		}
	}

    /* Most setup is done in libnds's initSystem() function, like:
     * irqInit()
     * irqEnable(IRQ_VBLANK)
     * fifoInit()
     */

    // VBLANK interrupt setup.
    irqSet(IRQ_VBLANK, vidBuf_VblankHandler);

    // Communication with ARM7
    if (!ipcInit()) {
		consoleClear();
		iprintf(" Failed to init IPC\n");
        return exitProgram();
    }

    // Get file name
    if (argc >= 2) {
        aviFileName = argv[1];
    } else {
        if (isDSiMode()) {
			aviFileName = DEFAULTFILEDSI;
		} else {
			aviFileName = DEFAULTFILE;
		}
    }

    // Load Video
    aviFile = fopen(aviFileName, "rb");
	
	// Use alternate higher quality video if on DSi. If it's not found default to the normal filepath.
	if (isDSiMode() && !aviFile) {
		aviFileName = DEFAULTFILE;
		aviFile = fopen(aviFileName, "rb");
	}
	
    if (!aviFile) {
        consoleClear();
		iprintf(" Error opening AVI file\n %s\n %s\n", aviFileName, strerror(errno));
        return exitProgram();
    }

    // xvid play
    play_movie(aviFile);

    if (fclose(aviFile) != 0) {
        consoleClear();
		iprintf(" Error closing AVI file\n %s\n", strerror(errno));
        return exitProgram();
    }
	
	consoleClear();

    iprintf(" Exiting...\n");

    ipcSend_Exit();

    return 0;
}

