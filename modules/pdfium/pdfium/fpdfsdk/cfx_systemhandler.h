// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CFX_SYSTEMHANDLER_H_
#define FPDFSDK_CFX_SYSTEMHANDLER_H_

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

using TimerCallback = void (*)(int32_t idEvent);

struct FX_SYSTEMTIME {
  FX_SYSTEMTIME()
      : wYear(0),
        wMonth(0),
        wDayOfWeek(0),
        wDay(0),
        wHour(0),
        wMinute(0),
        wSecond(0),
        wMilliseconds(0) {}

  uint16_t wYear;
  uint16_t wMonth;
  uint16_t wDayOfWeek;
  uint16_t wDay;
  uint16_t wHour;
  uint16_t wMinute;
  uint16_t wSecond;
  uint16_t wMilliseconds;
};

// Cursor style. These must match the values in public/fpdf_formfill.h
#define FXCT_ARROW 0
#define FXCT_NESW 1
#define FXCT_NWSE 2
#define FXCT_VBEAM 3
#define FXCT_HBEAM 4
#define FXCT_HAND 5

class CFFL_FormFiller;
class CPDF_Document;
class CPDF_Font;
class CPDFSDK_FormFillEnvironment;
class CPDFSDK_Widget;

class CFX_SystemHandler {
 public:
  explicit CFX_SystemHandler(CPDFSDK_FormFillEnvironment* pFormFillEnv)
      : m_pFormFillEnv(pFormFillEnv) {}
  ~CFX_SystemHandler() {}

  void InvalidateRect(CPDFSDK_Widget* widget, FX_RECT rect);
  void OutputSelectedRect(CFFL_FormFiller* pFormFiller, CFX_FloatRect& rect);
  bool IsSelectionImplemented() const;

  void SetCursor(int32_t nCursorType);

  bool FindNativeTrueTypeFont(CFX_ByteString sFontFaceName);
  CPDF_Font* AddNativeTrueTypeFontToPDF(CPDF_Document* pDoc,
                                        CFX_ByteString sFontFaceName,
                                        uint8_t nCharset);

  int32_t SetTimer(int32_t uElapse, TimerCallback lpTimerFunc);
  void KillTimer(int32_t nID);

  bool IsSHIFTKeyDown(uint32_t nFlag) const;
  bool IsCTRLKeyDown(uint32_t nFlag) const;
  bool IsALTKeyDown(uint32_t nFlag) const;

 private:
  CPDFSDK_FormFillEnvironment* const m_pFormFillEnv;
};

#endif  // FPDFSDK_CFX_SYSTEMHANDLER_H_
