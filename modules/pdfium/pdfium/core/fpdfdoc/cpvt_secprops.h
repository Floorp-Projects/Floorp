// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_SECPROPS_H_
#define CORE_FPDFDOC_CPVT_SECPROPS_H_

#include "core/fxcrt/fx_system.h"

struct CPVT_SecProps {
  CPVT_SecProps() : fLineLeading(0.0f), fLineIndent(0.0f), nAlignment(0) {}

  CPVT_SecProps(FX_FLOAT lineLeading, FX_FLOAT lineIndent, int32_t alignment)
      : fLineLeading(lineLeading),
        fLineIndent(lineIndent),
        nAlignment(alignment) {}

  CPVT_SecProps(const CPVT_SecProps& other)
      : fLineLeading(other.fLineLeading),
        fLineIndent(other.fLineIndent),
        nAlignment(other.nAlignment) {}

  FX_FLOAT fLineLeading;
  FX_FLOAT fLineIndent;
  int32_t nAlignment;
};

#endif  // CORE_FPDFDOC_CPVT_SECPROPS_H_
