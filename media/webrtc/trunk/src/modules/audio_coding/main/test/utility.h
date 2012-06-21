/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ACM_TEST_UTILITY_H
#define ACM_TEST_UTILITY_H

#include "audio_coding_module.h"
#include "gtest/gtest.h"

namespace webrtc {

//-----------------------------
#define CHECK_ERROR(f)                                                                      \
    do {                                                                                    \
        EXPECT_GE(f, 0) << "Error Calling API";                                             \
    }while(0)

//-----------------------------
#define CHECK_PROTECTED(f)                                                                  \
    do {                                                                                    \
        if(f >= 0) {                                                                        \
            ADD_FAILURE() << "Error Calling API";                                           \
        }                                                                                   \
        else {                                                                              \
            printf("An expected error is caught.\n");                                       \
        }                                                                                   \
    }while(0)

//----------------------------
#define CHECK_ERROR_MT(f)                                                                   \
    do {                                                                                    \
        if(f < 0) {                                                                         \
            fprintf(stderr, "Error Calling API in file %s at line %d \n",                   \
                __FILE__, __LINE__);                                                        \
        }                                                                                   \
    }while(0)

//----------------------------
#define CHECK_PROTECTED_MT(f)                                                               \
    do {                                                                                    \
        if(f >= 0) {                                                                        \
            fprintf(stderr, "Error Calling API in file %s at line %d \n",                   \
                __FILE__, __LINE__);                                                        \
        }                                                                                   \
        else {                                                                              \
            printf("An expected error is caught.\n");                                       \
        }                                                                                   \
    }while(0)



#ifdef WIN32
    /* Exclude rarely-used stuff from Windows headers */
    //#define WIN32_LEAN_AND_MEAN 
    /* OS-dependent case-insensitive string comparison */
    #define STR_CASE_CMP(x,y) ::_stricmp(x,y)
#else
    /* OS-dependent case-insensitive string comparison */
    #define STR_CASE_CMP(x,y) ::strcasecmp(x,y)
#endif

#define DESTROY_ACM(acm)                                                                    \
    do {                                                                                    \
        if(acm != NULL) {                                                                   \
            AudioCodingModule::Destroy(acm);                       \
            acm = NULL;                                                                     \
        }                                                                                   \
    } while(0)


#define DELETE_POINTER(p)                                                                   \
    do {                                                                                    \
        if(p != NULL) {                                                                     \
            delete p;                                                                       \
            p = NULL;                                                                       \
        }                                                                                   \
    } while(0)

class ACMTestTimer
{
public:
    ACMTestTimer();
    ~ACMTestTimer();

    void Reset();
    void Tick10ms();
    void Tick1ms();
    void Tick100ms();
    void Tick1sec();
    void CurrentTimeHMS(
        char* currTime);
    void CurrentTime(
        unsigned long&  h, 
        unsigned char&  m,
        unsigned char&  s,
        unsigned short& ms);

private:
    void Adjust();

    unsigned short _msec;
    unsigned char  _sec;
    unsigned char  _min;
    unsigned long  _hour;  
};



class CircularBuffer
{
public:
    CircularBuffer(WebRtc_UWord32 len);
    ~CircularBuffer();

    void SetArithMean(
        bool enable);
    void SetVariance(
        bool enable);

    void Update(
        const double newVal);
    void IsBufferFull();
    
    WebRtc_Word16 Variance(double& var);
    WebRtc_Word16 ArithMean(double& mean);

protected:
    double* _buff;
    WebRtc_UWord32 _idx;
    WebRtc_UWord32 _buffLen;

    bool         _buffIsFull;
    bool         _calcAvg;
    bool         _calcVar;
    double       _sum;
    double       _sumSqr;
};





WebRtc_Word16 ChooseCodec(
    CodecInst& codecInst);

void PrintCodecs();

bool FixedPayloadTypeCodec(const char* payloadName);




class DTMFDetector: public AudioCodingFeedback
{
public:
    DTMFDetector();
    ~DTMFDetector();
    // used for inband DTMF detection
    WebRtc_Word32 IncomingDtmf(const WebRtc_UWord8 digitDtmf, const bool toneEnded);
    void PrintDetectedDigits();

private:
    WebRtc_UWord32 _toneCntr[1000];

};




class VADCallback : public ACMVADCallback
{
public:
    VADCallback();
    ~VADCallback(){}

    WebRtc_Word32 InFrameType(
        WebRtc_Word16 frameType);

    void PrintFrameTypes();
    void Reset();

private:
    WebRtc_UWord32 _numFrameTypes[6];
};

} // namespace webrtc

#endif // ACM_TEST_UTILITY_H
