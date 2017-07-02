// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_substfont.h"

#include "core/fxge/fx_font.h"

CFX_SubstFont::CFX_SubstFont()
    : m_Charset(FXFONT_ANSI_CHARSET),
      m_SubstFlags(0),
      m_Weight(0),
      m_ItalicAngle(0),
      m_bSubstCJK(false),
      m_WeightCJK(0),
      m_bItalicCJK(false) {}
