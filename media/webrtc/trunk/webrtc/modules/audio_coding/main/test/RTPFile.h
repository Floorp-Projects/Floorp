/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTPFILE_H
#define RTPFILE_H

#include "audio_coding_module.h"
#include "module_common_types.h"
#include "typedefs.h"
#include "rw_lock_wrapper.h"
#include <stdio.h>
#include <queue>

namespace webrtc {

class RTPStream
{
public:
    virtual ~RTPStream(){}

    virtual void Write(const WebRtc_UWord8 payloadType, const WebRtc_UWord32 timeStamp,
                                     const WebRtc_Word16 seqNo, const WebRtc_UWord8* payloadData,
                                     const WebRtc_UWord16 payloadSize, WebRtc_UWord32 frequency) = 0;

    // Returns the packet's payload size. Zero should be treated as an
    // end-of-stream (in the case that EndOfFile() is true) or an error.
    virtual WebRtc_UWord16 Read(WebRtcRTPHeader* rtpInfo,
                    WebRtc_UWord8* payloadData,
                    WebRtc_UWord16 payloadSize,
                    WebRtc_UWord32* offset) = 0;
    virtual bool EndOfFile() const = 0;

protected:
    void MakeRTPheader(WebRtc_UWord8* rtpHeader, 
                                      WebRtc_UWord8 payloadType, WebRtc_Word16 seqNo, 
                                      WebRtc_UWord32 timeStamp, WebRtc_UWord32 ssrc);
    void ParseRTPHeader(WebRtcRTPHeader* rtpInfo, const WebRtc_UWord8* rtpHeader);
};

class RTPPacket
{
public:
    RTPPacket(WebRtc_UWord8 payloadType, WebRtc_UWord32 timeStamp,
                                     WebRtc_Word16 seqNo, const WebRtc_UWord8* payloadData,
                                     WebRtc_UWord16 payloadSize, WebRtc_UWord32 frequency);
    ~RTPPacket();
    WebRtc_UWord8 payloadType;
    WebRtc_UWord32 timeStamp;
    WebRtc_Word16 seqNo;
    WebRtc_UWord8* payloadData;
    WebRtc_UWord16 payloadSize;
    WebRtc_UWord32 frequency;
};

class RTPBuffer : public RTPStream
{
public:
    RTPBuffer();
    ~RTPBuffer();
    void Write(const WebRtc_UWord8 payloadType, const WebRtc_UWord32 timeStamp,
                                     const WebRtc_Word16 seqNo, const WebRtc_UWord8* payloadData,
                                     const WebRtc_UWord16 payloadSize, WebRtc_UWord32 frequency);
    WebRtc_UWord16 Read(WebRtcRTPHeader* rtpInfo,
                    WebRtc_UWord8* payloadData,
                    WebRtc_UWord16 payloadSize,
                    WebRtc_UWord32* offset);
    virtual bool EndOfFile() const;
private:
    RWLockWrapper*             _queueRWLock;
    std::queue<RTPPacket *>   _rtpQueue;
};

class RTPFile : public RTPStream
{
public:
    ~RTPFile(){}
    RTPFile() : _rtpFile(NULL),_rtpEOF(false) {}
    void Open(const char *outFilename, const char *mode);
    void Close();
    void WriteHeader();
    void ReadHeader();
    void Write(const WebRtc_UWord8 payloadType, const WebRtc_UWord32 timeStamp,
                                     const WebRtc_Word16 seqNo, const WebRtc_UWord8* payloadData,
                                     const WebRtc_UWord16 payloadSize, WebRtc_UWord32 frequency);
    WebRtc_UWord16 Read(WebRtcRTPHeader* rtpInfo,
                    WebRtc_UWord8* payloadData,
                    WebRtc_UWord16 payloadSize,
                    WebRtc_UWord32* offset);
    bool EndOfFile() const { return _rtpEOF; }
private:
    FILE*   _rtpFile;
    bool    _rtpEOF;
};

} // namespace webrtc
#endif
