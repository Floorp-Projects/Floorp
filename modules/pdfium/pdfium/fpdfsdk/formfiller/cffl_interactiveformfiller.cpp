// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_interactiveformfiller.h"

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interform.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cffl_checkbox.h"
#include "fpdfsdk/formfiller/cffl_combobox.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"
#include "fpdfsdk/formfiller/cffl_listbox.h"
#include "fpdfsdk/formfiller/cffl_pushbutton.h"
#include "fpdfsdk/formfiller/cffl_radiobutton.h"
#include "fpdfsdk/formfiller/cffl_textfield.h"
#include "fpdfsdk/pdfwindow/PWL_Utils.h"

#define FFL_MAXLISTBOXHEIGHT 140.0f

CFFL_InteractiveFormFiller::CFFL_InteractiveFormFiller(
    CPDFSDK_FormFillEnvironment* pFormFillEnv)
    : m_pFormFillEnv(pFormFillEnv), m_bNotifying(false) {}

CFFL_InteractiveFormFiller::~CFFL_InteractiveFormFiller() {}

bool CFFL_InteractiveFormFiller::Annot_HitTest(CPDFSDK_PageView* pPageView,
                                               CPDFSDK_Annot* pAnnot,
                                               const CFX_PointF& point) {
  return pAnnot->GetRect().Contains(point);
}

FX_RECT CFFL_InteractiveFormFiller::GetViewBBox(CPDFSDK_PageView* pPageView,
                                                CPDFSDK_Annot* pAnnot) {
  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot, false))
    return pFormFiller->GetViewBBox(pPageView, pAnnot);

  ASSERT(pPageView);

  CPDF_Annot* pPDFAnnot = pAnnot->GetPDFAnnot();
  CFX_FloatRect rcWin = CPWL_Utils::InflateRect(pPDFAnnot->GetRect(), 1);
  return rcWin.GetOuterRect();
}

void CFFL_InteractiveFormFiller::OnDraw(CPDFSDK_PageView* pPageView,
                                        CPDFSDK_Annot* pAnnot,
                                        CFX_RenderDevice* pDevice,
                                        CFX_Matrix* pUser2Device) {
  ASSERT(pPageView);
  CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;

  if (!IsVisible(pWidget))
    return;

  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot, false)) {
    if (pFormFiller->IsValid()) {
      pFormFiller->OnDraw(pPageView, pAnnot, pDevice, pUser2Device);
      pAnnot->GetPDFPage();

      if (m_pFormFillEnv->GetFocusAnnot() == pAnnot) {
        CFX_FloatRect rcFocus = pFormFiller->GetFocusBox(pPageView);
        if (!rcFocus.IsEmpty()) {
          CFX_PathData path;
          path.AppendPoint(CFX_PointF(rcFocus.left, rcFocus.top),
                           FXPT_TYPE::MoveTo, false);
          path.AppendPoint(CFX_PointF(rcFocus.left, rcFocus.bottom),
                           FXPT_TYPE::LineTo, false);
          path.AppendPoint(CFX_PointF(rcFocus.right, rcFocus.bottom),
                           FXPT_TYPE::LineTo, false);
          path.AppendPoint(CFX_PointF(rcFocus.right, rcFocus.top),
                           FXPT_TYPE::LineTo, false);
          path.AppendPoint(CFX_PointF(rcFocus.left, rcFocus.top),
                           FXPT_TYPE::LineTo, false);

          CFX_GraphStateData gsd;
          gsd.SetDashCount(1);
          gsd.m_DashArray[0] = 1.0f;
          gsd.m_DashPhase = 0;
          gsd.m_LineWidth = 1.0f;
          pDevice->DrawPath(&path, pUser2Device, &gsd, 0,
                            ArgbEncode(255, 0, 0, 0), FXFILL_ALTERNATE);
        }
      }
      return;
    }
  }

  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot, false)) {
    pFormFiller->OnDrawDeactive(pPageView, pAnnot, pDevice, pUser2Device);
  } else {
    pWidget->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Normal, nullptr);
  }

  if (!IsReadOnly(pWidget) && IsFillingAllowed(pWidget))
    pWidget->DrawShadow(pDevice, pPageView);
}

void CFFL_InteractiveFormFiller::OnDelete(CPDFSDK_Annot* pAnnot) {
  UnRegisterFormFiller(pAnnot);
}

void CFFL_InteractiveFormFiller::OnMouseEnter(
    CPDFSDK_PageView* pPageView,
    CPDFSDK_Annot::ObservedPtr* pAnnot,
    uint32_t nFlag) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot->Get());
    if (pWidget->GetAAction(CPDF_AAction::CursorEnter).GetDict()) {
      m_bNotifying = true;

      int nValueAge = pWidget->GetValueAge();
      pWidget->ClearAppModified();
      ASSERT(pPageView);

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);
      pWidget->OnAAction(CPDF_AAction::CursorEnter, fa, pPageView);
      m_bNotifying = false;
      if (!(*pAnnot))
        return;

      if (pWidget->IsAppModified()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false)) {
          pFormFiller->ResetPDFWindow(pPageView,
                                      pWidget->GetValueAge() == nValueAge);
        }
      }
    }
  }
  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), true))
    pFormFiller->OnMouseEnter(pPageView, pAnnot->Get());
}

void CFFL_InteractiveFormFiller::OnMouseExit(CPDFSDK_PageView* pPageView,
                                             CPDFSDK_Annot::ObservedPtr* pAnnot,
                                             uint32_t nFlag) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot->Get());
    if (pWidget->GetAAction(CPDF_AAction::CursorExit).GetDict()) {
      m_bNotifying = true;
      pWidget->GetAppearanceAge();

      int nValueAge = pWidget->GetValueAge();
      pWidget->ClearAppModified();
      ASSERT(pPageView);

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);
      pWidget->OnAAction(CPDF_AAction::CursorExit, fa, pPageView);
      m_bNotifying = false;
      if (!(*pAnnot))
        return;

      if (pWidget->IsAppModified()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false)) {
          pFormFiller->ResetPDFWindow(pPageView,
                                      nValueAge == pWidget->GetValueAge());
        }
      }
    }
  }
  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), false))
    pFormFiller->OnMouseExit(pPageView, pAnnot->Get());
}

bool CFFL_InteractiveFormFiller::OnLButtonDown(
    CPDFSDK_PageView* pPageView,
    CPDFSDK_Annot::ObservedPtr* pAnnot,
    uint32_t nFlags,
    const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot->Get());
    if (Annot_HitTest(pPageView, pAnnot->Get(), point) &&
        pWidget->GetAAction(CPDF_AAction::ButtonDown).GetDict()) {
      m_bNotifying = true;
      pWidget->GetAppearanceAge();

      int nValueAge = pWidget->GetValueAge();
      pWidget->ClearAppModified();
      ASSERT(pPageView);

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlags);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlags);
      pWidget->OnAAction(CPDF_AAction::ButtonDown, fa, pPageView);
      m_bNotifying = false;
      if (!(*pAnnot))
        return true;

      if (!IsValidAnnot(pPageView, pAnnot->Get()))
        return true;

      if (pWidget->IsAppModified()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false)) {
          pFormFiller->ResetPDFWindow(pPageView,
                                      nValueAge == pWidget->GetValueAge());
        }
      }
    }
  }
  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), false))
    return pFormFiller->OnLButtonDown(pPageView, pAnnot->Get(), nFlags, point);

  return false;
}

bool CFFL_InteractiveFormFiller::OnLButtonUp(CPDFSDK_PageView* pPageView,
                                             CPDFSDK_Annot::ObservedPtr* pAnnot,
                                             uint32_t nFlags,
                                             const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot->Get());

  switch (pWidget->GetFieldType()) {
    case FIELDTYPE_PUSHBUTTON:
    case FIELDTYPE_CHECKBOX:
    case FIELDTYPE_RADIOBUTTON:
      if (GetViewBBox(pPageView, pAnnot->Get())
              .Contains((int)point.x, (int)point.y))
        m_pFormFillEnv->SetFocusAnnot(pAnnot);
      break;
    default:
      m_pFormFillEnv->SetFocusAnnot(pAnnot);
      break;
  }

  bool bRet = false;
  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), false))
    bRet = pFormFiller->OnLButtonUp(pPageView, pAnnot->Get(), nFlags, point);

  if (m_pFormFillEnv->GetFocusAnnot() == pAnnot->Get()) {
    bool bExit = false;
    bool bReset = false;
    OnButtonUp(pAnnot, pPageView, bReset, bExit, nFlags);
    if (!pAnnot || bExit)
      return true;
#ifdef PDF_ENABLE_XFA
    OnClick(pWidget, pPageView, bReset, bExit, nFlags);
    if (!pAnnot || bExit)
      return true;
#endif  // PDF_ENABLE_XFA
  }
  return bRet;
}

void CFFL_InteractiveFormFiller::OnButtonUp(CPDFSDK_Annot::ObservedPtr* pAnnot,
                                            CPDFSDK_PageView* pPageView,
                                            bool& bReset,
                                            bool& bExit,
                                            uint32_t nFlag) {
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot->Get());
    if (pWidget->GetAAction(CPDF_AAction::ButtonUp).GetDict()) {
      m_bNotifying = true;

      int nAge = pWidget->GetAppearanceAge();
      int nValueAge = pWidget->GetValueAge();
      ASSERT(pPageView);

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);
      pWidget->OnAAction(CPDF_AAction::ButtonUp, fa, pPageView);
      m_bNotifying = false;
      if (!(*pAnnot) || !IsValidAnnot(pPageView, pWidget)) {
        bExit = true;
        return;
      }
      if (nAge != pWidget->GetAppearanceAge()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false)) {
          pFormFiller->ResetPDFWindow(pPageView,
                                      nValueAge == pWidget->GetValueAge());
        }
        bReset = true;
      }
    }
  }
}

bool CFFL_InteractiveFormFiller::OnLButtonDblClk(
    CPDFSDK_PageView* pPageView,
    CPDFSDK_Annot::ObservedPtr* pAnnot,
    uint32_t nFlags,
    const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), false);
  return pFormFiller &&
         pFormFiller->OnLButtonDblClk(pPageView, pAnnot->Get(), nFlags, point);
}

bool CFFL_InteractiveFormFiller::OnMouseMove(CPDFSDK_PageView* pPageView,
                                             CPDFSDK_Annot::ObservedPtr* pAnnot,
                                             uint32_t nFlags,
                                             const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), true);
  return pFormFiller &&
         pFormFiller->OnMouseMove(pPageView, pAnnot->Get(), nFlags, point);
}

bool CFFL_InteractiveFormFiller::OnMouseWheel(
    CPDFSDK_PageView* pPageView,
    CPDFSDK_Annot::ObservedPtr* pAnnot,
    uint32_t nFlags,
    short zDelta,
    const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), false);
  return pFormFiller &&
         pFormFiller->OnMouseWheel(pPageView, pAnnot->Get(), nFlags, zDelta,
                                   point);
}

bool CFFL_InteractiveFormFiller::OnRButtonDown(
    CPDFSDK_PageView* pPageView,
    CPDFSDK_Annot::ObservedPtr* pAnnot,
    uint32_t nFlags,
    const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), false);
  return pFormFiller &&
         pFormFiller->OnRButtonDown(pPageView, pAnnot->Get(), nFlags, point);
}

bool CFFL_InteractiveFormFiller::OnRButtonUp(CPDFSDK_PageView* pPageView,
                                             CPDFSDK_Annot::ObservedPtr* pAnnot,
                                             uint32_t nFlags,
                                             const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), false);
  return pFormFiller &&
         pFormFiller->OnRButtonUp(pPageView, pAnnot->Get(), nFlags, point);
}

bool CFFL_InteractiveFormFiller::OnKeyDown(CPDFSDK_Annot* pAnnot,
                                           uint32_t nKeyCode,
                                           uint32_t nFlags) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);

  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot, false)) {
    return pFormFiller->OnKeyDown(pAnnot, nKeyCode, nFlags);
  }

  return false;
}

bool CFFL_InteractiveFormFiller::OnChar(CPDFSDK_Annot* pAnnot,
                                        uint32_t nChar,
                                        uint32_t nFlags) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (nChar == FWL_VKEY_Tab)
    return true;

  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot, false))
    return pFormFiller->OnChar(pAnnot, nChar, nFlags);

  return false;
}

bool CFFL_InteractiveFormFiller::OnSetFocus(CPDFSDK_Annot::ObservedPtr* pAnnot,
                                            uint32_t nFlag) {
  if (!(*pAnnot))
    return false;

  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot->Get());
    if (pWidget->GetAAction(CPDF_AAction::GetFocus).GetDict()) {
      m_bNotifying = true;
      pWidget->GetAppearanceAge();

      int nValueAge = pWidget->GetValueAge();
      pWidget->ClearAppModified();

      CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, true);
      if (!pFormFiller)
        return false;

      CPDFSDK_PageView* pPageView = (*pAnnot)->GetPageView();
      ASSERT(pPageView);

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);
      pFormFiller->GetActionData(pPageView, CPDF_AAction::GetFocus, fa);
      pWidget->OnAAction(CPDF_AAction::GetFocus, fa, pPageView);
      m_bNotifying = false;
      if (!(*pAnnot))
        return false;

      if (pWidget->IsAppModified()) {
        if (CFFL_FormFiller* pFiller = GetFormFiller(pWidget, false)) {
          pFiller->ResetPDFWindow(pPageView,
                                  nValueAge == pWidget->GetValueAge());
        }
      }
    }
  }

  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), true))
    pFormFiller->SetFocusForAnnot(pAnnot->Get(), nFlag);

  return true;
}

bool CFFL_InteractiveFormFiller::OnKillFocus(CPDFSDK_Annot::ObservedPtr* pAnnot,
                                             uint32_t nFlag) {
  if (!(*pAnnot))
    return false;

  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get(), false)) {
    pFormFiller->KillFocusForAnnot(pAnnot->Get(), nFlag);
    if (!m_bNotifying) {
      CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot->Get());
      if (pWidget->GetAAction(CPDF_AAction::LoseFocus).GetDict()) {
        m_bNotifying = true;
        pWidget->ClearAppModified();

        CPDFSDK_PageView* pPageView = pWidget->GetPageView();
        ASSERT(pPageView);

        PDFSDK_FieldAction fa;
        fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
        fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);
        pFormFiller->GetActionData(pPageView, CPDF_AAction::LoseFocus, fa);
        pWidget->OnAAction(CPDF_AAction::LoseFocus, fa, pPageView);
        m_bNotifying = false;
        if (!(*pAnnot))
          return false;
      }
    }
  }
  return true;
}

bool CFFL_InteractiveFormFiller::IsVisible(CPDFSDK_Widget* pWidget) {
  return pWidget->IsVisible();
}

bool CFFL_InteractiveFormFiller::IsReadOnly(CPDFSDK_Widget* pWidget) {
  int nFieldFlags = pWidget->GetFieldFlags();
  return (nFieldFlags & FIELDFLAG_READONLY) == FIELDFLAG_READONLY;
}

bool CFFL_InteractiveFormFiller::IsFillingAllowed(CPDFSDK_Widget* pWidget) {
  if (pWidget->GetFieldType() == FIELDTYPE_PUSHBUTTON)
    return true;

  CPDF_Page* pPage = pWidget->GetPDFPage();
  CPDF_Document* pDocument = pPage->m_pDocument;
  uint32_t dwPermissions = pDocument->GetUserPermissions();
  return (dwPermissions & FPDFPERM_FILL_FORM) ||
         (dwPermissions & FPDFPERM_ANNOT_FORM) ||
         (dwPermissions & FPDFPERM_MODIFY);
}

CFFL_FormFiller* CFFL_InteractiveFormFiller::GetFormFiller(
    CPDFSDK_Annot* pAnnot,
    bool bRegister) {
  auto it = m_Maps.find(pAnnot);
  if (it != m_Maps.end())
    return it->second.get();

  if (!bRegister)
    return nullptr;

  CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;
  int nFieldType = pWidget->GetFieldType();
  CFFL_FormFiller* pFormFiller;
  switch (nFieldType) {
    case FIELDTYPE_PUSHBUTTON:
      pFormFiller = new CFFL_PushButton(m_pFormFillEnv, pWidget);
      break;
    case FIELDTYPE_CHECKBOX:
      pFormFiller = new CFFL_CheckBox(m_pFormFillEnv, pWidget);
      break;
    case FIELDTYPE_RADIOBUTTON:
      pFormFiller = new CFFL_RadioButton(m_pFormFillEnv, pWidget);
      break;
    case FIELDTYPE_TEXTFIELD:
      pFormFiller = new CFFL_TextField(m_pFormFillEnv, pWidget);
      break;
    case FIELDTYPE_LISTBOX:
      pFormFiller = new CFFL_ListBox(m_pFormFillEnv, pWidget);
      break;
    case FIELDTYPE_COMBOBOX:
      pFormFiller = new CFFL_ComboBox(m_pFormFillEnv, pWidget);
      break;
    case FIELDTYPE_UNKNOWN:
    default:
      pFormFiller = nullptr;
      break;
  }

  if (!pFormFiller)
    return nullptr;

  m_Maps[pAnnot].reset(pFormFiller);
  return pFormFiller;
}

void CFFL_InteractiveFormFiller::RemoveFormFiller(CPDFSDK_Annot* pAnnot) {
  if (pAnnot)
    UnRegisterFormFiller(pAnnot);
}

void CFFL_InteractiveFormFiller::UnRegisterFormFiller(CPDFSDK_Annot* pAnnot) {
  auto it = m_Maps.find(pAnnot);
  if (it == m_Maps.end())
    return;

  m_Maps.erase(it);
}

void CFFL_InteractiveFormFiller::QueryWherePopup(void* pPrivateData,
                                                 FX_FLOAT fPopupMin,
                                                 FX_FLOAT fPopupMax,
                                                 int32_t& nRet,
                                                 FX_FLOAT& fPopupRet) {
  CFFL_PrivateData* pData = (CFFL_PrivateData*)pPrivateData;

  CFX_FloatRect rcPageView(0, 0, 0, 0);
  rcPageView.right = pData->pWidget->GetPDFPage()->GetPageWidth();
  rcPageView.bottom = pData->pWidget->GetPDFPage()->GetPageHeight();
  rcPageView.Normalize();

  CFX_FloatRect rcAnnot = pData->pWidget->GetRect();

  FX_FLOAT fTop = 0.0f;
  FX_FLOAT fBottom = 0.0f;

  CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pData->pWidget;
  switch (pWidget->GetRotate() / 90) {
    default:
    case 0:
      fTop = rcPageView.top - rcAnnot.top;
      fBottom = rcAnnot.bottom - rcPageView.bottom;
      break;
    case 1:
      fTop = rcAnnot.left - rcPageView.left;
      fBottom = rcPageView.right - rcAnnot.right;
      break;
    case 2:
      fTop = rcAnnot.bottom - rcPageView.bottom;
      fBottom = rcPageView.top - rcAnnot.top;
      break;
    case 3:
      fTop = rcPageView.right - rcAnnot.right;
      fBottom = rcAnnot.left - rcPageView.left;
      break;
  }

  FX_FLOAT fFactHeight = 0;
  bool bBottom = true;
  FX_FLOAT fMaxListBoxHeight = 0;
  if (fPopupMax > FFL_MAXLISTBOXHEIGHT) {
    if (fPopupMin > FFL_MAXLISTBOXHEIGHT) {
      fMaxListBoxHeight = fPopupMin;
    } else {
      fMaxListBoxHeight = FFL_MAXLISTBOXHEIGHT;
    }
  } else {
    fMaxListBoxHeight = fPopupMax;
  }

  if (fBottom > fMaxListBoxHeight) {
    fFactHeight = fMaxListBoxHeight;
    bBottom = true;
  } else {
    if (fTop > fMaxListBoxHeight) {
      fFactHeight = fMaxListBoxHeight;
      bBottom = false;
    } else {
      if (fTop > fBottom) {
        fFactHeight = fTop;
        bBottom = false;
      } else {
        fFactHeight = fBottom;
        bBottom = true;
      }
    }
  }

  nRet = bBottom ? 0 : 1;
  fPopupRet = fFactHeight;
}

void CFFL_InteractiveFormFiller::OnKeyStrokeCommit(
    CPDFSDK_Annot::ObservedPtr* pAnnot,
    CPDFSDK_PageView* pPageView,
    bool& bRC,
    bool& bExit,
    uint32_t nFlag) {
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot->Get());
    if (pWidget->GetAAction(CPDF_AAction::KeyStroke).GetDict()) {
      ASSERT(pPageView);
      m_bNotifying = true;
      pWidget->ClearAppModified();

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);
      fa.bWillCommit = true;
      fa.bKeyDown = true;
      fa.bRC = true;

      CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false);
      pFormFiller->GetActionData(pPageView, CPDF_AAction::KeyStroke, fa);
      pFormFiller->SaveState(pPageView);
      pWidget->OnAAction(CPDF_AAction::KeyStroke, fa, pPageView);
      if (!(*pAnnot))
        return;

      bRC = fa.bRC;
      m_bNotifying = false;
    }
  }
}

void CFFL_InteractiveFormFiller::OnValidate(CPDFSDK_Annot::ObservedPtr* pAnnot,
                                            CPDFSDK_PageView* pPageView,
                                            bool& bRC,
                                            bool& bExit,
                                            uint32_t nFlag) {
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = static_cast<CPDFSDK_Widget*>(pAnnot->Get());
    if (pWidget->GetAAction(CPDF_AAction::Validate).GetDict()) {
      ASSERT(pPageView);
      m_bNotifying = true;
      pWidget->ClearAppModified();

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);
      fa.bKeyDown = true;
      fa.bRC = true;

      CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false);
      pFormFiller->GetActionData(pPageView, CPDF_AAction::Validate, fa);
      pFormFiller->SaveState(pPageView);
      pWidget->OnAAction(CPDF_AAction::Validate, fa, pPageView);
      if (!(*pAnnot))
        return;

      bRC = fa.bRC;
      m_bNotifying = false;
    }
  }
}

void CFFL_InteractiveFormFiller::OnCalculate(CPDFSDK_Widget* pWidget,
                                             CPDFSDK_PageView* pPageView,
                                             bool& bExit,
                                             uint32_t nFlag) {
  if (!m_bNotifying) {
    ASSERT(pWidget);
    CPDFSDK_InterForm* pInterForm = pPageView->GetFormFillEnv()->GetInterForm();
    pInterForm->OnCalculate(pWidget->GetFormField());
    m_bNotifying = false;
  }
}

void CFFL_InteractiveFormFiller::OnFormat(CPDFSDK_Widget* pWidget,
                                          CPDFSDK_PageView* pPageView,
                                          bool& bExit,
                                          uint32_t nFlag) {
  if (!m_bNotifying) {
    ASSERT(pWidget);
    CPDFSDK_InterForm* pInterForm = pPageView->GetFormFillEnv()->GetInterForm();

    bool bFormatted = false;
    CFX_WideString sValue =
        pInterForm->OnFormat(pWidget->GetFormField(), bFormatted);

    if (bExit)
      return;

    if (bFormatted) {
      pInterForm->ResetFieldAppearance(pWidget->GetFormField(), &sValue, true);
      pInterForm->UpdateField(pWidget->GetFormField());
    }

    m_bNotifying = false;
  }
}

#ifdef PDF_ENABLE_XFA
void CFFL_InteractiveFormFiller::OnClick(CPDFSDK_Widget* pWidget,
                                         CPDFSDK_PageView* pPageView,
                                         bool& bReset,
                                         bool& bExit,
                                         uint32_t nFlag) {
  if (!m_bNotifying) {
    if (pWidget->HasXFAAAction(PDFSDK_XFA_Click)) {
      m_bNotifying = true;
      int nAge = pWidget->GetAppearanceAge();
      int nValueAge = pWidget->GetValueAge();

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);

      pWidget->OnXFAAAction(PDFSDK_XFA_Click, fa, pPageView);
      m_bNotifying = false;

      if (!IsValidAnnot(pPageView, pWidget)) {
        bExit = true;
        return;
      }

      if (nAge != pWidget->GetAppearanceAge()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false)) {
          pFormFiller->ResetPDFWindow(pPageView,
                                      nValueAge == pWidget->GetValueAge());
        }

        bReset = true;
      }
    }
  }
}

void CFFL_InteractiveFormFiller::OnFull(CPDFSDK_Widget* pWidget,
                                        CPDFSDK_PageView* pPageView,
                                        bool& bReset,
                                        bool& bExit,
                                        uint32_t nFlag) {
  if (!m_bNotifying) {
    if (pWidget->HasXFAAAction(PDFSDK_XFA_Full)) {
      m_bNotifying = true;
      int nAge = pWidget->GetAppearanceAge();
      int nValueAge = pWidget->GetValueAge();

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);

      pWidget->OnXFAAAction(PDFSDK_XFA_Full, fa, pPageView);
      m_bNotifying = false;

      if (!IsValidAnnot(pPageView, pWidget)) {
        bExit = true;
        return;
      }

      if (nAge != pWidget->GetAppearanceAge()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false)) {
          pFormFiller->ResetPDFWindow(pPageView,
                                      nValueAge == pWidget->GetValueAge());
        }

        bReset = true;
      }
    }
  }
}

void CFFL_InteractiveFormFiller::OnPopupPreOpen(void* pPrivateData,
                                                bool& bExit,
                                                uint32_t nFlag) {
  CFFL_PrivateData* pData = (CFFL_PrivateData*)pPrivateData;
  ASSERT(pData);
  ASSERT(pData->pWidget);

  bool bTempReset = false;
  bool bTempExit = false;
  OnPreOpen(pData->pWidget, pData->pPageView, bTempReset, bTempExit, nFlag);
  if (bTempReset || bTempExit)
    bExit = true;
}

void CFFL_InteractiveFormFiller::OnPopupPostOpen(void* pPrivateData,
                                                 bool& bExit,
                                                 uint32_t nFlag) {
  CFFL_PrivateData* pData = (CFFL_PrivateData*)pPrivateData;
  ASSERT(pData);
  ASSERT(pData->pWidget);

  bool bTempReset = false;
  bool bTempExit = false;
  OnPostOpen(pData->pWidget, pData->pPageView, bTempReset, bTempExit, nFlag);
  if (bTempReset || bTempExit)
    bExit = true;
}

void CFFL_InteractiveFormFiller::OnPreOpen(CPDFSDK_Widget* pWidget,
                                           CPDFSDK_PageView* pPageView,
                                           bool& bReset,
                                           bool& bExit,
                                           uint32_t nFlag) {
  if (!m_bNotifying) {
    if (pWidget->HasXFAAAction(PDFSDK_XFA_PreOpen)) {
      m_bNotifying = true;
      int nAge = pWidget->GetAppearanceAge();
      int nValueAge = pWidget->GetValueAge();

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);

      pWidget->OnXFAAAction(PDFSDK_XFA_PreOpen, fa, pPageView);
      m_bNotifying = false;

      if (!IsValidAnnot(pPageView, pWidget)) {
        bExit = true;
        return;
      }

      if (nAge != pWidget->GetAppearanceAge()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false)) {
          pFormFiller->ResetPDFWindow(pPageView,
                                      nValueAge == pWidget->GetValueAge());
        }

        bReset = true;
      }
    }
  }
}

void CFFL_InteractiveFormFiller::OnPostOpen(CPDFSDK_Widget* pWidget,
                                            CPDFSDK_PageView* pPageView,
                                            bool& bReset,
                                            bool& bExit,
                                            uint32_t nFlag) {
  if (!m_bNotifying) {
    if (pWidget->HasXFAAAction(PDFSDK_XFA_PostOpen)) {
      m_bNotifying = true;
      int nAge = pWidget->GetAppearanceAge();
      int nValueAge = pWidget->GetValueAge();

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);

      pWidget->OnXFAAAction(PDFSDK_XFA_PostOpen, fa, pPageView);
      m_bNotifying = false;

      if (!IsValidAnnot(pPageView, pWidget)) {
        bExit = true;
        return;
      }

      if (nAge != pWidget->GetAppearanceAge()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget, false)) {
          pFormFiller->ResetPDFWindow(pPageView,
                                      nValueAge == pWidget->GetValueAge());
        }

        bReset = true;
      }
    }
  }
}
#endif  // PDF_ENABLE_XFA

bool CFFL_InteractiveFormFiller::IsValidAnnot(CPDFSDK_PageView* pPageView,
                                              CPDFSDK_Annot* pAnnot) {
  return pPageView && pPageView->IsValidAnnot(pAnnot->GetPDFAnnot());
}

void CFFL_InteractiveFormFiller::OnBeforeKeyStroke(
    void* pPrivateData,
    CFX_WideString& strChange,
    const CFX_WideString& strChangeEx,
    int nSelStart,
    int nSelEnd,
    bool bKeyDown,
    bool& bRC,
    bool& bExit,
    uint32_t nFlag) {
  CFFL_PrivateData* pData = (CFFL_PrivateData*)pPrivateData;
  ASSERT(pData->pWidget);

  CFFL_FormFiller* pFormFiller = GetFormFiller(pData->pWidget, false);

#ifdef PDF_ENABLE_XFA
  if (pFormFiller->IsFieldFull(pData->pPageView)) {
    bool bFullExit = false;
    bool bFullReset = false;
    OnFull(pData->pWidget, pData->pPageView, bFullReset, bFullExit, nFlag);

    if (bFullReset || bFullExit) {
      bExit = true;
      return;
    }
  }
#endif  // PDF_ENABLE_XFA

  if (!m_bNotifying) {
    if (pData->pWidget->GetAAction(CPDF_AAction::KeyStroke).GetDict()) {
      m_bNotifying = true;
      int nAge = pData->pWidget->GetAppearanceAge();
      int nValueAge = pData->pWidget->GetValueAge();

      CPDFSDK_FormFillEnvironment* pFormFillEnv =
          pData->pPageView->GetFormFillEnv();

      PDFSDK_FieldAction fa;
      fa.bModifier = m_pFormFillEnv->IsCTRLKeyDown(nFlag);
      fa.bShift = m_pFormFillEnv->IsSHIFTKeyDown(nFlag);
      fa.sChange = strChange;
      fa.sChangeEx = strChangeEx;
      fa.bKeyDown = bKeyDown;
      fa.bWillCommit = false;
      fa.bRC = true;
      fa.nSelStart = nSelStart;
      fa.nSelEnd = nSelEnd;

      pFormFiller->GetActionData(pData->pPageView, CPDF_AAction::KeyStroke, fa);
      pFormFiller->SaveState(pData->pPageView);

      CPDFSDK_Annot::ObservedPtr pObserved(pData->pWidget);
      if (pData->pWidget->OnAAction(CPDF_AAction::KeyStroke, fa,
                                    pData->pPageView)) {
        if (!pObserved || !IsValidAnnot(pData->pPageView, pData->pWidget)) {
          bExit = true;
          m_bNotifying = false;
          return;
        }

        if (nAge != pData->pWidget->GetAppearanceAge()) {
          CPWL_Wnd* pWnd = pFormFiller->ResetPDFWindow(
              pData->pPageView, nValueAge == pData->pWidget->GetValueAge());
          pData = (CFFL_PrivateData*)pWnd->GetAttachedData();
          bExit = true;
        }

        if (fa.bRC) {
          pFormFiller->SetActionData(pData->pPageView, CPDF_AAction::KeyStroke,
                                     fa);
          bRC = false;
        } else {
          pFormFiller->RestoreState(pData->pPageView);
          bRC = false;
        }

        if (pFormFillEnv->GetFocusAnnot() != pData->pWidget) {
          pFormFiller->CommitData(pData->pPageView, nFlag);
          bExit = true;
        }
      } else {
        if (!IsValidAnnot(pData->pPageView, pData->pWidget)) {
          bExit = true;
          m_bNotifying = false;
          return;
        }
      }

      m_bNotifying = false;
    }
  }
}
