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
    void blockPT(uint8_t pt);
    void selectSSRC(uint32_t ssrc);
    //int16_t payloadType();
    void parseHeader();
    void parseHeader(WebRtcNetEQ_RTPInfo & rtpInfo);
    WebRtcNetEQ_RTPInfo const * RTPinfo() const;
    uint8_t * datagram() const;
    uint8_t * payload() const;
    int16_t payloadLen();
    int16_t dataLen() const;
    bool isParsed() const;
    bool isLost() const;
    uint32_t time() const { return _receiveTime; };

    uint8_t  payloadType() const;
    uint16_t sequenceNumber() const;
    uint32_t timeStamp() const;
    uint32_t SSRC() const;
    uint8_t  markerBit() const;

    int setPayloadType(uint8_t pt);
    int setSequenceNumber(uint16_t sn);
    int setTimeStamp(uint32_t ts);
    int setSSRC(uint32_t ssrc);
    int setMarkerBit(uint8_t mb);
    void setTime(uint32_t receiveTime) { _receiveTime = receiveTime; };

    int setRTPheader(const WebRtcNetEQ_RTPInfo *RTPinfo);

    int splitStereo(NETEQTEST_RTPpacket* slaveRtp, enum stereoModes mode);

    int extractRED(int index, WebRtcNetEQ_RTPInfo& red);

    void scramblePayload(void);

    uint8_t *       _datagram;
    uint8_t *       _payloadPtr;
    int                 _memSize;
    int16_t         _datagramLen;
    int16_t         _payloadLen;
    WebRtcNetEQ_RTPInfo  _rtpInfo;
    bool                _rtpParsed;
    uint32_t        _receiveTime;
    bool                _lost;
    std::map<uint8_t, bool> _blockList;
    uint32_t            _selectSSRC;
    bool                _filterSSRC;

protected:
    static const int _kRDHeaderLen;
    static const int _kBasicHeaderLen;

    void parseBasicHeader(WebRtcNetEQ_RTPInfo *RTPinfo, int *i_P, int *i_X,
                          int *i_CC) const;
    int calcHeaderLength(int i_X, int i_CC) const;

private:
    void makeRTPheader(unsigned char* rtp_data, uint8_t payloadType,
                       uint16_t seqNo, uint32_t timestamp,
                       uint32_t ssrc, uint8_t markerBit) const;
    uint16_t parseRTPheader(WebRtcNetEQ_RTPInfo *RTPinfo,
                            uint8_t **payloadPtr = NULL) const;
    uint16_t parseRTPheader(uint8_t **payloadPtr = NULL)
        { return parseRTPheader(&_rtpInfo, payloadPtr);};
    int calcPadLength(int i_P) const;
    void splitStereoSample(NETEQTEST_RTPpacket* slaveRtp, int stride);
    void splitStereoFrame(NETEQTEST_RTPpacket* slaveRtp);
    void splitStereoDouble(NETEQTEST_RTPpacket* slaveRtp);
};

#endif //NETEQTEST_RTPPACKET_H
