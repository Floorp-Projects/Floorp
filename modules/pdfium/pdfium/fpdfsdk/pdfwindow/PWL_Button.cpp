// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pdfwindow/PWL_Button.h"
#include "fpdfsdk/pdfwindow/PWL_Utils.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"

CPWL_Button::CPWL_Button() : m_bMouseDown(false) {}

CPWL_Button::~CPWL_Button() {}

CFX_ByteString CPWL_Button::GetClassName() const {
  return "CPWL_Button";
}

void CPWL_Button::OnCreate(PWL_CREATEPARAM& cp) {
  cp.eCursorType = FXCT_HAND;
}

bool CPWL_Button::OnLButtonDown(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnLButtonDown(point, nFlag);

  m_bMouseDown = true;
  SetCapture();

  return true;
}

bool CPWL_Button::OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) {
  CPWL_Wnd::OnLButtonUp(point, nFlag);

  ReleaseCapture();
  m_bMouseDown = false;

  return true;
}
