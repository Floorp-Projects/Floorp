/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "utility.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio_coding_module.h"
#include "common_types.h"
#include "gtest/gtest.h"

#define NUM_CODECS_WITH_FIXED_PAYLOAD_TYPE 13

namespace webrtc {

ACMTestTimer::ACMTestTimer() :
_msec(0),
_sec(0),
_min(0),
_hour(0)
{
    return;
}

ACMTestTimer::~ACMTestTimer()
{
    return;
}

void ACMTestTimer::Reset()
{
    _msec = 0;
    _sec = 0;
    _min = 0;
    _hour = 0;
    return;
}
void ACMTestTimer::Tick10ms()
{
    _msec += 10;
    Adjust();
    return;
}

void ACMTestTimer::Tick1ms()
{
    _msec++;
    Adjust();
    return;
}

void ACMTestTimer::Tick100ms()
{
    _msec += 100;
    Adjust();
    return;
}

void ACMTestTimer::Tick1sec()
{
    _sec++;
    Adjust();
    return;
}

void ACMTestTimer::CurrentTimeHMS(char* currTime)
{
    sprintf(currTime, "%4lu:%02u:%06.3f", _hour, _min, (double)_sec + (double)_msec / 1000.); 
    return;
}

void ACMTestTimer::CurrentTime(
        unsigned long&  h, 
        unsigned char&  m,
        unsigned char&  s,
        unsigned short& ms)
{
    h = _hour;
    m = _min;
    s = _sec;
    ms = _msec;
    return;
}

void ACMTestTimer::Adjust()
{
    unsigned int n;
    if(_msec >= 1000)
    {
        n = _msec / 1000;
        _msec -= (1000 * n);
        _sec += n;
    }
    if(_sec >= 60)
    {
        n = _sec / 60;
        _sec -= (n * 60);
        _min += n;
    }
    if(_min >= 60)
    {
        n = _min / 60;
        _min -= (n * 60);
        _hour += n;
    }
}


WebRtc_Word16
ChooseCodec(
    CodecInst& codecInst)
{

    PrintCodecs();
    //AudioCodingModule* tmpACM = AudioCodingModule::Create(0);
    WebRtc_UWord8 noCodec = AudioCodingModule::NumberOfCodecs();
    WebRtc_Word8 codecID;
    bool outOfRange = false;
    char myStr[15] = "";
    do
    {
        printf("\nChoose a codec [0]: ");
        EXPECT_TRUE(fgets(myStr, 10, stdin) != NULL);
        codecID = atoi(myStr);
        if((codecID < 0) || (codecID >= noCodec))
        {
            printf("\nOut of range.\n");
            outOfRange = true;
        }
    } while(outOfRange);

    CHECK_ERROR(AudioCodingModule::Codec((WebRtc_UWord8)codecID, codecInst));
    return 0;
}

void
PrintCodecs()
{
    WebRtc_UWord8 noCodec = AudioCodingModule::NumberOfCodecs();
        
    CodecInst codecInst;
    printf("No  Name                [Hz]    [bps]\n");     
    for(WebRtc_UWord8 codecCntr = 0; codecCntr < noCodec; codecCntr++)
    {
        AudioCodingModule::Codec(codecCntr, codecInst);
        printf("%2d- %-18s %5d   %6d\n", 
            codecCntr, codecInst.plname, codecInst.plfreq, codecInst.rate);
    }

}

CircularBuffer::CircularBuffer(WebRtc_UWord32 len):
_buff(NULL),
_idx(0),
_buffIsFull(false),
_calcAvg(false),
_calcVar(false),
_sum(0),
_sumSqr(0)
{
    _buff = new double[len];
    if(_buff == NULL)
    {
        _buffLen = 0;
    }
    else
    {
        for(WebRtc_UWord32 n = 0; n < len; n++)
        {
            _buff[n] = 0;
        }
        _buffLen = len;
    }
}

CircularBuffer::~CircularBuffer()
{
    if(_buff != NULL)
    {
        delete [] _buff;
        _buff = NULL;
    }
}

void
CircularBuffer::Update(
    const double newVal)
{
    assert(_buffLen > 0);
    
    // store the value that is going to be overwritten
    double oldVal = _buff[_idx];
    // record the new value
    _buff[_idx] = newVal;
    // increment the index, to point to where we would
    // write next
    _idx++;
    // it is a circular buffer, if we are at the end
    // we have to cycle to the beginning 
    if(_idx >= _buffLen)
    {
        // flag that the buffer is filled up.
        _buffIsFull = true;
        _idx = 0;
    }
    
    // Update 

    if(_calcAvg)
    {
        // for the average we have to update
        // the sum
        _sum += (newVal - oldVal);
    }

    if(_calcVar)
    {
        // to calculate variance we have to update
        // the sum of squares 
        _sumSqr += (double)(newVal - oldVal) * (double)(newVal + oldVal);
    }
}

void 
CircularBuffer::SetArithMean(
    bool enable)
{
    assert(_buffLen > 0);

    if(enable && !_calcAvg)
    {
        WebRtc_UWord32 lim;
        if(_buffIsFull)
        {
            lim = _buffLen;
        }
        else
        {
            lim = _idx;
        }
        _sum = 0;
        for(WebRtc_UWord32 n = 0; n < lim; n++)
        {
            _sum += _buff[n];
        }
    }
    _calcAvg = enable;
}

void
CircularBuffer::SetVariance(
    bool enable)
{
    assert(_buffLen > 0);

    if(enable && !_calcVar)
    {
        WebRtc_UWord32 lim;
        if(_buffIsFull)
        {
            lim = _buffLen;
        }
        else
        {
            lim = _idx;
        }
        _sumSqr = 0;
        for(WebRtc_UWord32 n = 0; n < lim; n++)
        {
            _sumSqr += _buff[n] * _buff[n];
        }
    }
    _calcAvg = enable;
}

WebRtc_Word16
CircularBuffer::ArithMean(double& mean)
{
    assert(_buffLen > 0);

    if(_buffIsFull)
    {

        mean = _sum / (double)_buffLen;
        return 0;
    }
    else
    {
        if(_idx > 0)
        {
            mean = _sum / (double)_idx;
            return 0;
        }
        else
        {
            return -1;
        }

    }
}

WebRtc_Word16
CircularBuffer::Variance(double& var)
{
    assert(_buffLen > 0);

    if(_buffIsFull)
    {
        var = _sumSqr / (double)_buffLen;
        return 0;
    }
    else
    {
        if(_idx > 0)
        {
            var = _sumSqr / (double)_idx;
            return 0;
        }
        else
        {
            return -1;
        }
    }
}



bool
FixedPayloadTypeCodec(const char* payloadName)
{
    char fixPayloadTypeCodecs[NUM_CODECS_WITH_FIXED_PAYLOAD_TYPE][32] = {
        "PCMU",
        "PCMA",
        "GSM",
        "G723",
        "DVI4",
        "LPC",
        "PCMA",
        "G722",
        "QCELP",
        "CN",
        "MPA",
        "G728",
        "G729"
    };

    for(int n = 0; n < NUM_CODECS_WITH_FIXED_PAYLOAD_TYPE; n++)
    {
        if(!STR_CASE_CMP(payloadName, fixPayloadTypeCodecs[n]))
        {
            return true;
        }
    }
    return false;
}

DTMFDetector::DTMFDetector()
{
    for(WebRtc_Word16 n = 0; n < 1000; n++)
    {
        _toneCntr[n] = 0;
    }
}

DTMFDetector::~DTMFDetector()
{
}

WebRtc_Word32 DTMFDetector::IncomingDtmf(const WebRtc_UWord8 digitDtmf, const bool /* toneEnded */)
{
    fprintf(stdout, "%d-",digitDtmf);
    _toneCntr[digitDtmf]++;
    return 0;
}

void DTMFDetector::PrintDetectedDigits()
{
    for(WebRtc_Word16 n = 0; n < 1000; n++)
    {
        if(_toneCntr[n] > 0)
        {
            fprintf(stdout, "%d %u  msec, \n", n, _toneCntr[n]*10);
        }
    }
    fprintf(stdout, "\n");
    return;
}

void 
VADCallback::Reset()
{
    for(int n = 0; n < 6; n++)
    {
        _numFrameTypes[n] = 0;
    }
}

VADCallback::VADCallback()
{
    for(int n = 0; n < 6; n++)
    {
        _numFrameTypes[n] = 0;
    }
}

void
VADCallback::PrintFrameTypes()
{
    fprintf(stdout, "No encoding.................. %d\n", _numFrameTypes[0]);
    fprintf(stdout, "Active normal encoded........ %d\n", _numFrameTypes[1]);
    fprintf(stdout, "Passive normal encoded....... %d\n", _numFrameTypes[2]);
    fprintf(stdout, "Passive DTX wideband......... %d\n", _numFrameTypes[3]);
    fprintf(stdout, "Passive DTX narrowband....... %d\n", _numFrameTypes[4]);
    fprintf(stdout, "Passive DTX super-wideband... %d\n", _numFrameTypes[5]);
}

WebRtc_Word32 
VADCallback::InFrameType(
    WebRtc_Word16 frameType)
{
    _numFrameTypes[frameType]++;
    return 0;
}

} // namespace webrtc
