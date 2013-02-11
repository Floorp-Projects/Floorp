/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory.h>

#include "NETEQTEST_NetEQClass.h"


NETEQTEST_NetEQClass::NETEQTEST_NetEQClass()
    :
    _inst(NULL),
    _instMem(NULL),
    _bufferMem(NULL),
    _preparseRTP(false),
    _fsmult(1),
    _isMaster(true),
    _noDecode(false)
{
#ifdef WINDOWS_TIMING
    _totTimeRecIn.QuadPart = 0;
    _totTimeRecOut.QuadPart = 0;
#endif
}

NETEQTEST_NetEQClass::NETEQTEST_NetEQClass(enum WebRtcNetEQDecoder *usedCodec, int noOfCodecs,
        WebRtc_UWord16 fs, WebRtcNetEQNetworkType nwType)
    :
    _inst(NULL),
    _instMem(NULL),
    _bufferMem(NULL),
    _preparseRTP(false),
    _fsmult(1),
    _isMaster(true),
    _noDecode(false)
{
#ifdef WINDOWS_TIMING
    _totTimeRecIn.QuadPart = 0;
    _totTimeRecOut.QuadPart = 0;
#endif

    if (assign() == 0)
    {
        if (init(fs) == 0)
        {
            assignBuffer(usedCodec, noOfCodecs, nwType);
        }
    }
}


NETEQTEST_NetEQClass::~NETEQTEST_NetEQClass()
{
    if (_instMem)
    {
        delete [] _instMem;
        _instMem = NULL;
    }

    if (_bufferMem)
    {
        delete [] _bufferMem;
        _bufferMem = NULL;
    }

    _inst = NULL;
}

int NETEQTEST_NetEQClass::assign()
{
    int memSize;

    WebRtcNetEQ_AssignSize(&memSize);

    if (_instMem)
    {
        delete [] _instMem;
        _instMem = NULL;
    }

    _instMem = new WebRtc_Word8[memSize];

    int ret = WebRtcNetEQ_Assign(&_inst, _instMem);

    if (ret)
    {
        printError();
    }

    return (ret);
}


int NETEQTEST_NetEQClass::init(WebRtc_UWord16 fs)
{
    int ret;

    if (!_inst)
    {
        // not assigned
        ret = assign();

        if (ret != 0)
        {
            printError();
            return (ret);
        }
    }

    ret = WebRtcNetEQ_Init(_inst, fs);

    if (ret != 0)
    {
        printError();
    }

    return (ret);

}


int NETEQTEST_NetEQClass::assignBuffer(enum WebRtcNetEQDecoder *usedCodec, int noOfCodecs, WebRtcNetEQNetworkType nwType)
{
    int numPackets, memSize, ret;

    if (!_inst)
    {
        // not assigned
        ret = assign();

        if (ret != 0)
        {
            printError();
            return (ret);
        }

        ret = init();

        if (ret != 0)
        {
            printError();
            return (ret);
        }
    }

    ret = WebRtcNetEQ_GetRecommendedBufferSize(_inst, usedCodec, noOfCodecs, nwType, &numPackets, &memSize);

    if (ret != 0)
    {
        printError();
        return (ret);
    }

    if (_bufferMem)
    {
        delete [] _bufferMem;
        _bufferMem = NULL;
    }

    _bufferMem = new WebRtc_Word8[memSize];

    memset(_bufferMem, -1, memSize);

    ret = WebRtcNetEQ_AssignBuffer(_inst, numPackets, _bufferMem, memSize);

    if (ret != 0)
    {
        printError();
    }

    return (ret);
}

int NETEQTEST_NetEQClass::loadCodec(WebRtcNetEQ_CodecDef &codecInst)
{
    int err = WebRtcNetEQ_CodecDbAdd(_inst, &codecInst);

    if (err)
    {
        printError();
    }

    return (err);
}

void NETEQTEST_NetEQClass::printError()
{
    if (_inst)
    {
        int errorCode = WebRtcNetEQ_GetErrorCode(_inst);

        if (errorCode)
        {
            char errorName[WEBRTC_NETEQ_MAX_ERROR_NAME];

            WebRtcNetEQ_GetErrorName(errorCode, errorName, WEBRTC_NETEQ_MAX_ERROR_NAME);

            printf("Error %i: %s\n", errorCode, errorName);
        }
    }
}

void NETEQTEST_NetEQClass::printError(NETEQTEST_RTPpacket &rtp)
{
    // print regular error info
    printError();

    // print extra info from packet
    printf("\tRTP: TS=%u, SN=%u, PT=%u, M=%i, len=%i\n",
           rtp.timeStamp(), rtp.sequenceNumber(), rtp.payloadType(),
           rtp.markerBit(), rtp.payloadLen());

}

int NETEQTEST_NetEQClass::recIn(NETEQTEST_RTPpacket &rtp)
{

    int err;
#ifdef WINDOWS_TIMING
    LARGE_INTEGER countA, countB;
#endif

    if (_preparseRTP)
    {
        WebRtcNetEQ_RTPInfo rtpInfo;
        // parse RTP header
        rtp.parseHeader(rtpInfo);

#ifdef WINDOWS_TIMING
        QueryPerformanceCounter(&countA); // get start count for processor
#endif

        err = WebRtcNetEQ_RecInRTPStruct(_inst, &rtpInfo, rtp.payload(), rtp.payloadLen(), rtp.time() * _fsmult * 8);

#ifdef WINDOWS_TIMING
        QueryPerformanceCounter(&countB); // get stop count for processor
        _totTimeRecIn.QuadPart += (countB.QuadPart - countA.QuadPart);
#endif

    }
    else
    {

#ifdef WINDOWS_TIMING
        QueryPerformanceCounter(&countA); // get start count for processor
#endif

        err = WebRtcNetEQ_RecIn(_inst, (WebRtc_Word16 *) rtp.datagram(), rtp.dataLen(), rtp.time() * _fsmult * 8);

#ifdef WINDOWS_TIMING
        QueryPerformanceCounter(&countB); // get stop count for processor
        _totTimeRecIn.QuadPart += (countB.QuadPart - countA.QuadPart);
#endif

    }

    if (err)
    {
        printError(rtp);
    }

    return (err);

}


WebRtc_Word16 NETEQTEST_NetEQClass::recOut(WebRtc_Word16 *outData, void *msInfo, enum WebRtcNetEQOutputType *outputType)
{
    int err;
    WebRtc_Word16 outLen = 0;
#ifdef WINDOWS_TIMING
    LARGE_INTEGER countA, countB;
#endif

#ifdef WINDOWS_TIMING
    QueryPerformanceCounter(&countA); // get start count for processor
#endif

    if (!msInfo)
    {
        // no msInfo given, do mono mode
        if (_noDecode)
        {
            err = WebRtcNetEQ_RecOutNoDecode(_inst, outData, &outLen);
        }
        else
        {
            err = WebRtcNetEQ_RecOut(_inst, outData, &outLen);
        }
    }
    else
    {
        // master/slave mode
        err = WebRtcNetEQ_RecOutMasterSlave(_inst, outData, &outLen, msInfo, static_cast<WebRtc_Word16>(_isMaster));
    }

#ifdef WINDOWS_TIMING
    QueryPerformanceCounter(&countB); // get stop count for processor
    _totTimeRecOut.QuadPart += (countB.QuadPart - countA.QuadPart);
#endif

    if (err)
    {
        printError();
    }
    else
    {
        int newfsmult = static_cast<int>(outLen / 80);

        if (newfsmult != _fsmult)
        {
#ifdef NETEQTEST_PRINT_WARNINGS
            printf("Warning: output sample rate changed\n");
#endif  // NETEQTEST_PRINT_WARNINGS
            _fsmult = newfsmult;
        }
    }

    if (outputType != NULL)
    {
        err = WebRtcNetEQ_GetSpeechOutputType(_inst, outputType);

        if (err)
        {
            printError();
        }
    }

    return (outLen);
}


WebRtc_UWord32 NETEQTEST_NetEQClass::getSpeechTimeStamp()
{

    WebRtc_UWord32 ts = 0;
    int err;

    err = WebRtcNetEQ_GetSpeechTimeStamp(_inst, &ts);

    if (err)
    {
        printError();
        ts = 0;
    }

    return (ts);

}

WebRtcNetEQOutputType NETEQTEST_NetEQClass::getOutputType() {
  WebRtcNetEQOutputType type;

  int err = WebRtcNetEQ_GetSpeechOutputType(_inst, &type);
  if (err)
  {
    printError();
    type = kOutputNormal;
  }
  return (type);
}

//NETEQTEST_NetEQVector::NETEQTEST_NetEQVector(int numChannels)
//:
//channels(numChannels, new NETEQTEST_NetEQClass())
//{
//    //for (int i = 0; i < numChannels; i++)
//    //{
//    //    channels.push_back(new NETEQTEST_NetEQClass());
//    //}
//}
//
//NETEQTEST_NetEQVector::NETEQTEST_NetEQVector(int numChannels, enum WebRtcNetEQDecoder *usedCodec, int noOfCodecs,
//                      WebRtc_UWord16 fs, WebRtcNetEQNetworkType nwType)
//                      :
//channels(numChannels, new NETEQTEST_NetEQClass(usedCodec, noOfCodecs, fs, nwType))
//{
//    //for (int i = 0; i < numChannels; i++)
//    //{
//    //    channels.push_back(new NETEQTEST_NetEQClass(usedCodec, noOfCodecs, fs, nwType));
//    //}
//}
//
//NETEQTEST_NetEQVector::~NETEQTEST_NetEQVector()
//{
//}

