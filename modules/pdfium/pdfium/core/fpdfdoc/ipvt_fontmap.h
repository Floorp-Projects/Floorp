// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_IPVT_FONTMAP_H_
#define CORE_FPDFDOC_IPVT_FONTMAP_H_

#include <stdint.h>

#include "core/fxcrt/fx_string.h"

class CPDF_Font;

class IPVT_FontMap {
 public:
  virtual ~IPVT_FontMap() {}

  virtual CPDF_Font* GetPDFFont(int32_t nFontIndex) = 0;
  virtual CFX_ByteString GetPDFFontAlias(int32_t nFontIndex) = 0;
  virtual int32_t GetWordFontIndex(uint16_t word,
                                   int32_t charset,
                                   int32_t nFontIndex) = 0;
  virtual int32_t CharCodeFromUnicode(int32_t nFontIndex, uint16_t word) = 0;
  virtual int32_t CharSetFromUnicode(uint16_t word, int32_t nOldCharset) = 0;
};

#endif  // CORE_FPDFDOC_IPVT_FONTMAP_H_
