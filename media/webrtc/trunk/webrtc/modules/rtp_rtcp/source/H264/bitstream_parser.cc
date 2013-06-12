/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "bitstream_parser.h"

namespace webrtc {
BitstreamParser::BitstreamParser(const WebRtc_UWord8* data, const WebRtc_UWord32 dataLength) :
    _data(data),
    _dataLength(dataLength),
    _byteOffset(0),
    _bitOffset(0)
{
}
    // todo should we have any error codes from this?

WebRtc_UWord8
BitstreamParser::Get1Bit()
{
    WebRtc_UWord8 retVal = 0x1 & (_data[_byteOffset] >> (7-_bitOffset++));

    // prepare next byte
    if(_bitOffset == 8)
    {
        _bitOffset = 0;
        _byteOffset++;
    }
    return retVal;
}

WebRtc_UWord8
BitstreamParser::Get2Bits()
{
    WebRtc_UWord8 retVal = (Get1Bit() << 1);
    retVal += Get1Bit();
    return retVal;
}

WebRtc_UWord8
BitstreamParser::Get3Bits()
{
    WebRtc_UWord8 retVal = (Get1Bit() << 2);
    retVal += (Get1Bit() << 1);
    retVal += Get1Bit();
    return retVal;
}

WebRtc_UWord8
BitstreamParser::Get4Bits()
{
    WebRtc_UWord8 retVal = (Get1Bit() << 3);
    retVal += (Get1Bit() << 2);
    retVal += (Get1Bit() << 1);
    retVal += Get1Bit();
    return retVal;
}

WebRtc_UWord8
BitstreamParser::Get5Bits()
{
    WebRtc_UWord8 retVal = (Get1Bit() << 4);
    retVal += (Get1Bit() << 3);
    retVal += (Get1Bit() << 2);
    retVal += (Get1Bit() << 1);
    retVal += Get1Bit();
    return retVal;
}

WebRtc_UWord8
BitstreamParser::Get6Bits()
{
    WebRtc_UWord8 retVal = (Get1Bit() << 5);
    retVal += (Get1Bit() << 4);
    retVal += (Get1Bit() << 3);
    retVal += (Get1Bit() << 2);
    retVal += (Get1Bit() << 1);
    retVal += Get1Bit();
    return retVal;
}

WebRtc_UWord8
BitstreamParser::Get7Bits()
{
    WebRtc_UWord8 retVal = (Get1Bit() << 6);
    retVal += (Get1Bit() << 5);
    retVal += (Get1Bit() << 4);
    retVal += (Get1Bit() << 3);
    retVal += (Get1Bit() << 2);
    retVal += (Get1Bit() << 1);
    retVal += Get1Bit();
    return retVal;
}

WebRtc_UWord8
BitstreamParser::Get8Bits()
{
    WebRtc_UWord16 retVal;

    if(_bitOffset != 0)
    {
        // read 16 bits
        retVal = (_data[_byteOffset] << 8)+ (_data[_byteOffset+1]) ;
        retVal = retVal >> (8-_bitOffset);
    } else
    {
        retVal = _data[_byteOffset];
    }
    _byteOffset++;
    return (WebRtc_UWord8)retVal;
}

WebRtc_UWord16
BitstreamParser::Get16Bits()
{
    WebRtc_UWord32 retVal;

    if(_bitOffset != 0)
    {
        // read 24 bits
        retVal = (_data[_byteOffset] << 16) + (_data[_byteOffset+1] << 8) + (_data[_byteOffset+2]);
        retVal = retVal >> (8-_bitOffset);
    }else
    {
        // read 16 bits
        retVal = (_data[_byteOffset] << 8) + (_data[_byteOffset+1]) ;
    }
    _byteOffset += 2;
    return (WebRtc_UWord16)retVal;
}

WebRtc_UWord32
BitstreamParser::Get24Bits()
{
    WebRtc_UWord32 retVal;

    if(_bitOffset != 0)
    {
        // read 32 bits
        retVal = (_data[_byteOffset] << 24) + (_data[_byteOffset+1] << 16) + (_data[_byteOffset+2] << 8) + (_data[_byteOffset+3]);
        retVal = retVal >> (8-_bitOffset);
    }else
    {
        // read 24 bits
        retVal = (_data[_byteOffset] << 16) + (_data[_byteOffset+1] << 8) + (_data[_byteOffset+2]) ;
    }
    _byteOffset += 3;
    return retVal & 0x00ffffff; // we need to clean up the high 8 bits
}

WebRtc_UWord32
BitstreamParser::Get32Bits()
{
    WebRtc_UWord32 retVal;

    if(_bitOffset != 0)
    {
        // read 40 bits
        WebRtc_UWord64 tempVal = _data[_byteOffset];
        tempVal <<= 8;
        tempVal += _data[_byteOffset+1];
        tempVal <<= 8;
        tempVal += _data[_byteOffset+2];
        tempVal <<= 8;
        tempVal += _data[_byteOffset+3];
        tempVal <<= 8;
        tempVal += _data[_byteOffset+4];
        tempVal >>= (8-_bitOffset);

        retVal = WebRtc_UWord32(tempVal);
    }else
    {
        // read 32  bits
        retVal = (_data[_byteOffset]<< 24) + (_data[_byteOffset+1] << 16) + (_data[_byteOffset+2] << 8) + (_data[_byteOffset+3]) ;
    }
    _byteOffset += 4;
    return retVal;
}

// Exp-Golomb codes
/*
    with "prefix" and "suffix" bits and assignment to codeNum ranges (informative)
    Bit string form Range of codeNum
              1                0
            0 1 x0             1..2
          0 0 1 x1 x0          3..6
        0 0 0 1 x2 x1 x0       7..14
      0 0 0 0 1 x3 x2 x1 x0    15..30
    0 0 0 0 0 1 x4 x3 x2 x1 x0 31..62
*/

WebRtc_UWord32
BitstreamParser::GetUE()
{
    WebRtc_UWord32 retVal = 0;
    WebRtc_UWord8 numLeadingZeros = 0;

    while (Get1Bit() != 1)
    {
        numLeadingZeros++;
    }
    // prefix
    retVal = (1 << numLeadingZeros) - 1;

    // suffix
    while (numLeadingZeros)
    {
        retVal += (Get1Bit() << --numLeadingZeros);
    }
    return retVal;
}
} // namespace webrtc
