// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_UNICODEENCODING_H_
#define CORE_FXGE_CFX_UNICODEENCODING_H_

#include "core/fxge/fx_font.h"

#define ENCODING_INTERNAL 0
#define ENCODING_UNICODE 1

#ifdef PDF_ENABLE_XFA
#define FXFM_ENC_TAG(a, b, c, d)                                          \
  (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | \
   (uint32_t)(d))
#define FXFM_ENCODING_NONE FXFM_ENC_TAG(0, 0, 0, 0)
#define FXFM_ENCODING_MS_SYMBOL FXFM_ENC_TAG('s', 'y', 'm', 'b')
#define FXFM_ENCODING_UNICODE FXFM_ENC_TAG('u', 'n', 'i', 'c')
#define FXFM_ENCODING_MS_SJIS FXFM_ENC_TAG('s', 'j', 'i', 's')
#define FXFM_ENCODING_MS_GB2312 FXFM_ENC_TAG('g', 'b', ' ', ' ')
#define FXFM_ENCODING_MS_BIG5 FXFM_ENC_TAG('b', 'i', 'g', '5')
#define FXFM_ENCODING_MS_WANSUNG FXFM_ENC_TAG('w', 'a', 'n', 's')
#define FXFM_ENCODING_MS_JOHAB FXFM_ENC_TAG('j', 'o', 'h', 'a')
#define FXFM_ENCODING_ADOBE_STANDARD FXFM_ENC_TAG('A', 'D', 'O', 'B')
#define FXFM_ENCODING_ADOBE_EXPERT FXFM_ENC_TAG('A', 'D', 'B', 'E')
#define FXFM_ENCODING_ADOBE_CUSTOM FXFM_ENC_TAG('A', 'D', 'B', 'C')
#define FXFM_ENCODING_ADOBE_LATIN_1 FXFM_ENC_TAG('l', 'a', 't', '1')
#define FXFM_ENCODING_OLD_LATIN_2 FXFM_ENC_TAG('l', 'a', 't', '2')
#define FXFM_ENCODING_APPLE_ROMAN FXFM_ENC_TAG('a', 'r', 'm', 'n')
#endif  // PDF_ENABLE_XFA

class CFX_UnicodeEncoding {
 public:
  explicit CFX_UnicodeEncoding(CFX_Font* pFont);
  virtual ~CFX_UnicodeEncoding();

  virtual uint32_t GlyphFromCharCode(uint32_t charcode);

 protected:
  CFX_Font* m_pFont;  // Unowned, not nullptr.
};

#endif  // CORE_FXGE_CFX_UNICODEENCODING_H_
