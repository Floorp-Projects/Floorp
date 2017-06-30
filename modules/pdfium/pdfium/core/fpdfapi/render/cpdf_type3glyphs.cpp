// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_type3glyphs.h"

#include <map>

#include "core/fxge/fx_font.h"

CPDF_Type3Glyphs::CPDF_Type3Glyphs()
    : m_TopBlueCount(0), m_BottomBlueCount(0) {}

CPDF_Type3Glyphs::~CPDF_Type3Glyphs() {
  for (const auto& pair : m_GlyphMap)
    delete pair.second;
}

static int _AdjustBlue(FX_FLOAT pos, int& count, int blues[]) {
  FX_FLOAT min_distance = 1000000.0f;
  int closest_pos = -1;
  for (int i = 0; i < count; i++) {
    FX_FLOAT distance = FXSYS_fabs(pos - static_cast<FX_FLOAT>(blues[i]));
    if (distance < 1.0f * 80.0f / 100.0f && distance < min_distance) {
      min_distance = distance;
      closest_pos = i;
    }
  }
  if (closest_pos >= 0)
    return blues[closest_pos];
  int new_pos = FXSYS_round(pos);
  if (count == TYPE3_MAX_BLUES)
    return new_pos;
  blues[count++] = new_pos;
  return new_pos;
}

void CPDF_Type3Glyphs::AdjustBlue(FX_FLOAT top,
                                  FX_FLOAT bottom,
                                  int& top_line,
                                  int& bottom_line) {
  top_line = _AdjustBlue(top, m_TopBlueCount, m_TopBlue);
  bottom_line = _AdjustBlue(bottom, m_BottomBlueCount, m_BottomBlue);
}
