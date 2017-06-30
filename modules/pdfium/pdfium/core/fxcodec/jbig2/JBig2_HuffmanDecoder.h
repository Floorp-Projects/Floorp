// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_HUFFMANDECODER_H_
#define CORE_FXCODEC_JBIG2_JBIG2_HUFFMANDECODER_H_

#include "core/fxcodec/jbig2/JBig2_BitStream.h"
#include "core/fxcodec/jbig2/JBig2_HuffmanTable.h"
class CJBig2_HuffmanDecoder {
 public:
  explicit CJBig2_HuffmanDecoder(CJBig2_BitStream* pStream);

  ~CJBig2_HuffmanDecoder();

  int decodeAValue(CJBig2_HuffmanTable* pTable, int* nResult);

 private:
  CJBig2_BitStream* const m_pStream;
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_HUFFMANDECODER_H_
