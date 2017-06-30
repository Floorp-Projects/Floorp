// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_IMAGE_H_
#define CORE_FXCODEC_JBIG2_JBIG2_IMAGE_H_

#include "core/fxcodec/jbig2/JBig2_Define.h"

struct FX_RECT;

enum JBig2ComposeOp {
  JBIG2_COMPOSE_OR = 0,
  JBIG2_COMPOSE_AND = 1,
  JBIG2_COMPOSE_XOR = 2,
  JBIG2_COMPOSE_XNOR = 3,
  JBIG2_COMPOSE_REPLACE = 4
};

class CJBig2_Image {
 public:
  CJBig2_Image(int32_t w, int32_t h);
  CJBig2_Image(int32_t w, int32_t h, int32_t stride, uint8_t* pBuf);
  CJBig2_Image(const CJBig2_Image& im);
  ~CJBig2_Image();

  int32_t width() const { return m_nWidth; }
  int32_t height() const { return m_nHeight; }
  int32_t stride() const { return m_nStride; }

  int getPixel(int32_t x, int32_t y);
  int32_t setPixel(int32_t x, int32_t y, int bVal);

  void copyLine(int32_t hTo, int32_t hFrom);
  void fill(bool v);

  bool composeTo(CJBig2_Image* pDst, int32_t x, int32_t y, JBig2ComposeOp op);
  bool composeTo(CJBig2_Image* pDst,
                 int32_t x,
                 int32_t y,
                 JBig2ComposeOp op,
                 const FX_RECT* pSrcRect);

  bool composeTo_opt2(CJBig2_Image* pDst,
                      int32_t x,
                      int32_t y,
                      JBig2ComposeOp op);
  bool composeTo_opt2(CJBig2_Image* pDst,
                      int32_t x,
                      int32_t y,
                      JBig2ComposeOp op,
                      const FX_RECT* pSrcRect);

  bool composeFrom(int32_t x, int32_t y, CJBig2_Image* pSrc, JBig2ComposeOp op);
  bool composeFrom(int32_t x,
                   int32_t y,
                   CJBig2_Image* pSrc,
                   JBig2ComposeOp op,
                   const FX_RECT* pSrcRect);

  CJBig2_Image* subImage(int32_t x, int32_t y, int32_t w, int32_t h);
  void expand(int32_t h, bool v);

  uint8_t* m_pData;

 private:
  int32_t m_nWidth;   // 1-bit pixels
  int32_t m_nHeight;  // lines
  int32_t m_nStride;  // bytes
  bool m_bOwnsBuffer;
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_IMAGE_H_
