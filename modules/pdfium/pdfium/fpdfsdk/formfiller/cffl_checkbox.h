// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FORMFILLER_CFFL_CHECKBOX_H_
#define FPDFSDK_FORMFILLER_CFFL_CHECKBOX_H_

#include "fpdfsdk/formfiller/cffl_formfiller.h"

class CFFL_CheckBox : public CFFL_Button {
 public:
  CFFL_CheckBox(CPDFSDK_FormFillEnvironment* pApp, CPDFSDK_Widget* pWidget);
  ~CFFL_CheckBox() override;

  // CFFL_Button
  CPWL_Wnd* NewPDFWindow(const PWL_CREATEPARAM& cp,
                         CPDFSDK_PageView* pPageView) override;
  bool OnKeyDown(CPDFSDK_Annot* pAnnot,
                 uint32_t nKeyCode,
                 uint32_t nFlags) override;
  bool OnChar(CPDFSDK_Annot* pAnnot, uint32_t nChar, uint32_t nFlags) override;
  bool OnLButtonUp(CPDFSDK_PageView* pPageView,
                   CPDFSDK_Annot* pAnnot,
                   uint32_t nFlags,
                   const CFX_PointF& point) override;
  bool IsDataChanged(CPDFSDK_PageView* pPageView) override;
  void SaveData(CPDFSDK_PageView* pPageView) override;
};

#endif  // FPDFSDK_FORMFILLER_CFFL_CHECKBOX_H_
