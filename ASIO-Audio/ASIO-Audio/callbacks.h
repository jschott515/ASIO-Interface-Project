#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "asiosys.h"
#include "driverinfo.h"
#include "yin.hpp"

//#define RECORDING_MODE
#define AUDIOBUFFER_SIZE 240000

#define MAX_EFFECTS 1


void bufferSwitch(long index, ASIOBool processNow);
ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long index, ASIOBool processNow);
void sampleRateChanged(ASIOSampleRate sRate);
long asioMessages(long selector, long value, void* message, double* opt);

void tremEffect(void* buff, long processedSamples, long buffSize);

#endif // !CALLBACKS_H
