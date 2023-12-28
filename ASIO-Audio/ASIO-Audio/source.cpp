#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <iostream>
#include <atomic>

#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"
#include "driverinfo.h"
#include "callbacks.h"


// name of the ASIO device to be used
#define ASIO_DRIVER_NAME    "Focusrite USB ASIO"


DriverInfo asioDriverInfo = { 0 };
ASIOCallbacks asioCallbacks;

//----------------------------------------------------------------------------------
// some external references
extern AsioDrivers* asioDrivers;
bool loadAsioDriver(char* name);

#ifdef RECORDING_MODE
int* audioBuffer = new int[AUDIOBUFFER_SIZE](); // enough for 5 seconds of audio
#endif

bool* effectEn = new bool[MAX_EFFECTS]();


int main(int argc, char* argv[])
{
	static_assert(std::atomic<bool>::is_always_lock_free);

	// load the driver, this will setup all the necessary internal data structures
	if (loadAsioDriver((char*)ASIO_DRIVER_NAME))
	{
		// initialize the driver
		if (ASIOInit(&asioDriverInfo.driverInfo) == ASE_OK)
		{
			printf("asioVersion:   %d\n"
				"driverVersion: %d\n"
				"Name:          %s\n"
				"ErrorMessage:  %s\n",
				asioDriverInfo.driverInfo.asioVersion, asioDriverInfo.driverInfo.driverVersion,
				asioDriverInfo.driverInfo.name, asioDriverInfo.driverInfo.errorMessage);
			if (init_asio_static_data(&asioDriverInfo) == 0)
			{
				// ASIOControlPanel(); you might want to check wether the ASIOControlPanel() can open

				// set up the asioCallback structure and create the ASIO data buffer
				asioCallbacks.bufferSwitch = &bufferSwitch;
				asioCallbacks.sampleRateDidChange = &sampleRateChanged;
				asioCallbacks.asioMessage = &asioMessages;
				asioCallbacks.bufferSwitchTimeInfo = &bufferSwitchTimeInfo;
				if (create_asio_buffers(&asioDriverInfo) == ASE_OK)
				{
					if (ASIOStart() == ASE_OK)
					{
						// Now all is up and running
						//fprintf(stdout, "\nASIO Driver started succefully.\n\n");
						while (!asioDriverInfo.stopped)
						{
							//Sleep(100);	// goto sleep for 100 milliseconds
							// Toggle effect when any char is entered
							char n;
							std::cin >> n;
							effectEn[0] = !effectEn[0];
						}
						ASIOStop();
					}
					ASIODisposeBuffers();
				}
			}
			ASIOExit();
		}
		asioDrivers->removeCurrentDriver();
	}

#ifdef RECORDING_MODE
	FILE* fp = fopen("audio/newtest.wav", "wb");

	short fmt = 1;						  // Must be PCM (1)
	int fmtSize = 16;				      // Size of fmt data
	short chn = 1;						  // Number of Channels
	int Fs = 48000;						  // Sample Rate
	short sampleSize = 32;				  // Size of sample data in bits
	int dataSize = AUDIOBUFFER_SIZE * 4;  // Size of data section
	int fileSize = dataSize + 44;		  // Size of full file
	int byteRate = 192000;                // Fs * sampleSize * chn / 8
	short blockAlign = 4;                 // sampleSize * chn / 8

	fwrite("RIFF", 4, 1, fp);
	fwrite(&fileSize, 4, 1, fp);
	fwrite("WAVE", 4, 1, fp);
	fwrite("fmt ", 4, 1, fp);
	fwrite(&fmtSize, 4, 1, fp);
	fwrite(&fmt, 2, 1, fp);
	fwrite(&chn, 2, 1, fp);
	fwrite(&Fs, 4, 1, fp);
	fwrite(&byteRate, 4, 1, fp);
	fwrite(&blockAlign, 2, 1, fp);
	fwrite(&sampleSize, 2, 1, fp);
	fwrite("data", 4, 1, fp);
	fwrite(&dataSize, 4, 1, fp);

	fwrite(audioBuffer, sizeof(int), AUDIOBUFFER_SIZE, fp);

	fclose(fp);
	delete(audioBuffer);
#endif
	return 0;
}
