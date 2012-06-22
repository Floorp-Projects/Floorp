/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_BUILDER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_BUILDER_H_

#include "typedefs.h"

namespace webrtc {
class BitstreamBuilder
{
public:
    BitstreamBuilder(WebRtc_UWord8* data, const WebRtc_UWord32 dataSize);

    WebRtc_UWord32 Length() const;

    WebRtc_Word32 Add1Bit(const WebRtc_UWord8 bit);
    WebRtc_Word32 Add2Bits(const WebRtc_UWord8 bits);
    WebRtc_Word32 Add3Bits(const WebRtc_UWord8 bits);
    WebRtc_Word32 Add4Bits(const WebRtc_UWord8 bits);
    WebRtc_Word32 Add5Bits(const WebRtc_UWord8 bits);
    WebRtc_Word32 Add6Bits(const WebRtc_UWord8 bits);
    WebRtc_Word32 Add7Bits(const WebRtc_UWord8 bits);
    WebRtc_Word32 Add8Bits(const WebRtc_UWord8 bits);
    WebRtc_Word32 Add16Bits(const WebRtc_UWord16 bits);
    WebRtc_Word32 Add24Bits(const WebRtc_UWord32 bits);
    WebRtc_Word32 Add32Bits(const WebRtc_UWord32 bits);

    // Exp-Golomb codes
    WebRtc_Word32 AddUE(const WebRtc_UWord32 value);

private:
    WebRtc_Word32 AddPrefix(const WebRtc_UWord8 numZeros);
    void AddSuffix(const WebRtc_UWord8 numBits, const WebRtc_UWord32 rest);
    void Add1BitWithoutSanity(const WebRtc_UWord8 bit);

    WebRtc_UWord8* _data;
    WebRtc_UWord32 _dataSize;

    WebRtc_UWord32 _byteOffset;
    WebRtc_UWord8  _bitOffset;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_BUILDER_H_
