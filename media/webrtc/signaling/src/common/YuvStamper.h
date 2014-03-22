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
  YuvStamper(unsigned char* pYData,
	     uint32_t width, uint32_t height, uint32_t stride,
	     uint32_t x = 0, uint32_t y = 0);
  static bool Encode(uint32_t width, uint32_t height, uint32_t stride,
		     unsigned char* pYData, unsigned char* pMsg, size_t msg_len,
		     uint32_t x, uint32_t y);

  static bool Decode(uint32_t width, uint32_t height, uint32_t stride,
		     unsigned char* pYData, unsigned char* pMsg, size_t msg_len,
		     uint32_t x, uint32_t y);

 private:
  uint32_t Capacity();
  bool AdvanceCursor();
  bool WriteBit(bool one);
  bool Write8(unsigned char value);
  bool ReadBit(unsigned char &value);
  bool Read8(unsigned char &bit);

  const static uint32_t sBitSize = 4;
  const static uint32_t sBitThreshold = 60;
  const static unsigned char sYOn = 0x80;
  const static unsigned char sYOff = 0;

  unsigned char* pYData;
  uint32_t mStride;
  uint32_t mWidth;
  uint32_t mHeight;

  struct Cursor {
    Cursor(uint32_t x, uint32_t y):
      x(x), y(y) {}
    uint32_t x;
    uint32_t y;
  } mCursor;
};

}

#endif

