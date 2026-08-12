/*---------------------------------------------------------------------------------

	default ARM7 core

	Copyright (C) 2005
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <string.h>

#include "soundcommon.h"
#include "sound7.h"
#include "ipc7.h"

// #define REG_VRAMSTAT (*(vu8*)0x04000240)
#define VRAM_START ((char*)0x06000000)
#define VRAM_END ((char*)0x06020000)

extern char *fake_heap_start;
extern char *fake_heap_end;
volatile bool exitflag = false;

static void VblankHandler(void)
{
	inputGetAndSend();
    // Not using Wifi
    //Wifi_Update();
}

static void powerButtonCB(void)
{
    exitflag = true;
}

/* Use VRAM as heap for malloc. This must be called before any malloc is done. */
static void initVramHeap(void)
{
    // Wait for VRAM bank D to become available
    while ((REG_VRAMSTAT & 0x02) == 0) {
        // Busy wait loop
    }

    // Clear VRAM
    memset(VRAM_START, 0, VRAM_END - VRAM_START);

    // Use VRAM as heap
    fake_heap_start = VRAM_START;
    fake_heap_end = VRAM_END;
}

void toggleBottomLight(void)
{
    u16 oldIME = enterCriticalSection();

    // Toggle lower screen's backlight
    int oldPowerReg = readPowerManagement(PM_CONTROL_REG);
    writePowerManagement(PM_CONTROL_REG, oldPowerReg ^ PM_BACKLIGHT_BOTTOM);

    leaveCriticalSection(oldIME);
}

void exitMainLoop(void)
{
    exitflag = true;
}

int main(void)
{
    // Initialize sound hardware
    enableSound();

    // Read user information from the firmware (name, birthday, etc)
    readUserSettings();

    // Stop LED blinking
    ledBlink(LED_ALWAYS_ON);

    // Using the calibration values read from the firmware with
    // readUserSettings(), calculate some internal values to convert raw
    // coordinates into screen coordinates.
    touchInit();

    irqInit();
    fifoInit();

    installSystemFIFO(); // Sleep mode, storage, firmware...

    // This sets a callback that is called when the power button in a DSi
    // console is pressed. It has no effect in a DS.
    setPowerButtonCB(powerButtonCB);

    // Read current date from the RTC and setup an interrupt to update the time
    // regularly. The interrupt simply adds one second every time, it doesn't
    // read the date. Reading the RTC is very slow, so it's a bad idea to do it
    // frequently.
    initClockIRQTimer(3);

    // Now that the FIFO is setup we can start sending input data to the ARM9.
	irqSet(IRQ_VBLANK, VblankHandler);
	irqEnable(IRQ_VBLANK);

    // Communication with ARM9
    ipcInit();

    initVramHeap();

    // Provide MP3 playback
    SoundInit();

    // Keep the ARM7 mostly idle
    while (!exitflag) {
        if (0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
            exitflag = true;
            SoundState_Stop();
        }
        SoundLoopStep();
        swiWaitForVBlank();
    }

    // Stop sound before exiting, in case something is still playing
    SCHANNEL_CR(0) = 0;
    SCHANNEL_CR(1) = 0;
    REG_SOUNDCNT = 0;

    return 0;
}

