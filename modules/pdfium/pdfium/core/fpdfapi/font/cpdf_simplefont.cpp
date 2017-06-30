// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/cpdf_simplefont.h"

#include "core/fpdfapi/font/font_int.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fxge/fx_freetype.h"
#include "third_party/base/numerics/safe_math.h"

CPDF_SimpleFont::CPDF_SimpleFont() : m_BaseEncoding(PDFFONT_ENCODING_BUILTIN) {
  FXSYS_memset(m_CharWidth, 0xff, sizeof(m_CharWidth));
  FXSYS_memset(m_GlyphIndex, 0xff, sizeof(m_GlyphIndex));
  FXSYS_memset(m_ExtGID, 0xff, sizeof(m_ExtGID));
  for (size_t i = 0; i < FX_ArraySize(m_CharBBox); ++i)
    m_CharBBox[i] = FX_RECT(-1, -1, -1, -1);
}

CPDF_SimpleFont::~CPDF_SimpleFont() {}

int CPDF_SimpleFont::GlyphFromCharCode(uint32_t charcode, bool* pVertGlyph) {
  if (pVertGlyph)
    *pVertGlyph = false;

  if (charcode > 0xff)
    return -1;

  int index = m_GlyphIndex[(uint8_t)charcode];
  return index != 0xffff ? index : -1;
}

void CPDF_SimpleFont::LoadCharMetrics(int charcode) {
  if (!m_Font.GetFace())
    return;

  if (charcode < 0 || charcode > 0xff) {
    return;
  }
  int glyph_index = m_GlyphIndex[charcode];
  if (glyph_index == 0xffff) {
    if (!m_pFontFile && charcode != 32) {
      LoadCharMetrics(32);
      m_CharBBox[charcode] = m_CharBBox[32];
      if (m_bUseFontWidth) {
        m_CharWidth[charcode] = m_CharWidth[32];
      }
    }
    return;
  }
  FXFT_Face face = m_Font.GetFace();
  int err = FXFT_Load_Glyph(
      face, glyph_index,
      FXFT_LOAD_NO_SCALE | FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
  if (err)
    return;

  int iHoriBearingX = FXFT_Get_Glyph_HoriBearingX(face);
  int iHoriBearingY = FXFT_Get_Glyph_HoriBearingY(face);
  m_CharBBox[charcode] =
      FX_RECT(TT2PDF(iHoriBearingX, face), TT2PDF(iHoriBearingY, face),
              TT2PDF(iHoriBearingX + FXFT_Get_Glyph_Width(face), face),
              TT2PDF(iHoriBearingY - FXFT_Get_Glyph_Height(face), face));

  if (m_bUseFontWidth) {
    int TT_Width = TT2PDF(FXFT_Get_Glyph_HoriAdvance(face), face);
    if (m_CharWidth[charcode] == 0xffff) {
      m_CharWidth[charcode] = TT_Width;
    } else if (TT_Width && !IsEmbedded()) {
      m_CharBBox[charcode].right =
          m_CharBBox[charcode].right * m_CharWidth[charcode] / TT_Width;
      m_CharBBox[charcode].left =
          m_CharBBox[charcode].left * m_CharWidth[charcode] / TT_Width;
    }
  }
}

int CPDF_SimpleFont::GetCharWidthF(uint32_t charcode) {
  if (charcode > 0xff)
    charcode = 0;

  if (m_CharWidth[charcode] == 0xffff) {
    LoadCharMetrics(charcode);
    if (m_CharWidth[charcode] == 0xffff) {
      m_CharWidth[charcode] = 0;
    }
  }
  return m_CharWidth[charcode];
}

FX_RECT CPDF_SimpleFont::GetCharBBox(uint32_t charcode) {
  if (charcode > 0xff)
    charcode = 0;

  if (m_CharBBox[charcode].left == -1)
    LoadCharMetrics(charcode);

  return m_CharBBox[charcode];
}

bool CPDF_SimpleFont::LoadCommon() {
  CPDF_Dictionary* pFontDesc = m_pFontDict->GetDictFor("FontDescriptor");
  if (pFontDesc) {
    LoadFontDescriptor(pFontDesc);
  }
  CPDF_Array* pWidthArray = m_pFontDict->GetArrayFor("Widths");
  m_bUseFontWidth = !pWidthArray;
  if (pWidthArray) {
    if (pFontDesc && pFontDesc->KeyExist("MissingWidth")) {
      int MissingWidth = pFontDesc->GetIntegerFor("MissingWidth");
      for (int i = 0; i < 256; i++) {
        m_CharWidth[i] = MissingWidth;
      }
    }
    size_t width_start = m_pFontDict->GetIntegerFor("FirstChar", 0);
    size_t width_end = m_pFontDict->GetIntegerFor("LastChar", 0);
    if (width_start <= 255) {
      if (width_end == 0 || width_end >= width_start + pWidthArray->GetCount())
        width_end = width_start + pWidthArray->GetCount() - 1;
      if (width_end > 255)
        width_end = 255;
      for (size_t i = width_start; i <= width_end; i++)
        m_CharWidth[i] = pWidthArray->GetIntegerAt(i - width_start);
    }
  }
  if (m_pFontFile) {
    if (m_BaseFont.GetLength() > 8 && m_BaseFont[7] == '+')
      m_BaseFont = m_BaseFont.Mid(8);
  } else {
    LoadSubstFont();
  }
  if (!(m_Flags & FXFONT_SYMBOLIC))
    m_BaseEncoding = PDFFONT_ENCODING_STANDARD;
  CPDF_Object* pEncoding = m_pFontDict->GetDirectObjectFor("Encoding");
  LoadPDFEncoding(pEncoding, m_BaseEncoding, &m_CharNames, !!m_pFontFile,
                  m_Font.IsTTFont());
  LoadGlyphMap();
  m_CharNames.clear();
  if (!m_Font.GetFace())
    return true;

  if (m_Flags & FXFONT_ALLCAP) {
    unsigned char kLowercases[][2] = {{'a', 'z'}, {0xe0, 0xf6}, {0xf8, 0xfd}};
    for (size_t range = 0; range < FX_ArraySize(kLowercases); ++range) {
      const auto& lower = kLowercases[range];
      for (int i = lower[0]; i <= lower[1]; ++i) {
        if (m_GlyphIndex[i] != 0xffff && m_pFontFile)
          continue;

        int j = i - 32;
        m_GlyphIndex[i] = m_GlyphIndex[j];
        if (m_CharWidth[j]) {
          m_CharWidth[i] = m_CharWidth[j];
          m_CharBBox[i] = m_CharBBox[j];
        }
      }
    }
  }
  CheckFontMetrics();
  return true;
}

void CPDF_SimpleFont::LoadSubstFont() {
  if (!m_bUseFontWidth && !(m_Flags & FXFONT_FIXED_PITCH)) {
    int width = 0, i;
    for (i = 0; i < 256; i++) {
      if (m_CharWidth[i] == 0 || m_CharWidth[i] == 0xffff)
        continue;

      if (width == 0)
        width = m_CharWidth[i];
      else if (width != m_CharWidth[i])
        break;
    }
    if (i == 256 && width)
      m_Flags |= FXFONT_FIXED_PITCH;
  }
  pdfium::base::CheckedNumeric<int> safeStemV(m_StemV);
  if (m_StemV < 140)
    safeStemV *= 5;
  else
    safeStemV = safeStemV * 4 + 140;
  m_Font.LoadSubst(m_BaseFont, IsTrueTypeFont(), m_Flags,
                   safeStemV.ValueOrDefault(FXFONT_FW_NORMAL), m_ItalicAngle, 0,
                   false);
}

bool CPDF_SimpleFont::IsUnicodeCompatible() const {
  return m_BaseEncoding != PDFFONT_ENCODING_BUILTIN &&
         m_BaseEncoding != PDFFONT_ENCODING_ADOBE_SYMBOL &&
         m_BaseEncoding != PDFFONT_ENCODING_ZAPFDINGBATS;
}

CFX_WideString CPDF_SimpleFont::UnicodeFromCharCode(uint32_t charcode) const {
  CFX_WideString unicode = CPDF_Font::UnicodeFromCharCode(charcode);
  if (!unicode.IsEmpty())
    return unicode;
  FX_WCHAR ret = m_Encoding.UnicodeFromCharCode((uint8_t)charcode);
  if (ret == 0)
    return CFX_WideString();
  return ret;
}

uint32_t CPDF_SimpleFont::CharCodeFromUnicode(FX_WCHAR unicode) const {
  uint32_t ret = CPDF_Font::CharCodeFromUnicode(unicode);
  if (ret)
    return ret;
  return m_Encoding.CharCodeFromUnicode(unicode);
}
