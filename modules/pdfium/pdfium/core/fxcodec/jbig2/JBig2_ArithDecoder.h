// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_ARITHDECODER_H_
#define CORE_FXCODEC_JBIG2_JBIG2_ARITHDECODER_H_

class CJBig2_BitStream;

struct JBig2ArithCtx {
  JBig2ArithCtx() : MPS(0), I(0) {}

  unsigned int MPS;
  unsigned int I;
};

class CJBig2_ArithDecoder {
 public:
  explicit CJBig2_ArithDecoder(CJBig2_BitStream* pStream);

  ~CJBig2_ArithDecoder();

  int DECODE(JBig2ArithCtx* pCX);

 private:
  void BYTEIN();
  void ReadValueA();

  unsigned char m_B;
  unsigned int m_C;
  unsigned int m_A;
  unsigned int m_CT;
  CJBig2_BitStream* const m_pStream;
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_ARITHDECODER_H_
