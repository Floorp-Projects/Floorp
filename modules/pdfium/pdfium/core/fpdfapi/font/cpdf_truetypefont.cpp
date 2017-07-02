// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/cpdf_truetypefont.h"

#include "core/fpdfapi/font/font_int.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fxge/fx_font.h"

namespace {

const uint8_t kPrefix[4] = {0x00, 0xf0, 0xf1, 0xf2};

}  // namespace

CPDF_TrueTypeFont::CPDF_TrueTypeFont() {}

bool CPDF_TrueTypeFont::IsTrueTypeFont() const {
  return true;
}

const CPDF_TrueTypeFont* CPDF_TrueTypeFont::AsTrueTypeFont() const {
  return this;
}

CPDF_TrueTypeFont* CPDF_TrueTypeFont::AsTrueTypeFont() {
  return this;
}

bool CPDF_TrueTypeFont::Load() {
  return LoadCommon();
}

void CPDF_TrueTypeFont::LoadGlyphMap() {
  if (!m_Font.GetFace())
    return;

  int baseEncoding = m_BaseEncoding;
  if (m_pFontFile && m_Font.GetFace()->num_charmaps > 0 &&
      (baseEncoding == PDFFONT_ENCODING_MACROMAN ||
       baseEncoding == PDFFONT_ENCODING_WINANSI) &&
      (m_Flags & FXFONT_SYMBOLIC)) {
    bool bSupportWin = false;
    bool bSupportMac = false;
    for (int i = 0; i < FXFT_Get_Face_CharmapCount(m_Font.GetFace()); i++) {
      int platform_id = FXFT_Get_Charmap_PlatformID(
          FXFT_Get_Face_Charmaps(m_Font.GetFace())[i]);
      if (platform_id == 0 || platform_id == 3) {
        bSupportWin = true;
      } else if (platform_id == 0 || platform_id == 1) {
        bSupportMac = true;
      }
    }
    if (baseEncoding == PDFFONT_ENCODING_WINANSI && !bSupportWin) {
      baseEncoding =
          bSupportMac ? PDFFONT_ENCODING_MACROMAN : PDFFONT_ENCODING_BUILTIN;
    } else if (baseEncoding == PDFFONT_ENCODING_MACROMAN && !bSupportMac) {
      baseEncoding =
          bSupportWin ? PDFFONT_ENCODING_WINANSI : PDFFONT_ENCODING_BUILTIN;
    }
  }
  if (((baseEncoding == PDFFONT_ENCODING_MACROMAN ||
        baseEncoding == PDFFONT_ENCODING_WINANSI) &&
       m_CharNames.empty()) ||
      (m_Flags & FXFONT_NONSYMBOLIC)) {
    if (!FXFT_Has_Glyph_Names(m_Font.GetFace()) &&
        (!m_Font.GetFace()->num_charmaps || !m_Font.GetFace()->charmaps)) {
      int nStartChar = m_pFontDict->GetIntegerFor("FirstChar");
      if (nStartChar < 0 || nStartChar > 255)
        return;

      int charcode = 0;
      for (; charcode < nStartChar; charcode++) {
        m_GlyphIndex[charcode] = 0;
      }
      uint16_t nGlyph = charcode - nStartChar + 3;
      for (; charcode < 256; charcode++, nGlyph++) {
        m_GlyphIndex[charcode] = nGlyph;
      }
      return;
    }
    bool bMSUnicode = FT_UseTTCharmap(m_Font.GetFace(), 3, 1);
    bool bMacRoman = false;
    bool bMSSymbol = false;
    if (!bMSUnicode) {
      if (m_Flags & FXFONT_NONSYMBOLIC) {
        bMacRoman = FT_UseTTCharmap(m_Font.GetFace(), 1, 0);
        bMSSymbol = !bMacRoman && FT_UseTTCharmap(m_Font.GetFace(), 3, 0);
      } else {
        bMSSymbol = FT_UseTTCharmap(m_Font.GetFace(), 3, 0);
        bMacRoman = !bMSSymbol && FT_UseTTCharmap(m_Font.GetFace(), 1, 0);
      }
    }
    bool bToUnicode = m_pFontDict->KeyExist("ToUnicode");
    for (int charcode = 0; charcode < 256; charcode++) {
      const FX_CHAR* name =
          GetAdobeCharName(baseEncoding, m_CharNames, charcode);
      if (!name) {
        m_GlyphIndex[charcode] =
            m_pFontFile ? FXFT_Get_Char_Index(m_Font.GetFace(), charcode) : -1;
        continue;
      }
      m_Encoding.m_Unicodes[charcode] = PDF_UnicodeFromAdobeName(name);
      if (bMSSymbol) {
        for (size_t j = 0; j < FX_ArraySize(kPrefix); j++) {
          uint16_t unicode = kPrefix[j] * 256 + charcode;
          m_GlyphIndex[charcode] =
              FXFT_Get_Char_Index(m_Font.GetFace(), unicode);
          if (m_GlyphIndex[charcode]) {
            break;
          }
        }
      } else if (m_Encoding.m_Unicodes[charcode]) {
        if (bMSUnicode) {
          m_GlyphIndex[charcode] = FXFT_Get_Char_Index(
              m_Font.GetFace(), m_Encoding.m_Unicodes[charcode]);
        } else if (bMacRoman) {
          uint32_t maccode = FT_CharCodeFromUnicode(
              FXFT_ENCODING_APPLE_ROMAN, m_Encoding.m_Unicodes[charcode]);
          if (!maccode) {
            m_GlyphIndex[charcode] =
                FXFT_Get_Name_Index(m_Font.GetFace(), (char*)name);
          } else {
            m_GlyphIndex[charcode] =
                FXFT_Get_Char_Index(m_Font.GetFace(), maccode);
          }
        }
      }
      if ((m_GlyphIndex[charcode] == 0 || m_GlyphIndex[charcode] == 0xffff) &&
          name) {
        if (name[0] == '.' && FXSYS_strcmp(name, ".notdef") == 0) {
          m_GlyphIndex[charcode] = FXFT_Get_Char_Index(m_Font.GetFace(), 32);
        } else {
          m_GlyphIndex[charcode] =
              FXFT_Get_Name_Index(m_Font.GetFace(), (char*)name);
          if (m_GlyphIndex[charcode] == 0) {
            if (bToUnicode) {
              CFX_WideString wsUnicode = UnicodeFromCharCode(charcode);
              if (!wsUnicode.IsEmpty()) {
                m_GlyphIndex[charcode] =
                    FXFT_Get_Char_Index(m_Font.GetFace(), wsUnicode[0]);
                m_Encoding.m_Unicodes[charcode] = wsUnicode[0];
              }
            }
            if (m_GlyphIndex[charcode] == 0) {
              m_GlyphIndex[charcode] =
                  FXFT_Get_Char_Index(m_Font.GetFace(), charcode);
            }
          }
        }
      }
    }
    return;
  }
  if (FT_UseTTCharmap(m_Font.GetFace(), 3, 0)) {
    bool bFound = false;
    for (int charcode = 0; charcode < 256; charcode++) {
      for (size_t j = 0; j < FX_ArraySize(kPrefix); j++) {
        uint16_t unicode = kPrefix[j] * 256 + charcode;
        m_GlyphIndex[charcode] = FXFT_Get_Char_Index(m_Font.GetFace(), unicode);
        if (m_GlyphIndex[charcode]) {
          bFound = true;
          break;
        }
      }
    }
    if (bFound) {
      if (baseEncoding != PDFFONT_ENCODING_BUILTIN) {
        for (int charcode = 0; charcode < 256; charcode++) {
          const FX_CHAR* name =
              GetAdobeCharName(baseEncoding, m_CharNames, charcode);
          if (name)
            m_Encoding.m_Unicodes[charcode] = PDF_UnicodeFromAdobeName(name);
        }
      } else if (FT_UseTTCharmap(m_Font.GetFace(), 1, 0)) {
        for (int charcode = 0; charcode < 256; charcode++) {
          m_Encoding.m_Unicodes[charcode] =
              FT_UnicodeFromCharCode(FXFT_ENCODING_APPLE_ROMAN, charcode);
        }
      }
      return;
    }
  }
  if (FT_UseTTCharmap(m_Font.GetFace(), 1, 0)) {
    bool bFound = false;
    for (int charcode = 0; charcode < 256; charcode++) {
      m_GlyphIndex[charcode] = FXFT_Get_Char_Index(m_Font.GetFace(), charcode);
      m_Encoding.m_Unicodes[charcode] =
          FT_UnicodeFromCharCode(FXFT_ENCODING_APPLE_ROMAN, charcode);
      if (m_GlyphIndex[charcode]) {
        bFound = true;
      }
    }
    if (m_pFontFile || bFound)
      return;
  }
  if (FXFT_Select_Charmap(m_Font.GetFace(), FXFT_ENCODING_UNICODE) == 0) {
    bool bFound = false;
    const uint16_t* pUnicodes = PDF_UnicodesForPredefinedCharSet(baseEncoding);
    for (int charcode = 0; charcode < 256; charcode++) {
      if (m_pFontFile) {
        m_Encoding.m_Unicodes[charcode] = charcode;
      } else {
        const FX_CHAR* name = GetAdobeCharName(0, m_CharNames, charcode);
        if (name)
          m_Encoding.m_Unicodes[charcode] = PDF_UnicodeFromAdobeName(name);
        else if (pUnicodes)
          m_Encoding.m_Unicodes[charcode] = pUnicodes[charcode];
      }
      m_GlyphIndex[charcode] = FXFT_Get_Char_Index(
          m_Font.GetFace(), m_Encoding.m_Unicodes[charcode]);
      if (m_GlyphIndex[charcode])
        bFound = true;
    }
    if (bFound)
      return;
  }
  for (int charcode = 0; charcode < 256; charcode++)
    m_GlyphIndex[charcode] = charcode;
}
