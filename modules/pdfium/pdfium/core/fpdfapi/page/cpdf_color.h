// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_COLOR_H_
#define CORE_FPDFAPI_PAGE_CPDF_COLOR_H_

#include "core/fpdfapi/page/cpdf_colorspace.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Pattern;

class CPDF_Color {
 public:
  CPDF_Color();
  ~CPDF_Color();

  bool IsNull() const { return !m_pBuffer; }
  bool IsPattern() const;

  void Copy(const CPDF_Color* pSrc);

  void SetColorSpace(CPDF_ColorSpace* pCS);
  void SetValue(FX_FLOAT* comp);
  void SetValue(CPDF_Pattern* pPattern, FX_FLOAT* comp, int ncomps);

  bool GetRGB(int& R, int& G, int& B) const;
  CPDF_Pattern* GetPattern() const;
  const CPDF_ColorSpace* GetColorSpace() const { return m_pCS; }

 protected:
  void ReleaseBuffer();
  void ReleaseColorSpace();

  CPDF_ColorSpace* m_pCS;
  FX_FLOAT* m_pBuffer;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_COLOR_H_
