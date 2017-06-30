// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_SHADINGPATTERN_H_
#define CORE_FPDFAPI_PAGE_CPDF_SHADINGPATTERN_H_

#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_countedobject.h"
#include "core/fpdfapi/page/cpdf_pattern.h"
#include "core/fxcrt/fx_system.h"

enum ShadingType {
  kInvalidShading = 0,
  kFunctionBasedShading = 1,
  kAxialShading = 2,
  kRadialShading = 3,
  kFreeFormGouraudTriangleMeshShading = 4,
  kLatticeFormGouraudTriangleMeshShading = 5,
  kCoonsPatchMeshShading = 6,
  kTensorProductPatchMeshShading = 7,
  kMaxShading = 8
};

class CFX_Matrix;
class CPDF_ColorSpace;
class CPDF_Document;
class CPDF_Function;
class CPDF_Object;

class CPDF_ShadingPattern : public CPDF_Pattern {
 public:
  CPDF_ShadingPattern(CPDF_Document* pDoc,
                      CPDF_Object* pPatternObj,
                      bool bShading,
                      const CFX_Matrix& parentMatrix);
  ~CPDF_ShadingPattern() override;

  CPDF_TilingPattern* AsTilingPattern() override;
  CPDF_ShadingPattern* AsShadingPattern() override;

  bool IsMeshShading() const {
    return m_ShadingType == kFreeFormGouraudTriangleMeshShading ||
           m_ShadingType == kLatticeFormGouraudTriangleMeshShading ||
           m_ShadingType == kCoonsPatchMeshShading ||
           m_ShadingType == kTensorProductPatchMeshShading;
  }
  bool Load();

  ShadingType GetShadingType() const { return m_ShadingType; }
  bool IsShadingObject() const { return m_bShadingObj; }
  CPDF_Object* GetShadingObject() const { return m_pShadingObj; }
  CPDF_ColorSpace* GetCS() const { return m_pCS; }
  const std::vector<std::unique_ptr<CPDF_Function>>& GetFuncs() const {
    return m_pFunctions;
  }

 private:
  ShadingType m_ShadingType;
  bool m_bShadingObj;
  CPDF_Object* m_pShadingObj;

  // Still keep |m_pCS| as some CPDF_ColorSpace (name object) are not managed
  // as counted objects. Refer to CPDF_DocPageData::GetColorSpace.
  CPDF_ColorSpace* m_pCS;

  CPDF_CountedColorSpace* m_pCountedCS;
  std::vector<std::unique_ptr<CPDF_Function>> m_pFunctions;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_SHADINGPATTERN_H_
