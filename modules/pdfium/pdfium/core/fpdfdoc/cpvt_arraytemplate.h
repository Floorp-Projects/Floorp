// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_ARRAYTEMPLATE_H_
#define CORE_FPDFDOC_CPVT_ARRAYTEMPLATE_H_

#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_system.h"

template <class TYPE>
class CPVT_ArrayTemplate : public CFX_ArrayTemplate<TYPE> {
 public:
  bool IsEmpty() { return CFX_ArrayTemplate<TYPE>::GetSize() <= 0; }

  TYPE GetAt(int nIndex) const {
    if (nIndex >= 0 && nIndex < CFX_ArrayTemplate<TYPE>::GetSize())
      return CFX_ArrayTemplate<TYPE>::GetAt(nIndex);
    return nullptr;
  }

  void RemoveAt(int nIndex) {
    if (nIndex >= 0 && nIndex < CFX_ArrayTemplate<TYPE>::GetSize())
      CFX_ArrayTemplate<TYPE>::RemoveAt(nIndex);
  }
};

#endif  // CORE_FPDFDOC_CPVT_ARRAYTEMPLATE_H_
