// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_PATTERN_H_
#define CORE_FPDFAPI_PAGE_CPDF_PATTERN_H_

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Document;
class CPDF_Object;
class CPDF_ShadingPattern;
class CPDF_TilingPattern;

class CPDF_Pattern {
 public:
  enum PatternType { TILING = 1, SHADING };

  virtual ~CPDF_Pattern();

  virtual CPDF_TilingPattern* AsTilingPattern() = 0;
  virtual CPDF_ShadingPattern* AsShadingPattern() = 0;

  CPDF_Document* document() { return m_pDocument; }
  CPDF_Object* pattern_obj() { return m_pPatternObj; }
  CFX_Matrix* pattern_to_form() { return &m_Pattern2Form; }
  const CFX_Matrix& parent_matrix() const { return m_ParentMatrix; }

 protected:
  CPDF_Pattern(PatternType type,
               CPDF_Document* pDoc,
               CPDF_Object* pObj,
               const CFX_Matrix& parentMatrix);

  const PatternType m_PatternType;
  CPDF_Document* const m_pDocument;
  CPDF_Object* const m_pPatternObj;
  CFX_Matrix m_Pattern2Form;
  const CFX_Matrix m_ParentMatrix;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_PATTERN_H_
