// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_type3cache.h"

#include <map>
#include <memory>

#include "core/fpdfapi/font/cpdf_type3char.h"
#include "core/fpdfapi/font/cpdf_type3font.h"
#include "core/fpdfapi/render/cpdf_type3glyphs.h"
#include "core/fxge/fx_dib.h"
#include "core/fxge/fx_font.h"

namespace {

struct CPDF_UniqueKeyGen {
  void Generate(int count, ...);
  FX_CHAR m_Key[128];
  int m_KeyLen;
};

void CPDF_UniqueKeyGen::Generate(int count, ...) {
  va_list argList;
  va_start(argList, count);
  for (int i = 0; i < count; i++) {
    int p = va_arg(argList, int);
    (reinterpret_cast<uint32_t*>(m_Key))[i] = p;
  }
  va_end(argList);
  m_KeyLen = count * sizeof(uint32_t);
}

bool IsScanLine1bpp(uint8_t* pBuf, int width) {
  int size = width / 8;
  for (int i = 0; i < size; i++) {
    if (pBuf[i])
      return true;
  }
  return (width % 8) && (pBuf[width / 8] & (0xff << (8 - width % 8)));
}

bool IsScanLine8bpp(uint8_t* pBuf, int width) {
  for (int i = 0; i < width; i++) {
    if (pBuf[i] > 0x40)
      return true;
  }
  return false;
}

int DetectFirstLastScan(const CFX_DIBitmap* pBitmap, bool bFirst) {
  int height = pBitmap->GetHeight();
  int pitch = pBitmap->GetPitch();
  int width = pBitmap->GetWidth();
  int bpp = pBitmap->GetBPP();
  if (bpp > 8)
    width *= bpp / 8;
  uint8_t* pBuf = pBitmap->GetBuffer();
  int line = bFirst ? 0 : height - 1;
  int line_step = bFirst ? 1 : -1;
  int line_end = bFirst ? height : -1;
  while (line != line_end) {
    if (bpp == 1) {
      if (IsScanLine1bpp(pBuf + line * pitch, width))
        return line;
    } else {
      if (IsScanLine8bpp(pBuf + line * pitch, width))
        return line;
    }
    line += line_step;
  }
  return -1;
}

}  // namespace

CPDF_Type3Cache::CPDF_Type3Cache(CPDF_Type3Font* pFont) : m_pFont(pFont) {}

CPDF_Type3Cache::~CPDF_Type3Cache() {
  for (const auto& pair : m_SizeMap)
    delete pair.second;
  m_SizeMap.clear();
}

CFX_GlyphBitmap* CPDF_Type3Cache::LoadGlyph(uint32_t charcode,
                                            const CFX_Matrix* pMatrix,
                                            FX_FLOAT retinaScaleX,
                                            FX_FLOAT retinaScaleY) {
  CPDF_UniqueKeyGen keygen;
  keygen.Generate(
      4, FXSYS_round(pMatrix->a * 10000), FXSYS_round(pMatrix->b * 10000),
      FXSYS_round(pMatrix->c * 10000), FXSYS_round(pMatrix->d * 10000));
  CFX_ByteString FaceGlyphsKey(keygen.m_Key, keygen.m_KeyLen);
  CPDF_Type3Glyphs* pSizeCache;
  auto it = m_SizeMap.find(FaceGlyphsKey);
  if (it == m_SizeMap.end()) {
    pSizeCache = new CPDF_Type3Glyphs;
    m_SizeMap[FaceGlyphsKey] = pSizeCache;
  } else {
    pSizeCache = it->second;
  }
  auto it2 = pSizeCache->m_GlyphMap.find(charcode);
  if (it2 != pSizeCache->m_GlyphMap.end())
    return it2->second;

  CFX_GlyphBitmap* pGlyphBitmap =
      RenderGlyph(pSizeCache, charcode, pMatrix, retinaScaleX, retinaScaleY);
  pSizeCache->m_GlyphMap[charcode] = pGlyphBitmap;
  return pGlyphBitmap;
}

CFX_GlyphBitmap* CPDF_Type3Cache::RenderGlyph(CPDF_Type3Glyphs* pSize,
                                              uint32_t charcode,
                                              const CFX_Matrix* pMatrix,
                                              FX_FLOAT retinaScaleX,
                                              FX_FLOAT retinaScaleY) {
  const CPDF_Type3Char* pChar = m_pFont->LoadChar(charcode);
  if (!pChar || !pChar->m_pBitmap)
    return nullptr;

  CFX_DIBitmap* pBitmap = pChar->m_pBitmap.get();
  CFX_Matrix image_matrix = pChar->m_ImageMatrix;
  CFX_Matrix text_matrix(pMatrix->a, pMatrix->b, pMatrix->c, pMatrix->d, 0, 0);
  image_matrix.Concat(text_matrix);

  std::unique_ptr<CFX_DIBitmap> pResBitmap;
  int left = 0;
  int top = 0;
  if (FXSYS_fabs(image_matrix.b) < FXSYS_fabs(image_matrix.a) / 100 &&
      FXSYS_fabs(image_matrix.c) < FXSYS_fabs(image_matrix.d) / 100) {
    int top_line = DetectFirstLastScan(pBitmap, true);
    int bottom_line = DetectFirstLastScan(pBitmap, false);
    if (top_line == 0 && bottom_line == pBitmap->GetHeight() - 1) {
      FX_FLOAT top_y = image_matrix.d + image_matrix.f;
      FX_FLOAT bottom_y = image_matrix.f;
      bool bFlipped = top_y > bottom_y;
      if (bFlipped) {
        FX_FLOAT temp = top_y;
        top_y = bottom_y;
        bottom_y = temp;
      }
      pSize->AdjustBlue(top_y, bottom_y, top_line, bottom_line);
      pResBitmap = pBitmap->StretchTo(
          (int)(FXSYS_round(image_matrix.a) * retinaScaleX),
          (int)((bFlipped ? top_line - bottom_line : bottom_line - top_line) *
                retinaScaleY));
      top = top_line;
      if (image_matrix.a < 0) {
        image_matrix.Scale(retinaScaleX, retinaScaleY);
        left = FXSYS_round(image_matrix.e + image_matrix.a);
      } else {
        left = FXSYS_round(image_matrix.e);
      }
    }
  }
  if (!pResBitmap) {
    image_matrix.Scale(retinaScaleX, retinaScaleY);
    pResBitmap = pBitmap->TransformTo(&image_matrix, left, top);
  }
  if (!pResBitmap)
    return nullptr;

  CFX_GlyphBitmap* pGlyph = new CFX_GlyphBitmap;
  pGlyph->m_Left = left;
  pGlyph->m_Top = -top;
  pGlyph->m_Bitmap.TakeOver(pResBitmap.get());
  return pGlyph;
}
