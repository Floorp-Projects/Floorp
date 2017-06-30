// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FORMFILLER_CFFL_COMBOBOX_H_
#define FPDFSDK_FORMFILLER_CFFL_COMBOBOX_H_

#include "core/fxcrt/fx_string.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"

class CBA_FontMap;

struct FFL_ComboBoxState {
  int nIndex;
  int nStart;
  int nEnd;
  CFX_WideString sValue;
};

class CFFL_ComboBox : public CFFL_FormFiller, public IPWL_FocusHandler {
 public:
  CFFL_ComboBox(CPDFSDK_FormFillEnvironment* pApp, CPDFSDK_Annot* pWidget);
  ~CFFL_ComboBox() override;

  // CFFL_FormFiller:
  PWL_CREATEPARAM GetCreateParam() override;
  CPWL_Wnd* NewPDFWindow(const PWL_CREATEPARAM& cp,
                         CPDFSDK_PageView* pPageView) override;
  bool OnChar(CPDFSDK_Annot* pAnnot, uint32_t nChar, uint32_t nFlags) override;
  bool IsDataChanged(CPDFSDK_PageView* pPageView) override;
  void SaveData(CPDFSDK_PageView* pPageView) override;
  void GetActionData(CPDFSDK_PageView* pPageView,
                     CPDF_AAction::AActionType type,
                     PDFSDK_FieldAction& fa) override;
  void SetActionData(CPDFSDK_PageView* pPageView,
                     CPDF_AAction::AActionType type,
                     const PDFSDK_FieldAction& fa) override;
  bool IsActionDataChanged(CPDF_AAction::AActionType type,
                           const PDFSDK_FieldAction& faOld,
                           const PDFSDK_FieldAction& faNew) override;
  void SaveState(CPDFSDK_PageView* pPageView) override;
  void RestoreState(CPDFSDK_PageView* pPageView) override;
  CPWL_Wnd* ResetPDFWindow(CPDFSDK_PageView* pPageView,
                           bool bRestoreValue) override;

  // IPWL_FocusHandler:
  void OnSetFocus(CPWL_Wnd* pWnd) override;

#ifdef PDF_ENABLE_XFA
  // CFFL_FormFiller:
  bool IsFieldFull(CPDFSDK_PageView* pPageView) override;
#endif  // PDF_ENABLE_XFA

 private:
  CFX_WideString GetSelectExportText();

  CBA_FontMap* m_pFontMap;
  FFL_ComboBoxState m_State;
};

#endif  // FPDFSDK_FORMFILLER_CFFL_COMBOBOX_H_
