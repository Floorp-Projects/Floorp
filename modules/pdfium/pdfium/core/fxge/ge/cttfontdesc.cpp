// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/ge/cttfontdesc.h"

#include "core/fxge/fx_freetype.h"

CTTFontDesc::~CTTFontDesc() {
  if (m_Type == 1) {
    if (m_SingleFace)
      FXFT_Done_Face(m_SingleFace);
  } else if (m_Type == 2) {
    for (int i = 0; i < 16; i++) {
      if (m_TTCFaces[i])
        FXFT_Done_Face(m_TTCFaces[i]);
    }
  }
  FX_Free(m_pFontData);
}

int CTTFontDesc::ReleaseFace(FXFT_Face face) {
  if (m_Type == 1) {
    if (m_SingleFace != face)
      return -1;
  } else if (m_Type == 2) {
    int i;
    for (i = 0; i < 16; i++) {
      if (m_TTCFaces[i] == face)
        break;
    }
    if (i == 16)
      return -1;
  }
  m_RefCount--;
  if (m_RefCount)
    return m_RefCount;
  delete this;
  return 0;
}
