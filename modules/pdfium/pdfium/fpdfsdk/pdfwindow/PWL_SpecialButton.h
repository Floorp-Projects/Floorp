// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PDFWINDOW_PWL_SPECIALBUTTON_H_
#define FPDFSDK_PDFWINDOW_PWL_SPECIALBUTTON_H_

#include "fpdfsdk/pdfwindow/PWL_Button.h"

class CPWL_PushButton : public CPWL_Button {
 public:
  CPWL_PushButton();
  ~CPWL_PushButton() override;

  // CPWL_Button
  CFX_ByteString GetClassName() const override;
  CFX_FloatRect GetFocusRect() const override;
};

class CPWL_CheckBox : public CPWL_Button {
 public:
  CPWL_CheckBox();
  ~CPWL_CheckBox() override;

  // CPWL_Button
  CFX_ByteString GetClassName() const override;
  bool OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) override;
  bool OnChar(uint16_t nChar, uint32_t nFlag) override;

  void SetCheck(bool bCheck);
  bool IsChecked() const;

 private:
  bool m_bChecked;
};

class CPWL_RadioButton : public CPWL_Button {
 public:
  CPWL_RadioButton();
  ~CPWL_RadioButton() override;

  // CPWL_Button
  CFX_ByteString GetClassName() const override;
  bool OnLButtonUp(const CFX_PointF& point, uint32_t nFlag) override;
  bool OnChar(uint16_t nChar, uint32_t nFlag) override;

  void SetCheck(bool bCheck);
  bool IsChecked() const;

 private:
  bool m_bChecked;
};

#endif  // FPDFSDK_PDFWINDOW_PWL_SPECIALBUTTON_H_
