// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_TILINGPATTERN_H_
#define CORE_FPDFAPI_PAGE_CPDF_TILINGPATTERN_H_

#include <memory>

#include "core/fpdfapi/page/cpdf_pattern.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Document;
class CPDF_Form;
class CPDF_Object;

class CPDF_TilingPattern : public CPDF_Pattern {
 public:
  CPDF_TilingPattern(CPDF_Document* pDoc,
                     CPDF_Object* pPatternObj,
                     const CFX_Matrix& parentMatrix);
  ~CPDF_TilingPattern() override;

  CPDF_TilingPattern* AsTilingPattern() override;
  CPDF_ShadingPattern* AsShadingPattern() override;

  bool Load();

  bool colored() const { return m_bColored; }
  const CFX_FloatRect& bbox() const { return m_BBox; }
  FX_FLOAT x_step() const { return m_XStep; }
  FX_FLOAT y_step() const { return m_YStep; }
  CPDF_Form* form() const { return m_pForm.get(); }

 private:
  bool m_bColored;
  CFX_FloatRect m_BBox;
  FX_FLOAT m_XStep;
  FX_FLOAT m_YStep;
  std::unique_ptr<CPDF_Form> m_pForm;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_TILINGPATTERN_H_
