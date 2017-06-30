// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_pushbutton.h"

#include "fpdfsdk/formfiller/cffl_formfiller.h"
#include "fpdfsdk/pdfwindow/PWL_SpecialButton.h"

CFFL_PushButton::CFFL_PushButton(CPDFSDK_FormFillEnvironment* pApp,
                                 CPDFSDK_Annot* pAnnot)
    : CFFL_Button(pApp, pAnnot) {}

CFFL_PushButton::~CFFL_PushButton() {}

CPWL_Wnd* CFFL_PushButton::NewPDFWindow(const PWL_CREATEPARAM& cp,
                                        CPDFSDK_PageView* pPageView) {
  CPWL_PushButton* pWnd = new CPWL_PushButton();
  pWnd->Create(cp);

  return pWnd;
}

bool CFFL_PushButton::OnChar(CPDFSDK_Annot* pAnnot,
                             uint32_t nChar,
                             uint32_t nFlags) {
  return CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
}

void CFFL_PushButton::OnDraw(CPDFSDK_PageView* pPageView,
                             CPDFSDK_Annot* pAnnot,
                             CFX_RenderDevice* pDevice,
                             CFX_Matrix* pUser2Device) {
  CFFL_Button::OnDraw(pPageView, pAnnot, pDevice, pUser2Device);
}
