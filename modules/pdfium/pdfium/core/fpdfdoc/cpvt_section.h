// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_SECTION_H_
#define CORE_FPDFDOC_CPVT_SECTION_H_

#include "core/fpdfdoc/cpvt_secprops.h"
#include "core/fpdfdoc/cpvt_wordplace.h"
#include "core/fpdfdoc/cpvt_wordprops.h"
#include "core/fxcrt/fx_coordinates.h"

struct CPVT_Section {
  CPVT_WordPlace secplace;
  CFX_FloatRect rcSection;
  CPVT_SecProps SecProps;
  CPVT_WordProps WordProps;
};

#endif  // CORE_FPDFDOC_CPVT_SECTION_H_
