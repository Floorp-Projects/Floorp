/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef YUV_STAMPER_H_
#define YUV_STAMPER_H_

#include "nptypes.h"

namespace mozilla {

class
YuvStamper {
public:
  bool WriteDigits(uint32_t value);

  template<typename T>
  static bool Write(uint32_t width, uint32_t height, uint32_t stride,
                    unsigned char *pYData, const T& value,
                    uint32_t x=0, uint32_t y=0)
  {
    YuvStamper stamper(pYData, width, height, stride,
		       x, y,
		       (sDigitWidth + sInterDigit) * sPixelSize,
		       (sDigitHeight + sInterLine) * sPixelSize);
    return stamper.WriteDigits(value);
  }

  static bool Encode(uint32_t width, uint32_t height, uint32_t stride,
		     unsigned char* pYData, unsigned char* pMsg, size_t msg_len,
		     uint32_t x = 0, uint32_t y = 0);

  static bool Decode(uint32_t width, uint32_t height, uint32_t stride,
		     unsigned char* pYData, unsigned char* pMsg, size_t msg_len,
		     uint32_t x = 0, uint32_t y = 0);

 private:
  YuvStamper(unsigned char* pYData,
	     uint32_t width, uint32_t height, uint32_t stride,
	     uint32_t x, uint32_t y,
	     unsigned char symbol_width, unsigned char symbol_height);

  bool WriteDigit(unsigned char digit);
  void WritePixel(unsigned char* data, uint32_t x, uint32_t y);
  uint32_t Capacity();
  bool AdvanceCursor();
  bool WriteBit(bool one);
  bool Write8(unsigned char value);
  bool ReadBit(unsigned char &value);
  bool Read8(unsigned char &bit);

  const static unsigned char sPixelSize = 3;
  const static unsigned char sDigitWidth = 6;
  const static unsigned char sDigitHeight = 7;
  const static unsigned char sInterDigit = 1;
  const static unsigned char sInterLine = 1;
  const static uint32_t sBitSize = 4;
  const static uint32_t sBitThreshold = 60;
  const static unsigned char sYOn = 0x80;
  const static unsigned char sYOff = 0;
  const static unsigned char sLumaThreshold = 96;
  const static unsigned char sLumaMin = 16;
  const static unsigned char sLumaMax = 235;

  unsigned char* pYData;
  uint32_t mStride;
  uint32_t mWidth;
  uint32_t mHeight;
  unsigned char mSymbolWidth;
  unsigned char mSymbolHeight;

  struct Cursor {
    Cursor(uint32_t x, uint32_t y):
      x(x), y(y) {}
    uint32_t x;
    uint32_t y;
  } mCursor;
};

}

#endif

