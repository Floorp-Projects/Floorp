// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_PAGELABEL_H_
#define CORE_FPDFDOC_CPDF_PAGELABEL_H_

#include "core/fxcrt/fx_string.h"

class CPDF_Document;

class CPDF_PageLabel {
 public:
  explicit CPDF_PageLabel(CPDF_Document* pDocument);

  bool GetLabel(int nPage, CFX_WideString* wsLabel) const;
  int32_t GetPageByLabel(const CFX_ByteStringC& bsLabel) const;
  int32_t GetPageByLabel(const CFX_WideStringC& wsLabel) const;

 private:
  CPDF_Document* const m_pDocument;
};

#endif  // CORE_FPDFDOC_CPDF_PAGELABEL_H_
