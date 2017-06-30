// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/cpdf_type1font.h"

#include "core/fpdfapi/font/font_int.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/fx_freetype.h"

#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
#include "core/fxge/apple/apple_int.h"
#endif

namespace {

#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
struct GlyphNameMap {
  const FX_CHAR* m_pStrAdobe;
  const FX_CHAR* m_pStrUnicode;
};

const GlyphNameMap g_GlyphNameSubsts[] = {{"ff", "uniFB00"},
                                          {"ffi", "uniFB03"},
                                          {"ffl", "uniFB04"},
                                          {"fi", "uniFB01"},
                                          {"fl", "uniFB02"}};

int compareString(const void* key, const void* element) {
  return FXSYS_stricmp(static_cast<const FX_CHAR*>(key),
                       static_cast<const GlyphNameMap*>(element)->m_pStrAdobe);
}

const FX_CHAR* GlyphNameRemap(const FX_CHAR* pStrAdobe) {
  const GlyphNameMap* found = static_cast<const GlyphNameMap*>(FXSYS_bsearch(
      pStrAdobe, g_GlyphNameSubsts, FX_ArraySize(g_GlyphNameSubsts),
      sizeof(GlyphNameMap), compareString));
  return found ? found->m_pStrUnicode : nullptr;
}

#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_

bool FT_UseType1Charmap(FXFT_Face face) {
  if (FXFT_Get_Face_CharmapCount(face) == 0) {
    return false;
  }
  if (FXFT_Get_Face_CharmapCount(face) == 1 &&
      FXFT_Get_Charmap_Encoding(FXFT_Get_Face_Charmaps(face)[0]) ==
          FXFT_ENCODING_UNICODE) {
    return false;
  }
  if (FXFT_Get_Charmap_Encoding(FXFT_Get_Face_Charmaps(face)[0]) ==
      FXFT_ENCODING_UNICODE) {
    FXFT_Set_Charmap(face, FXFT_Get_Face_Charmaps(face)[1]);
  } else {
    FXFT_Set_Charmap(face, FXFT_Get_Face_Charmaps(face)[0]);
  }
  return true;
}

}  // namespace

CPDF_Type1Font::CPDF_Type1Font() : m_Base14Font(-1) {}

bool CPDF_Type1Font::IsType1Font() const {
  return true;
}

const CPDF_Type1Font* CPDF_Type1Font::AsType1Font() const {
  return this;
}

CPDF_Type1Font* CPDF_Type1Font::AsType1Font() {
  return this;
}

bool CPDF_Type1Font::Load() {
  m_Base14Font = PDF_GetStandardFontName(&m_BaseFont);
  if (m_Base14Font >= 0) {
    CPDF_Dictionary* pFontDesc = m_pFontDict->GetDictFor("FontDescriptor");
    if (pFontDesc && pFontDesc->KeyExist("Flags"))
      m_Flags = pFontDesc->GetIntegerFor("Flags");
    else
      m_Flags = m_Base14Font >= 12 ? FXFONT_SYMBOLIC : FXFONT_NONSYMBOLIC;

    if (m_Base14Font < 4) {
      for (int i = 0; i < 256; i++)
        m_CharWidth[i] = 600;
    }
    if (m_Base14Font == 12)
      m_BaseEncoding = PDFFONT_ENCODING_ADOBE_SYMBOL;
    else if (m_Base14Font == 13)
      m_BaseEncoding = PDFFONT_ENCODING_ZAPFDINGBATS;
    else if (m_Flags & FXFONT_NONSYMBOLIC)
      m_BaseEncoding = PDFFONT_ENCODING_STANDARD;
  }
  return LoadCommon();
}

int CPDF_Type1Font::GlyphFromCharCodeExt(uint32_t charcode) {
  if (charcode > 0xff) {
    return -1;
  }
  int index = m_ExtGID[(uint8_t)charcode];
  if (index == 0xffff) {
    return -1;
  }
  return index;
}

void CPDF_Type1Font::LoadGlyphMap() {
  if (!m_Font.GetFace())
    return;

#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  bool bCoreText = true;
  CQuartz2D& quartz2d =
      static_cast<CApplePlatform*>(CFX_GEModule::Get()->GetPlatformData())
          ->m_quartz2d;
  if (!m_Font.GetPlatformFont()) {
    if (m_Font.GetPsName() == "DFHeiStd-W5")
      bCoreText = false;

    m_Font.SetPlatformFont(
        quartz2d.CreateFont(m_Font.GetFontData(), m_Font.GetSize()));
    if (!m_Font.GetPlatformFont())
      bCoreText = false;
  }
#endif
  if (!IsEmbedded() && (m_Base14Font < 12) && m_Font.IsTTFont()) {
    if (FT_UseTTCharmap(m_Font.GetFace(), 3, 0)) {
      bool bGotOne = false;
      for (int charcode = 0; charcode < 256; charcode++) {
        const uint8_t prefix[4] = {0x00, 0xf0, 0xf1, 0xf2};
        for (int j = 0; j < 4; j++) {
          uint16_t unicode = prefix[j] * 256 + charcode;
          m_GlyphIndex[charcode] =
              FXFT_Get_Char_Index(m_Font.GetFace(), unicode);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
          CalcExtGID(charcode);
#endif
          if (m_GlyphIndex[charcode]) {
            bGotOne = true;
            break;
          }
        }
      }
      if (bGotOne) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
        if (!bCoreText)
          FXSYS_memcpy(m_ExtGID, m_GlyphIndex, 256);
#endif
        return;
      }
    }
    FXFT_Select_Charmap(m_Font.GetFace(), FXFT_ENCODING_UNICODE);
    if (m_BaseEncoding == 0) {
      m_BaseEncoding = PDFFONT_ENCODING_STANDARD;
    }
    for (int charcode = 0; charcode < 256; charcode++) {
      const FX_CHAR* name =
          GetAdobeCharName(m_BaseEncoding, m_CharNames, charcode);
      if (!name)
        continue;

      m_Encoding.m_Unicodes[charcode] = PDF_UnicodeFromAdobeName(name);
      m_GlyphIndex[charcode] = FXFT_Get_Char_Index(
          m_Font.GetFace(), m_Encoding.m_Unicodes[charcode]);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      CalcExtGID(charcode);
#endif
      if (m_GlyphIndex[charcode] == 0 && FXSYS_strcmp(name, ".notdef") == 0) {
        m_Encoding.m_Unicodes[charcode] = 0x20;
        m_GlyphIndex[charcode] = FXFT_Get_Char_Index(m_Font.GetFace(), 0x20);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
        CalcExtGID(charcode);
#endif
      }
    }
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    if (!bCoreText)
      FXSYS_memcpy(m_ExtGID, m_GlyphIndex, 256);
#endif
    return;
  }
  FT_UseType1Charmap(m_Font.GetFace());
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  if (bCoreText) {
    if (m_Flags & FXFONT_SYMBOLIC) {
      for (int charcode = 0; charcode < 256; charcode++) {
        const FX_CHAR* name =
            GetAdobeCharName(m_BaseEncoding, m_CharNames, charcode);
        if (name) {
          m_Encoding.m_Unicodes[charcode] = PDF_UnicodeFromAdobeName(name);
          m_GlyphIndex[charcode] =
              FXFT_Get_Name_Index(m_Font.GetFace(), (char*)name);
          SetExtGID(name, charcode);
        } else {
          m_GlyphIndex[charcode] =
              FXFT_Get_Char_Index(m_Font.GetFace(), charcode);
          FX_WCHAR unicode = 0;
          if (m_GlyphIndex[charcode]) {
            unicode =
                FT_UnicodeFromCharCode(PDFFONT_ENCODING_STANDARD, charcode);
          }
          FX_CHAR name_glyph[256];
          FXSYS_memset(name_glyph, 0, sizeof(name_glyph));
          FXFT_Get_Glyph_Name(m_Font.GetFace(), m_GlyphIndex[charcode],
                              name_glyph, 256);
          name_glyph[255] = 0;
          if (unicode == 0 && name_glyph[0] != 0) {
            unicode = PDF_UnicodeFromAdobeName(name_glyph);
          }
          m_Encoding.m_Unicodes[charcode] = unicode;
          SetExtGID(name_glyph, charcode);
        }
      }
      return;
    }
    bool bUnicode = false;
    if (0 == FXFT_Select_Charmap(m_Font.GetFace(), FXFT_ENCODING_UNICODE)) {
      bUnicode = true;
    }
    for (int charcode = 0; charcode < 256; charcode++) {
      const FX_CHAR* name =
          GetAdobeCharName(m_BaseEncoding, m_CharNames, charcode);
      if (!name) {
        continue;
      }
      m_Encoding.m_Unicodes[charcode] = PDF_UnicodeFromAdobeName(name);
      const FX_CHAR* pStrUnicode = GlyphNameRemap(name);
      if (pStrUnicode &&
          0 == FXFT_Get_Name_Index(m_Font.GetFace(), (char*)name)) {
        name = pStrUnicode;
      }
      m_GlyphIndex[charcode] =
          FXFT_Get_Name_Index(m_Font.GetFace(), (char*)name);
      SetExtGID(name, charcode);
      if (m_GlyphIndex[charcode] == 0) {
        if (FXSYS_strcmp(name, ".notdef") != 0 &&
            FXSYS_strcmp(name, "space") != 0) {
          m_GlyphIndex[charcode] = FXFT_Get_Char_Index(
              m_Font.GetFace(),
              bUnicode ? m_Encoding.m_Unicodes[charcode] : charcode);
          CalcExtGID(charcode);
        } else {
          m_Encoding.m_Unicodes[charcode] = 0x20;
          m_GlyphIndex[charcode] =
              bUnicode ? FXFT_Get_Char_Index(m_Font.GetFace(), 0x20) : 0xffff;
          CalcExtGID(charcode);
        }
      }
    }
    return;
  }
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  if (m_Flags & FXFONT_SYMBOLIC) {
    for (int charcode = 0; charcode < 256; charcode++) {
      const FX_CHAR* name =
          GetAdobeCharName(m_BaseEncoding, m_CharNames, charcode);
      if (name) {
        m_Encoding.m_Unicodes[charcode] = PDF_UnicodeFromAdobeName(name);
        m_GlyphIndex[charcode] =
            FXFT_Get_Name_Index(m_Font.GetFace(), (char*)name);
      } else {
        m_GlyphIndex[charcode] =
            FXFT_Get_Char_Index(m_Font.GetFace(), charcode);
        if (m_GlyphIndex[charcode]) {
          FX_WCHAR unicode =
              FT_UnicodeFromCharCode(PDFFONT_ENCODING_STANDARD, charcode);
          if (unicode == 0) {
            FX_CHAR name_glyph[256];
            FXSYS_memset(name_glyph, 0, sizeof(name_glyph));
            FXFT_Get_Glyph_Name(m_Font.GetFace(), m_GlyphIndex[charcode],
                                name_glyph, 256);
            name_glyph[255] = 0;
            if (name_glyph[0] != 0) {
              unicode = PDF_UnicodeFromAdobeName(name_glyph);
            }
          }
          m_Encoding.m_Unicodes[charcode] = unicode;
        }
      }
    }
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    if (!bCoreText)
      FXSYS_memcpy(m_ExtGID, m_GlyphIndex, 256);

#endif
    return;
  }
  bool bUnicode = false;
  if (0 == FXFT_Select_Charmap(m_Font.GetFace(), FXFT_ENCODING_UNICODE)) {
    bUnicode = true;
  }
  for (int charcode = 0; charcode < 256; charcode++) {
    const FX_CHAR* name =
        GetAdobeCharName(m_BaseEncoding, m_CharNames, charcode);
    if (!name) {
      continue;
    }
    m_Encoding.m_Unicodes[charcode] = PDF_UnicodeFromAdobeName(name);
    m_GlyphIndex[charcode] = FXFT_Get_Name_Index(m_Font.GetFace(), (char*)name);
    if (m_GlyphIndex[charcode] == 0) {
      if (FXSYS_strcmp(name, ".notdef") != 0 &&
          FXSYS_strcmp(name, "space") != 0) {
        m_GlyphIndex[charcode] = FXFT_Get_Char_Index(
            m_Font.GetFace(),
            bUnicode ? m_Encoding.m_Unicodes[charcode] : charcode);
      } else {
        m_Encoding.m_Unicodes[charcode] = 0x20;
        m_GlyphIndex[charcode] = 0xffff;
      }
    }
  }
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  if (!bCoreText)
    FXSYS_memcpy(m_ExtGID, m_GlyphIndex, 256);
#endif
}

#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
void CPDF_Type1Font::SetExtGID(const FX_CHAR* name, int charcode) {
  CFStringRef name_ct = CFStringCreateWithCStringNoCopy(
      kCFAllocatorDefault, name, kCFStringEncodingASCII, kCFAllocatorNull);
  m_ExtGID[charcode] =
      CGFontGetGlyphWithGlyphName((CGFontRef)m_Font.GetPlatformFont(), name_ct);
  if (name_ct)
    CFRelease(name_ct);
}

void CPDF_Type1Font::CalcExtGID(int charcode) {
  FX_CHAR name_glyph[256];
  FXFT_Get_Glyph_Name(m_Font.GetFace(), m_GlyphIndex[charcode], name_glyph,
                      256);
  name_glyph[255] = 0;
  SetExtGID(name_glyph, charcode);
}
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
