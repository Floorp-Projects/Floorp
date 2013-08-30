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

#include "webrtc/typedefs.h"

namespace webrtc {
class BitstreamBuilder
{
public:
    BitstreamBuilder(uint8_t* data, const uint32_t dataSize);

    uint32_t Length() const;

    int32_t Add1Bit(const uint8_t bit);
    int32_t Add2Bits(const uint8_t bits);
    int32_t Add3Bits(const uint8_t bits);
    int32_t Add4Bits(const uint8_t bits);
    int32_t Add5Bits(const uint8_t bits);
    int32_t Add6Bits(const uint8_t bits);
    int32_t Add7Bits(const uint8_t bits);
    int32_t Add8Bits(const uint8_t bits);
    int32_t Add16Bits(const uint16_t bits);
    int32_t Add24Bits(const uint32_t bits);
    int32_t Add32Bits(const uint32_t bits);

    // Exp-Golomb codes
    int32_t AddUE(const uint32_t value);

private:
    int32_t AddPrefix(const uint8_t numZeros);
    void AddSuffix(const uint8_t numBits, const uint32_t rest);
    void Add1BitWithoutSanity(const uint8_t bit);

    uint8_t* _data;
    uint32_t _dataSize;

    uint32_t _byteOffset;
    uint8_t  _bitOffset;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_BUILDER_H_
