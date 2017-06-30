// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_XFAWIDGET_H_
#define FPDFSDK_CPDFSDK_XFAWIDGET_H_

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"
#include "fpdfsdk/cpdfsdk_annot.h"

class CPDFSDK_InterForm;
class CPDFSDK_PageView;
class CXFA_FFWidget;

class CPDFSDK_XFAWidget : public CPDFSDK_Annot {
 public:
  CPDFSDK_XFAWidget(CXFA_FFWidget* pAnnot,
                    CPDFSDK_PageView* pPageView,
                    CPDFSDK_InterForm* pInterForm);
  ~CPDFSDK_XFAWidget() override {}

  bool IsXFAField() override;
  CXFA_FFWidget* GetXFAWidget() const override;
  CPDF_Annot::Subtype GetAnnotSubtype() const override;
  CFX_FloatRect GetRect() const override;

  CPDFSDK_InterForm* GetInterForm() { return m_pInterForm; }

 private:
  CPDFSDK_InterForm* m_pInterForm;
  CXFA_FFWidget* m_hXFAWidget;
};

#endif  // FPDFSDK_CPDFSDK_XFAWIDGET_H_
