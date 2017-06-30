// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CBA_ANNOTITERATOR_H_
#define FPDFSDK_CBA_ANNOTITERATOR_H_

#include <vector>

#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"

class CPDFSDK_Annot;
class CPDFSDK_PageView;

class CBA_AnnotIterator {
 public:
  enum TabOrder { STRUCTURE = 0, ROW, COLUMN };

  CBA_AnnotIterator(CPDFSDK_PageView* pPageView,
                    CPDF_Annot::Subtype nAnnotSubtype);
  ~CBA_AnnotIterator();

  CPDFSDK_Annot* GetFirstAnnot();
  CPDFSDK_Annot* GetLastAnnot();
  CPDFSDK_Annot* GetNextAnnot(CPDFSDK_Annot* pAnnot);
  CPDFSDK_Annot* GetPrevAnnot(CPDFSDK_Annot* pAnnot);

 private:
  void GenerateResults();
  void CollectAnnots(std::vector<CPDFSDK_Annot*>* pArray);
  CFX_FloatRect AddToAnnotsList(std::vector<CPDFSDK_Annot*>* sa, size_t idx);
  void AddSelectedToAnnots(std::vector<CPDFSDK_Annot*>* sa,
                           std::vector<size_t>* aSelect);

  TabOrder m_eTabOrder;
  CPDFSDK_PageView* m_pPageView;
  CPDF_Annot::Subtype m_nAnnotSubtype;
  std::vector<CPDFSDK_Annot*> m_Annots;
};

#endif  // FPDFSDK_CBA_ANNOTITERATOR_H_
