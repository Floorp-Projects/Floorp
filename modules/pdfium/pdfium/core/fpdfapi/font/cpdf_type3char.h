// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FONT_CPDF_TYPE3CHAR_H_
#define CORE_FPDFAPI_FONT_CPDF_TYPE3CHAR_H_

#include <memory>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CFX_DIBitmap;
class CPDF_Form;
class CPDF_RenderContext;

class CPDF_Type3Char {
 public:
  // Takes ownership of |pForm|.
  explicit CPDF_Type3Char(CPDF_Form* pForm);
  ~CPDF_Type3Char();

  bool LoadBitmap(CPDF_RenderContext* pContext);

  std::unique_ptr<CPDF_Form> m_pForm;
  std::unique_ptr<CFX_DIBitmap> m_pBitmap;
  bool m_bColored;
  int m_Width;
  CFX_Matrix m_ImageMatrix;
  FX_RECT m_BBox;
};

#endif  // CORE_FPDFAPI_FONT_CPDF_TYPE3CHAR_H_
