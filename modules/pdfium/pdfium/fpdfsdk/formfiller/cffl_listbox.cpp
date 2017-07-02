// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_listbox.h"

#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cba_fontmap.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"
#include "fpdfsdk/formfiller/cffl_interactiveformfiller.h"
#include "fpdfsdk/fsdk_common.h"
#include "fpdfsdk/pdfwindow/PWL_ListBox.h"
#include "third_party/base/ptr_util.h"

#define FFL_DEFAULTLISTBOXFONTSIZE 12.0f

CFFL_ListBox::CFFL_ListBox(CPDFSDK_FormFillEnvironment* pApp,
                           CPDFSDK_Annot* pWidget)
    : CFFL_FormFiller(pApp, pWidget) {}

CFFL_ListBox::~CFFL_ListBox() {}

PWL_CREATEPARAM CFFL_ListBox::GetCreateParam() {
  PWL_CREATEPARAM cp = CFFL_FormFiller::GetCreateParam();

  uint32_t dwFieldFlag = m_pWidget->GetFieldFlags();

  if (dwFieldFlag & FIELDFLAG_MULTISELECT) {
    cp.dwFlags |= PLBS_MULTIPLESEL;
  }

  cp.dwFlags |= PWS_VSCROLL;

  if (cp.dwFlags & PWS_AUTOFONTSIZE)
    cp.fFontSize = FFL_DEFAULTLISTBOXFONTSIZE;

  if (!m_pFontMap) {
    m_pFontMap = pdfium::MakeUnique<CBA_FontMap>(
        m_pWidget, m_pFormFillEnv->GetSysHandler());
  }
  cp.pFontMap = m_pFontMap.get();

  return cp;
}

CPWL_Wnd* CFFL_ListBox::NewPDFWindow(const PWL_CREATEPARAM& cp,
                                     CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pWnd = new CPWL_ListBox();
  pWnd->AttachFFLData(this);
  pWnd->Create(cp);
  pWnd->SetFillerNotify(m_pFormFillEnv->GetInteractiveFormFiller());

  for (int32_t i = 0, sz = m_pWidget->CountOptions(); i < sz; i++)
    pWnd->AddString(m_pWidget->GetOptionLabel(i));

  if (pWnd->HasFlag(PLBS_MULTIPLESEL)) {
    m_OriginSelections.clear();

    bool bSetCaret = false;
    for (int32_t i = 0, sz = m_pWidget->CountOptions(); i < sz; i++) {
      if (m_pWidget->IsOptionSelected(i)) {
        if (!bSetCaret) {
          pWnd->SetCaret(i);
          bSetCaret = true;
        }
        pWnd->Select(i);
        m_OriginSelections.insert(i);
      }
    }
  } else {
    for (int i = 0, sz = m_pWidget->CountOptions(); i < sz; i++) {
      if (m_pWidget->IsOptionSelected(i)) {
        pWnd->Select(i);
        break;
      }
    }
  }

  pWnd->SetTopVisibleIndex(m_pWidget->GetTopVisibleIndex());

  return pWnd;
}

bool CFFL_ListBox::OnChar(CPDFSDK_Annot* pAnnot,
                          uint32_t nChar,
                          uint32_t nFlags) {
  return CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
}

bool CFFL_ListBox::IsDataChanged(CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pListBox = (CPWL_ListBox*)GetPDFWindow(pPageView, false);
  if (!pListBox)
    return false;

  if (m_pWidget->GetFieldFlags() & FIELDFLAG_MULTISELECT) {
    size_t nSelCount = 0;
    for (int32_t i = 0, sz = pListBox->GetCount(); i < sz; ++i) {
      if (pListBox->IsItemSelected(i)) {
        if (m_OriginSelections.count(i) == 0)
          return true;

        ++nSelCount;
      }
    }

    return nSelCount != m_OriginSelections.size();
  }
  return pListBox->GetCurSel() != m_pWidget->GetSelectedIndex(0);
}

void CFFL_ListBox::SaveData(CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pListBox =
      static_cast<CPWL_ListBox*>(GetPDFWindow(pPageView, false));
  if (!pListBox)
    return;

  int32_t nNewTopIndex = pListBox->GetTopVisibleIndex();
  m_pWidget->ClearSelection(false);
  if (m_pWidget->GetFieldFlags() & FIELDFLAG_MULTISELECT) {
    for (int32_t i = 0, sz = pListBox->GetCount(); i < sz; i++) {
      if (pListBox->IsItemSelected(i))
        m_pWidget->SetOptionSelection(i, true, false);
    }
  } else {
    m_pWidget->SetOptionSelection(pListBox->GetCurSel(), true, false);
  }
  m_pWidget->SetTopVisibleIndex(nNewTopIndex);
  m_pWidget->ResetFieldAppearance(true);
  m_pWidget->UpdateField();
  SetChangeMark();
}

void CFFL_ListBox::GetActionData(CPDFSDK_PageView* pPageView,
                                 CPDF_AAction::AActionType type,
                                 PDFSDK_FieldAction& fa) {
  switch (type) {
    case CPDF_AAction::Validate:
      if (m_pWidget->GetFieldFlags() & FIELDFLAG_MULTISELECT) {
        fa.sValue = L"";
      } else {
        if (CPWL_ListBox* pListBox =
                (CPWL_ListBox*)GetPDFWindow(pPageView, false)) {
          int32_t nCurSel = pListBox->GetCurSel();
          if (nCurSel >= 0)
            fa.sValue = m_pWidget->GetOptionLabel(nCurSel);
        }
      }
      break;
    case CPDF_AAction::LoseFocus:
    case CPDF_AAction::GetFocus:
      if (m_pWidget->GetFieldFlags() & FIELDFLAG_MULTISELECT) {
        fa.sValue = L"";
      } else {
        int32_t nCurSel = m_pWidget->GetSelectedIndex(0);
        if (nCurSel >= 0)
          fa.sValue = m_pWidget->GetOptionLabel(nCurSel);
      }
      break;
    default:
      break;
  }
}

void CFFL_ListBox::SaveState(CPDFSDK_PageView* pPageView) {
  ASSERT(pPageView);

  CPWL_ListBox* pListBox =
      static_cast<CPWL_ListBox*>(GetPDFWindow(pPageView, false));
  if (!pListBox)
    return;

  for (int32_t i = 0, sz = pListBox->GetCount(); i < sz; i++) {
    if (pListBox->IsItemSelected(i))
      m_State.push_back(i);
  }
}

void CFFL_ListBox::RestoreState(CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pListBox =
      static_cast<CPWL_ListBox*>(GetPDFWindow(pPageView, false));
  if (!pListBox)
    return;

  for (const auto& item : m_State)
    pListBox->Select(item);
}

CPWL_Wnd* CFFL_ListBox::ResetPDFWindow(CPDFSDK_PageView* pPageView,
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
