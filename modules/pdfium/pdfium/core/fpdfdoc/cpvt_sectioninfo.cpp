// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpvt_sectioninfo.h"
#include "third_party/base/ptr_util.h"

CPVT_SectionInfo::CPVT_SectionInfo() : rcSection(), nTotalLine(0) {}

CPVT_SectionInfo::~CPVT_SectionInfo() {}

CPVT_SectionInfo::CPVT_SectionInfo(const CPVT_SectionInfo& other) {
  operator=(other);
}

void CPVT_SectionInfo::operator=(const CPVT_SectionInfo& other) {
  if (this == &other)
    return;

  rcSection = other.rcSection;
  nTotalLine = other.nTotalLine;
  if (other.pSecProps)
    pSecProps = pdfium::MakeUnique<CPVT_SecProps>(*other.pSecProps);
  else
    pSecProps.reset();

  if (other.pWordProps)
    pWordProps = pdfium::MakeUnique<CPVT_WordProps>(*other.pWordProps);
  else
    pWordProps.reset();
}
