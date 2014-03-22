/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#elif defined XP_WIN
#include <winsock2.h>
#endif

#include "YuvStamper.h"

typedef uint32_t UINT4; //Needed for r_crc32() call
extern "C" {
#include "r_crc32.h"
}

namespace mozilla {

  YuvStamper::YuvStamper(unsigned char* pYData,
			 uint32_t width,
			 uint32_t height,
			 uint32_t stride,
			 uint32_t x,
			 uint32_t y):
    pYData(pYData), mStride(stride),
    mWidth(width), mHeight(height),
    mCursor(x, y) {}

  bool YuvStamper::Encode(uint32_t width, uint32_t height, uint32_t stride,
			  unsigned char* pYData, unsigned char* pMsg, size_t msg_len,
			  uint32_t x, uint32_t y)
  {
    YuvStamper stamper(pYData, width, height, stride, x, y);

    // Reserve space for a checksum.
    if (stamper.Capacity() < 8 * (msg_len + sizeof(uint32_t)))
    {
      return false;
    }

    bool ok = false;
    uint32_t crc;
    uint8_t* pCrc = reinterpret_cast<unsigned char*>(&crc);
    r_crc32(reinterpret_cast<char*>(pMsg), (int)msg_len, &crc);
    crc = htonl(crc);

    while (msg_len-- > 0) {
      if (!stamper.Write8(*pMsg++)) {
	return false;
      }
    }

    // Add checksum after the message.
    ok = stamper.Write8(*pCrc++) &&
         stamper.Write8(*pCrc++) &&
         stamper.Write8(*pCrc++) &&
         stamper.Write8(*pCrc++);

    return ok;
  }

  bool YuvStamper::Decode(uint32_t width, uint32_t height, uint32_t stride,
			  unsigned char* pYData, unsigned char* pMsg, size_t msg_len,
			  uint32_t x, uint32_t y)
  {
    YuvStamper stamper(pYData, width, height, stride, x, y);
    unsigned char* ptr = pMsg;
    size_t len = msg_len;
    uint32_t crc, msg_crc;
    unsigned char* pCrc = reinterpret_cast<unsigned char*>(&crc);

    // Account for space reserved for the checksum
    if (stamper.Capacity() < 8 * (len + sizeof(uint32_t))) {
      return false;
    }

    while (len-- > 0) {
      if(!stamper.Read8(*ptr++)) {
	return false;
      }
    }

    if (!(stamper.Read8(*pCrc++) &&
          stamper.Read8(*pCrc++) &&
          stamper.Read8(*pCrc++) &&
          stamper.Read8(*pCrc++))) {
      return false;
    }

    r_crc32(reinterpret_cast<char*>(pMsg), (int)msg_len, &msg_crc);
    return crc == htonl(msg_crc);
  }

  inline uint32_t YuvStamper::Capacity()
  {
    // Enforce at least a symbol width and height offset from outer edges.
    if (mCursor.y + sBitSize > mHeight) {
      return 0;
    }

    if (mCursor.x + sBitSize > mWidth && !AdvanceCursor()) {
      return 0;
    }

    // Normalize frame integral to sBitSize x sBitSize
    uint32_t width = mWidth / sBitSize;
    uint32_t height = mHeight / sBitSize;
    uint32_t x = mCursor.x / sBitSize;
    uint32_t y = mCursor.y / sBitSize;

    return (width * height - width * y)- x;
  }

  bool YuvStamper::Write8(unsigned char value)
  {
    // Encode MSB to LSB.
    unsigned char mask = 0x80;
    while (mask) {
      if (!WriteBit(!!(value & mask))) {
	return false;
      }
      mask >>= 1;
    }
    return true;
  }

  bool YuvStamper::WriteBit(bool one)
  {
    // A bit is mapped to a sBitSize x sBitSize square of luma data points.
    unsigned char value = one ? sYOn : sYOff;
    for (uint32_t y = 0; y < sBitSize; y++) {
      for (uint32_t x = 0; x < sBitSize; x++) {
	*(pYData + (mCursor.x + x) + ((mCursor.y + y) * mStride)) = value;
      }
    }

    return AdvanceCursor();
  }

  bool YuvStamper::AdvanceCursor()
  {
    mCursor.x += sBitSize;
    if (mCursor.x + sBitSize > mWidth) {
      // move to the start of the next row if possible.
      mCursor.y += sBitSize;
      if (mCursor.y + sBitSize > mHeight) {
	// end of frame, do not advance
	mCursor.y -= sBitSize;
	mCursor.x -= sBitSize;
	return false;
      } else {
	mCursor.x = 0;
      }
    }

    return true;
  }

  bool YuvStamper::Read8(unsigned char &value) {
    unsigned char octet = 0;
    unsigned char bit = 0;

    for (int i = 8; i > 0; --i) {
      if (!ReadBit(bit)) {
	return false;
      }
      octet <<= 1;
      octet |= bit;
    }

    value = octet;
    return true;
  }

  bool YuvStamper::ReadBit(unsigned char &bit)
  {
    uint32_t sum = 0;
    for (uint32_t y = 0; y < sBitSize; y++) {
      for (uint32_t x = 0; x < sBitSize; x++) {
	sum += *(pYData + mStride * (mCursor.y + y) + mCursor.x + x);
      }
    }

    // apply threshold to collected bit square
    bit = (sum > (sBitThreshold * sBitSize * sBitSize)) ? 1 : 0;
    return AdvanceCursor();
  }

}  // Namespace mozilla.
