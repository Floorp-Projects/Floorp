// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_WORDPROPS_H_
#define CORE_FPDFDOC_CPVT_WORDPROPS_H_

#include "core/fpdfdoc/cpdf_variabletext.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_dib.h"

struct CPVT_WordProps {
  CPVT_WordProps()
      : nFontIndex(-1),
        fFontSize(0.0f),
        dwWordColor(0),
        nScriptType(CPDF_VariableText::ScriptType::Normal),
        nWordStyle(0),
        fCharSpace(0.0f),
        nHorzScale(0) {}

  CPVT_WordProps(int32_t fontIndex,
                 FX_FLOAT fontSize,
                 FX_COLORREF wordColor = 0,
                 CPDF_VariableText::ScriptType scriptType =
                     CPDF_VariableText::ScriptType::Normal,
                 int32_t wordStyle = 0,
                 FX_FLOAT charSpace = 0,
                 int32_t horzScale = 100)
      : nFontIndex(fontIndex),
        fFontSize(fontSize),
        dwWordColor(wordColor),
        nScriptType(scriptType),
        nWordStyle(wordStyle),
        fCharSpace(charSpace),
        nHorzScale(horzScale) {}

  CPVT_WordProps(const CPVT_WordProps& other)
      : nFontIndex(other.nFontIndex),
        fFontSize(other.fFontSize),
        dwWordColor(other.dwWordColor),
        nScriptType(other.nScriptType),
        nWordStyle(other.nWordStyle),
        fCharSpace(other.fCharSpace),
        nHorzScale(other.nHorzScale) {}

  int32_t nFontIndex;
  FX_FLOAT fFontSize;
  FX_COLORREF dwWordColor;
  CPDF_VariableText::ScriptType nScriptType;
  int32_t nWordStyle;
  FX_FLOAT fCharSpace;
  int32_t nHorzScale;
};

#endif  // CORE_FPDFDOC_CPVT_WORDPROPS_H_
