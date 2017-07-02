// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_SUBSTFONT_H_
#define CORE_FXGE_CFX_SUBSTFONT_H_

#include "core/fxcrt/fx_string.h"

#define FXFONT_SUBST_MM 0x01
#define FXFONT_SUBST_EXACT 0x40

class CFX_SubstFont {
 public:
  CFX_SubstFont();

  CFX_ByteString m_Family;
  int m_Charset;
  uint32_t m_SubstFlags;
  int m_Weight;
  int m_ItalicAngle;
  bool m_bSubstCJK;
  int m_WeightCJK;
  bool m_bItalicCJK;
};

#endif  // CORE_FXGE_CFX_SUBSTFONT_H_
