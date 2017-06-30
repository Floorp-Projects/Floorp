// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_defaultappearance.h"

#include <algorithm>

#include "core/fpdfapi/parser/cpdf_simple_parser.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfdoc/cpdf_formcontrol.h"

bool CPDF_DefaultAppearance::HasFont() {
  if (m_csDA.IsEmpty())
    return false;

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  return syntax.FindTagParamFromStart("Tf", 2);
}

CFX_ByteString CPDF_DefaultAppearance::GetFontString() {
  CFX_ByteString csFont;
  if (m_csDA.IsEmpty())
    return csFont;

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  if (syntax.FindTagParamFromStart("Tf", 2)) {
    csFont += syntax.GetWord();
    csFont += " ";
    csFont += syntax.GetWord();
    csFont += " ";
    csFont += syntax.GetWord();
  }
  return csFont;
}

void CPDF_DefaultAppearance::GetFont(CFX_ByteString& csFontNameTag,
                                     FX_FLOAT& fFontSize) {
  csFontNameTag = "";
  fFontSize = 0;
  if (m_csDA.IsEmpty())
    return;

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  if (syntax.FindTagParamFromStart("Tf", 2)) {
    csFontNameTag = CFX_ByteString(syntax.GetWord());
    csFontNameTag.Delete(0, 1);
    fFontSize = FX_atof(syntax.GetWord());
  }
  csFontNameTag = PDF_NameDecode(csFontNameTag);
}

bool CPDF_DefaultAppearance::HasColor(PaintOperation nOperation) {
  if (m_csDA.IsEmpty())
    return false;

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "G" : "g"), 1)) {
    return true;
  }
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "RG" : "rg"), 3)) {
    return true;
  }
  return syntax.FindTagParamFromStart(
      (nOperation == PaintOperation::STROKE ? "K" : "k"), 4);
}

CFX_ByteString CPDF_DefaultAppearance::GetColorString(
    PaintOperation nOperation) {
  CFX_ByteString csColor;
  if (m_csDA.IsEmpty())
    return csColor;

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "G" : "g"), 1)) {
    csColor += syntax.GetWord();
    csColor += " ";
    csColor += syntax.GetWord();
    return csColor;
  }
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "RG" : "rg"), 3)) {
    csColor += syntax.GetWord();
    csColor += " ";
    csColor += syntax.GetWord();
    csColor += " ";
    csColor += syntax.GetWord();
    csColor += " ";
    csColor += syntax.GetWord();
    return csColor;
  }
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "K" : "k"), 4)) {
    csColor += syntax.GetWord();
    csColor += " ";
    csColor += syntax.GetWord();
    csColor += " ";
    csColor += syntax.GetWord();
    csColor += " ";
    csColor += syntax.GetWord();
    csColor += " ";
    csColor += syntax.GetWord();
  }
  return csColor;
}

void CPDF_DefaultAppearance::GetColor(int& iColorType,
                                      FX_FLOAT fc[4],
                                      PaintOperation nOperation) {
  iColorType = COLORTYPE_TRANSPARENT;
  for (int c = 0; c < 4; c++)
    fc[c] = 0;

  if (m_csDA.IsEmpty())
    return;

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "G" : "g"), 1)) {
    iColorType = COLORTYPE_GRAY;
    fc[0] = FX_atof(syntax.GetWord());
    return;
  }
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "RG" : "rg"), 3)) {
    iColorType = COLORTYPE_RGB;
    fc[0] = FX_atof(syntax.GetWord());
    fc[1] = FX_atof(syntax.GetWord());
    fc[2] = FX_atof(syntax.GetWord());
    return;
  }
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "K" : "k"), 4)) {
    iColorType = COLORTYPE_CMYK;
    fc[0] = FX_atof(syntax.GetWord());
    fc[1] = FX_atof(syntax.GetWord());
    fc[2] = FX_atof(syntax.GetWord());
    fc[3] = FX_atof(syntax.GetWord());
  }
}

void CPDF_DefaultAppearance::GetColor(FX_ARGB& color,
                                      int& iColorType,
                                      PaintOperation nOperation) {
  color = 0;
  iColorType = COLORTYPE_TRANSPARENT;
  if (m_csDA.IsEmpty())
    return;

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "G" : "g"), 1)) {
    iColorType = COLORTYPE_GRAY;
    FX_FLOAT g = FX_atof(syntax.GetWord()) * 255 + 0.5f;
    color = ArgbEncode(255, (int)g, (int)g, (int)g);
    return;
  }
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "RG" : "rg"), 3)) {
    iColorType = COLORTYPE_RGB;
    FX_FLOAT r = FX_atof(syntax.GetWord()) * 255 + 0.5f;
    FX_FLOAT g = FX_atof(syntax.GetWord()) * 255 + 0.5f;
    FX_FLOAT b = FX_atof(syntax.GetWord()) * 255 + 0.5f;
    color = ArgbEncode(255, (int)r, (int)g, (int)b);
    return;
  }
  if (syntax.FindTagParamFromStart(
          (nOperation == PaintOperation::STROKE ? "K" : "k"), 4)) {
    iColorType = COLORTYPE_CMYK;
    FX_FLOAT c = FX_atof(syntax.GetWord());
    FX_FLOAT m = FX_atof(syntax.GetWord());
    FX_FLOAT y = FX_atof(syntax.GetWord());
    FX_FLOAT k = FX_atof(syntax.GetWord());
    FX_FLOAT r = 1.0f - std::min(1.0f, c + k);
    FX_FLOAT g = 1.0f - std::min(1.0f, m + k);
    FX_FLOAT b = 1.0f - std::min(1.0f, y + k);
    color = ArgbEncode(255, (int)(r * 255 + 0.5f), (int)(g * 255 + 0.5f),
                       (int)(b * 255 + 0.5f));
  }
}

bool CPDF_DefaultAppearance::HasTextMatrix() {
  if (m_csDA.IsEmpty())
    return false;

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  return syntax.FindTagParamFromStart("Tm", 6);
}

CFX_ByteString CPDF_DefaultAppearance::GetTextMatrixString() {
  CFX_ByteString csTM;
  if (m_csDA.IsEmpty())
    return csTM;

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  if (syntax.FindTagParamFromStart("Tm", 6)) {
    for (int i = 0; i < 6; i++) {
      csTM += syntax.GetWord();
      csTM += " ";
    }
    csTM += syntax.GetWord();
  }
  return csTM;
}

CFX_Matrix CPDF_DefaultAppearance::GetTextMatrix() {
  if (m_csDA.IsEmpty())
    return CFX_Matrix();

  CPDF_SimpleParser syntax(m_csDA.AsStringC());
  if (!syntax.FindTagParamFromStart("Tm", 6))
    return CFX_Matrix();

  FX_FLOAT f[6];
  for (int i = 0; i < 6; i++)
    f[i] = FX_atof(syntax.GetWord());
  return CFX_Matrix(f);
}
