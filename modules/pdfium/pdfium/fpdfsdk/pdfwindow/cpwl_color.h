// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PDFWINDOW_CPWL_COLOR_H_
#define FPDFSDK_PDFWINDOW_CPWL_COLOR_H_

#include "core/fpdfdoc/cpdf_formcontrol.h"

struct CPWL_Color {
  CPWL_Color(int32_t type = COLORTYPE_TRANSPARENT,
             FX_FLOAT color1 = 0.0f,
             FX_FLOAT color2 = 0.0f,
             FX_FLOAT color3 = 0.0f,
             FX_FLOAT color4 = 0.0f)
      : nColorType(type),
        fColor1(color1),
        fColor2(color2),
        fColor3(color3),
        fColor4(color4) {}

  CPWL_Color(int32_t r, int32_t g, int32_t b)
      : nColorType(COLORTYPE_RGB),
        fColor1(r / 255.0f),
        fColor2(g / 255.0f),
        fColor3(b / 255.0f),
        fColor4(0) {}

  CPWL_Color operator/(FX_FLOAT fColorDivide) const;
  CPWL_Color operator-(FX_FLOAT fColorSub) const;

  CPWL_Color ConvertColorType(int32_t other_nColorType) const;

  FX_COLORREF ToFXColor(int32_t nTransparency) const;

  void Reset() {
    nColorType = COLORTYPE_TRANSPARENT;
    fColor1 = 0.0f;
    fColor2 = 0.0f;
    fColor3 = 0.0f;
    fColor4 = 0.0f;
  }

  int32_t nColorType;
  FX_FLOAT fColor1;
  FX_FLOAT fColor2;
  FX_FLOAT fColor3;
  FX_FLOAT fColor4;
};

#endif  // FPDFSDK_PDFWINDOW_CPWL_COLOR_H_
