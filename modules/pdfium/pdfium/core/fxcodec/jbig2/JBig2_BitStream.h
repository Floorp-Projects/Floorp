// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_BITSTREAM_H_
#define CORE_FXCODEC_JBIG2_JBIG2_BITSTREAM_H_

#include "core/fxcrt/fx_basic.h"

class CPDF_StreamAcc;

class CJBig2_BitStream {
 public:
  explicit CJBig2_BitStream(CPDF_StreamAcc* pSrcStream);
  ~CJBig2_BitStream();

  // TODO(thestig): readFoo() should return bool.
  int32_t readNBits(uint32_t nBits, uint32_t* dwResult);
  int32_t readNBits(uint32_t nBits, int32_t* nResult);
  int32_t read1Bit(uint32_t* dwResult);
  int32_t read1Bit(bool* bResult);
  int32_t read1Byte(uint8_t* cResult);
  int32_t readInteger(uint32_t* dwResult);
  int32_t readShortInteger(uint16_t* wResult);
  void alignByte();
  uint8_t getCurByte() const;
  void incByteIdx();
  uint8_t getCurByte_arith() const;
  uint8_t getNextByte_arith() const;
  uint32_t getOffset() const;
  void setOffset(uint32_t dwOffset);
  uint32_t getBitPos() const;
  void setBitPos(uint32_t dwBitPos);
  const uint8_t* getBuf() const;
  uint32_t getLength() const { return m_dwLength; }
  const uint8_t* getPointer() const;
  void offset(uint32_t dwOffset);
  uint32_t getByteLeft() const;
  uint32_t getObjNum() const;

 private:
  void AdvanceBit();
  bool IsInBound() const;
  uint32_t LengthInBits() const;

  const uint8_t* m_pBuf;
  uint32_t m_dwLength;
  uint32_t m_dwByteIdx;
  uint32_t m_dwBitIdx;
  const uint32_t m_dwObjNum;

  CJBig2_BitStream(const CJBig2_BitStream&) = delete;
  void operator=(const CJBig2_BitStream&) = delete;
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_BITSTREAM_H_
