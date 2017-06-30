// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_TEXTSTATE_H_
#define CORE_FPDFAPI_PAGE_CPDF_TEXTSTATE_H_

#include "core/fxcrt/cfx_shared_copy_on_write.h"
#include "core/fxcrt/fx_basic.h"

class CPDF_Document;
class CPDF_Font;

// See PDF Reference 1.7, page 402, table 5.3.
enum class TextRenderingMode {
  MODE_FILL = 0,
  MODE_STROKE = 1,
  MODE_FILL_STROKE = 2,
  MODE_INVISIBLE = 3,
  MODE_FILL_CLIP = 4,
  MODE_STROKE_CLIP = 5,
  MODE_FILL_STROKE_CLIP = 6,
  MODE_CLIP = 7,
};

class CPDF_TextState {
 public:
  CPDF_TextState();
  ~CPDF_TextState();

  void Emplace();

  CPDF_Font* GetFont() const;
  void SetFont(CPDF_Font* pFont);

  FX_FLOAT GetFontSize() const;
  void SetFontSize(FX_FLOAT size);

  const FX_FLOAT* GetMatrix() const;
  FX_FLOAT* GetMutableMatrix();

  FX_FLOAT GetCharSpace() const;
  void SetCharSpace(FX_FLOAT sp);

  FX_FLOAT GetWordSpace() const;
  void SetWordSpace(FX_FLOAT sp);

  FX_FLOAT GetFontSizeV() const;
  FX_FLOAT GetFontSizeH() const;
  FX_FLOAT GetBaselineAngle() const;
  FX_FLOAT GetShearAngle() const;

  TextRenderingMode GetTextMode() const;
  void SetTextMode(TextRenderingMode mode);

  const FX_FLOAT* GetCTM() const;
  FX_FLOAT* GetMutableCTM();

 private:
  class TextData {
   public:
    TextData();
    TextData(const TextData& src);
    ~TextData();

    void SetFont(CPDF_Font* pFont);
    FX_FLOAT GetFontSizeV() const;
    FX_FLOAT GetFontSizeH() const;
    FX_FLOAT GetBaselineAngle() const;
    FX_FLOAT GetShearAngle() const;

    CPDF_Font* m_pFont;
    CPDF_Document* m_pDocument;
    FX_FLOAT m_FontSize;
    FX_FLOAT m_CharSpace;
    FX_FLOAT m_WordSpace;
    TextRenderingMode m_TextMode;
    FX_FLOAT m_Matrix[4];
    FX_FLOAT m_CTM[4];
  };

  CFX_SharedCopyOnWrite<TextData> m_Ref;
};

bool SetTextRenderingModeFromInt(int iMode, TextRenderingMode* mode);
bool TextRenderingModeIsClipMode(const TextRenderingMode& mode);
bool TextRenderingModeIsStrokeMode(const TextRenderingMode& mode);

#endif  // CORE_FPDFAPI_PAGE_CPDF_TEXTSTATE_H_
