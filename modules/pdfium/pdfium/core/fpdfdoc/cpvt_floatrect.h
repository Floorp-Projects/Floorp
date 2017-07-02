// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_FLOATRECT_H_
#define CORE_FPDFDOC_CPVT_FLOATRECT_H_

#include "core/fxcrt/fx_coordinates.h"

class CPVT_FloatRect : public CFX_FloatRect {
 public:
  CPVT_FloatRect() { left = top = right = bottom = 0.0f; }

  CPVT_FloatRect(FX_FLOAT other_left,
                 FX_FLOAT other_top,
                 FX_FLOAT other_right,
                 FX_FLOAT other_bottom) {
    left = other_left;
    top = other_top;
    right = other_right;
    bottom = other_bottom;
  }

  explicit CPVT_FloatRect(const CFX_FloatRect& rect) {
    left = rect.left;
    top = rect.top;
    right = rect.right;
    bottom = rect.bottom;
  }

  void Default() { left = top = right = bottom = 0.0f; }

  FX_FLOAT Height() const {
    if (top > bottom)
      return top - bottom;
    return bottom - top;
  }
};

#endif  // CORE_FPDFDOC_CPVT_FLOATRECT_H_
