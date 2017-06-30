// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pdfwindow/PWL_Button.h"
#include "fpdfsdk/pdfwindow/PWL_SpecialButton.h"
#include "fpdfsdk/pdfwindow/PWL_Utils.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"

CPWL_PushButton::CPWL_PushButton() {}

CPWL_PushButton::~CPWL_PushButton() {}

CFX_ByteString CPWL_PushButton::GetClassName() const {
  return "CPWL_PushButton";
}

CFX_FloatRect CPWL_PushButton::GetFocusRect() const {
  return CPWL_Utils::DeflateRect(GetWindowRect(), (FX_FLOAT)GetBorderWidth());
}

CPWL_CheckBox::CPWL_CheckBox() : m_bChecked(false) {}

CPWL_CheckBox::~CPWL_CheckBox() {}

CFX_ByteString CPWL_CheckBox::GetClassName() const {
  return "CPWL_CheckBox";
}

void CPWL_CheckBox::SetCheck(bool bCheck) {
  m_bChecked = bCheck;
}

bool CPWL_CheckBox::IsChecked() const {
  return m_bChecked;
}

bool CPWL_CheckBox::OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) {
  if (IsReadOnly())
    return false;

  SetCheck(!IsChecked());
  return true;
}

bool CPWL_CheckBox::OnChar(uint16_t nChar, uint32_t nFlag) {
  SetCheck(!IsChecked());
  return true;
}

CPWL_RadioButton::CPWL_RadioButton() : m_bChecked(false) {}

CPWL_RadioButton::~CPWL_RadioButton() {}

CFX_ByteString CPWL_RadioButton::GetClassName() const {
  return "CPWL_RadioButton";
}

bool CPWL_RadioButton::OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) {
  if (IsReadOnly())
    return false;

  SetCheck(true);
  return true;
}

void CPWL_RadioButton::SetCheck(bool bCheck) {
  m_bChecked = bCheck;
}

bool CPWL_RadioButton::IsChecked() const {
  return m_bChecked;
}

bool CPWL_RadioButton::OnChar(uint16_t nChar, uint32_t nFlag) {
  SetCheck(true);
  return true;
}
