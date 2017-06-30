// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_DASH_H_
#define CORE_FPDFDOC_CPVT_DASH_H_

#include <stdint.h>

struct CPVT_Dash {
  CPVT_Dash(int32_t dash, int32_t gap, int32_t phase)
      : nDash(dash), nGap(gap), nPhase(phase) {}

  int32_t nDash;
  int32_t nGap;
  int32_t nPhase;
};

#endif  // CORE_FPDFDOC_CPVT_DASH_H_
