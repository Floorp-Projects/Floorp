// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_iconfit.h"

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fxcrt/fx_string.h"

CPDF_IconFit::ScaleMethod CPDF_IconFit::GetScaleMethod() {
  if (!m_pDict)
    return Always;

  CFX_ByteString csSW = m_pDict->GetStringFor("SW", "A");
  if (csSW == "B")
    return Bigger;
  if (csSW == "S")
    return Smaller;
  if (csSW == "N")
    return Never;
  return Always;
}

bool CPDF_IconFit::IsProportionalScale() {
  return m_pDict ? m_pDict->GetStringFor("S", "P") != "A" : true;
}

void CPDF_IconFit::GetIconPosition(FX_FLOAT& fLeft, FX_FLOAT& fBottom) {
  fLeft = fBottom = 0.5;
  if (!m_pDict)
    return;

  CPDF_Array* pA = m_pDict->GetArrayFor("A");
  if (pA) {
    uint32_t dwCount = pA->GetCount();
    if (dwCount > 0)
      fLeft = pA->GetNumberAt(0);
    if (dwCount > 1)
      fBottom = pA->GetNumberAt(1);
  }
}

bool CPDF_IconFit::GetFittingBounds() {
  return m_pDict ? m_pDict->GetBooleanFor("FB") : false;
}
