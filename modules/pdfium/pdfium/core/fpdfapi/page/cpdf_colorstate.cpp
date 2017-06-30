// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_colorstate.h"

#include "core/fpdfapi/page/cpdf_pattern.h"
#include "core/fpdfapi/page/cpdf_tilingpattern.h"
#include "core/fxge/fx_dib.h"

CPDF_ColorState::CPDF_ColorState() {}

CPDF_ColorState::CPDF_ColorState(const CPDF_ColorState& that)
    : m_Ref(that.m_Ref) {}

CPDF_ColorState::~CPDF_ColorState() {}

void CPDF_ColorState::Emplace() {
  m_Ref.Emplace();
}

void CPDF_ColorState::SetDefault() {
  m_Ref.GetPrivateCopy()->SetDefault();
}

uint32_t CPDF_ColorState::GetFillRGB() const {
  return m_Ref.GetObject()->m_FillRGB;
}

void CPDF_ColorState::SetFillRGB(uint32_t rgb) {
  m_Ref.GetPrivateCopy()->m_FillRGB = rgb;
}

uint32_t CPDF_ColorState::GetStrokeRGB() const {
  return m_Ref.GetObject()->m_StrokeRGB;
}

void CPDF_ColorState::SetStrokeRGB(uint32_t rgb) {
  m_Ref.GetPrivateCopy()->m_StrokeRGB = rgb;
}

const CPDF_Color* CPDF_ColorState::GetFillColor() const {
  const ColorData* pData = m_Ref.GetObject();
  return pData ? &pData->m_FillColor : nullptr;
}

CPDF_Color* CPDF_ColorState::GetMutableFillColor() {
  return &m_Ref.GetPrivateCopy()->m_FillColor;
}

bool CPDF_ColorState::HasFillColor() const {
  const CPDF_Color* pColor = GetFillColor();
  return pColor && !pColor->IsNull();
}

const CPDF_Color* CPDF_ColorState::GetStrokeColor() const {
  const ColorData* pData = m_Ref.GetObject();
  return pData ? &pData->m_StrokeColor : nullptr;
}

CPDF_Color* CPDF_ColorState::GetMutableStrokeColor() {
  return &m_Ref.GetPrivateCopy()->m_StrokeColor;
}

bool CPDF_ColorState::HasStrokeColor() const {
  const CPDF_Color* pColor = GetStrokeColor();
  return pColor && !pColor->IsNull();
}

void CPDF_ColorState::SetFillColor(CPDF_ColorSpace* pCS,
                                   FX_FLOAT* pValue,
                                   uint32_t nValues) {
  ColorData* pData = m_Ref.GetPrivateCopy();
  SetColor(pData->m_FillColor, pData->m_FillRGB, pCS, pValue, nValues);
}

void CPDF_ColorState::SetStrokeColor(CPDF_ColorSpace* pCS,
                                     FX_FLOAT* pValue,
                                     uint32_t nValues) {
  ColorData* pData = m_Ref.GetPrivateCopy();
  SetColor(pData->m_StrokeColor, pData->m_StrokeRGB, pCS, pValue, nValues);
}

void CPDF_ColorState::SetFillPattern(CPDF_Pattern* pPattern,
                                     FX_FLOAT* pValue,
                                     uint32_t nValues) {
  ColorData* pData = m_Ref.GetPrivateCopy();
  pData->m_FillColor.SetValue(pPattern, pValue, nValues);
  int R, G, B;
  bool ret = pData->m_FillColor.GetRGB(R, G, B);
  if (CPDF_TilingPattern* pTilingPattern = pPattern->AsTilingPattern()) {
    if (!ret && pTilingPattern->colored()) {
      pData->m_FillRGB = 0x00BFBFBF;
      return;
    }
  }
  pData->m_FillRGB = ret ? FXSYS_RGB(R, G, B) : (uint32_t)-1;
}

void CPDF_ColorState::SetStrokePattern(CPDF_Pattern* pPattern,
                                       FX_FLOAT* pValue,
                                       uint32_t nValues) {
  ColorData* pData = m_Ref.GetPrivateCopy();
  pData->m_StrokeColor.SetValue(pPattern, pValue, nValues);
  int R, G, B;
  bool ret = pData->m_StrokeColor.GetRGB(R, G, B);
  if (CPDF_TilingPattern* pTilingPattern = pPattern->AsTilingPattern()) {
    if (!ret && pTilingPattern->colored()) {
      pData->m_StrokeRGB = 0x00BFBFBF;
      return;
    }
  }
  pData->m_StrokeRGB =
      pData->m_StrokeColor.GetRGB(R, G, B) ? FXSYS_RGB(R, G, B) : (uint32_t)-1;
}

void CPDF_ColorState::SetColor(CPDF_Color& color,
                               uint32_t& rgb,
                               CPDF_ColorSpace* pCS,
                               FX_FLOAT* pValue,
                               uint32_t nValues) {
  if (pCS)
    color.SetColorSpace(pCS);
  else if (color.IsNull())
    color.SetColorSpace(CPDF_ColorSpace::GetStockCS(PDFCS_DEVICEGRAY));

  if (color.GetColorSpace()->CountComponents() > nValues)
    return;

  color.SetValue(pValue);
  int R;
  int G;
  int B;
  rgb = color.GetRGB(R, G, B) ? FXSYS_RGB(R, G, B) : (uint32_t)-1;
}

CPDF_ColorState::ColorData::ColorData() : m_FillRGB(0), m_StrokeRGB(0) {}

CPDF_ColorState::ColorData::ColorData(const ColorData& src)
    : m_FillRGB(src.m_FillRGB), m_StrokeRGB(src.m_StrokeRGB) {
  m_FillColor.Copy(&src.m_FillColor);
  m_StrokeColor.Copy(&src.m_StrokeColor);
}

CPDF_ColorState::ColorData::~ColorData() {}

void CPDF_ColorState::ColorData::SetDefault() {
  m_FillRGB = 0;
  m_StrokeRGB = 0;
  m_FillColor.SetColorSpace(CPDF_ColorSpace::GetStockCS(PDFCS_DEVICEGRAY));
  m_StrokeColor.SetColorSpace(CPDF_ColorSpace::GetStockCS(PDFCS_DEVICEGRAY));
}
