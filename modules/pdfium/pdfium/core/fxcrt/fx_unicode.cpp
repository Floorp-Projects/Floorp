// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/fx_ucd.h"

uint32_t FX_GetUnicodeProperties(FX_WCHAR wch) {
  size_t idx = static_cast<size_t>(wch);
  if (idx < kTextLayoutCodePropertiesSize)
    return kTextLayoutCodeProperties[(uint16_t)wch];
  return 0;
}

#ifdef PDF_ENABLE_XFA
bool FX_IsCtrlCode(FX_WCHAR ch) {
  uint32_t dwRet = (FX_GetUnicodeProperties(ch) & FX_CHARTYPEBITSMASK);
  return dwRet == FX_CHARTYPE_Tab || dwRet == FX_CHARTYPE_Control;
}
#endif  // PDF_ENABLE_XFA

FX_WCHAR FX_GetMirrorChar(FX_WCHAR wch, bool bRTL, bool bVertical) {
  uint32_t dwProps = FX_GetUnicodeProperties(wch);
  uint32_t dwTemp = (dwProps & 0xFF800000);
  if (bRTL && dwTemp < 0xFF800000) {
    size_t idx = dwTemp >> 23;
    if (idx < kFXTextLayoutBidiMirrorSize) {
      wch = kFXTextLayoutBidiMirror[idx];
      dwProps = FX_GetUnicodeProperties(wch);
    }
  }
  if (bVertical) {
    dwTemp = (dwProps & 0x007E0000);
    if (dwTemp < 0x007E0000) {
      size_t idx = dwTemp >> 17;
      if (idx < kFXTextLayoutVerticalMirrorSize)
        wch = kFXTextLayoutVerticalMirror[idx];
    }
  }
  return wch;
}

#ifdef PDF_ENABLE_XFA
FX_WCHAR FX_GetMirrorChar(FX_WCHAR wch,
                          uint32_t dwProps,
                          bool bRTL,
                          bool bVertical) {
  uint32_t dwTemp = (dwProps & 0xFF800000);
  if (bRTL && dwTemp < 0xFF800000) {
    size_t idx = dwTemp >> 23;
    if (idx < kFXTextLayoutBidiMirrorSize) {
      wch = kFXTextLayoutBidiMirror[idx];
      dwProps = FX_GetUnicodeProperties(wch);
    }
  }
  if (bVertical) {
    dwTemp = (dwProps & 0x007E0000);
    if (dwTemp < 0x007E0000) {
      size_t idx = dwTemp >> 17;
      if (idx < kFXTextLayoutVerticalMirrorSize)
        wch = kFXTextLayoutVerticalMirror[idx];
    }
  }
  return wch;
}
#endif  // PDF_ENABLE_XFA
