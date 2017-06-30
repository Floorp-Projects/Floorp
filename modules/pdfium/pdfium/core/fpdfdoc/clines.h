// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CLINES_H_
#define CORE_FPDFDOC_CLINES_H_

#include <stdint.h>

#include "core/fpdfdoc/cpvt_arraytemplate.h"
#include "core/fpdfdoc/cpvt_lineinfo.h"

class CLine;

class CLines final {
 public:
  CLines();
  ~CLines();

  int32_t GetSize() const;
  CLine* GetAt(int32_t nIndex) const;

  void Empty();
  void RemoveAll();
  int32_t Add(const CPVT_LineInfo& lineinfo);
  void Clear();

 private:
  CPVT_ArrayTemplate<CLine*> m_Lines;
  int32_t m_nTotal;
};

#endif  // CORE_FPDFDOC_CLINES_H_
