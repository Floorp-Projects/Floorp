// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_ArithDecoder.h"

#include "core/fxcodec/jbig2/JBig2_BitStream.h"
#include "core/fxcrt/fx_basic.h"

namespace {

struct JBig2ArithQe {
  unsigned int Qe;
  unsigned int NMPS;
  unsigned int NLPS;
  unsigned int nSwitch;
};

const JBig2ArithQe kQeTable[] = {
    // Stupid hack to keep clang-format from reformatting this badly.
    {0x5601, 1, 1, 1},   {0x3401, 2, 6, 0},   {0x1801, 3, 9, 0},
    {0x0AC1, 4, 12, 0},  {0x0521, 5, 29, 0},  {0x0221, 38, 33, 0},
    {0x5601, 7, 6, 1},   {0x5401, 8, 14, 0},  {0x4801, 9, 14, 0},
    {0x3801, 10, 14, 0}, {0x3001, 11, 17, 0}, {0x2401, 12, 18, 0},
    {0x1C01, 13, 20, 0}, {0x1601, 29, 21, 0}, {0x5601, 15, 14, 1},
    {0x5401, 16, 14, 0}, {0x5101, 17, 15, 0}, {0x4801, 18, 16, 0},
    {0x3801, 19, 17, 0}, {0x3401, 20, 18, 0}, {0x3001, 21, 19, 0},
    {0x2801, 22, 19, 0}, {0x2401, 23, 20, 0}, {0x2201, 24, 21, 0},
    {0x1C01, 25, 22, 0}, {0x1801, 26, 23, 0}, {0x1601, 27, 24, 0},
    {0x1401, 28, 25, 0}, {0x1201, 29, 26, 0}, {0x1101, 30, 27, 0},
    {0x0AC1, 31, 28, 0}, {0x09C1, 32, 29, 0}, {0x08A1, 33, 30, 0},
    {0x0521, 34, 31, 0}, {0x0441, 35, 32, 0}, {0x02A1, 36, 33, 0},
    {0x0221, 37, 34, 0}, {0x0141, 38, 35, 0}, {0x0111, 39, 36, 0},
    {0x0085, 40, 37, 0}, {0x0049, 41, 38, 0}, {0x0025, 42, 39, 0},
    {0x0015, 43, 40, 0}, {0x0009, 44, 41, 0}, {0x0005, 45, 42, 0},
    {0x0001, 45, 43, 0}, {0x5601, 46, 46, 0}};

const unsigned int kDefaultAValue = 0x8000;

int DecodeNMPS(JBig2ArithCtx* pCX, const JBig2ArithQe& qe) {
  pCX->I = qe.NMPS;
  return pCX->MPS;
}

int DecodeNLPS(JBig2ArithCtx* pCX, const JBig2ArithQe& qe) {
  // TODO(thestig): |D|, |MPS| and friends probably should be booleans.
  int D = 1 - pCX->MPS;
  if (qe.nSwitch == 1)
    pCX->MPS = 1 - pCX->MPS;
  pCX->I = qe.NLPS;
  return D;
}

}  // namespace

CJBig2_ArithDecoder::CJBig2_ArithDecoder(CJBig2_BitStream* pStream)
    : m_pStream(pStream) {
  m_B = m_pStream->getCurByte_arith();
  m_C = (m_B ^ 0xff) << 16;
  BYTEIN();
  m_C = m_C << 7;
  m_CT = m_CT - 7;
  m_A = kDefaultAValue;
}

CJBig2_ArithDecoder::~CJBig2_ArithDecoder() {}

int CJBig2_ArithDecoder::DECODE(JBig2ArithCtx* pCX) {
  if (!pCX || pCX->I >= FX_ArraySize(kQeTable))
    return 0;

  const JBig2ArithQe& qe = kQeTable[pCX->I];
  m_A -= qe.Qe;
  if ((m_C >> 16) < m_A) {
    if (m_A & kDefaultAValue)
      return pCX->MPS;

    const int D = m_A < qe.Qe ? DecodeNLPS(pCX, qe) : DecodeNMPS(pCX, qe);
    ReadValueA();
    return D;
  }

  m_C -= m_A << 16;
  const int D = m_A < qe.Qe ? DecodeNMPS(pCX, qe) : DecodeNLPS(pCX, qe);
  m_A = qe.Qe;
  ReadValueA();
  return D;
}

void CJBig2_ArithDecoder::BYTEIN() {
  unsigned char B1;
  if (m_B == 0xff) {
    B1 = m_pStream->getNextByte_arith();
    if (B1 > 0x8f) {
      m_CT = 8;
    } else {
      m_pStream->incByteIdx();
      m_B = B1;
      m_C = m_C + 0xfe00 - (m_B << 9);
      m_CT = 7;
    }
  } else {
    m_pStream->incByteIdx();
    m_B = m_pStream->getCurByte_arith();
    m_C = m_C + 0xff00 - (m_B << 8);
    m_CT = 8;
  }
}

void CJBig2_ArithDecoder::ReadValueA() {
  do {
    if (m_CT == 0)
      BYTEIN();
    m_A <<= 1;
    m_C <<= 1;
    --m_CT;
  } while ((m_A & kDefaultAValue) == 0);
}
