// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_annotiteration.h"

#include <algorithm>
#include <utility>

#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_pageview.h"

CPDFSDK_AnnotIteration::CPDFSDK_AnnotIteration(CPDFSDK_PageView* pPageView,
                                               bool bReverse) {
  // Copying/sorting ObservedPtrs is expensive, so do it once at the end.
  std::vector<CPDFSDK_Annot*> copiedList = pPageView->GetAnnotList();
  std::stable_sort(copiedList.begin(), copiedList.end(),
                   [](const CPDFSDK_Annot* p1, const CPDFSDK_Annot* p2) {
                     return p1->GetLayoutOrder() < p2->GetLayoutOrder();
                   });

  CPDFSDK_Annot* pTopMostAnnot = pPageView->GetFocusAnnot();
  if (pTopMostAnnot) {
    auto it = std::find(copiedList.begin(), copiedList.end(), pTopMostAnnot);
    if (it != copiedList.end()) {
      copiedList.erase(it);
      copiedList.insert(copiedList.begin(), pTopMostAnnot);
    }
  }
  if (bReverse)
    std::reverse(copiedList.begin(), copiedList.end());

  m_List.reserve(copiedList.size());
  for (const auto& pAnnot : copiedList)
    m_List.emplace_back(pAnnot);
}

CPDFSDK_AnnotIteration::~CPDFSDK_AnnotIteration() {}
