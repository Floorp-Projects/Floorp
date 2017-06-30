// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_WIN32_CPSOUTPUT_H_
#define CORE_FXGE_WIN32_CPSOUTPUT_H_

#include <windows.h>

#include "core/fxcrt/fx_system.h"

class CPSOutput {
 public:
  explicit CPSOutput(HDC hDC);
  ~CPSOutput();

  // IFX_PSOutput
  void Release();
  void OutputPS(const FX_CHAR* str, int len);

  HDC m_hDC;
};

#endif  // CORE_FXGE_WIN32_CPSOUTPUT_H_
