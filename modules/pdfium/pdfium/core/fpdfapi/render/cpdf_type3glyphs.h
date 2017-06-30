// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_TYPE3GLYPHS_H_
#define CORE_FPDFAPI_RENDER_CPDF_TYPE3GLYPHS_H_

#include <map>

#include "core/fxcrt/fx_system.h"

class CFX_GlyphBitmap;

#define TYPE3_MAX_BLUES 16

class CPDF_Type3Glyphs {
 public:
  CPDF_Type3Glyphs();
  ~CPDF_Type3Glyphs();

  void AdjustBlue(FX_FLOAT top,
                  FX_FLOAT bottom,
                  int& top_line,
                  int& bottom_line);

  std::map<uint32_t, CFX_GlyphBitmap*> m_GlyphMap;
  int m_TopBlue[TYPE3_MAX_BLUES];
  int m_BottomBlue[TYPE3_MAX_BLUES];
  int m_TopBlueCount;
  int m_BottomBlueCount;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_TYPE3GLYPHS_H_
