/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_PARSER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_PARSER_H_

#include "webrtc/typedefs.h"

namespace webrtc {
class BitstreamParser
{
public:
    BitstreamParser(const uint8_t* data, const uint32_t dataLength);

    uint8_t Get1Bit();
    uint8_t Get2Bits();
    uint8_t Get3Bits();
    uint8_t Get4Bits();
    uint8_t Get5Bits();
    uint8_t Get6Bits();
    uint8_t Get7Bits();
    uint8_t Get8Bits();
    uint16_t Get16Bits();
    uint32_t Get24Bits();
    uint32_t Get32Bits();

    // Exp-Golomb codes
    uint32_t GetUE();

private:
    const uint8_t* _data;
    const uint32_t _dataLength;

    uint32_t _byteOffset;
    uint8_t  _bitOffset;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_PARSER_H_
