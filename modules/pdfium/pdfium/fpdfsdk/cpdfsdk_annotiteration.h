// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_ANNOTITERATION_H_
#define FPDFSDK_CPDFSDK_ANNOTITERATION_H_

#include <vector>

#include "fpdfsdk/cpdfsdk_annot.h"

class CPDFSDK_PageView;

class CPDFSDK_AnnotIteration {
 public:
  using const_iterator =
      std::vector<CPDFSDK_Annot::ObservedPtr>::const_iterator;

  CPDFSDK_AnnotIteration(CPDFSDK_PageView* pPageView, bool bReverse);
  ~CPDFSDK_AnnotIteration();

  const_iterator begin() const { return m_List.begin(); }
  const_iterator end() const { return m_List.end(); }

 private:
  std::vector<CPDFSDK_Annot::ObservedPtr> m_List;
};

#endif  // FPDFSDK_CPDFSDK_ANNOTITERATION_H_
