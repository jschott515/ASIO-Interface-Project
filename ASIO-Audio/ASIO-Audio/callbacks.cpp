#define _USE_MATH_DEFINES

#include <stdio.h>
#include <math.h>
#include "callbacks.h"

// NOTE PROCESSEDSAMPLES WILL OVERFLOW IN CURRENT IMPLEMENTATION...

#define TEST_RUN_TIME  60.0		// run for 5 seconds

#define CONV_FACTOR 2147483648.49999

extern bool* effectEn;
extern DriverInfo asioDriverInfo;
#ifdef RECORDING_MODE
extern int* audioBuffer;
#endif

//----------------------------------------------------------------------------------
// conversion from 64 bit ASIOSample/ASIOTimeStamp to double float
#if NATIVE_INT64
#define ASIO64toDouble(a)  (a)
#else
const double twoRaisedTo32 = 4294967296.;
#define ASIO64toDouble(a)  ((a).lo + (a).hi * twoRaisedTo32)
#endif


void tremEffect(void* buff, long processedSamples, long buffSize)
{
	int* inBuffInt = (int*)buff;
	float* inBuffFloat = (float*)buff;

	int Fx = 5;
	float F = Fx / (float)asioDriverInfo.sampleRate;
	float alpha = 0.5;
	float trem;

	// Add trem effect in place
	for (int i = 0; i < buffSize; i++)
	{
		// Conv to float
		inBuffFloat[i] = (float)inBuffInt[i] / CONV_FACTOR;
		// Apply trem
		trem = (1 + alpha * sin(2 * M_PI * (processedSamples + i) * F));
		inBuffFloat[i] *= trem;
		// Conv to int
		inBuffInt[i] = (int)(inBuffFloat[i] * CONV_FACTOR);
	}
}


ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long index, ASIOBool processNow)
{
	static long processedSamples = 0;

	// buffer size in samples
	long buffSize = asioDriverInfo.preferredSize;

#ifdef RECORDING_MODE
	if (processedSamples < AUDIOBUFFER_SIZE - buffSize)
	{
		memcpy(&(audioBuffer[processedSamples]), asioDriverInfo.bufferInfos[1].buffers[index], buffSize * 4);
		processedSamples += buffSize;
	}
	else
		asioDriverInfo.stopped = true;
#else
	void (*func)(void*, long, long) = tremEffect;
	if (effectEn[0])
		func(asioDriverInfo.bufferInfos[1].buffers[index], processedSamples, buffSize);

	// perform the processing
	for (int i = 0; i < asioDriverInfo.inputBuffers + asioDriverInfo.outputBuffers; i++)
	{
		if (asioDriverInfo.bufferInfos[i].isInput == false)
		{
			memcpy(asioDriverInfo.bufferInfos[i].buffers[index], asioDriverInfo.bufferInfos[1].buffers[index], buffSize * 4);
		}
	}

	// finally if the driver supports the ASIOOutputReady() optimization, do it here, all data are in place
	if (asioDriverInfo.postOutput)
		ASIOOutputReady();

	if (processedSamples >= asioDriverInfo.sampleRate * TEST_RUN_TIME)	// roughly measured
		asioDriverInfo.stopped = true;
	else
		processedSamples += buffSize;

#endif
	return 0L;
}

//----------------------------------------------------------------------------------
void bufferSwitch(long index, ASIOBool processNow)
{	// the actual processing callback.
	// Beware that this is normally in a seperate thread, hence be sure that you take care
	// about thread synchronization. This is omitted here for simplicity.

	// as this is a "back door" into the bufferSwitchTimeInfo a timeInfo needs to be created
	// though it will only set the timeInfo.samplePosition and timeInfo.systemTime fields and the according flags
	ASIOTime  timeInfo;
	memset(&timeInfo, 0, sizeof(timeInfo));

	// get the time stamp of the buffer, not necessary if no
	// synchronization to other media is required
	if (ASIOGetSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime) == ASE_OK)
		timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

	bufferSwitchTimeInfo(&timeInfo, index, processNow);
}


//----------------------------------------------------------------------------------
void sampleRateChanged(ASIOSampleRate sRate)
{
	// do whatever you need to do if the sample rate changed
	// usually this only happens during external sync.
	// Audio processing is not stopped by the driver, actual sample rate
	// might not have even changed, maybe only the sample rate status of an
	// AES/EBU or S/PDIF digital input at the audio device.
	// You might have to update time/sample related conversion routines, etc.
}

//----------------------------------------------------------------------------------
long asioMessages(long selector, long value, void* message, double* opt)
{
	// currently the parameters "value", "message" and "opt" are not used.
	long ret = 0;
	switch (selector)
	{
	case kAsioSelectorSupported:
		if (value == kAsioResetRequest
			|| value == kAsioEngineVersion
			|| value == kAsioResyncRequest
			|| value == kAsioLatenciesChanged
			// the following three were added for ASIO 2.0, you don't necessarily have to support them
			|| value == kAsioSupportsTimeInfo
			|| value == kAsioSupportsTimeCode
			|| value == kAsioSupportsInputMonitor)
			ret = 1L;
		break;
	case kAsioResetRequest:
		// defer the task and perform the reset of the driver during the next "safe" situation
		// You cannot reset the driver right now, as this code is called from the driver.
		// Reset the driver is done by completely destruct is. I.e. ASIOStop(), ASIODisposeBuffers(), Destruction
		// Afterwards you initialize the driver again.
		asioDriverInfo.stopped;  // In this sample the processing will just stop
		ret = 1L;
		break;
	case kAsioResyncRequest:
		// This informs the application, that the driver encountered some non fatal data loss.
		// It is used for synchronization purposes of different media.
		// Added mainly to work around the Win16Mutex problems in Windows 95/98 with the
		// Windows Multimedia system, which could loose data because the Mutex was hold too long
		// by another thread.
		// However a driver can issue it in other situations, too.
		ret = 1L;
		break;
	case kAsioLatenciesChanged:
		// This will inform the host application that the drivers were latencies changed.
		// Beware, it this does not mean that the buffer sizes have changed!
		// You might need to update internal delay data.
		ret = 1L;
		break;
	case kAsioEngineVersion:
		// return the supported ASIO version of the host application
		// If a host applications does not implement this selector, ASIO 1.0 is assumed
		// by the driver
		ret = 2L;
		break;
	case kAsioSupportsTimeInfo:
		// informs the driver wether the asioCallbacks.bufferSwitchTimeInfo() callback
		// is supported.
		// For compatibility with ASIO 1.0 drivers the host application should always support
		// the "old" bufferSwitch method, too.
		ret = 1;
		break;
	case kAsioSupportsTimeCode:
		// informs the driver wether application is interested in time code info.
		// If an application does not need to know about time code, the driver has less work
		// to do.
		ret = 0;
		break;
	}
	return ret;
}
