// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_GE_CTTFONTDESC_H_
#define CORE_FXGE_GE_CTTFONTDESC_H_

#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_font.h"

#define FX_FONT_FLAG_SERIF 0x01
#define FX_FONT_FLAG_FIXEDPITCH 0x02
#define FX_FONT_FLAG_ITALIC 0x04
#define FX_FONT_FLAG_BOLD 0x08
#define FX_FONT_FLAG_SYMBOLIC_SYMBOL 0x10
#define FX_FONT_FLAG_SYMBOLIC_DINGBATS 0x20
#define FX_FONT_FLAG_MULTIPLEMASTER 0x40

class CTTFontDesc {
 public:
  CTTFontDesc() : m_Type(0), m_pFontData(nullptr), m_RefCount(0) {}
  ~CTTFontDesc();
  // ret < 0, releaseface not appropriate for this object.
  // ret == 0, object released
  // ret > 0, object still alive, other referrers.
  int ReleaseFace(FXFT_Face face);

  int m_Type;

  union {
    FXFT_Face m_SingleFace;
    FXFT_Face m_TTCFaces[16];
  };
  uint8_t* m_pFontData;
  int m_RefCount;
};

#endif  // CORE_FXGE_GE_CTTFONTDESC_H_
