// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpvt_color.h"

#include "core/fpdfapi/parser/cpdf_simple_parser.h"

// Static.
CPVT_Color CPVT_Color::ParseColor(const CFX_ByteString& str) {
  CPDF_SimpleParser syntax(str.AsStringC());
  if (syntax.FindTagParamFromStart("g", 1))
    return CPVT_Color(CPVT_Color::kGray, FX_atof(syntax.GetWord()));

  if (syntax.FindTagParamFromStart("rg", 3)) {
    FX_FLOAT f1 = FX_atof(syntax.GetWord());
    FX_FLOAT f2 = FX_atof(syntax.GetWord());
    FX_FLOAT f3 = FX_atof(syntax.GetWord());
    return CPVT_Color(CPVT_Color::kRGB, f1, f2, f3);
  }
  if (syntax.FindTagParamFromStart("k", 4)) {
    FX_FLOAT f1 = FX_atof(syntax.GetWord());
    FX_FLOAT f2 = FX_atof(syntax.GetWord());
    FX_FLOAT f3 = FX_atof(syntax.GetWord());
    FX_FLOAT f4 = FX_atof(syntax.GetWord());
    return CPVT_Color(CPVT_Color::kCMYK, f1, f2, f3, f4);
  }
  return CPVT_Color(CPVT_Color::kTransparent);
}

// Static.
CPVT_Color CPVT_Color::ParseColor(const CPDF_Array& array) {
  CPVT_Color rt;
  switch (array.GetCount()) {
    case 1:
      rt = CPVT_Color(CPVT_Color::kGray, array.GetFloatAt(0));
      break;
    case 3:
      rt = CPVT_Color(CPVT_Color::kRGB, array.GetFloatAt(0),
                      array.GetFloatAt(1), array.GetFloatAt(2));
      break;
    case 4:
      rt = CPVT_Color(CPVT_Color::kCMYK, array.GetFloatAt(0),
                      array.GetFloatAt(1), array.GetFloatAt(2),
                      array.GetFloatAt(3));
      break;
  }
  return rt;
}
