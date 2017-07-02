// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FX_ARB_H_
#define CORE_FXCRT_FX_ARB_H_

#include <vector>

#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/fx_ucd.h"

struct FX_ARBFORMTABLE {
  uint16_t wIsolated;
  uint16_t wFinal;
  uint16_t wInitial;
  uint16_t wMedial;
};

struct FX_ARAALEF {
  uint16_t wAlef;
  uint16_t wIsolated;
};

struct FX_ARASHADDA {
  uint16_t wShadda;
  uint16_t wIsolated;
};

const FX_ARBFORMTABLE* FX_GetArabicFormTable(FX_WCHAR unicode);
FX_WCHAR FX_GetArabicFromAlefTable(FX_WCHAR alef);
FX_WCHAR FX_GetArabicFromShaddaTable(FX_WCHAR shadda);

enum FX_ARBPOSITION {
  FX_ARBPOSITION_Isolated = 0,
  FX_ARBPOSITION_Final,
  FX_ARBPOSITION_Initial,
  FX_ARBPOSITION_Medial,
};

void FX_BidiLine(CFX_WideString& wsText, int32_t iBaseLevel = 0);
void FX_BidiLine(std::vector<CFX_TxtChar>& chars,
                 int32_t iCount,
                 int32_t iBaseLevel = 0);
void FX_BidiLine(std::vector<CFX_RTFChar>& chars,
                 int32_t iCount,
                 int32_t iBaseLevel = 0);

#endif  // CORE_FXCRT_FX_ARB_H_
