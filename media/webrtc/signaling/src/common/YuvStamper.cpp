/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#elif defined XP_WIN
#include <winsock2.h>
#endif
#include <string.h>

#include "nspr.h"
#include "YuvStamper.h"
#include "mozilla/Sprintf.h"

typedef uint32_t UINT4; //Needed for r_crc32() call
extern "C" {
#include "r_crc32.h"
}

namespace mozilla {

#define ON_5 0x20
#define ON_4 0x10
#define ON_3 0x08
#define ON_2 0x04
#define ON_1 0x02
#define ON_0 0x01

/*
  0, 0, 1, 1, 0, 0,
  0, 1, 0, 0, 1, 0,
  1, 0, 0, 0, 0, 1,
  1, 0, 0, 0, 0, 1,
  1, 0, 0, 0, 0, 1,
  0, 1, 0, 0, 1, 0,
  0, 0, 1, 1, 0, 0
*/
static unsigned char DIGIT_0 [] =
  { ON_3 | ON_2,
    ON_4 | ON_1,
    ON_5 | ON_0,
    ON_5 | ON_0,
    ON_5 | ON_0,
    ON_4 | ON_1,
    ON_3 | ON_2
  };
    
/*
  0, 0, 0, 1, 0, 0,
  0, 0, 0, 1, 0, 0,
  0, 0, 0, 1, 0, 0,
  0, 0, 0, 1, 0, 0,
  0, 0, 0, 1, 0, 0,
  0, 0, 0, 1, 0, 0,
  0, 0, 0, 1, 0, 0,
*/
static unsigned char DIGIT_1 [] =
  { ON_2,
    ON_2,
    ON_2,
    ON_2,
    ON_2,
    ON_2,
    ON_2
  };

/*
  1, 1, 1, 1, 1, 0,
  0, 0, 0, 0, 0, 1,
  0, 0, 0, 0, 0, 1,
  0, 1, 1, 1, 1, 0,
  1, 0, 0, 0, 0, 0,
  1, 0, 0, 0, 0, 0,
  0, 1, 1, 1, 1, 1,
*/
static unsigned char DIGIT_2 [] =
  { ON_5 | ON_4 | ON_3 | ON_2 | ON_1,
    ON_0,
    ON_0,
    ON_4 | ON_3 | ON_2 | ON_1,
    ON_5,
    ON_5,
    ON_4 | ON_3 | ON_2 | ON_1 | ON_0,
  };

/*
  1, 1, 1, 1, 1, 0,
  0, 0, 0, 0, 0, 1,
  0, 0, 0, 0, 0, 1,
  0, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 1,
  0, 0, 0, 0, 0, 1,
  1, 1, 1, 1, 1, 0,
*/
static unsigned char DIGIT_3 [] =
  { ON_5 | ON_4 | ON_3 | ON_2 | ON_1,
    ON_0,
    ON_0,
    ON_4 | ON_3 | ON_2 | ON_1 | ON_0,
    ON_0,
    ON_0,
    ON_5 | ON_4 | ON_3 | ON_2 | ON_1,
  };

/*
  0, 1, 0, 0, 0, 1,
  0, 1, 0, 0, 0, 1,
  0, 1, 0, 0, 0, 1,
  0, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 1,
  0, 0, 0, 0, 0, 1,
  0, 0, 0, 0, 0, 1
*/
static unsigned char DIGIT_4 [] =
  { ON_4 | ON_0,
    ON_4 | ON_0,
    ON_4 | ON_0,
    ON_4 | ON_3 | ON_2 | ON_1 | ON_0,
    ON_0,
    ON_0,
    ON_0,
  };

/*
  0, 1, 1, 1, 1, 1,
  1, 0, 0, 0, 0, 0,
  1, 0, 0, 0, 0, 0,
  0, 1, 1, 1, 1, 0,
  0, 0, 0, 0, 0, 1,
  0, 0, 0, 0, 0, 1,
  1, 1, 1, 1, 1, 0,
*/
static unsigned char DIGIT_5 [] =
  { ON_4 | ON_3 | ON_2 | ON_1 | ON_0,
    ON_5,
    ON_5,
    ON_4 | ON_3 | ON_2 | ON_1,
    ON_0,
    ON_0,
    ON_5 | ON_4 | ON_3 | ON_2 | ON_1,
  };

/*
  0, 1, 1, 1, 1, 1,
  1, 0, 0, 0, 0, 0,
  1, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 0,
  1, 0, 0, 0, 0, 1,
  1, 0, 0, 0, 0, 1,
  0, 1, 1, 1, 1, 0,
*/
static unsigned char DIGIT_6 [] =
  { ON_4 | ON_3 | ON_2 | ON_1 | ON_0,
    ON_5,
    ON_5,
    ON_4 | ON_3 | ON_2 | ON_1,
    ON_5 | ON_0,
    ON_5 | ON_0,
    ON_4 | ON_3 | ON_2 | ON_1,
  };

/*
  1, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 1,
  0, 0, 0, 0, 1, 0,
  0, 0, 0, 1, 0, 0,
  0, 0, 1, 0, 0, 0,
  0, 1, 0, 0, 0, 0,
  1, 0, 0, 0, 0, 0
*/
static unsigned char DIGIT_7 [] =
  { ON_5 | ON_4 | ON_3 | ON_2 | ON_1 | ON_0,
    ON_0,
    ON_1,
    ON_2,
    ON_3,
    ON_4,
    ON_5
  };

/*
  0, 1, 1, 1, 1, 1,
  1, 0, 0, 0, 0, 1,
  1, 0, 0, 0, 0, 1,
  0, 1, 1, 1, 1, 0,
  1, 0, 0, 0, 0, 1,
  1, 0, 0, 0, 0, 1,
  0, 1, 1, 1, 1, 0
*/
static unsigned char DIGIT_8 [] =
  { ON_4 | ON_3 | ON_2 | ON_1,
    ON_5 | ON_0,
    ON_5 | ON_0,
    ON_4 | ON_3 | ON_2 | ON_1,
    ON_5 | ON_0,
    ON_5 | ON_0,
    ON_4 | ON_3 | ON_2 | ON_1,
  };

/*
  0, 1, 1, 1, 1, 1,
  1, 0, 0, 0, 0, 1,
  1, 0, 0, 0, 0, 1,
  0, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 1,
  0, 0, 0, 0, 0, 1,
  0, 1, 1, 1, 1, 0
*/
static unsigned char DIGIT_9 [] =
  { ON_4 | ON_3 | ON_2 | ON_1 | ON_0,
    ON_5 | ON_0,
    ON_5 | ON_0,
    ON_4 | ON_3 | ON_2 | ON_1 | ON_0,
    ON_0,
    ON_0,
    ON_4 | ON_3 | ON_2 | ON_1,
  };

static unsigned char *DIGITS[] = {
    DIGIT_0,
    DIGIT_1,
    DIGIT_2,
    DIGIT_3,
    DIGIT_4,
    DIGIT_5,
    DIGIT_6,
    DIGIT_7,
    DIGIT_8,
    DIGIT_9
};

  YuvStamper::YuvStamper(unsigned char* pYData,
			 uint32_t width,
			 uint32_t height,
			 uint32_t stride,
			 uint32_t x,
			 uint32_t y,
			 unsigned char symbol_width,
			 unsigned char symbol_height):
    pYData(pYData), mStride(stride),
    mWidth(width), mHeight(height),
    mSymbolWidth(symbol_width), mSymbolHeight(symbol_height),
    mCursor(x, y) {}

  bool YuvStamper::Encode(uint32_t width, uint32_t height, uint32_t stride,
			  unsigned char* pYData, unsigned char* pMsg, size_t msg_len,
			  uint32_t x, uint32_t y)
  {
    YuvStamper stamper(pYData, width, height, stride,
		       x, y, sBitSize, sBitSize);

    // Reserve space for a checksum.
    if (stamper.Capacity() < 8 * (msg_len + sizeof(uint32_t)))
    {
      return false;
    }

    bool ok = false;
    uint32_t crc;
    unsigned char* pCrc = reinterpret_cast<unsigned char*>(&crc);
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
    YuvStamper stamper(pYData, width, height, stride,
		       x, y, sBitSize, sBitSize);

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
    if (mCursor.y + mSymbolHeight > mHeight) {
      return 0;
    }

    if (mCursor.x + mSymbolWidth > mWidth && !AdvanceCursor()) {
      return 0;
    }

    // Normalize frame integral to mSymbolWidth x mSymbolHeight
    uint32_t width = mWidth / mSymbolWidth;
    uint32_t height = mHeight / mSymbolHeight;
    uint32_t x = mCursor.x / mSymbolWidth;
    uint32_t y = mCursor.y / mSymbolHeight;

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
    // A bit is mapped to a mSymbolWidth x mSymbolHeight square of luma data points.
    // Don't use ternary op.: https://bugzilla.mozilla.org/show_bug.cgi?id=1001708
    unsigned char value;
    if (one)
      value = sYOn;
    else
      value = sYOff;

    for (uint32_t y = 0; y < mSymbolHeight; y++) {
      for (uint32_t x = 0; x < mSymbolWidth; x++) {
	*(pYData + (mCursor.x + x) + ((mCursor.y + y) * mStride)) = value;
      }
    }

    return AdvanceCursor();
  }

  bool YuvStamper::AdvanceCursor()
  {
    mCursor.x += mSymbolWidth;
    if (mCursor.x + mSymbolWidth > mWidth) {
      // move to the start of the next row if possible.
      mCursor.y += mSymbolHeight;
      if (mCursor.y + mSymbolHeight > mHeight) {
	// end of frame, do not advance
	mCursor.y -= mSymbolHeight;
	mCursor.x -= mSymbolWidth;
	return false;
      } else {
	mCursor.x = 0;
      }
    }

    return true;
  }

  bool YuvStamper::Read8(unsigned char &value)
  {
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
    for (uint32_t y = 0; y < mSymbolHeight; y++) {
      for (uint32_t x = 0; x < mSymbolWidth; x++) {
	sum += *(pYData + mStride * (mCursor.y + y) + mCursor.x + x);
      }
    }

    // apply threshold to collected bit square
    bit = (sum > (sBitThreshold * mSymbolWidth * mSymbolHeight)) ? 1 : 0;
    return AdvanceCursor();
  }

  bool YuvStamper::WriteDigits(uint32_t value)
  {
    char buf[20];
    SprintfLiteral(buf, "%.5u", value);
    size_t size = strlen(buf);

    if (Capacity() < size) {
      return false;
    }

    for (size_t i=0; i < size; ++i) {
      if (!WriteDigit(buf[i] - '0'))
	return false;
      if (!AdvanceCursor()) {
	return false;
      }
    }

    return true;
  }

  bool YuvStamper::WriteDigit(unsigned char digit) {
    if (digit > sizeof(DIGITS)/sizeof(DIGITS[0]))
      return false;

    unsigned char *dig = DIGITS[digit];
    for (uint32_t row = 0; row < sDigitHeight; ++row) {
      unsigned char mask = 0x01 << (sDigitWidth - 1);
      for (uint32_t col = 0; col < sDigitWidth; ++col, mask >>= 1) {
	if (dig[row] & mask) {
	  for (uint32_t xx=0; xx < sPixelSize; ++xx) {
	    for (uint32_t yy=0; yy < sPixelSize; ++yy) {
	      WritePixel(pYData,
			 mCursor.x + (col * sPixelSize) + xx,
			 mCursor.y + (row * sPixelSize) + yy);
	    }
	  }
	}
      }
    }

    return true;
  }

  void YuvStamper::WritePixel(unsigned char *data, uint32_t x, uint32_t y) {
    unsigned char *ptr = &data[y * mStride + x];
    // Don't use ternary op.: https://bugzilla.mozilla.org/show_bug.cgi?id=1001708
    if (*ptr > sLumaThreshold)
      *ptr = sLumaMin;
    else
      *ptr = sLumaMax;
   }

} // namespace mozilla.
