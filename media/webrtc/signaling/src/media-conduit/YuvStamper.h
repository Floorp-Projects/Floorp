/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

namespace mozilla {

class
YuvStamper {
public:
  bool WriteDigits(uint32_t value);

  template<typename T>
  static bool Write(uint32_t width, uint32_t height, uint32_t stride,
                    uint8_t *pYData, const T& value,
                    uint32_t x=0, uint32_t y=0)
  {
    YuvStamper stamper(pYData, width, height, stride,
		       x, y,
		       (sDigitWidth + sInterDigit) * sPixelSize,
		       (sDigitHeight + sInterLine) * sPixelSize);
    return stamper.WriteDigits(value);
  }

  static bool Encode(uint32_t width, uint32_t height, uint32_t stride,
		     uint8_t* pYData, uint8_t* pMsg, size_t msg_len,
		     uint32_t x = 0, uint32_t y = 0);

  static bool Decode(uint32_t width, uint32_t height, uint32_t stride,
		     uint8_t* pYData, uint8_t* pMsg, size_t msg_len,
		     uint32_t x = 0, uint32_t y = 0);

 private:
  YuvStamper(uint8_t* pYData,
	     uint32_t width, uint32_t height, uint32_t stride,
	     uint32_t x, uint32_t y,
	     uint8_t symbol_width, uint8_t symbol_height);

  bool WriteDigit(uint8_t digit);
  void WritePixel(uint8_t* data, uint32_t x, uint32_t y);

  uint32_t Capacity();
  bool AdvanceCursor();
  bool WriteBit(bool one);
  bool Write8(uint8_t value);
  bool ReadBit(uint8_t &value);
  bool Read8(uint8_t &bit);

  const static uint8_t sPixelSize = 3;
  const static uint8_t sDigitWidth = 6;
  const static uint8_t sDigitHeight = 7;
  const static uint8_t sInterDigit = 1;
  const static uint8_t sInterLine = 1;
  const static uint32_t sBitSize = 4;
  const static uint32_t sBitThreshold = 60;
  const static uint8_t sYOn = 0x80;
  const static uint8_t sYOff = 0;
  const static uint8_t sLumaThreshold = 96;
  const static uint8_t sLumaMin = 16;
  const static uint8_t sLumaMax = 235;

  uint8_t* pYData;
  uint32_t mStride;
  uint32_t mWidth;
  uint32_t mHeight;
  uint8_t mSymbolWidth;
  uint8_t mSymbolHeight;

  struct Cursor {
    Cursor(uint32_t x, uint32_t y):
      x(x), y(y) {}
    uint32_t x;
    uint32_t y;
  } mCursor;
};

}
