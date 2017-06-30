// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_textrenderer.h"

#include <algorithm>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/render/cpdf_charposlist.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"

// static
bool CPDF_TextRenderer::DrawTextPath(CFX_RenderDevice* pDevice,
                                     const std::vector<uint32_t>& charCodes,
                                     const std::vector<FX_FLOAT>& charPos,
                                     CPDF_Font* pFont,
                                     FX_FLOAT font_size,
                                     const CFX_Matrix* pText2User,
                                     const CFX_Matrix* pUser2Device,
                                     const CFX_GraphStateData* pGraphState,
                                     FX_ARGB fill_argb,
                                     FX_ARGB stroke_argb,
                                     CFX_PathData* pClippingPath,
                                     int nFlag) {
  CPDF_CharPosList CharPosList;
  CharPosList.Load(charCodes, charPos, pFont, font_size);
  if (CharPosList.m_nChars == 0)
    return true;

  bool bDraw = true;
  int32_t fontPosition = CharPosList.m_pCharPos[0].m_FallbackFontPosition;
  uint32_t startIndex = 0;
  for (uint32_t i = 0; i < CharPosList.m_nChars; i++) {
    int32_t curFontPosition = CharPosList.m_pCharPos[i].m_FallbackFontPosition;
    if (fontPosition == curFontPosition)
      continue;
    auto* font = fontPosition == -1
                     ? &pFont->m_Font
                     : pFont->m_FontFallbacks[fontPosition].get();
    if (!pDevice->DrawTextPath(i - startIndex,
                               CharPosList.m_pCharPos + startIndex, font,
                               font_size, pText2User, pUser2Device, pGraphState,
                               fill_argb, stroke_argb, pClippingPath, nFlag)) {
      bDraw = false;
    }
    fontPosition = curFontPosition;
    startIndex = i;
  }
  auto* font = fontPosition == -1 ? &pFont->m_Font
                                  : pFont->m_FontFallbacks[fontPosition].get();
  if (!pDevice->DrawTextPath(CharPosList.m_nChars - startIndex,
                             CharPosList.m_pCharPos + startIndex, font,
                             font_size, pText2User, pUser2Device, pGraphState,
                             fill_argb, stroke_argb, pClippingPath, nFlag)) {
    bDraw = false;
  }
  return bDraw;
}

// static
void CPDF_TextRenderer::DrawTextString(CFX_RenderDevice* pDevice,
                                       FX_FLOAT origin_x,
                                       FX_FLOAT origin_y,
                                       CPDF_Font* pFont,
                                       FX_FLOAT font_size,
                                       const CFX_Matrix* pMatrix,
                                       const CFX_ByteString& str,
                                       FX_ARGB fill_argb,
                                       const CFX_GraphStateData* pGraphState,
                                       const CPDF_RenderOptions* pOptions) {
  if (pFont->IsType3Font())
    return;

  int nChars = pFont->CountChar(str.c_str(), str.GetLength());
  if (nChars <= 0)
    return;

  int offset = 0;
  std::vector<uint32_t> codes;
  std::vector<FX_FLOAT> positions;
  codes.resize(nChars);
  positions.resize(nChars - 1);
  FX_FLOAT cur_pos = 0;
  for (int i = 0; i < nChars; i++) {
    codes[i] = pFont->GetNextChar(str.c_str(), str.GetLength(), offset);
    if (i)
      positions[i - 1] = cur_pos;
    cur_pos += pFont->GetCharWidthF(codes[i]) * font_size / 1000;
  }
  CFX_Matrix matrix;
  if (pMatrix)
    matrix = *pMatrix;

  matrix.e = origin_x;
  matrix.f = origin_y;

  DrawNormalText(pDevice, codes, positions, pFont, font_size, &matrix,
                 fill_argb, pOptions);
}

// static
bool CPDF_TextRenderer::DrawNormalText(CFX_RenderDevice* pDevice,
                                       const std::vector<uint32_t>& charCodes,
                                       const std::vector<FX_FLOAT>& charPos,
                                       CPDF_Font* pFont,
                                       FX_FLOAT font_size,
                                       const CFX_Matrix* pText2Device,
                                       FX_ARGB fill_argb,
                                       const CPDF_RenderOptions* pOptions) {
  CPDF_CharPosList CharPosList;
  CharPosList.Load(charCodes, charPos, pFont, font_size);
  if (CharPosList.m_nChars == 0)
    return true;
  int FXGE_flags = 0;
  if (pOptions) {
    uint32_t dwFlags = pOptions->m_Flags;
    if (dwFlags & RENDER_CLEARTYPE) {
      FXGE_flags |= FXTEXT_CLEARTYPE;
      if (dwFlags & RENDER_BGR_STRIPE)
        FXGE_flags |= FXTEXT_BGR_STRIPE;
    }
    if (dwFlags & RENDER_NOTEXTSMOOTH)
      FXGE_flags |= FXTEXT_NOSMOOTH;
    if (dwFlags & RENDER_PRINTGRAPHICTEXT)
      FXGE_flags |= FXTEXT_PRINTGRAPHICTEXT;
    if (dwFlags & RENDER_NO_NATIVETEXT)
      FXGE_flags |= FXTEXT_NO_NATIVETEXT;
    if (dwFlags & RENDER_PRINTIMAGETEXT)
      FXGE_flags |= FXTEXT_PRINTIMAGETEXT;
  } else {
    FXGE_flags = FXTEXT_CLEARTYPE;
  }
  if (pFont->IsCIDFont())
    FXGE_flags |= FXFONT_CIDFONT;
  bool bDraw = true;
  int32_t fontPosition = CharPosList.m_pCharPos[0].m_FallbackFontPosition;
  uint32_t startIndex = 0;
  for (uint32_t i = 0; i < CharPosList.m_nChars; i++) {
    int32_t curFontPosition = CharPosList.m_pCharPos[i].m_FallbackFontPosition;
    if (fontPosition == curFontPosition)
      continue;
    auto* font = fontPosition == -1
                     ? &pFont->m_Font
                     : pFont->m_FontFallbacks[fontPosition].get();
    if (!pDevice->DrawNormalText(
            i - startIndex, CharPosList.m_pCharPos + startIndex, font,
            font_size, pText2Device, fill_argb, FXGE_flags)) {
      bDraw = false;
    }
    fontPosition = curFontPosition;
    startIndex = i;
  }
  auto* font = fontPosition == -1 ? &pFont->m_Font
                                  : pFont->m_FontFallbacks[fontPosition].get();
  if (!pDevice->DrawNormalText(CharPosList.m_nChars - startIndex,
                               CharPosList.m_pCharPos + startIndex, font,
                               font_size, pText2Device, fill_argb,
                               FXGE_flags)) {
    bDraw = false;
  }
  return bDraw;
}
