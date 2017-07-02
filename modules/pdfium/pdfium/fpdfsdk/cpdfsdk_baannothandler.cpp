// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_baannothandler.h"

#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfdoc/cpdf_interform.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_baannot.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#endif  // PDF_ENABLE_XFA

namespace {

void UpdateAnnotRects(CPDFSDK_PageView* pPageView, CPDFSDK_BAAnnot* pBAAnnot) {
  std::vector<CFX_FloatRect> rects;
  rects.push_back(pBAAnnot->GetRect());
  if (CPDF_Annot* pPopupAnnot = pBAAnnot->GetPDFPopupAnnot())
    rects.push_back(pPopupAnnot->GetRect());

  // Make the rects round up to avoid https://crbug.com/662804
  for (CFX_FloatRect& rect : rects)
    rect.Inflate(1, 1);

  pPageView->UpdateRects(rects);
}

}  // namespace

CPDFSDK_BAAnnotHandler::CPDFSDK_BAAnnotHandler() {}

CPDFSDK_BAAnnotHandler::~CPDFSDK_BAAnnotHandler() {}

bool CPDFSDK_BAAnnotHandler::CanAnswer(CPDFSDK_Annot* pAnnot) {
  return false;
}

CPDFSDK_Annot* CPDFSDK_BAAnnotHandler::NewAnnot(CPDF_Annot* pAnnot,
                                                CPDFSDK_PageView* pPage) {
  return new CPDFSDK_BAAnnot(pAnnot, pPage);
}

#ifdef PDF_ENABLE_XFA
CPDFSDK_Annot* CPDFSDK_BAAnnotHandler::NewAnnot(CXFA_FFWidget* hWidget,
                                                CPDFSDK_PageView* pPage) {
  return nullptr;
}
#endif  // PDF_ENABLE_XFA

void CPDFSDK_BAAnnotHandler::ReleaseAnnot(CPDFSDK_Annot* pAnnot) {
  delete pAnnot;
}

void CPDFSDK_BAAnnotHandler::OnDraw(CPDFSDK_PageView* pPageView,
                                    CPDFSDK_Annot* pAnnot,
                                    CFX_RenderDevice* pDevice,
                                    CFX_Matrix* pUser2Device,
                                    bool bDrawAnnots) {
#ifdef PDF_ENABLE_XFA
  if (pAnnot->IsXFAField())
    return;
#endif  // PDF_ENABLE_XFA
  if (bDrawAnnots && pAnnot->GetAnnotSubtype() == CPDF_Annot::Subtype::POPUP) {
    static_cast<CPDFSDK_BAAnnot*>(pAnnot)->DrawAppearance(
        pDevice, pUser2Device, CPDF_Annot::Normal, nullptr);
  }
}

void CPDFSDK_BAAnnotHandler::OnMouseEnter(CPDFSDK_PageView* pPageView,
                                          CPDFSDK_Annot::ObservedPtr* pAnnot,
                                          uint32_t nFlag) {
  CPDFSDK_BAAnnot* pBAAnnot = static_cast<CPDFSDK_BAAnnot*>(pAnnot->Get());
  pBAAnnot->SetOpenState(true);
  UpdateAnnotRects(pPageView, pBAAnnot);
}

void CPDFSDK_BAAnnotHandler::OnMouseExit(CPDFSDK_PageView* pPageView,
                                         CPDFSDK_Annot::ObservedPtr* pAnnot,
                                         uint32_t nFlag) {
  CPDFSDK_BAAnnot* pBAAnnot = static_cast<CPDFSDK_BAAnnot*>(pAnnot->Get());
  pBAAnnot->SetOpenState(false);
  UpdateAnnotRects(pPageView, pBAAnnot);
}

bool CPDFSDK_BAAnnotHandler::OnLButtonDown(CPDFSDK_PageView* pPageView,
                                           CPDFSDK_Annot::ObservedPtr* pAnnot,
                                           uint32_t nFlags,
                                           const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnLButtonUp(CPDFSDK_PageView* pPageView,
                                         CPDFSDK_Annot::ObservedPtr* pAnnot,
                                         uint32_t nFlags,
                                         const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnLButtonDblClk(CPDFSDK_PageView* pPageView,
                                             CPDFSDK_Annot::ObservedPtr* pAnnot,
                                             uint32_t nFlags,
                                             const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnMouseMove(CPDFSDK_PageView* pPageView,
                                         CPDFSDK_Annot::ObservedPtr* pAnnot,
                                         uint32_t nFlags,
                                         const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnMouseWheel(CPDFSDK_PageView* pPageView,
                                          CPDFSDK_Annot::ObservedPtr* pAnnot,
                                          uint32_t nFlags,
                                          short zDelta,
                                          const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnRButtonDown(CPDFSDK_PageView* pPageView,
                                           CPDFSDK_Annot::ObservedPtr* pAnnot,
                                           uint32_t nFlags,
                                           const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnRButtonUp(CPDFSDK_PageView* pPageView,
                                         CPDFSDK_Annot::ObservedPtr* pAnnot,
                                         uint32_t nFlags,
                                         const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnRButtonDblClk(CPDFSDK_PageView* pPageView,
                                             CPDFSDK_Annot::ObservedPtr* pAnnot,
                                             uint32_t nFlags,
                                             const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnChar(CPDFSDK_Annot* pAnnot,
                                    uint32_t nChar,
                                    uint32_t nFlags) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnKeyDown(CPDFSDK_Annot* pAnnot,
                                       int nKeyCode,
                                       int nFlag) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnKeyUp(CPDFSDK_Annot* pAnnot,
                                     int nKeyCode,
                                     int nFlag) {
  return false;
}

void CPDFSDK_BAAnnotHandler::OnLoad(CPDFSDK_Annot* pAnnot) {}

bool CPDFSDK_BAAnnotHandler::OnSetFocus(CPDFSDK_Annot::ObservedPtr* pAnnot,
                                        uint32_t nFlag) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnKillFocus(CPDFSDK_Annot::ObservedPtr* pAnnot,
                                         uint32_t nFlag) {
  return false;
}

#ifdef PDF_ENABLE_XFA
bool CPDFSDK_BAAnnotHandler::OnXFAChangedFocus(
    CPDFSDK_Annot::ObservedPtr* pOldAnnot,
    CPDFSDK_Annot::ObservedPtr* pNewAnnot) {
  return true;
}
#endif  // PDF_ENABLE_XFA

CFX_FloatRect CPDFSDK_BAAnnotHandler::GetViewBBox(CPDFSDK_PageView* pPageView,
                                                  CPDFSDK_Annot* pAnnot) {
  return pAnnot->GetRect();
}

bool CPDFSDK_BAAnnotHandler::HitTest(CPDFSDK_PageView* pPageView,
                                     CPDFSDK_Annot* pAnnot,
                                     const CFX_PointF& point) {
  ASSERT(pPageView);
  ASSERT(pAnnot);
  return GetViewBBox(pPageView, pAnnot).Contains(point);
}
