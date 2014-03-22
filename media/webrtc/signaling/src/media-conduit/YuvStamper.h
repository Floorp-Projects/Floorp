/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

namespace mozilla {

class
YuvStamper {
public:
  YuvStamper(uint8_t* pYData,
	     uint32_t width, uint32_t height, uint32_t stride,
	     uint32_t x = 0, uint32_t y = 0);
  static bool Encode(uint32_t width, uint32_t height, uint32_t stride,
		     uint8_t* pYData, uint8_t* pMsg, size_t msg_len,
		     uint32_t x, uint32_t y);

  static bool Decode(uint32_t width, uint32_t height, uint32_t stride,
		     uint8_t* pYData, uint8_t* pMsg, size_t msg_len,
		     uint32_t x, uint32_t y);

 private:
  uint32_t Capacity();
  bool AdvanceCursor();
  bool WriteBit(bool one);
  bool Write8(uint8_t value);
  bool ReadBit(uint8_t &value);
  bool Read8(uint8_t &bit);

  const static uint32_t sBitSize = 4;
  const static uint32_t sBitThreshold = 60;
  const static uint8_t sYOn = 0x80;
  const static uint8_t sYOff = 0;

  uint8_t* pYData;
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
