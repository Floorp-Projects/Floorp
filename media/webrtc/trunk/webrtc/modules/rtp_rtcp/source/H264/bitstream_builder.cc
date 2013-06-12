/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "bitstream_builder.h"

#include <string.h>

namespace webrtc {
BitstreamBuilder::BitstreamBuilder(uint8_t* data, const uint32_t dataSize) :
    _data(data),
    _dataSize(dataSize),
    _byteOffset(0),
    _bitOffset(0)
{
    memset(data, 0, dataSize);
}

uint32_t
BitstreamBuilder::Length() const
{
    return _byteOffset+ (_bitOffset?1:0);
}

int32_t
BitstreamBuilder::Add1Bit(const uint8_t bit)
{
    // sanity
    if(_bitOffset + 1 > 8)
    {
        if(_dataSize < Length()+1)
        {
            // not enough space in buffer
            return -1;
        }
    }
    Add1BitWithoutSanity(bit);
    return 0;
}

void
BitstreamBuilder::Add1BitWithoutSanity(const uint8_t bit)
{
    if(bit & 0x1)
    {
        _data[_byteOffset] += (1 << (7-_bitOffset));
    }

    if(_bitOffset == 7)
    {
        // last bit in byte
        _bitOffset = 0;
        _byteOffset++;
    } else
    {
        _bitOffset++;
    }
}

int32_t
BitstreamBuilder::Add2Bits(const uint8_t bits)
{
    // sanity
    if(_bitOffset + 2 > 8)
    {
        if(_dataSize < Length()+1)
        {
            // not enough space in buffer
            return -1;
        }
    }
    Add1BitWithoutSanity(bits >> 1);
    Add1BitWithoutSanity(bits);
    return 0;
}

int32_t
BitstreamBuilder::Add3Bits(const uint8_t bits)
{
    // sanity
    if(_bitOffset + 3 > 8)
    {
        if(_dataSize < Length()+1)
        {
            // not enough space in buffer
            return -1;
        }
    }
    Add1BitWithoutSanity(bits >> 2);
    Add1BitWithoutSanity(bits >> 1);
    Add1BitWithoutSanity(bits);
    return 0;
}

int32_t
BitstreamBuilder::Add4Bits(const uint8_t bits)
{
    // sanity
    if(_bitOffset + 4 > 8)
    {
        if(_dataSize < Length()+1)
        {
            // not enough space in buffer
            return -1;
        }
    }
    Add1BitWithoutSanity(bits >> 3);
    Add1BitWithoutSanity(bits >> 2);
    Add1BitWithoutSanity(bits >> 1);
    Add1BitWithoutSanity(bits);
    return 0;
}

int32_t
BitstreamBuilder::Add5Bits(const uint8_t bits)
{
    // sanity
    if(_bitOffset + 5 > 8)
    {
        if(_dataSize < Length()+1)
        {
            // not enough space in buffer
            return -1;
        }
    }
    Add1BitWithoutSanity(bits >> 4);
    Add1BitWithoutSanity(bits >> 3);
    Add1BitWithoutSanity(bits >> 2);
    Add1BitWithoutSanity(bits >> 1);
    Add1BitWithoutSanity(bits);
    return 0;
}

int32_t
BitstreamBuilder::Add6Bits(const uint8_t bits)
{
    // sanity
    if(_bitOffset + 6 > 8)
    {
        if(_dataSize < Length()+1)
        {
            // not enough space in buffer
            return -1;
        }
    }
    Add1BitWithoutSanity(bits >> 5);
    Add1BitWithoutSanity(bits >> 4);
    Add1BitWithoutSanity(bits >> 3);
    Add1BitWithoutSanity(bits >> 2);
    Add1BitWithoutSanity(bits >> 1);
    Add1BitWithoutSanity(bits);
    return 0;
}

int32_t
BitstreamBuilder::Add7Bits(const uint8_t bits)
{
    // sanity
    if(_bitOffset + 7 > 8)
    {
        if(_dataSize < Length()+1)
        {
            // not enough space in buffer
            return -1;
        }
    }
    Add1BitWithoutSanity(bits >> 6);
    Add1BitWithoutSanity(bits >> 5);
    Add1BitWithoutSanity(bits >> 4);
    Add1BitWithoutSanity(bits >> 3);
    Add1BitWithoutSanity(bits >> 2);
    Add1BitWithoutSanity(bits >> 1);
    Add1BitWithoutSanity(bits);
    return 0;
}

int32_t
BitstreamBuilder::Add8Bits(const uint8_t bits)
{
    // sanity
    if(_dataSize < Length()+1)
    {
        // not enough space in buffer
        return -1;
    }
    if(_bitOffset == 0)
    {
        _data[_byteOffset] = bits;
    } else
    {
        _data[_byteOffset] += (bits >> _bitOffset);
        _data[_byteOffset+1] += (bits << (8-_bitOffset));
    }
    _byteOffset++;
    return 0;
}

int32_t
BitstreamBuilder::Add16Bits(const uint16_t bits)
{
    // sanity
    if(_dataSize < Length()+2)
    {
        // not enough space in buffer
        return -1;
    }
    if(_bitOffset == 0)
    {
        _data[_byteOffset] = (uint8_t)(bits >> 8);
        _data[_byteOffset+1] = (uint8_t)(bits);
    } else
    {
        _data[_byteOffset] += (uint8_t)(bits >> (_bitOffset + 8));
        _data[_byteOffset+1] += (uint8_t)(bits >> _bitOffset);
        _data[_byteOffset+2] += (uint8_t)(bits << (8-_bitOffset));
    }
    _byteOffset += 2;
    return 0;
}

int32_t
BitstreamBuilder::Add24Bits(const uint32_t bits)
{
    // sanity
    if(_dataSize < Length()+3)
    {
        // not enough space in buffer
        return -1;
    }
    if(_bitOffset == 0)
    {
        _data[_byteOffset] = (uint8_t)(bits >> 16);
        _data[_byteOffset+1] = (uint8_t)(bits >> 8);
        _data[_byteOffset+2] = (uint8_t)(bits);
    } else
    {
        _data[_byteOffset]   += (uint8_t)(bits >> (_bitOffset+16));
        _data[_byteOffset+1] += (uint8_t)(bits >> (_bitOffset+8));
        _data[_byteOffset+2] += (uint8_t)(bits >> (_bitOffset));
        _data[_byteOffset+3] += (uint8_t)(bits << (8-_bitOffset));
    }
    _byteOffset += 3;
    return 0;
}

int32_t
BitstreamBuilder::Add32Bits(const uint32_t bits)
{
    // sanity
    if(_dataSize < Length()+4)
    {
        // not enough space in buffer
        return -1;
    }
    if(_bitOffset == 0)
    {
        _data[_byteOffset]   = (uint8_t)(bits >> 24);
        _data[_byteOffset+1] = (uint8_t)(bits >> 16);
        _data[_byteOffset+2] = (uint8_t)(bits >> 8);
        _data[_byteOffset+3] = (uint8_t)(bits);
    } else
    {
        _data[_byteOffset]   += (uint8_t)(bits >> (_bitOffset+24));
        _data[_byteOffset+1] += (uint8_t)(bits >> (_bitOffset+16));
        _data[_byteOffset+2] += (uint8_t)(bits >> (_bitOffset+8));
        _data[_byteOffset+3] += (uint8_t)(bits >> (_bitOffset));
        _data[_byteOffset+4] += (uint8_t)(bits << (8-_bitOffset));
    }
    _byteOffset += 4;
    return 0;
}

// Exp-Golomb codes
/*
    with "prefix" and "suffix" bits and assignment to codeNum ranges (informative)
    Bit string form Range of codeNum
              1                0
            0 1 x0             1..2      2bits-1
          0 0 1 x1 x0          3..6      3bits-1
        0 0 0 1 x2 x1 x0       7..14     4bits-1
      0 0 0 0 1 x3 x2 x1 x0    15..30
    0 0 0 0 0 1 x4 x3 x2 x1 x0 31..62
*/
int32_t
BitstreamBuilder::AddUE(const uint32_t value)
{
    // un-rolled on 8 bit base to avoid too deep if else chain
    if(value < 0x0000ffff)
    {
        if(value < 0x000000ff)
        {
            if(value == 0)
            {
                if(AddPrefix(0) != 0)
                {
                    return -1;
                }
            } else if(value < 3)
            {
                if(AddPrefix(1) != 0)
                {
                    return -1;
                }
                AddSuffix(1, value-1);
            } else if(value < 7)
            {
                if(AddPrefix(2) != 0)
                {
                    return -1;
                }
                AddSuffix(2, value-3);
            } else if(value < 15)
            {
                if(AddPrefix(3) != 0)
                {
                    return -1;
                }
                AddSuffix(3, value-7);
            } else if(value < 31)
            {
                if(AddPrefix(4) != 0)
                {
                    return -1;
                }
                AddSuffix(4, value-15);
            } else if(value < 63)
            {
                if(AddPrefix(5) != 0)
                {
                    return -1;
                }
                AddSuffix(5, value-31);
            } else if(value < 127)
            {
                if(AddPrefix(6) != 0)
                {
                    return -1;
                }
                AddSuffix(6, value-63);
            } else
            {
                if(AddPrefix(7) != 0)
                {
                    return -1;
                }
                AddSuffix(7, value-127);
            }
        }else
        {
            if(value < 0x000001ff)
            {
                if(AddPrefix(8) != 0)
                {
                    return -1;
                }
                AddSuffix(8, value-0x000000ff);
            } else if(value < 0x000003ff)
            {
                if(AddPrefix(9) != 0)
                {
                    return -1;
                }
                AddSuffix(9, value-0x000001ff);
            } else if(value < 0x000007ff)
            {
                if(AddPrefix(10) != 0)
                {
                    return -1;
                }
                AddSuffix(10, value-0x000003ff);
            } else if(value < 0x00000fff)
            {
                if(AddPrefix(11) != 0)
                {
                    return -1;
                }
                AddSuffix(1, value-0x000007ff);
            } else if(value < 0x00001fff)
            {
                if(AddPrefix(12) != 0)
                {
                    return -1;
                }
                AddSuffix(12, value-0x00000fff);
            } else if(value < 0x00003fff)
            {
                if(AddPrefix(13) != 0)
                {
                    return -1;
                }
                AddSuffix(13, value-0x00001fff);
            } else if(value < 0x00007fff)
            {
                if(AddPrefix(14) != 0)
                {
                    return -1;
                }
                AddSuffix(14, value-0x00003fff);
            } else
            {
                if(AddPrefix(15) != 0)
                {
                    return -1;
                }
                AddSuffix(15, value-0x00007fff);
            }
        }
    }else
    {
        if(value < 0x00ffffff)
        {
            if(value < 0x0001ffff)
            {
                if(AddPrefix(16) != 0)
                {
                    return -1;
                }
                AddSuffix(16, value-0x0000ffff);
            } else if(value < 0x0003ffff)
            {
                if(AddPrefix(17) != 0)
                {
                    return -1;
                }
                AddSuffix(17, value-0x0001ffff);
            } else if(value < 0x0007ffff)
            {
                if(AddPrefix(18) != 0)
                {
                    return -1;
                }
                AddSuffix(18, value-0x0003ffff);
            } else if(value < 0x000fffff)
            {
                if(AddPrefix(19) != 0)
                {
                    return -1;
                }
                AddSuffix(19, value-0x0007ffff);
            } else if(value < 0x001fffff)
            {
                if(AddPrefix(20) != 0)
                {
                    return -1;
                }
                AddSuffix(20, value-0x000fffff);
            } else if(value < 0x003fffff)
            {
                if(AddPrefix(21) != 0)
                {
                    return -1;
                }
                AddSuffix(21, value-0x001fffff);
            } else if(value < 0x007fffff)
            {
                if(AddPrefix(22) != 0)
                {
                    return -1;
                }
                AddSuffix(22, value-0x003fffff);
            } else
            {
                if(AddPrefix(23) != 0)
                {
                    return -1;
                }
                AddSuffix(23, value-0x007fffff);
            }
        } else
        {
            if(value < 0x01ffffff)
            {
                if(AddPrefix(24) != 0)
                {
                    return -1;
                }
                AddSuffix(24, value-0x00ffffff);
            } else if(value < 0x03ffffff)
            {
                if(AddPrefix(25) != 0)
                {
                    return -1;
                }
                AddSuffix(25, value-0x01ffffff);
            } else if(value < 0x07ffffff)
            {
                if(AddPrefix(26) != 0)
                {
                    return -1;
                }
                AddSuffix(26, value-0x03ffffff);
            } else if(value < 0x0fffffff)
            {
                if(AddPrefix(27) != 0)
                {
                    return -1;
                }
                AddSuffix(27, value-0x07ffffff);
            } else if(value < 0x1fffffff)
            {
                if(AddPrefix(28) != 0)
                {
                    return -1;
                }
                AddSuffix(28, value-0x0fffffff);
            } else if(value < 0x3fffffff)
            {
                if(AddPrefix(29) != 0)
                {
                    return -1;
                }
                AddSuffix(29, value-0x1fffffff);
            } else if(value < 0x7fffffff)
            {
                if(AddPrefix(30) != 0)
                {
                    return -1;
                }
                AddSuffix(30, value-0x3fffffff);
            } else if(value < 0xffffffff)
            {
                if(AddPrefix(31) != 0)
                {
                    return -1;
                }
                AddSuffix(31, value-0x7ffffff);
            } else
            {
                if(AddPrefix(32) != 0)
                {
                    return -1;
                }
                AddSuffix(32, 0); // exactly 0xffffffff
            }
        }
    }
    return 0;
}

int32_t
BitstreamBuilder::AddPrefix(const uint8_t numZeros)
{
    // sanity for the sufix too
    uint32_t numBitsToAdd = numZeros * 2 + 1;
    if(((_dataSize - _byteOffset) *8 + 8-_bitOffset) < numBitsToAdd)
    {
        return -1;
    }

    // add numZeros
    for (uint32_t i = 0; i < numZeros; i++)
    {
        Add1Bit(0);
    }
    Add1Bit(1);
    return 0;
}

void
BitstreamBuilder::AddSuffix(const uint8_t numBits, const uint32_t rest)
{
    // most significant bit first
    for(int32_t i = numBits - 1; i >= 0; i--)
    {
        if(( rest >> i) & 0x1)
        {
            Add1Bit(1);
        }else
        {
            Add1Bit(0);
        }
    }
}
} // namespace webrtc
