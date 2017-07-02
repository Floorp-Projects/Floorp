// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_radiobutton.h"

#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"
#include "fpdfsdk/pdfwindow/PWL_SpecialButton.h"
#include "public/fpdf_fwlevent.h"

CFFL_RadioButton::CFFL_RadioButton(CPDFSDK_FormFillEnvironment* pApp,
                                   CPDFSDK_Annot* pWidget)
    : CFFL_Button(pApp, pWidget) {}

CFFL_RadioButton::~CFFL_RadioButton() {}

CPWL_Wnd* CFFL_RadioButton::NewPDFWindow(const PWL_CREATEPARAM& cp,
                                         CPDFSDK_PageView* pPageView) {
  CPWL_RadioButton* pWnd = new CPWL_RadioButton();
  pWnd->Create(cp);

  pWnd->SetCheck(m_pWidget->IsChecked());

  return pWnd;
}

bool CFFL_RadioButton::OnKeyDown(CPDFSDK_Annot* pAnnot,
                                 uint32_t nKeyCode,
                                 uint32_t nFlags) {
  switch (nKeyCode) {
    case FWL_VKEY_Return:
    case FWL_VKEY_Space:
      return true;
    default:
      return CFFL_FormFiller::OnKeyDown(pAnnot, nKeyCode, nFlags);
  }
}

bool CFFL_RadioButton::OnChar(CPDFSDK_Annot* pAnnot,
                              uint32_t nChar,
                              uint32_t nFlags) {
  switch (nChar) {
    case FWL_VKEY_Return:
    case FWL_VKEY_Space: {
      CPDFSDK_PageView* pPageView = pAnnot->GetPageView();
      ASSERT(pPageView);

      bool bReset = false;
      bool bExit = false;
      CPDFSDK_Annot::ObservedPtr pObserved(m_pWidget);
      m_pFormFillEnv->GetInteractiveFormFiller()->OnButtonUp(
          &pObserved, pPageView, bReset, bExit, nFlags);
      if (!pObserved || bReset || bExit)
        return true;

      CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
      if (CPWL_RadioButton* pWnd =
              (CPWL_RadioButton*)GetPDFWindow(pPageView, true))
        pWnd->SetCheck(true);
      CommitData(pPageView, nFlags);
      return true;
    }
    default:
      return CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
  }
}

bool CFFL_RadioButton::OnLButtonUp(CPDFSDK_PageView* pPageView,
                                   CPDFSDK_Annot* pAnnot,
                                   uint32_t nFlags,
                                   const CFX_PointF& point) {
  CFFL_Button::OnLButtonUp(pPageView, pAnnot, nFlags, point);

  if (IsValid()) {
    if (CPWL_RadioButton* pWnd =
            (CPWL_RadioButton*)GetPDFWindow(pPageView, true))
      pWnd->SetCheck(true);

    if (!CommitData(pPageView, nFlags))
      return false;
  }

  return true;
}

bool CFFL_RadioButton::IsDataChanged(CPDFSDK_PageView* pPageView) {
  if (CPWL_RadioButton* pWnd =
          (CPWL_RadioButton*)GetPDFWindow(pPageView, false)) {
    return pWnd->IsChecked() != m_pWidget->IsChecked();
  }

  return false;
}

void CFFL_RadioButton::SaveData(CPDFSDK_PageView* pPageView) {
  if (CPWL_RadioButton* pWnd =
          (CPWL_RadioButton*)GetPDFWindow(pPageView, false)) {
    bool bNewChecked = pWnd->IsChecked();

    if (bNewChecked) {
      CPDF_FormField* pField = m_pWidget->GetFormField();
      for (int32_t i = 0, sz = pField->CountControls(); i < sz; i++) {
        if (CPDF_FormControl* pCtrl = pField->GetControl(i)) {
          if (pCtrl->IsChecked()) {
            break;
          }
        }
      }
    }

    m_pWidget->SetCheck(bNewChecked, false);
    m_pWidget->UpdateField();
    SetChangeMark();
  }
}
