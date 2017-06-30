// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_textobject.h"

#include <algorithm>

#include "core/fpdfapi/font/cpdf_cidfont.h"
#include "core/fpdfapi/font/cpdf_font.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

CPDF_TextObjectItem::CPDF_TextObjectItem() : m_CharCode(0) {}

CPDF_TextObjectItem::~CPDF_TextObjectItem() = default;

CPDF_TextObject::CPDF_TextObject() {}

CPDF_TextObject::~CPDF_TextObject() {}

int CPDF_TextObject::CountItems() const {
  return pdfium::CollectionSize<int>(m_CharCodes);
}

void CPDF_TextObject::GetItemInfo(int index, CPDF_TextObjectItem* pInfo) const {
  pInfo->m_CharCode = m_CharCodes[index];
  pInfo->m_Origin = CFX_PointF(index ? m_CharPos[index - 1] : 0, 0);
  if (pInfo->m_CharCode == CPDF_Font::kInvalidCharCode)
    return;

  CPDF_Font* pFont = m_TextState.GetFont();
  if (!pFont->IsCIDFont())
    return;
  if (!pFont->AsCIDFont()->IsVertWriting())
    return;

  uint16_t CID = pFont->AsCIDFont()->CIDFromCharCode(pInfo->m_CharCode);
  pInfo->m_Origin = CFX_PointF(0, pInfo->m_Origin.x);

  short vx;
  short vy;
  pFont->AsCIDFont()->GetVertOrigin(CID, vx, vy);

  FX_FLOAT fontsize = m_TextState.GetFontSize();
  pInfo->m_Origin.x -= fontsize * vx / 1000;
  pInfo->m_Origin.y -= fontsize * vy / 1000;
}

int CPDF_TextObject::CountChars() const {
  int count = 0;
  for (uint32_t charcode : m_CharCodes) {
    if (charcode != CPDF_Font::kInvalidCharCode)
      count++;
  }
  return count;
}

void CPDF_TextObject::GetCharInfo(int index,
                                  uint32_t* charcode,
                                  FX_FLOAT* kerning) const {
  int count = 0;
  for (size_t i = 0; i < m_CharCodes.size(); ++i) {
    if (m_CharCodes[i] == CPDF_Font::kInvalidCharCode)
      continue;
    if (count++ != index)
      continue;
    *charcode = m_CharCodes[i];
    if (i == m_CharCodes.size() - 1 ||
        m_CharCodes[i + 1] != CPDF_Font::kInvalidCharCode) {
      *kerning = 0;
    } else {
      *kerning = m_CharPos[i];
    }
    return;
  }
}

void CPDF_TextObject::GetCharInfo(int index, CPDF_TextObjectItem* pInfo) const {
  int count = 0;
  for (int i = 0; i < pdfium::CollectionSize<int>(m_CharCodes); ++i) {
    uint32_t charcode = m_CharCodes[i];
    if (charcode == CPDF_Font::kInvalidCharCode)
      continue;
    if (count++ != index)
      continue;
    GetItemInfo(i, pInfo);
    break;
  }
}

std::unique_ptr<CPDF_TextObject> CPDF_TextObject::Clone() const {
  auto obj = pdfium::MakeUnique<CPDF_TextObject>();
  obj->CopyData(this);
  obj->m_CharCodes = m_CharCodes;
  obj->m_CharPos = m_CharPos;
  obj->m_Pos = m_Pos;
  return obj;
}

CPDF_PageObject::Type CPDF_TextObject::GetType() const {
  return TEXT;
}

void CPDF_TextObject::Transform(const CFX_Matrix& matrix) {
  CFX_Matrix text_matrix = GetTextMatrix();
  text_matrix.Concat(matrix);

  FX_FLOAT* pTextMatrix = m_TextState.GetMutableMatrix();
  pTextMatrix[0] = text_matrix.a;
  pTextMatrix[1] = text_matrix.c;
  pTextMatrix[2] = text_matrix.b;
  pTextMatrix[3] = text_matrix.d;
  m_Pos = CFX_PointF(text_matrix.e, text_matrix.f);
  CalcPositionData(0);
}

bool CPDF_TextObject::IsText() const {
  return true;
}

CPDF_TextObject* CPDF_TextObject::AsText() {
  return this;
}

const CPDF_TextObject* CPDF_TextObject::AsText() const {
  return this;
}

CFX_Matrix CPDF_TextObject::GetTextMatrix() const {
  const FX_FLOAT* pTextMatrix = m_TextState.GetMatrix();
  return CFX_Matrix(pTextMatrix[0], pTextMatrix[2], pTextMatrix[1],
                    pTextMatrix[3], m_Pos.x, m_Pos.y);
}

void CPDF_TextObject::SetSegments(const CFX_ByteString* pStrs,
                                  const FX_FLOAT* pKerning,
                                  int nsegs) {
  m_CharCodes.clear();
  m_CharPos.clear();
  CPDF_Font* pFont = m_TextState.GetFont();
  int nChars = 0;
  for (int i = 0; i < nsegs; ++i)
    nChars += pFont->CountChar(pStrs[i].c_str(), pStrs[i].GetLength());
  nChars += nsegs - 1;
  m_CharCodes.resize(nChars);
  m_CharPos.resize(nChars - 1);
  int index = 0;
  for (int i = 0; i < nsegs; ++i) {
    const FX_CHAR* segment = pStrs[i].c_str();
    int len = pStrs[i].GetLength();
    int offset = 0;
    while (offset < len)
      m_CharCodes[index++] = pFont->GetNextChar(segment, len, offset);
    if (i != nsegs - 1) {
      m_CharPos[index - 1] = pKerning[i];
      m_CharCodes[index++] = CPDF_Font::kInvalidCharCode;
    }
  }
}

void CPDF_TextObject::SetText(const CFX_ByteString& str) {
  SetSegments(&str, nullptr, 1);
  RecalcPositionData();
}

FX_FLOAT CPDF_TextObject::GetCharWidth(uint32_t charcode) const {
  FX_FLOAT fontsize = m_TextState.GetFontSize() / 1000;
  CPDF_Font* pFont = m_TextState.GetFont();
  bool bVertWriting = false;
  CPDF_CIDFont* pCIDFont = pFont->AsCIDFont();
  if (pCIDFont)
    bVertWriting = pCIDFont->IsVertWriting();
  if (!bVertWriting)
    return pFont->GetCharWidthF(charcode) * fontsize;

  uint16_t CID = pCIDFont->CIDFromCharCode(charcode);
  return pCIDFont->GetVertWidth(CID) * fontsize;
}

CPDF_Font* CPDF_TextObject::GetFont() const {
  return m_TextState.GetFont();
}

FX_FLOAT CPDF_TextObject::GetFontSize() const {
  return m_TextState.GetFontSize();
}

CFX_PointF CPDF_TextObject::CalcPositionData(FX_FLOAT horz_scale) {
  FX_FLOAT curpos = 0;
  FX_FLOAT min_x = 10000 * 1.0f;
  FX_FLOAT max_x = -10000 * 1.0f;
  FX_FLOAT min_y = 10000 * 1.0f;
  FX_FLOAT max_y = -10000 * 1.0f;
  CPDF_Font* pFont = m_TextState.GetFont();
  bool bVertWriting = false;
  CPDF_CIDFont* pCIDFont = pFont->AsCIDFont();
  if (pCIDFont)
    bVertWriting = pCIDFont->IsVertWriting();

  FX_FLOAT fontsize = m_TextState.GetFontSize();
  for (int i = 0; i < pdfium::CollectionSize<int>(m_CharCodes); ++i) {
    uint32_t charcode = m_CharCodes[i];
    if (i > 0) {
      if (charcode == CPDF_Font::kInvalidCharCode) {
        curpos -= (m_CharPos[i - 1] * fontsize) / 1000;
        continue;
      }
      m_CharPos[i - 1] = curpos;
    }

    FX_RECT char_rect = pFont->GetCharBBox(charcode);
    FX_FLOAT charwidth;
    if (!bVertWriting) {
      min_y = std::min(min_y, static_cast<FX_FLOAT>(
                                  std::min(char_rect.top, char_rect.bottom)));
      max_y = std::max(max_y, static_cast<FX_FLOAT>(
                                  std::max(char_rect.top, char_rect.bottom)));
      FX_FLOAT char_left = curpos + char_rect.left * fontsize / 1000;
      FX_FLOAT char_right = curpos + char_rect.right * fontsize / 1000;
      min_x = std::min(min_x, std::min(char_left, char_right));
      max_x = std::max(max_x, std::max(char_left, char_right));
      charwidth = pFont->GetCharWidthF(charcode) * fontsize / 1000;
    } else {
      uint16_t CID = pCIDFont->CIDFromCharCode(charcode);
      short vx;
      short vy;
      pCIDFont->GetVertOrigin(CID, vx, vy);
      char_rect.left -= vx;
      char_rect.right -= vx;
      char_rect.top -= vy;
      char_rect.bottom -= vy;
      min_x = std::min(min_x, static_cast<FX_FLOAT>(
                                  std::min(char_rect.left, char_rect.right)));
      max_x = std::max(max_x, static_cast<FX_FLOAT>(
                                  std::max(char_rect.left, char_rect.right)));
      FX_FLOAT char_top = curpos + char_rect.top * fontsize / 1000;
      FX_FLOAT char_bottom = curpos + char_rect.bottom * fontsize / 1000;
      min_y = std::min(min_y, std::min(char_top, char_bottom));
      max_y = std::max(max_y, std::max(char_top, char_bottom));
      charwidth = pCIDFont->GetVertWidth(CID) * fontsize / 1000;
    }
    curpos += charwidth;
    if (charcode == ' ' && (!pCIDFont || pCIDFont->GetCharSize(' ') == 1))
      curpos += m_TextState.GetWordSpace();

    curpos += m_TextState.GetCharSpace();
  }

  CFX_PointF ret;
  if (bVertWriting) {
    ret.y = curpos;
    min_x = min_x * fontsize / 1000;
    max_x = max_x * fontsize / 1000;
  } else {
    ret.x = curpos * horz_scale;
    min_y = min_y * fontsize / 1000;
    max_y = max_y * fontsize / 1000;
  }

  m_Left = min_x;
  m_Right = max_x;
  m_Bottom = min_y;
  m_Top = max_y;
  GetTextMatrix().TransformRect(m_Left, m_Right, m_Top, m_Bottom);

  if (!TextRenderingModeIsStrokeMode(m_TextState.GetTextMode()))
    return ret;

  FX_FLOAT half_width = m_GraphState.GetLineWidth() / 2;
  m_Left -= half_width;
  m_Right += half_width;
  m_Top += half_width;
  m_Bottom -= half_width;

  return ret;
}

void CPDF_TextObject::SetPosition(FX_FLOAT x, FX_FLOAT y) {
  FX_FLOAT dx = x - m_Pos.x;
  FX_FLOAT dy = y - m_Pos.y;
  m_Pos.x = x;
  m_Pos.y = y;
  m_Left += dx;
  m_Right += dx;
  m_Top += dy;
  m_Bottom += dy;
}

void CPDF_TextObject::RecalcPositionData() {
  CalcPositionData(1);
}
