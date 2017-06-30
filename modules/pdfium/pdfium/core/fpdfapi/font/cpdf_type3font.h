// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FONT_CPDF_TYPE3FONT_H_
#define CORE_FPDFAPI_FONT_CPDF_TYPE3FONT_H_

#include <map>
#include <memory>

#include "core/fpdfapi/font/cpdf_simplefont.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Dictionary;
class CPDF_Type3Char;

class CPDF_Type3Font : public CPDF_SimpleFont {
 public:
  CPDF_Type3Font();
  ~CPDF_Type3Font() override;

  // CPDF_Font:
  bool IsType3Font() const override;
  const CPDF_Type3Font* AsType3Font() const override;
  CPDF_Type3Font* AsType3Font() override;
  int GetCharWidthF(uint32_t charcode) override;
  FX_RECT GetCharBBox(uint32_t charcode) override;

  void SetPageResources(CPDF_Dictionary* pResources) {
    m_pPageResources = pResources;
  }
  CPDF_Type3Char* LoadChar(uint32_t charcode);
  void CheckType3FontMetrics();

  CFX_Matrix& GetFontMatrix() { return m_FontMatrix; }

 protected:
  CFX_Matrix m_FontMatrix;

 private:
  // CPDF_Font:
  bool Load() override;

  // CPDF_SimpleFont:
  void LoadGlyphMap() override {}

  int m_CharWidthL[256];
  CPDF_Dictionary* m_pCharProcs;
  CPDF_Dictionary* m_pPageResources;
  CPDF_Dictionary* m_pFontResources;
  std::map<uint32_t, std::unique_ptr<CPDF_Type3Char>> m_CacheMap;
  // The depth char loading is in, to avoid recurive calling LoadChar().
  int m_CharLoadingDepth;
};

#endif  // CORE_FPDFAPI_FONT_CPDF_TYPE3FONT_H_
