// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/cpdf_type3font.h"

#include <utility>

#include "core/fpdfapi/font/cpdf_type3char.h"
#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/fx_system.h"
#include "third_party/base/stl_util.h"

#define FPDF_MAX_TYPE3_FORM_LEVEL 4

CPDF_Type3Font::CPDF_Type3Font()
    : m_pCharProcs(nullptr),
      m_pPageResources(nullptr),
      m_pFontResources(nullptr),
      m_CharLoadingDepth(0) {
  FXSYS_memset(m_CharWidthL, 0, sizeof(m_CharWidthL));
}

CPDF_Type3Font::~CPDF_Type3Font() {}

bool CPDF_Type3Font::IsType3Font() const {
  return true;
}

const CPDF_Type3Font* CPDF_Type3Font::AsType3Font() const {
  return this;
}

CPDF_Type3Font* CPDF_Type3Font::AsType3Font() {
  return this;
}

bool CPDF_Type3Font::Load() {
  m_pFontResources = m_pFontDict->GetDictFor("Resources");
  CPDF_Array* pMatrix = m_pFontDict->GetArrayFor("FontMatrix");
  FX_FLOAT xscale = 1.0f, yscale = 1.0f;
  if (pMatrix) {
    m_FontMatrix = pMatrix->GetMatrix();
    xscale = m_FontMatrix.a;
    yscale = m_FontMatrix.d;
  }
  CPDF_Array* pBBox = m_pFontDict->GetArrayFor("FontBBox");
  if (pBBox) {
    m_FontBBox.left = (int32_t)(pBBox->GetNumberAt(0) * xscale * 1000);
    m_FontBBox.bottom = (int32_t)(pBBox->GetNumberAt(1) * yscale * 1000);
    m_FontBBox.right = (int32_t)(pBBox->GetNumberAt(2) * xscale * 1000);
    m_FontBBox.top = (int32_t)(pBBox->GetNumberAt(3) * yscale * 1000);
  }
  int StartChar = m_pFontDict->GetIntegerFor("FirstChar");
  CPDF_Array* pWidthArray = m_pFontDict->GetArrayFor("Widths");
  if (pWidthArray && (StartChar >= 0 && StartChar < 256)) {
    size_t count = pWidthArray->GetCount();
    if (count > 256)
      count = 256;
    if (StartChar + count > 256)
      count = 256 - StartChar;
    for (size_t i = 0; i < count; i++) {
      m_CharWidthL[StartChar + i] =
          FXSYS_round(pWidthArray->GetNumberAt(i) * xscale * 1000);
    }
  }
  m_pCharProcs = m_pFontDict->GetDictFor("CharProcs");
  CPDF_Object* pEncoding = m_pFontDict->GetDirectObjectFor("Encoding");
  if (pEncoding)
    LoadPDFEncoding(pEncoding, m_BaseEncoding, &m_CharNames, false, false);
  return true;
}

void CPDF_Type3Font::CheckType3FontMetrics() {
  CheckFontMetrics();
}

CPDF_Type3Char* CPDF_Type3Font::LoadChar(uint32_t charcode) {
  if (m_CharLoadingDepth >= FPDF_MAX_TYPE3_FORM_LEVEL)
    return nullptr;

  auto it = m_CacheMap.find(charcode);
  if (it != m_CacheMap.end())
    return it->second.get();

  const FX_CHAR* name = GetAdobeCharName(m_BaseEncoding, m_CharNames, charcode);
  if (!name)
    return nullptr;

  CPDF_Stream* pStream =
      ToStream(m_pCharProcs ? m_pCharProcs->GetDirectObjectFor(name) : nullptr);
  if (!pStream)
    return nullptr;

  std::unique_ptr<CPDF_Type3Char> pNewChar(new CPDF_Type3Char(new CPDF_Form(
      m_pDocument, m_pFontResources ? m_pFontResources : m_pPageResources,
      pStream, nullptr)));

  // This can trigger recursion into this method. The content of |m_CacheMap|
  // can change as a result. Thus after it returns, check the cache again for
  // a cache hit.
  m_CharLoadingDepth++;
  pNewChar->m_pForm->ParseContent(nullptr, nullptr, pNewChar.get());
  m_CharLoadingDepth--;
  it = m_CacheMap.find(charcode);
  if (it != m_CacheMap.end())
    return it->second.get();

  FX_FLOAT scale = m_FontMatrix.GetXUnit();
  pNewChar->m_Width = (int32_t)(pNewChar->m_Width * scale + 0.5f);
  FX_RECT& rcBBox = pNewChar->m_BBox;
  CFX_FloatRect char_rect(
      (FX_FLOAT)rcBBox.left / 1000.0f, (FX_FLOAT)rcBBox.bottom / 1000.0f,
      (FX_FLOAT)rcBBox.right / 1000.0f, (FX_FLOAT)rcBBox.top / 1000.0f);
  if (rcBBox.right <= rcBBox.left || rcBBox.bottom >= rcBBox.top)
    char_rect = pNewChar->m_pForm->CalcBoundingBox();

  m_FontMatrix.TransformRect(char_rect);
  rcBBox.left = FXSYS_round(char_rect.left * 1000);
  rcBBox.right = FXSYS_round(char_rect.right * 1000);
  rcBBox.top = FXSYS_round(char_rect.top * 1000);
  rcBBox.bottom = FXSYS_round(char_rect.bottom * 1000);

  ASSERT(!pdfium::ContainsKey(m_CacheMap, charcode));
  m_CacheMap[charcode] = std::move(pNewChar);
  CPDF_Type3Char* pCachedChar = m_CacheMap[charcode].get();
  if (pCachedChar->m_pForm->GetPageObjectList()->empty())
    pCachedChar->m_pForm.reset();
  return pCachedChar;
}

int CPDF_Type3Font::GetCharWidthF(uint32_t charcode) {
  if (charcode >= FX_ArraySize(m_CharWidthL))
    charcode = 0;

  if (m_CharWidthL[charcode])
    return m_CharWidthL[charcode];

  const CPDF_Type3Char* pChar = LoadChar(charcode);
  return pChar ? pChar->m_Width : 0;
}

FX_RECT CPDF_Type3Font::GetCharBBox(uint32_t charcode) {
  const CPDF_Type3Char* pChar = LoadChar(charcode);
  return pChar ? pChar->m_BBox : FX_RECT();
}
