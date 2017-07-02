// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_SECTIONINFO_H_
#define CORE_FPDFDOC_CPVT_SECTIONINFO_H_

#include <memory>

#include "core/fpdfdoc/cpvt_floatrect.h"
#include "core/fpdfdoc/cpvt_secprops.h"
#include "core/fpdfdoc/cpvt_wordprops.h"

struct CPVT_SectionInfo {
  CPVT_SectionInfo();
  CPVT_SectionInfo(const CPVT_SectionInfo& other);
  ~CPVT_SectionInfo();

  void operator=(const CPVT_SectionInfo& other);

  CPVT_FloatRect rcSection;
  int32_t nTotalLine;
  std::unique_ptr<CPVT_SecProps> pSecProps;
  std::unique_ptr<CPVT_WordProps> pWordProps;
};

#endif  // CORE_FPDFDOC_CPVT_SECTIONINFO_H_
