// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cba_annotiterator.h"

#include <algorithm>

#include "core/fpdfapi/page/cpdf_page.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_pageview.h"

namespace {

CFX_FloatRect GetAnnotRect(const CPDFSDK_Annot* pAnnot) {
  return pAnnot->GetPDFAnnot()->GetRect();
}

bool CompareByLeftAscending(const CPDFSDK_Annot* p1, const CPDFSDK_Annot* p2) {
  return GetAnnotRect(p1).left < GetAnnotRect(p2).left;
}

bool CompareByTopDescending(const CPDFSDK_Annot* p1, const CPDFSDK_Annot* p2) {
  return GetAnnotRect(p1).top > GetAnnotRect(p2).top;
}

}  // namespace

CBA_AnnotIterator::CBA_AnnotIterator(CPDFSDK_PageView* pPageView,
                                     CPDF_Annot::Subtype nAnnotSubtype)
    : m_eTabOrder(STRUCTURE),
      m_pPageView(pPageView),
      m_nAnnotSubtype(nAnnotSubtype) {
  CPDF_Page* pPDFPage = m_pPageView->GetPDFPage();
  CFX_ByteString sTabs = pPDFPage->m_pFormDict->GetStringFor("Tabs");
  if (sTabs == "R")
    m_eTabOrder = ROW;
  else if (sTabs == "C")
    m_eTabOrder = COLUMN;

  GenerateResults();
}

CBA_AnnotIterator::~CBA_AnnotIterator() {}

CPDFSDK_Annot* CBA_AnnotIterator::GetFirstAnnot() {
  return m_Annots.empty() ? nullptr : m_Annots.front();
}

CPDFSDK_Annot* CBA_AnnotIterator::GetLastAnnot() {
  return m_Annots.empty() ? nullptr : m_Annots.back();
}

CPDFSDK_Annot* CBA_AnnotIterator::GetNextAnnot(CPDFSDK_Annot* pAnnot) {
  auto iter = std::find(m_Annots.begin(), m_Annots.end(), pAnnot);
  if (iter == m_Annots.end())
    return nullptr;
  ++iter;
  if (iter == m_Annots.end())
    iter = m_Annots.begin();
  return *iter;
}

CPDFSDK_Annot* CBA_AnnotIterator::GetPrevAnnot(CPDFSDK_Annot* pAnnot) {
  auto iter = std::find(m_Annots.begin(), m_Annots.end(), pAnnot);
  if (iter == m_Annots.end())
    return nullptr;
  if (iter == m_Annots.begin())
    iter = m_Annots.end();
  return *(--iter);
}

void CBA_AnnotIterator::CollectAnnots(std::vector<CPDFSDK_Annot*>* pArray) {
  for (auto pAnnot : m_pPageView->GetAnnotList()) {
    if (pAnnot->GetAnnotSubtype() == m_nAnnotSubtype &&
        !pAnnot->IsSignatureWidget()) {
      pArray->push_back(pAnnot);
    }
  }
}

CFX_FloatRect CBA_AnnotIterator::AddToAnnotsList(
    std::vector<CPDFSDK_Annot*>* sa,
    size_t idx) {
  CPDFSDK_Annot* pLeftTopAnnot = sa->at(idx);
  CFX_FloatRect rcLeftTop = GetAnnotRect(pLeftTopAnnot);
  m_Annots.push_back(pLeftTopAnnot);
  sa->erase(sa->begin() + idx);
  return rcLeftTop;
}

void CBA_AnnotIterator::AddSelectedToAnnots(std::vector<CPDFSDK_Annot*>* sa,
                                            std::vector<size_t>* aSelect) {
  for (size_t i = 0; i < aSelect->size(); ++i)
    m_Annots.push_back(sa->at(aSelect->at(i)));

  for (int i = aSelect->size() - 1; i >= 0; --i)
    sa->erase(sa->begin() + aSelect->at(i));
}

void CBA_AnnotIterator::GenerateResults() {
  switch (m_eTabOrder) {
    case STRUCTURE:
      CollectAnnots(&m_Annots);
      break;

    case ROW: {
      std::vector<CPDFSDK_Annot*> sa;
      CollectAnnots(&sa);
      std::sort(sa.begin(), sa.end(), CompareByLeftAscending);

      while (!sa.empty()) {
        int nLeftTopIndex = -1;
        FX_FLOAT fTop = 0.0f;
        for (int i = sa.size() - 1; i >= 0; i--) {
          CFX_FloatRect rcAnnot = GetAnnotRect(sa[i]);
          if (rcAnnot.top > fTop) {
            nLeftTopIndex = i;
            fTop = rcAnnot.top;
          }
        }
        if (nLeftTopIndex < 0)
          continue;

        CFX_FloatRect rcLeftTop = AddToAnnotsList(&sa, nLeftTopIndex);

        std::vector<size_t> aSelect;
        for (size_t i = 0; i < sa.size(); ++i) {
          CFX_FloatRect rcAnnot = GetAnnotRect(sa[i]);
          FX_FLOAT fCenterY = (rcAnnot.top + rcAnnot.bottom) / 2.0f;
          if (fCenterY > rcLeftTop.bottom && fCenterY < rcLeftTop.top)
            aSelect.push_back(i);
        }
        AddSelectedToAnnots(&sa, &aSelect);
      }
      break;
    }

    case COLUMN: {
      std::vector<CPDFSDK_Annot*> sa;
      CollectAnnots(&sa);
      std::sort(sa.begin(), sa.end(), CompareByTopDescending);

      while (!sa.empty()) {
        int nLeftTopIndex = -1;
        FX_FLOAT fLeft = -1.0f;
        for (int i = sa.size() - 1; i >= 0; --i) {
          CFX_FloatRect rcAnnot = GetAnnotRect(sa[i]);
          if (fLeft < 0) {
            nLeftTopIndex = 0;
            fLeft = rcAnnot.left;
          } else if (rcAnnot.left < fLeft) {
            nLeftTopIndex = i;
            fLeft = rcAnnot.left;
          }
        }
        if (nLeftTopIndex < 0)
          continue;

        CFX_FloatRect rcLeftTop = AddToAnnotsList(&sa, nLeftTopIndex);

        std::vector<size_t> aSelect;
        for (size_t i = 0; i < sa.size(); ++i) {
          CFX_FloatRect rcAnnot = GetAnnotRect(sa[i]);
          FX_FLOAT fCenterX = (rcAnnot.left + rcAnnot.right) / 2.0f;
          if (fCenterX > rcLeftTop.left && fCenterX < rcLeftTop.right)
            aSelect.push_back(i);
        }
        AddSelectedToAnnots(&sa, &aSelect);
      }
      break;
    }
  }
}
