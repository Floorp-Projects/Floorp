// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_apsettings.h"

#include <algorithm>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfdoc/cpdf_formcontrol.h"

CPDF_ApSettings::CPDF_ApSettings(CPDF_Dictionary* pDict) : m_pDict(pDict) {}

bool CPDF_ApSettings::HasMKEntry(const CFX_ByteString& csEntry) const {
  return m_pDict && m_pDict->KeyExist(csEntry);
}

int CPDF_ApSettings::GetRotation() const {
  return m_pDict ? m_pDict->GetIntegerFor("R") : 0;
}

FX_ARGB CPDF_ApSettings::GetColor(int& iColorType,
                                  const CFX_ByteString& csEntry) const {
  iColorType = COLORTYPE_TRANSPARENT;
  if (!m_pDict)
    return 0;

  CPDF_Array* pEntry = m_pDict->GetArrayFor(csEntry);
  if (!pEntry)
    return 0;

  FX_ARGB color = 0;
  size_t dwCount = pEntry->GetCount();
  if (dwCount == 1) {
    iColorType = COLORTYPE_GRAY;
    FX_FLOAT g = pEntry->GetNumberAt(0) * 255;
    return ArgbEncode(255, (int)g, (int)g, (int)g);
  }
  if (dwCount == 3) {
    iColorType = COLORTYPE_RGB;
    FX_FLOAT r = pEntry->GetNumberAt(0) * 255;
    FX_FLOAT g = pEntry->GetNumberAt(1) * 255;
    FX_FLOAT b = pEntry->GetNumberAt(2) * 255;
    return ArgbEncode(255, (int)r, (int)g, (int)b);
  }
  if (dwCount == 4) {
    iColorType = COLORTYPE_CMYK;
    FX_FLOAT c = pEntry->GetNumberAt(0);
    FX_FLOAT m = pEntry->GetNumberAt(1);
    FX_FLOAT y = pEntry->GetNumberAt(2);
    FX_FLOAT k = pEntry->GetNumberAt(3);
    FX_FLOAT r = 1.0f - std::min(1.0f, c + k);
    FX_FLOAT g = 1.0f - std::min(1.0f, m + k);
    FX_FLOAT b = 1.0f - std::min(1.0f, y + k);
    return ArgbEncode(255, (int)(r * 255), (int)(g * 255), (int)(b * 255));
  }
  return color;
}

FX_FLOAT CPDF_ApSettings::GetOriginalColor(
    int index,
    const CFX_ByteString& csEntry) const {
  if (!m_pDict)
    return 0;

  CPDF_Array* pEntry = m_pDict->GetArrayFor(csEntry);
  return pEntry ? pEntry->GetNumberAt(index) : 0;
}

void CPDF_ApSettings::GetOriginalColor(int& iColorType,
                                       FX_FLOAT fc[4],
                                       const CFX_ByteString& csEntry) const {
  iColorType = COLORTYPE_TRANSPARENT;
  for (int i = 0; i < 4; i++)
    fc[i] = 0;

  if (!m_pDict)
    return;

  CPDF_Array* pEntry = m_pDict->GetArrayFor(csEntry);
  if (!pEntry)
    return;

  size_t dwCount = pEntry->GetCount();
  if (dwCount == 1) {
    iColorType = COLORTYPE_GRAY;
    fc[0] = pEntry->GetNumberAt(0);
  } else if (dwCount == 3) {
    iColorType = COLORTYPE_RGB;
    fc[0] = pEntry->GetNumberAt(0);
    fc[1] = pEntry->GetNumberAt(1);
    fc[2] = pEntry->GetNumberAt(2);
  } else if (dwCount == 4) {
    iColorType = COLORTYPE_CMYK;
    fc[0] = pEntry->GetNumberAt(0);
    fc[1] = pEntry->GetNumberAt(1);
    fc[2] = pEntry->GetNumberAt(2);
    fc[3] = pEntry->GetNumberAt(3);
  }
}

CFX_WideString CPDF_ApSettings::GetCaption(
    const CFX_ByteString& csEntry) const {
  return m_pDict ? m_pDict->GetUnicodeTextFor(csEntry) : CFX_WideString();
}

CPDF_Stream* CPDF_ApSettings::GetIcon(const CFX_ByteString& csEntry) const {
  return m_pDict ? m_pDict->GetStreamFor(csEntry) : nullptr;
}

CPDF_IconFit CPDF_ApSettings::GetIconFit() const {
  return CPDF_IconFit(m_pDict ? m_pDict->GetDictFor("IF") : nullptr);
}

int CPDF_ApSettings::GetTextPosition() const {
  return m_pDict ? m_pDict->GetIntegerFor("TP", TEXTPOS_CAPTION)
                 : TEXTPOS_CAPTION;
}
