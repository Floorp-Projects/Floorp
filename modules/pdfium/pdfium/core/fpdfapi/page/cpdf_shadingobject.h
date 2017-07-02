// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_SHADINGOBJECT_H_
#define CORE_FPDFAPI_PAGE_CPDF_SHADINGOBJECT_H_

#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fxcrt/fx_coordinates.h"

class CPDF_ShadingPattern;

class CPDF_ShadingObject : public CPDF_PageObject {
 public:
  CPDF_ShadingObject();
  ~CPDF_ShadingObject() override;

  // CPDF_PageObject
  Type GetType() const override;
  void Transform(const CFX_Matrix& matrix) override;
  bool IsShading() const override;
  CPDF_ShadingObject* AsShading() override;
  const CPDF_ShadingObject* AsShading() const override;

  void CalcBoundingBox();

  CPDF_ShadingPattern* m_pShading;
  CFX_Matrix m_Matrix;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_SHADINGOBJECT_H_
