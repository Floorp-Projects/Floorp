// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FORMFILLER_CFFL_PUSHBUTTON_H_
#define FPDFSDK_FORMFILLER_CFFL_PUSHBUTTON_H_

#include "fpdfsdk/formfiller/cffl_formfiller.h"

class CFFL_PushButton : public CFFL_Button {
 public:
  CFFL_PushButton(CPDFSDK_FormFillEnvironment* pApp, CPDFSDK_Annot* pAnnot);
  ~CFFL_PushButton() override;

  // CFFL_Button
  CPWL_Wnd* NewPDFWindow(const PWL_CREATEPARAM& cp,
                         CPDFSDK_PageView* pPageView) override;
  bool OnChar(CPDFSDK_Annot* pAnnot, uint32_t nChar, uint32_t nFlags) override;
  void OnDraw(CPDFSDK_PageView* pPageView,
              CPDFSDK_Annot* pAnnot,
              CFX_RenderDevice* pDevice,
              CFX_Matrix* pUser2Device) override;
};

#endif  // FPDFSDK_FORMFILLER_CFFL_PUSHBUTTON_H_
