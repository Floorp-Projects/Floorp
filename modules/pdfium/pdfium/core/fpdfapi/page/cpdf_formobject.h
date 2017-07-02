// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_FORMOBJECT_H_
#define CORE_FPDFAPI_PAGE_CPDF_FORMOBJECT_H_

#include <memory>

#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fxcrt/fx_coordinates.h"

class CPDF_Form;

class CPDF_FormObject : public CPDF_PageObject {
 public:
  CPDF_FormObject();
  ~CPDF_FormObject() override;

  // CPDF_PageObject:
  Type GetType() const override;
  void Transform(const CFX_Matrix& matrix) override;
  bool IsForm() const override;
  CPDF_FormObject* AsForm() override;
  const CPDF_FormObject* AsForm() const override;

  void CalcBoundingBox();
  const CPDF_Form* form() const { return m_pForm.get(); }

  std::unique_ptr<CPDF_Form> m_pForm;
  CFX_Matrix m_FormMatrix;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_FORMOBJECT_H_
