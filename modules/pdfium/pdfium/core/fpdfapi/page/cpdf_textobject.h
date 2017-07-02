// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_TEXTOBJECT_H_
#define CORE_FPDFAPI_PAGE_CPDF_TEXTOBJECT_H_

#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CPDF_TextObjectItem {
 public:
  CPDF_TextObjectItem();
  ~CPDF_TextObjectItem();

  uint32_t m_CharCode;
  CFX_PointF m_Origin;
};

class CPDF_TextObject : public CPDF_PageObject {
 public:
  CPDF_TextObject();
  ~CPDF_TextObject() override;

  // CPDF_PageObject
  Type GetType() const override;
  void Transform(const CFX_Matrix& matrix) override;
  bool IsText() const override;
  CPDF_TextObject* AsText() override;
  const CPDF_TextObject* AsText() const override;

  std::unique_ptr<CPDF_TextObject> Clone() const;
  int CountItems() const;
  void GetItemInfo(int index, CPDF_TextObjectItem* pInfo) const;
  int CountChars() const;
  void GetCharInfo(int index, uint32_t* charcode, FX_FLOAT* kerning) const;
  void GetCharInfo(int index, CPDF_TextObjectItem* pInfo) const;
  FX_FLOAT GetCharWidth(uint32_t charcode) const;
  CFX_PointF GetPos() const { return m_Pos; }
  CFX_Matrix GetTextMatrix() const;
  CPDF_Font* GetFont() const;
  FX_FLOAT GetFontSize() const;

  void SetText(const CFX_ByteString& text);
  void SetPosition(FX_FLOAT x, FX_FLOAT y);

  void RecalcPositionData();

 private:
  friend class CPDF_RenderStatus;
  friend class CPDF_StreamContentParser;
  friend class CPDF_TextRenderer;
  friend class CPDF_PageContentGenerator;

  void SetSegments(const CFX_ByteString* pStrs,
                   const FX_FLOAT* pKerning,
                   int nSegs);

  CFX_PointF CalcPositionData(FX_FLOAT horz_scale);

  CFX_PointF m_Pos;
  std::vector<uint32_t> m_CharCodes;
  std::vector<FX_FLOAT> m_CharPos;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_TEXTOBJECT_H_
