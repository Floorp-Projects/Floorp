// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_UNICODEENCODINGEX_H_
#define CORE_FXGE_CFX_UNICODEENCODINGEX_H_

#include <map>
#include <memory>
#include <vector>

#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_unicodeencoding.h"
#include "core/fxge/fx_dib.h"
#include "core/fxge/fx_freetype.h"

class CFX_UnicodeEncodingEx : public CFX_UnicodeEncoding {
 public:
  CFX_UnicodeEncodingEx(CFX_Font* pFont, uint32_t EncodingID);
  ~CFX_UnicodeEncodingEx() override;

  // CFX_UnicodeEncoding:
  uint32_t GlyphFromCharCode(uint32_t charcode) override;

  uint32_t CharCodeFromUnicode(FX_WCHAR Unicode) const;

 private:
  uint32_t m_nEncodingID;
};

CFX_UnicodeEncodingEx* FX_CreateFontEncodingEx(
    CFX_Font* pFont,
    uint32_t nEncodingID = FXFM_ENCODING_NONE);

#endif  // CORE_FXGE_CFX_UNICODEENCODINGEX_H_
