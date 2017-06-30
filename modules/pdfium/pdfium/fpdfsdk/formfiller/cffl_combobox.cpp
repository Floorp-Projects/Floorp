// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_combobox.h"

#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cba_fontmap.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"
#include "fpdfsdk/formfiller/cffl_interactiveformfiller.h"
#include "fpdfsdk/fsdk_common.h"
#include "fpdfsdk/pdfwindow/PWL_ComboBox.h"

CFFL_ComboBox::CFFL_ComboBox(CPDFSDK_FormFillEnvironment* pApp,
                             CPDFSDK_Annot* pAnnot)
    : CFFL_FormFiller(pApp, pAnnot), m_pFontMap(nullptr) {
  m_State.nIndex = 0;
  m_State.nStart = 0;
  m_State.nEnd = 0;
}

CFFL_ComboBox::~CFFL_ComboBox() {
  for (const auto& it : m_Maps)
    it.second->InvalidateFocusHandler(this);

  // See comment in cffl_formfiller.h.
  // The font map should be stored somewhere more appropriate so it will live
  // until the PWL_Edit is done with it. pdfium:566
  DestroyWindows();
  delete m_pFontMap;
}

PWL_CREATEPARAM CFFL_ComboBox::GetCreateParam() {
  PWL_CREATEPARAM cp = CFFL_FormFiller::GetCreateParam();

  int nFlags = m_pWidget->GetFieldFlags();
  if (nFlags & FIELDFLAG_EDIT) {
    cp.dwFlags |= PCBS_ALLOWCUSTOMTEXT;
  }

  if (!m_pFontMap)
    m_pFontMap = new CBA_FontMap(m_pWidget, GetSystemHandler());
  cp.pFontMap = m_pFontMap;
  cp.pFocusHandler = this;

  return cp;
}

CPWL_Wnd* CFFL_ComboBox::NewPDFWindow(const PWL_CREATEPARAM& cp,
                                      CPDFSDK_PageView* pPageView) {
  CPWL_ComboBox* pWnd = new CPWL_ComboBox();
  pWnd->AttachFFLData(this);
  pWnd->Create(cp);

  CFFL_InteractiveFormFiller* pFormFiller =
      m_pFormFillEnv->GetInteractiveFormFiller();
  pWnd->SetFillerNotify(pFormFiller);

  int32_t nCurSel = m_pWidget->GetSelectedIndex(0);
  CFX_WideString swText;
  if (nCurSel < 0)
    swText = m_pWidget->GetValue();
  else
    swText = m_pWidget->GetOptionLabel(nCurSel);

  for (int32_t i = 0, sz = m_pWidget->CountOptions(); i < sz; i++) {
    pWnd->AddString(m_pWidget->GetOptionLabel(i));
  }

  pWnd->SetSelect(nCurSel);
  pWnd->SetText(swText);
  return pWnd;
}

bool CFFL_ComboBox::OnChar(CPDFSDK_Annot* pAnnot,
                           uint32_t nChar,
                           uint32_t nFlags) {
  return CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
}

bool CFFL_ComboBox::IsDataChanged(CPDFSDK_PageView* pPageView) {
  CPWL_ComboBox* pWnd = (CPWL_ComboBox*)GetPDFWindow(pPageView, false);
  if (!pWnd)
    return false;

  int32_t nCurSel = pWnd->GetSelect();
  if (!(m_pWidget->GetFieldFlags() & FIELDFLAG_EDIT))
    return nCurSel != m_pWidget->GetSelectedIndex(0);

  if (nCurSel >= 0)
    return nCurSel != m_pWidget->GetSelectedIndex(0);

  return pWnd->GetText() != m_pWidget->GetValue();
}

void CFFL_ComboBox::SaveData(CPDFSDK_PageView* pPageView) {
  CPWL_ComboBox* pWnd =
      static_cast<CPWL_ComboBox*>(GetPDFWindow(pPageView, false));
  if (!pWnd)
    return;

  CFX_WideString swText = pWnd->GetText();
  int32_t nCurSel = pWnd->GetSelect();

  bool bSetValue = false;

  if (m_pWidget->GetFieldFlags() & FIELDFLAG_EDIT)
    bSetValue = (nCurSel < 0) || (swText != m_pWidget->GetOptionLabel(nCurSel));

  if (bSetValue) {
    m_pWidget->SetValue(swText, false);
  } else {
    m_pWidget->GetSelectedIndex(0);
    m_pWidget->SetOptionSelection(nCurSel, true, false);
  }

  m_pWidget->ResetFieldAppearance(true);
  m_pWidget->UpdateField();
  SetChangeMark();

  m_pWidget->GetPDFPage();
}

void CFFL_ComboBox::GetActionData(CPDFSDK_PageView* pPageView,
                                  CPDF_AAction::AActionType type,
                                  PDFSDK_FieldAction& fa) {
  switch (type) {
    case CPDF_AAction::KeyStroke:
      if (CPWL_ComboBox* pComboBox =
              static_cast<CPWL_ComboBox*>(GetPDFWindow(pPageView, false))) {
        if (CPWL_Edit* pEdit = pComboBox->GetEdit()) {
          fa.bFieldFull = pEdit->IsTextFull();
          int nSelStart = 0;
          int nSelEnd = 0;
          pEdit->GetSel(nSelStart, nSelEnd);
          fa.nSelEnd = nSelEnd;
          fa.nSelStart = nSelStart;
          fa.sValue = pEdit->GetText();
          fa.sChangeEx = GetSelectExportText();

          if (fa.bFieldFull) {
            fa.sChange = L"";
            fa.sChangeEx = L"";
          }
        }
      }
      break;
    case CPDF_AAction::Validate:
      if (CPWL_ComboBox* pComboBox =
              static_cast<CPWL_ComboBox*>(GetPDFWindow(pPageView, false))) {
        if (CPWL_Edit* pEdit = pComboBox->GetEdit()) {
          fa.sValue = pEdit->GetText();
        }
      }
      break;
    case CPDF_AAction::LoseFocus:
    case CPDF_AAction::GetFocus:
      fa.sValue = m_pWidget->GetValue();
      break;
    default:
      break;
  }
}

void CFFL_ComboBox::SetActionData(CPDFSDK_PageView* pPageView,
                                  CPDF_AAction::AActionType type,
                                  const PDFSDK_FieldAction& fa) {
  switch (type) {
    case CPDF_AAction::KeyStroke:
      if (CPWL_ComboBox* pComboBox =
              static_cast<CPWL_ComboBox*>(GetPDFWindow(pPageView, false))) {
        if (CPWL_Edit* pEdit = pComboBox->GetEdit()) {
          pEdit->SetSel(fa.nSelStart, fa.nSelEnd);
          pEdit->ReplaceSel(fa.sChange);
        }
      }
      break;
    default:
      break;
  }
}

bool CFFL_ComboBox::IsActionDataChanged(CPDF_AAction::AActionType type,
                                        const PDFSDK_FieldAction& faOld,
                                        const PDFSDK_FieldAction& faNew) {
  switch (type) {
    case CPDF_AAction::KeyStroke:
      return (!faOld.bFieldFull && faOld.nSelEnd != faNew.nSelEnd) ||
             faOld.nSelStart != faNew.nSelStart ||
             faOld.sChange != faNew.sChange;
    default:
      break;
  }

  return false;
}

void CFFL_ComboBox::SaveState(CPDFSDK_PageView* pPageView) {
  ASSERT(pPageView);

  if (CPWL_ComboBox* pComboBox =
          static_cast<CPWL_ComboBox*>(GetPDFWindow(pPageView, false))) {
    m_State.nIndex = pComboBox->GetSelect();

    if (CPWL_Edit* pEdit = pComboBox->GetEdit()) {
      pEdit->GetSel(m_State.nStart, m_State.nEnd);
      m_State.sValue = pEdit->GetText();
    }
  }
}

void CFFL_ComboBox::RestoreState(CPDFSDK_PageView* pPageView) {
  ASSERT(pPageView);

  if (CPWL_ComboBox* pComboBox =
          static_cast<CPWL_ComboBox*>(GetPDFWindow(pPageView, true))) {
    if (m_State.nIndex >= 0) {
      pComboBox->SetSelect(m_State.nIndex);
    } else {
      if (CPWL_Edit* pEdit = pComboBox->GetEdit()) {
        pEdit->SetText(m_State.sValue);
        pEdit->SetSel(m_State.nStart, m_State.nEnd);
      }
    }
  }
}

CPWL_Wnd* CFFL_ComboBox::ResetPDFWindow(CPDFSDK_PageView* pPageView,
                                        bool bRestoreValue) {
  if (bRestoreValue)
    SaveState(pPageView);

  DestroyPDFWindow(pPageView);

  CPWL_Wnd* pRet = nullptr;

  if (bRestoreValue) {
    RestoreState(pPageView);
    pRet = GetPDFWindow(pPageView, false);
  } else {
    pRet = GetPDFWindow(pPageView, true);
  }

  m_pWidget->UpdateField();

  return pRet;
}

#ifdef PDF_ENABLE_XFA
bool CFFL_ComboBox::IsFieldFull(CPDFSDK_PageView* pPageView) {
  if (CPWL_ComboBox* pComboBox =
          static_cast<CPWL_ComboBox*>(GetPDFWindow(pPageView, false))) {
    if (CPWL_Edit* pEdit = pComboBox->GetEdit())
      return pEdit->IsTextFull();
  }
  return false;
}
#endif  // PDF_ENABLE_XFA

void CFFL_ComboBox::OnSetFocus(CPWL_Wnd* pWnd) {
  ASSERT(m_pFormFillEnv);

  if (pWnd->GetClassName() == PWL_CLASSNAME_EDIT) {
    CPWL_Edit* pEdit = (CPWL_Edit*)pWnd;
    pEdit->SetCharSet(FXFONT_GB2312_CHARSET);
    pEdit->SetCodePage(936);

    pEdit->SetReadyToInput();
    CFX_WideString wsText = pEdit->GetText();
    int nCharacters = wsText.GetLength();
    CFX_ByteString bsUTFText = wsText.UTF16LE_Encode();
    unsigned short* pBuffer = (unsigned short*)bsUTFText.c_str();
    m_pFormFillEnv->OnSetFieldInputFocus(pBuffer, nCharacters, true);
  }
}

CFX_WideString CFFL_ComboBox::GetSelectExportText() {
  CFX_WideString swRet;

  int nExport = -1;
  CPDFSDK_PageView* pPageView = GetCurPageView(true);
  if (CPWL_ComboBox* pComboBox =
          (CPWL_ComboBox*)GetPDFWindow(pPageView, false)) {
    nExport = pComboBox->GetSelect();
  }

  if (nExport >= 0) {
    if (CPDF_FormField* pFormField = m_pWidget->GetFormField()) {
      swRet = pFormField->GetOptionValue(nExport);
      if (swRet.IsEmpty())
        swRet = pFormField->GetOptionLabel(nExport);
    }
  }

  return swRet;
}
