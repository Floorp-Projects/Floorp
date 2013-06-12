/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETEQTEST_RTPPACKET_H
#define NETEQTEST_RTPPACKET_H

#include <map>
#include <stdio.h>
#include "typedefs.h"
#include "webrtc_neteq_internal.h"

enum stereoModes {
    stereoModeMono,
    stereoModeSample1,
    stereoModeSample2,
    stereoModeFrame,
    stereoModeDuplicate
};

class NETEQTEST_RTPpacket
{
public:
    NETEQTEST_RTPpacket();
    bool operator !() const { return (dataLen() < 0); };
    virtual ~NETEQTEST_RTPpacket();
    void reset();
    static int skipFileHeader(FILE *fp);
    virtual int readFromFile(FILE *fp);
    int readFixedFromFile(FILE *fp, size_t len);
    virtual int writeToFile(FILE *fp);
    void blockPT(WebRtc_UWord8 pt);
    void selectSSRC(uint32_t ssrc);
    //WebRtc_Word16 payloadType();
    void parseHeader();
    void parseHeader(WebRtcNetEQ_RTPInfo & rtpInfo);
    WebRtcNetEQ_RTPInfo const * RTPinfo() const;
    WebRtc_UWord8 * datagram() const;
    WebRtc_UWord8 * payload() const;
    WebRtc_Word16 payloadLen();
    WebRtc_Word16 dataLen() const;
    bool isParsed() const;
    bool isLost() const;
    WebRtc_UWord32 time() const { return _receiveTime; };

    WebRtc_UWord8  payloadType() const;
    WebRtc_UWord16 sequenceNumber() const;
    WebRtc_UWord32 timeStamp() const;
    WebRtc_UWord32 SSRC() const;
    WebRtc_UWord8  markerBit() const;

    int setPayloadType(WebRtc_UWord8 pt);
    int setSequenceNumber(WebRtc_UWord16 sn);
    int setTimeStamp(WebRtc_UWord32 ts);
    int setSSRC(WebRtc_UWord32 ssrc);
    int setMarkerBit(WebRtc_UWord8 mb);
    void setTime(WebRtc_UWord32 receiveTime) { _receiveTime = receiveTime; };

    int setRTPheader(const WebRtcNetEQ_RTPInfo *RTPinfo);

    int splitStereo(NETEQTEST_RTPpacket* slaveRtp, enum stereoModes mode);

    int extractRED(int index, WebRtcNetEQ_RTPInfo& red);

    void scramblePayload(void);

    WebRtc_UWord8 *       _datagram;
    WebRtc_UWord8 *       _payloadPtr;
    int                 _memSize;
    WebRtc_Word16         _datagramLen;
    WebRtc_Word16         _payloadLen;
    WebRtcNetEQ_RTPInfo  _rtpInfo;
    bool                _rtpParsed;
    WebRtc_UWord32        _receiveTime;
    bool                _lost;
    std::map<WebRtc_UWord8, bool> _blockList;
    uint32_t            _selectSSRC;
    bool                _filterSSRC;

protected:
    static const int _kRDHeaderLen;
    static const int _kBasicHeaderLen;

    void parseBasicHeader(WebRtcNetEQ_RTPInfo *RTPinfo, int *i_P, int *i_X,
                          int *i_CC) const;
    int calcHeaderLength(int i_X, int i_CC) const;

private:
    void makeRTPheader(unsigned char* rtp_data, WebRtc_UWord8 payloadType,
                       WebRtc_UWord16 seqNo, WebRtc_UWord32 timestamp,
                       WebRtc_UWord32 ssrc, WebRtc_UWord8 markerBit) const;
    WebRtc_UWord16 parseRTPheader(WebRtcNetEQ_RTPInfo *RTPinfo,
                                  WebRtc_UWord8 **payloadPtr = NULL) const;
    WebRtc_UWord16 parseRTPheader(WebRtc_UWord8 **payloadPtr = NULL)
        { return parseRTPheader(&_rtpInfo, payloadPtr);};
    int calcPadLength(int i_P) const;
    void splitStereoSample(NETEQTEST_RTPpacket* slaveRtp, int stride);
    void splitStereoFrame(NETEQTEST_RTPpacket* slaveRtp);
    void splitStereoDouble(NETEQTEST_RTPpacket* slaveRtp);
};

#endif //NETEQTEST_RTPPACKET_H
