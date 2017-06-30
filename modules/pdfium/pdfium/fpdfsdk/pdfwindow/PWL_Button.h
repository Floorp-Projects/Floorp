// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PDFWINDOW_PWL_BUTTON_H_
#define FPDFSDK_PDFWINDOW_PWL_BUTTON_H_

#include "fpdfsdk/pdfwindow/PWL_Wnd.h"

class CPWL_Button : public CPWL_Wnd {
 public:
  CPWL_Button();
  ~CPWL_Button() override;

  // CPWL_Wnd
  CFX_ByteString GetClassName() const override;
  void OnCreate(PWL_CREATEPARAM& cp) override;
  bool OnLButtonDown(const CFX_PointF& point, uint32_t nFlag) override;
  bool OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) override;

 protected:
  bool m_bMouseDown;
};

#endif  // FPDFSDK_PDFWINDOW_PWL_BUTTON_H_
