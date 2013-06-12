/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETEQTEST_NETEQCLASS_H
#define NETEQTEST_NETEQCLASS_H

#include <stdio.h>
#include <vector>

#include "webrtc_neteq.h"
#include "webrtc_neteq_internal.h"

#include "NETEQTEST_RTPpacket.h"

#ifdef WIN32
#define WINDOWS_TIMING // complexity measurement only implemented for windows
//TODO(hlundin):Add complexity testing for Linux.
#include <windows.h>
#endif

class NETEQTEST_NetEQClass
{
public:
    NETEQTEST_NetEQClass();
    NETEQTEST_NetEQClass(enum WebRtcNetEQDecoder *usedCodec, int noOfCodecs, 
        uint16_t fs = 8000, WebRtcNetEQNetworkType nwType = kTCPLargeJitter);
    ~NETEQTEST_NetEQClass();

    int assign();
    int init(uint16_t fs = 8000);
    int assignBuffer(enum WebRtcNetEQDecoder *usedCodec, int noOfCodecs, WebRtcNetEQNetworkType nwType = kTCPLargeJitter);
    int loadCodec(WebRtcNetEQ_CodecDef & codecInst);
    int recIn(NETEQTEST_RTPpacket & rtp);
    int16_t recOut(int16_t *outData, void *msInfo = NULL, enum WebRtcNetEQOutputType *outputType = NULL);
    uint32_t getSpeechTimeStamp();
    WebRtcNetEQOutputType getOutputType();

    void * instance() { return (_inst); };
    void usePreparseRTP( bool useIt = true ) { _preparseRTP = useIt; };
    bool usingPreparseRTP() { return (_preparseRTP); };
    void setMaster( bool isMaster = true ) { _isMaster = isMaster; };
    void setSlave() { _isMaster = false; };
    void setNoDecode(bool noDecode = true) { _noDecode = noDecode; };
    bool isMaster() { return (_isMaster); };
    bool isSlave() { return (!_isMaster); };
    bool isNoDecode() { return _noDecode; };

#ifdef WINDOWS_TIMING
    double getRecInTime() { return (static_cast<double>( _totTimeRecIn.QuadPart )); };
    double getRecOutTime() { return (static_cast<double>( _totTimeRecOut.QuadPart )); };
#else
    double getRecInTime() { return (0.0); };
    double getRecOutTime() { return (0.0); };

#endif

    void printError();
    void printError(NETEQTEST_RTPpacket & rtp);

private:
    void *          _inst;
    int8_t *    _instMem;
    int8_t *    _bufferMem;
    bool            _preparseRTP;
    int             _fsmult;
    bool            _isMaster;
    bool            _noDecode;
#ifdef WINDOWS_TIMING
    LARGE_INTEGER   _totTimeRecIn;
    LARGE_INTEGER   _totTimeRecOut;
#endif
};

#endif //NETEQTEST_NETEQCLASS_H
