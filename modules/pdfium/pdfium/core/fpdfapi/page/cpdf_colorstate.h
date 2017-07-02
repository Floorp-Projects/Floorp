// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_COLORSTATE_H_
#define CORE_FPDFAPI_PAGE_CPDF_COLORSTATE_H_

#include "core/fpdfapi/page/cpdf_color.h"
#include "core/fxcrt/cfx_shared_copy_on_write.h"
#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Color;
class CPDF_ColorSpace;
class CPDF_Pattern;

class CPDF_ColorState {
 public:
  CPDF_ColorState();
  CPDF_ColorState(const CPDF_ColorState& that);
  ~CPDF_ColorState();

  void Emplace();
  void SetDefault();

  uint32_t GetFillRGB() const;
  void SetFillRGB(uint32_t rgb);

  uint32_t GetStrokeRGB() const;
  void SetStrokeRGB(uint32_t rgb);

  const CPDF_Color* GetFillColor() const;
  CPDF_Color* GetMutableFillColor();
  bool HasFillColor() const;

  const CPDF_Color* GetStrokeColor() const;
  CPDF_Color* GetMutableStrokeColor();
  bool HasStrokeColor() const;

  void SetFillColor(CPDF_ColorSpace* pCS, FX_FLOAT* pValue, uint32_t nValues);
  void SetStrokeColor(CPDF_ColorSpace* pCS, FX_FLOAT* pValue, uint32_t nValues);
  void SetFillPattern(CPDF_Pattern* pattern,
                      FX_FLOAT* pValue,
                      uint32_t nValues);
  void SetStrokePattern(CPDF_Pattern* pattern,
                        FX_FLOAT* pValue,
                        uint32_t nValues);

  explicit operator bool() const { return !!m_Ref; }

 private:
  class ColorData {
   public:
    ColorData();
    ColorData(const ColorData& src);
    ~ColorData();

    void SetDefault();

    uint32_t m_FillRGB;
    uint32_t m_StrokeRGB;
    CPDF_Color m_FillColor;
    CPDF_Color m_StrokeColor;
  };

  void SetColor(CPDF_Color& color,
                uint32_t& rgb,
                CPDF_ColorSpace* pCS,
                FX_FLOAT* pValue,
                uint32_t nValues);

  CFX_SharedCopyOnWrite<ColorData> m_Ref;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_COLORSTATE_H_
