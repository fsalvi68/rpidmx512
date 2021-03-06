/**
 * @file main.cpp
 *
 */
/* Copyright (C) 2019 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>

#include "hardwarebaremetal.h"
#include "networkh3emac.h"
#include "ledblinkbaremetal.h"

#include "console.h"

#include "artnetnode.h"
#include "artnetparams.h"
#include "ipprog.h"

#include "midi.h"
#include "midiparams.h"

#include "display.h"
#include "displaymax7219.h"

#include "artnetreader.h"
#include "ltcreader.h"
#include "midireader.h"

#include "ltcparams.h"

#if defined(ORANGE_PI)
 #include "spiflashinstall.h"
 #include "spiflashstore.h"
#endif

#include "software_version.h"

static const char NETWORK_INIT[] = "Network init ...";
static const char NODE_PARMAS[] = "Setting Node parameters ...";
static const char START_NODE[] = "Starting the Node ...";
static const char NODE_STARTED[] = "Node started";

extern "C" {

void notmain(void) {
	HardwareBaremetal hw;
	NetworkH3emac nw;
	LedBlinkBaremetal lb;

#if defined (ORANGE_PI)
	if (hw.GetBootDevice() == BOOT_DEVICE_MMC0) {
		SpiFlashInstall spiFlashInstall;
	}

	//SpiFlashStore spiFlashStore;
#endif

	Display display(0,4);

	LtcParams ltcParams;

	if (ltcParams.Load()) {
		ltcParams.Dump();
	}

	Midi midi;
	ArtNetNode node;

	LtcReader ltcReader(&node);
	MidiReader midiReader(&node);
	ArtNetReader artnetReader;

	uint8_t nHwTextLength;
	printf("[V%s] %s Compiled on %s at %s\n", SOFTWARE_VERSION, hw.GetBoardName(nHwTextLength), __DATE__, __TIME__);

	hw.SetLed(HARDWARE_LED_ON);

	console_status(CONSOLE_YELLOW, NETWORK_INIT);
	display.TextStatus(NETWORK_INIT);

	nw.Init();
	nw.Print();

	console_status(CONSOLE_YELLOW, NODE_PARMAS);
	display.TextStatus(NODE_PARMAS);

	node.SetShortName("LTC Node");

	ArtNetParams artnetParams;

	if (artnetParams.Load()) {
		artnetParams.Set(&node);
		artnetParams.Dump();
	}

	IpProg ipprog;
	node.SetIpProgHandler(&ipprog);

	console_status(CONSOLE_YELLOW, START_NODE);
	display.TextStatus(START_NODE);

	node.Print();
	node.Start();

	console_status(CONSOLE_GREEN, NODE_STARTED);
	display.TextStatus(NODE_STARTED);

	DisplayMax7219 max7219(ltcParams.GetMax7219Type());
	max7219.Init(ltcParams.GetMax7219Intensity());

	const TLtcReaderSource source = ltcParams.GetSource();

	if (source != LTC_READER_SOURCE_MIDI) {
		midi.Init(MIDI_DIRECTION_OUTPUT);
	}

	switch (source) {
	case LTC_READER_SOURCE_ARTNET:
		node.SetTimeCodeHandler(&artnetReader);
		artnetReader.Start();
		break;
	case LTC_READER_SOURCE_MIDI:
		midiReader.Start();
		break;
	default:
		ltcReader.Start();
		break;
	}

	midi.Print();

	console_puts("Source : ");
	display.ClearLine(4);
	display.Printf(4, "Src: ");

	switch (source) {
	case LTC_READER_SOURCE_ARTNET:
		puts("Art-Net");
		display.PutString("Art-Net");
		break;
	case LTC_READER_SOURCE_MIDI:
		puts("MIDI");
		display.PutString("MIDI");
		break;
	default:
		puts("LTC");
		display.PutString("LTC");
		break;
	}

	hw.WatchdogInit();

	for (;;) {
		hw.WatchdogFeed();

		nw.Run();
		node.HandlePacket();

		switch (source) {
		case LTC_READER_SOURCE_LTC:
			ltcReader.Run();
			break;
		case LTC_READER_SOURCE_ARTNET:
			artnetReader.Run();	// Handles MIDI Quarter Frame output messages
			break;
		case LTC_READER_SOURCE_MIDI:
			midiReader.Run();
			break;
		default:
			break;
		}
	}
}

}
