// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_ArithIntDecoder.h"

#include <vector>

#include "core/fxcrt/fx_basic.h"

namespace {

int ShiftOr(int val, int bitwise_or_val) {
  return (val << 1) | bitwise_or_val;
}

const struct ArithIntDecodeData {
  int nNeedBits;
  int nValue;
} g_ArithIntDecodeData[] = {
    {2, 0}, {4, 4}, {6, 20}, {8, 84}, {12, 340}, {32, 4436},
};

size_t RecursiveDecode(CJBig2_ArithDecoder* decoder,
                       std::vector<JBig2ArithCtx>* context,
                       int* prev,
                       size_t depth) {
  static const size_t kDepthEnd = FX_ArraySize(g_ArithIntDecodeData) - 1;
  if (depth == kDepthEnd)
    return kDepthEnd;

  JBig2ArithCtx* pCX = &(*context)[*prev];
  int D = decoder->DECODE(pCX);
  *prev = ShiftOr(*prev, D);
  if (!D)
    return depth;
  return RecursiveDecode(decoder, context, prev, depth + 1);
}

}  // namespace

CJBig2_ArithIntDecoder::CJBig2_ArithIntDecoder() {
  m_IAx.resize(512);
}

CJBig2_ArithIntDecoder::~CJBig2_ArithIntDecoder() {}

bool CJBig2_ArithIntDecoder::decode(CJBig2_ArithDecoder* pArithDecoder,
                                    int* nResult) {
  int PREV = 1;
  const int S = pArithDecoder->DECODE(&m_IAx[PREV]);
  PREV = ShiftOr(PREV, S);

  const size_t nDecodeDataIndex =
      RecursiveDecode(pArithDecoder, &m_IAx, &PREV, 0);

  int nTemp = 0;
  for (int i = 0; i < g_ArithIntDecodeData[nDecodeDataIndex].nNeedBits; ++i) {
    int D = pArithDecoder->DECODE(&m_IAx[PREV]);
    PREV = ShiftOr(PREV, D);
    if (PREV >= 256)
      PREV = (PREV & 511) | 256;
    nTemp = ShiftOr(nTemp, D);
  }
  int nValue = g_ArithIntDecodeData[nDecodeDataIndex].nValue;
  nValue += nTemp;
  if (S == 1 && nValue > 0)
    nValue = -nValue;

  *nResult = nValue;
  return S != 1 || nValue != 0;
}

CJBig2_ArithIaidDecoder::CJBig2_ArithIaidDecoder(unsigned char SBSYMCODELENA)
    : SBSYMCODELEN(SBSYMCODELENA) {
  m_IAID.resize(static_cast<size_t>(1) << SBSYMCODELEN);
}

CJBig2_ArithIaidDecoder::~CJBig2_ArithIaidDecoder() {}

void CJBig2_ArithIaidDecoder::decode(CJBig2_ArithDecoder* pArithDecoder,
                                     uint32_t* nResult) {
  int PREV = 1;
  for (unsigned char i = 0; i < SBSYMCODELEN; ++i) {
    JBig2ArithCtx* pCX = &m_IAID[PREV];
    int D = pArithDecoder->DECODE(pCX);
    PREV = ShiftOr(PREV, D);
  }
  *nResult = PREV - (1 << SBSYMCODELEN);
}
