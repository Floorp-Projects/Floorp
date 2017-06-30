// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_fontmapper.h"

#include <memory>
#include <utility>
#include <vector>

#include "core/fxge/cfx_substfont.h"
#include "core/fxge/fx_font.h"
#include "core/fxge/ifx_systemfontinfo.h"

#include "third_party/base/stl_util.h"

#define FX_FONT_STYLE_None 0x00
#define FX_FONT_STYLE_Bold 0x01
#define FX_FONT_STYLE_Italic 0x02
#define FX_FONT_STYLE_BoldBold 0x04

namespace {

const int kNumStandardFonts = 14;

const FX_CHAR* const g_Base14FontNames[kNumStandardFonts] = {
    "Courier",
    "Courier-Bold",
    "Courier-BoldOblique",
    "Courier-Oblique",
    "Helvetica",
    "Helvetica-Bold",
    "Helvetica-BoldOblique",
    "Helvetica-Oblique",
    "Times-Roman",
    "Times-Bold",
    "Times-BoldItalic",
    "Times-Italic",
    "Symbol",
    "ZapfDingbats",
};

const struct AltFontName {
  const FX_CHAR* m_pName;
  int m_Index;
} g_AltFontNames[] = {
    {"Arial", 4},
    {"Arial,Bold", 5},
    {"Arial,BoldItalic", 6},
    {"Arial,Italic", 7},
    {"Arial-Bold", 5},
    {"Arial-BoldItalic", 6},
    {"Arial-BoldItalicMT", 6},
    {"Arial-BoldMT", 5},
    {"Arial-Italic", 7},
    {"Arial-ItalicMT", 7},
    {"ArialBold", 5},
    {"ArialBoldItalic", 6},
    {"ArialItalic", 7},
    {"ArialMT", 4},
    {"ArialMT,Bold", 5},
    {"ArialMT,BoldItalic", 6},
    {"ArialMT,Italic", 7},
    {"ArialRoundedMTBold", 5},
    {"Courier", 0},
    {"Courier,Bold", 1},
    {"Courier,BoldItalic", 2},
    {"Courier,Italic", 3},
    {"Courier-Bold", 1},
    {"Courier-BoldOblique", 2},
    {"Courier-Oblique", 3},
    {"CourierBold", 1},
    {"CourierBoldItalic", 2},
    {"CourierItalic", 3},
    {"CourierNew", 0},
    {"CourierNew,Bold", 1},
    {"CourierNew,BoldItalic", 2},
    {"CourierNew,Italic", 3},
    {"CourierNew-Bold", 1},
    {"CourierNew-BoldItalic", 2},
    {"CourierNew-Italic", 3},
    {"CourierNewBold", 1},
    {"CourierNewBoldItalic", 2},
    {"CourierNewItalic", 3},
    {"CourierNewPS-BoldItalicMT", 2},
    {"CourierNewPS-BoldMT", 1},
    {"CourierNewPS-ItalicMT", 3},
    {"CourierNewPSMT", 0},
    {"CourierStd", 0},
    {"CourierStd-Bold", 1},
    {"CourierStd-BoldOblique", 2},
    {"CourierStd-Oblique", 3},
    {"Helvetica", 4},
    {"Helvetica,Bold", 5},
    {"Helvetica,BoldItalic", 6},
    {"Helvetica,Italic", 7},
    {"Helvetica-Bold", 5},
    {"Helvetica-BoldItalic", 6},
    {"Helvetica-BoldOblique", 6},
    {"Helvetica-Italic", 7},
    {"Helvetica-Oblique", 7},
    {"HelveticaBold", 5},
    {"HelveticaBoldItalic", 6},
    {"HelveticaItalic", 7},
    {"Symbol", 12},
    {"SymbolMT", 12},
    {"Times-Bold", 9},
    {"Times-BoldItalic", 10},
    {"Times-Italic", 11},
    {"Times-Roman", 8},
    {"TimesBold", 9},
    {"TimesBoldItalic", 10},
    {"TimesItalic", 11},
    {"TimesNewRoman", 8},
    {"TimesNewRoman,Bold", 9},
    {"TimesNewRoman,BoldItalic", 10},
    {"TimesNewRoman,Italic", 11},
    {"TimesNewRoman-Bold", 9},
    {"TimesNewRoman-BoldItalic", 10},
    {"TimesNewRoman-Italic", 11},
    {"TimesNewRomanBold", 9},
    {"TimesNewRomanBoldItalic", 10},
    {"TimesNewRomanItalic", 11},
    {"TimesNewRomanPS", 8},
    {"TimesNewRomanPS-Bold", 9},
    {"TimesNewRomanPS-BoldItalic", 10},
    {"TimesNewRomanPS-BoldItalicMT", 10},
    {"TimesNewRomanPS-BoldMT", 9},
    {"TimesNewRomanPS-Italic", 11},
    {"TimesNewRomanPS-ItalicMT", 11},
    {"TimesNewRomanPSMT", 8},
    {"TimesNewRomanPSMT,Bold", 9},
    {"TimesNewRomanPSMT,BoldItalic", 10},
    {"TimesNewRomanPSMT,Italic", 11},
    {"ZapfDingbats", 13},
};

const struct AltFontFamily {
  const FX_CHAR* m_pFontName;
  const FX_CHAR* m_pFontFamily;
} g_AltFontFamilies[] = {
    {"AGaramondPro", "Adobe Garamond Pro"},
    {"BankGothicBT-Medium", "BankGothic Md BT"},
    {"ForteMT", "Forte"},
};

const struct FX_FontStyle {
  const FX_CHAR* style;
  int32_t len;
} g_FontStyles[] = {
    {"Bold", 4}, {"Italic", 6}, {"BoldItalic", 10}, {"Reg", 3}, {"Regular", 7},
};

const struct CODEPAGE_MAP {
  uint16_t codepage;
  uint8_t charset;
} g_Codepage2CharsetTable[] = {
    {0, 1},      {42, 2},     {437, 254},  {850, 255},  {874, 222},
    {932, 128},  {936, 134},  {949, 129},  {950, 136},  {1250, 238},
    {1251, 204}, {1252, 0},   {1253, 161}, {1254, 162}, {1255, 177},
    {1256, 178}, {1257, 186}, {1258, 163}, {1361, 130}, {10000, 77},
    {10001, 78}, {10002, 81}, {10003, 79}, {10004, 84}, {10005, 83},
    {10006, 85}, {10007, 89}, {10008, 80}, {10021, 87}, {10029, 88},
    {10081, 86},
};

int CompareFontFamilyString(const void* key, const void* element) {
  CFX_ByteString str_key((const FX_CHAR*)key);
  const AltFontFamily* family = reinterpret_cast<const AltFontFamily*>(element);
  if (str_key.Find(family->m_pFontName) != -1)
    return 0;
  return FXSYS_stricmp(reinterpret_cast<const FX_CHAR*>(key),
                       family->m_pFontName);
}

int CompareString(const void* key, const void* element) {
  return FXSYS_stricmp(reinterpret_cast<const FX_CHAR*>(key),
                       reinterpret_cast<const AltFontName*>(element)->m_pName);
}

CFX_ByteString TT_NormalizeName(const FX_CHAR* family) {
  CFX_ByteString norm(family);
  norm.Remove(' ');
  norm.Remove('-');
  norm.Remove(',');
  int pos = norm.Find('+');
  if (pos > 0)
    norm = norm.Left(pos);
  norm.MakeLower();
  return norm;
}

uint8_t GetCharsetFromCodePage(uint16_t codepage) {
  const CODEPAGE_MAP* pEnd =
      g_Codepage2CharsetTable + FX_ArraySize(g_Codepage2CharsetTable);
  const CODEPAGE_MAP* pCharmap =
      std::lower_bound(g_Codepage2CharsetTable, pEnd, codepage,
                       [](const CODEPAGE_MAP& charset, uint16_t page) {
                         return charset.codepage < page;
                       });
  if (pCharmap < pEnd && codepage == pCharmap->codepage)
    return pCharmap->charset;
  return FXFONT_DEFAULT_CHARSET;
}

CFX_ByteString GetFontFamily(CFX_ByteString fontName, int nStyle) {
  if (fontName.Find("Script") >= 0) {
    if ((nStyle & FX_FONT_STYLE_Bold) == FX_FONT_STYLE_Bold)
      fontName = "ScriptMTBold";
    else if (fontName.Find("Palace") >= 0)
      fontName = "PalaceScriptMT";
    else if (fontName.Find("French") >= 0)
      fontName = "FrenchScriptMT";
    else if (fontName.Find("FreeStyle") >= 0)
      fontName = "FreeStyleScript";
    return fontName;
  }
  AltFontFamily* found = reinterpret_cast<AltFontFamily*>(FXSYS_bsearch(
      fontName.c_str(), g_AltFontFamilies, FX_ArraySize(g_AltFontFamilies),
      sizeof(AltFontFamily), CompareFontFamilyString));
  return found ? CFX_ByteString(found->m_pFontFamily) : fontName;
}

CFX_ByteString ParseStyle(const FX_CHAR* pStyle, int iLen, int iIndex) {
  CFX_ByteTextBuf buf;
  if (!iLen || iLen <= iIndex)
    return buf.MakeString();
  while (iIndex < iLen) {
    if (pStyle[iIndex] == ',')
      break;
    buf.AppendChar(pStyle[iIndex]);
    ++iIndex;
  }
  return buf.MakeString();
}

int32_t GetStyleType(const CFX_ByteString& bsStyle, bool bReverse) {
  int32_t iLen = bsStyle.GetLength();
  if (!iLen)
    return -1;
  int iSize = FX_ArraySize(g_FontStyles);
  const FX_FontStyle* pStyle = nullptr;
  for (int i = iSize - 1; i >= 0; --i) {
    pStyle = g_FontStyles + i;
    if (!pStyle || pStyle->len > iLen)
      continue;

    if (bReverse) {
      if (bsStyle.Right(pStyle->len).Compare(pStyle->style) == 0)
        return i;
    } else {
      if (bsStyle.Left(pStyle->len).Compare(pStyle->style) == 0)
        return i;
    }
  }
  return -1;
}

bool CheckSupportThirdPartFont(CFX_ByteString name, int& PitchFamily) {
  if (name == "MyriadPro") {
    PitchFamily &= ~FXFONT_FF_ROMAN;
    return true;
  }
  return false;
}

void UpdatePitchFamily(uint32_t flags, int& PitchFamily) {
  if (flags & FXFONT_SERIF)
    PitchFamily |= FXFONT_FF_ROMAN;
  if (flags & FXFONT_SCRIPT)
    PitchFamily |= FXFONT_FF_SCRIPT;
  if (flags & FXFONT_FIXED_PITCH)
    PitchFamily |= FXFONT_FF_FIXEDPITCH;
}

}  // namespace

CFX_FontMapper::CFX_FontMapper(CFX_FontMgr* mgr)
    : m_bListLoaded(false), m_pFontMgr(mgr) {
  m_MMFaces[0] = nullptr;
  m_MMFaces[1] = nullptr;
  FXSYS_memset(m_FoxitFaces, 0, sizeof(m_FoxitFaces));
}

CFX_FontMapper::~CFX_FontMapper() {
  for (size_t i = 0; i < FX_ArraySize(m_FoxitFaces); ++i) {
    if (m_FoxitFaces[i])
      FXFT_Done_Face(m_FoxitFaces[i]);
  }
  if (m_MMFaces[0])
    FXFT_Done_Face(m_MMFaces[0]);
  if (m_MMFaces[1])
    FXFT_Done_Face(m_MMFaces[1]);
}

void CFX_FontMapper::SetSystemFontInfo(
    std::unique_ptr<IFX_SystemFontInfo> pFontInfo) {
  if (!pFontInfo)
    return;

  m_pFontInfo = std::move(pFontInfo);
}

CFX_ByteString CFX_FontMapper::GetPSNameFromTT(void* hFont) {
  if (!m_pFontInfo)
    return CFX_ByteString();

  uint32_t size = m_pFontInfo->GetFontData(hFont, kTableNAME, nullptr, 0);
  if (!size)
    return CFX_ByteString();

  std::vector<uint8_t> buffer(size);
  uint8_t* buffer_ptr = buffer.data();
  uint32_t bytes_read =
      m_pFontInfo->GetFontData(hFont, kTableNAME, buffer_ptr, size);
  return bytes_read == size ? GetNameFromTT(buffer_ptr, bytes_read, 6)
                            : CFX_ByteString();
}

void CFX_FontMapper::AddInstalledFont(const CFX_ByteString& name, int charset) {
  if (!m_pFontInfo)
    return;

  m_FaceArray.push_back({name, static_cast<uint32_t>(charset)});
  if (name == m_LastFamily)
    return;

  const uint8_t* ptr = name.raw_str();
  bool bLocalized = false;
  for (int i = 0; i < name.GetLength(); i++) {
    if (ptr[i] > 0x80) {
      bLocalized = true;
      break;
    }
  }

  if (bLocalized) {
    void* hFont = m_pFontInfo->GetFont(name.c_str());
    if (!hFont) {
      int iExact;
      hFont = m_pFontInfo->MapFont(0, 0, FXFONT_DEFAULT_CHARSET, 0,
                                   name.c_str(), iExact);
      if (!hFont)
        return;
    }

    CFX_ByteString new_name = GetPSNameFromTT(hFont);
    if (!new_name.IsEmpty())
      m_LocalizedTTFonts.push_back(std::make_pair(new_name, name));
    m_pFontInfo->DeleteFont(hFont);
  }
  m_InstalledTTFonts.push_back(name);
  m_LastFamily = name;
}

void CFX_FontMapper::LoadInstalledFonts() {
  if (!m_pFontInfo || m_bListLoaded)
    return;

  m_pFontInfo->EnumFontList(this);
  m_bListLoaded = true;
}

CFX_ByteString CFX_FontMapper::MatchInstalledFonts(
    const CFX_ByteString& norm_name) {
  LoadInstalledFonts();
  int i;
  for (i = pdfium::CollectionSize<int>(m_InstalledTTFonts) - 1; i >= 0; i--) {
    CFX_ByteString norm1 = TT_NormalizeName(m_InstalledTTFonts[i].c_str());
    if (norm1 == norm_name)
      return m_InstalledTTFonts[i];
  }
  for (i = pdfium::CollectionSize<int>(m_LocalizedTTFonts) - 1; i >= 0; i--) {
    CFX_ByteString norm1 =
        TT_NormalizeName(m_LocalizedTTFonts[i].first.c_str());
    if (norm1 == norm_name)
      return m_LocalizedTTFonts[i].second;
  }
  return CFX_ByteString();
}

FXFT_Face CFX_FontMapper::UseInternalSubst(CFX_SubstFont* pSubstFont,
                                           int iBaseFont,
                                           int italic_angle,
                                           int weight,
                                           int picthfamily) {
  if (iBaseFont < kNumStandardFonts) {
    if (m_FoxitFaces[iBaseFont])
      return m_FoxitFaces[iBaseFont];
    const uint8_t* pFontData = nullptr;
    uint32_t size = 0;
    if (m_pFontMgr->GetBuiltinFont(iBaseFont, &pFontData, &size)) {
      m_FoxitFaces[iBaseFont] = m_pFontMgr->GetFixedFace(pFontData, size, 0);
      return m_FoxitFaces[iBaseFont];
    }
  }
  pSubstFont->m_SubstFlags |= FXFONT_SUBST_MM;
  pSubstFont->m_ItalicAngle = italic_angle;
  if (weight)
    pSubstFont->m_Weight = weight;
  if (picthfamily & FXFONT_FF_ROMAN) {
    pSubstFont->m_Weight = pSubstFont->m_Weight * 4 / 5;
    pSubstFont->m_Family = "Chrome Serif";
    if (m_MMFaces[1])
      return m_MMFaces[1];
    const uint8_t* pFontData = nullptr;
    uint32_t size = 0;
    m_pFontMgr->GetBuiltinFont(14, &pFontData, &size);
    m_MMFaces[1] = m_pFontMgr->GetFixedFace(pFontData, size, 0);
    return m_MMFaces[1];
  }
  pSubstFont->m_Family = "Chrome Sans";
  if (m_MMFaces[0])
    return m_MMFaces[0];
  const uint8_t* pFontData = nullptr;
  uint32_t size = 0;
  m_pFontMgr->GetBuiltinFont(15, &pFontData, &size);
  m_MMFaces[0] = m_pFontMgr->GetFixedFace(pFontData, size, 0);
  return m_MMFaces[0];
}

FXFT_Face CFX_FontMapper::FindSubstFont(const CFX_ByteString& name,
                                        bool bTrueType,
                                        uint32_t flags,
                                        int weight,
                                        int italic_angle,
                                        int WindowCP,
                                        CFX_SubstFont* pSubstFont) {
  if (!(flags & FXFONT_USEEXTERNATTR)) {
    weight = FXFONT_FW_NORMAL;
    italic_angle = 0;
  }
  CFX_ByteString SubstName = name;
  SubstName.Remove(' ');
  if (bTrueType && name[0] == '@')
    SubstName = name.Mid(1);
  PDF_GetStandardFontName(&SubstName);
  if (SubstName == "Symbol" && !bTrueType) {
    pSubstFont->m_Family = "Chrome Symbol";
    pSubstFont->m_Charset = FXFONT_SYMBOL_CHARSET;
    return UseInternalSubst(pSubstFont, 12, italic_angle, weight, 0);
  }
  if (SubstName == "ZapfDingbats") {
    pSubstFont->m_Family = "Chrome Dingbats";
    pSubstFont->m_Charset = FXFONT_SYMBOL_CHARSET;
    return UseInternalSubst(pSubstFont, 13, italic_angle, weight, 0);
  }
  int iBaseFont = 0;
  CFX_ByteString family;
  CFX_ByteString style;
  bool bHasComma = false;
  bool bHasHyphen = false;
  int find = SubstName.Find(",", 0);
  if (find >= 0) {
    family = SubstName.Left(find);
    PDF_GetStandardFontName(&family);
    style = SubstName.Mid(find + 1);
    bHasComma = true;
  } else {
    family = SubstName;
  }
  for (; iBaseFont < 12; iBaseFont++) {
    if (family == CFX_ByteStringC(g_Base14FontNames[iBaseFont]))
      break;
  }
  int PitchFamily = 0;
  bool bItalic = false;
  uint32_t nStyle = 0;
  bool bStyleAvail = false;
  if (iBaseFont < 12) {
    if ((iBaseFont % 4) == 1 || (iBaseFont % 4) == 2)
      nStyle |= FX_FONT_STYLE_Bold;
    if ((iBaseFont % 4) / 2)
      nStyle |= FX_FONT_STYLE_Italic;
    if (iBaseFont < 4)
      PitchFamily |= FXFONT_FF_FIXEDPITCH;
    if (iBaseFont >= 8)
      PitchFamily |= FXFONT_FF_ROMAN;
  } else {
    iBaseFont = kNumStandardFonts;
    if (!bHasComma) {
      find = family.ReverseFind('-');
      if (find >= 0) {
        style = family.Mid(find + 1);
        family = family.Left(find);
        bHasHyphen = true;
      }
    }
    if (!bHasHyphen) {
      int nLen = family.GetLength();
      int32_t nRet = GetStyleType(family, true);
      if (nRet > -1) {
        family = family.Left(nLen - g_FontStyles[nRet].len);
        if (nRet == 0)
          nStyle |= FX_FONT_STYLE_Bold;
        else if (nRet == 1)
          nStyle |= FX_FONT_STYLE_Italic;
        else if (nRet == 2)
          nStyle |= (FX_FONT_STYLE_Bold | FX_FONT_STYLE_Italic);
      }
    }
    UpdatePitchFamily(flags, PitchFamily);
  }
  if (!style.IsEmpty()) {
    int nLen = style.GetLength();
    const FX_CHAR* pStyle = style.c_str();
    int i = 0;
    bool bFirstItem = true;
    CFX_ByteString buf;
    while (i < nLen) {
      buf = ParseStyle(pStyle, nLen, i);
      int32_t nRet = GetStyleType(buf, false);
      if ((i && !bStyleAvail) || (!i && nRet < 0)) {
        family = SubstName;
        iBaseFont = kNumStandardFonts;
        break;
      }
      if (nRet >= 0) {
        bStyleAvail = true;
      }
      if (nRet == 1) {
        if (bFirstItem) {
          nStyle |= FX_FONT_STYLE_Italic;
        } else {
          family = SubstName;
          iBaseFont = kNumStandardFonts;
        }
        break;
      }
      if (nRet == 0) {
        if (nStyle & FX_FONT_STYLE_Bold)
          nStyle |= FX_FONT_STYLE_BoldBold;
        else
          nStyle |= FX_FONT_STYLE_Bold;
        bFirstItem = false;
      } else if (nRet == 2) {
        nStyle |= FX_FONT_STYLE_Italic;
        if (nStyle & FX_FONT_STYLE_Bold)
          nStyle |= FX_FONT_STYLE_BoldBold;
        else
          nStyle |= FX_FONT_STYLE_Bold;
        bFirstItem = false;
      }
      i += buf.GetLength() + 1;
    }
  }
  weight = weight ? weight : FXFONT_FW_NORMAL;
  int old_weight = weight;
  if (nStyle) {
    weight =
        nStyle & FX_FONT_STYLE_BoldBold
            ? 900
            : (nStyle & FX_FONT_STYLE_Bold ? FXFONT_FW_BOLD : FXFONT_FW_NORMAL);
  }
  if (nStyle & FX_FONT_STYLE_Italic)
    bItalic = true;
  int iExact = 0;
  int Charset = FXFONT_ANSI_CHARSET;
  if (WindowCP)
    Charset = GetCharsetFromCodePage(WindowCP);
  else if (iBaseFont == kNumStandardFonts && (flags & FXFONT_SYMBOLIC))
    Charset = FXFONT_SYMBOL_CHARSET;
  bool bCJK =
      (Charset == FXFONT_SHIFTJIS_CHARSET || Charset == FXFONT_GB2312_CHARSET ||
       Charset == FXFONT_HANGUL_CHARSET ||
       Charset == FXFONT_CHINESEBIG5_CHARSET);
  if (!m_pFontInfo) {
    return UseInternalSubst(pSubstFont, iBaseFont, italic_angle, old_weight,
                            PitchFamily);
  }
  family = GetFontFamily(family, nStyle);
  CFX_ByteString match = MatchInstalledFonts(TT_NormalizeName(family.c_str()));
  if (match.IsEmpty() && family != SubstName &&
      (!bHasComma && (!bHasHyphen || (bHasHyphen && !bStyleAvail)))) {
    match = MatchInstalledFonts(TT_NormalizeName(SubstName.c_str()));
  }
  if (match.IsEmpty() && iBaseFont >= kNumStandardFonts) {
    if (!bCJK) {
      if (!CheckSupportThirdPartFont(family, PitchFamily)) {
        bItalic = italic_angle != 0;
        weight = old_weight;
      }
#if _FXM_PLATFORM_ == _FXM_PLATFORM_LINUX_
      if (SubstName.Find("Narrow") > 0 || SubstName.Find("Condensed") > 0)
        family = "LiberationSansNarrow";
#elif _FXM_PLATFORM_ == _FXM_PLATFORM_ANDROID_
      if (family.Find("Narrow") > 0 || family.Find("Condensed") > 0)
        family = "RobotoCondensed";
#else
      if (family.Find("Narrow") > 0 || family.Find("Condensed") > 0)
        family = "ArialNarrow";
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_LINUX_
    } else {
      pSubstFont->m_bSubstCJK = true;
      if (nStyle)
        pSubstFont->m_WeightCJK = nStyle ? weight : FXFONT_FW_NORMAL;
      if (nStyle & FX_FONT_STYLE_Italic)
        pSubstFont->m_bItalicCJK = true;
    }
  } else {
    italic_angle = 0;
    weight =
        nStyle & FX_FONT_STYLE_BoldBold
            ? 900
            : (nStyle & FX_FONT_STYLE_Bold ? FXFONT_FW_BOLD : FXFONT_FW_NORMAL);
  }
  if (!match.IsEmpty() || iBaseFont < kNumStandardFonts) {
    if (!match.IsEmpty())
      family = match;
    if (iBaseFont < kNumStandardFonts) {
      if (nStyle && !(iBaseFont % 4)) {
        if ((nStyle & 0x3) == 1)
          iBaseFont += 1;
        if ((nStyle & 0x3) == 2)
          iBaseFont += 3;
        if ((nStyle & 0x3) == 3)
          iBaseFont += 2;
      }
      family = g_Base14FontNames[iBaseFont];
    }
  } else {
    if (flags & FXFONT_ITALIC)
      bItalic = true;
  }
  iExact = !match.IsEmpty();
  void* hFont = m_pFontInfo->MapFont(weight, bItalic, Charset, PitchFamily,
                                     family.c_str(), iExact);
  if (iExact)
    pSubstFont->m_SubstFlags |= FXFONT_SUBST_EXACT;
  if (!hFont) {
#ifdef PDF_ENABLE_XFA
    if (flags & FXFONT_EXACTMATCH)
      return nullptr;
#endif  // PDF_ENABLE_XFA
    if (bCJK) {
      bItalic = italic_angle != 0;
      weight = old_weight;
    }
    if (!match.IsEmpty()) {
      hFont = m_pFontInfo->GetFont(match.c_str());
      if (!hFont) {
        return UseInternalSubst(pSubstFont, iBaseFont, italic_angle, old_weight,
                                PitchFamily);
      }
    } else {
      if (Charset == FXFONT_SYMBOL_CHARSET) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_ || \
    _FXM_PLATFORM_ == _FXM_PLATFORM_ANDROID_
        if (SubstName == "Symbol") {
          pSubstFont->m_Family = "Chrome Symbol";
          pSubstFont->m_Charset = FXFONT_SYMBOL_CHARSET;
          return UseInternalSubst(pSubstFont, 12, italic_angle, old_weight,
                                  PitchFamily);
        }
#endif
        return FindSubstFont(family, bTrueType, flags & ~FXFONT_SYMBOLIC,
                             weight, italic_angle, 0, pSubstFont);
      }
      if (Charset == FXFONT_ANSI_CHARSET) {
        return UseInternalSubst(pSubstFont, iBaseFont, italic_angle, old_weight,
                                PitchFamily);
      }

      auto it =
          std::find_if(m_FaceArray.begin(), m_FaceArray.end(),
                       [Charset](const FaceData& face) {
                         return face.charset == static_cast<uint32_t>(Charset);
                       });
      if (it == m_FaceArray.end()) {
        return UseInternalSubst(pSubstFont, iBaseFont, italic_angle, old_weight,
                                PitchFamily);
      }
      hFont = m_pFontInfo->GetFont(it->name.c_str());
    }
  }
  if (!hFont)
    return nullptr;

  m_pFontInfo->GetFaceName(hFont, SubstName);
  if (Charset == FXFONT_DEFAULT_CHARSET)
    m_pFontInfo->GetFontCharset(hFont, Charset);
  uint32_t ttc_size = m_pFontInfo->GetFontData(hFont, kTableTTCF, nullptr, 0);
  uint32_t font_size = m_pFontInfo->GetFontData(hFont, 0, nullptr, 0);
  if (font_size == 0 && ttc_size == 0) {
    m_pFontInfo->DeleteFont(hFont);
    return nullptr;
  }
  FXFT_Face face = nullptr;
  if (ttc_size)
    face = GetCachedTTCFace(hFont, kTableTTCF, ttc_size, font_size);
  else
    face = GetCachedFace(hFont, SubstName, weight, bItalic, font_size);
  if (!face) {
    m_pFontInfo->DeleteFont(hFont);
    return nullptr;
  }
  pSubstFont->m_Family = SubstName;
  pSubstFont->m_Charset = Charset;
  bool bNeedUpdateWeight = false;
  if (FXFT_Is_Face_Bold(face))
    bNeedUpdateWeight = weight != FXFONT_FW_BOLD;
  else
    bNeedUpdateWeight = weight != FXFONT_FW_NORMAL;
  if (bNeedUpdateWeight)
    pSubstFont->m_Weight = weight;
  if (bItalic && !FXFT_Is_Face_Italic(face)) {
    if (italic_angle == 0)
      italic_angle = -12;
    else if (FXSYS_abs(italic_angle) < 5)
      italic_angle = 0;
    pSubstFont->m_ItalicAngle = italic_angle;
  }
  m_pFontInfo->DeleteFont(hFont);
  return face;
}

#ifdef PDF_ENABLE_XFA
FXFT_Face CFX_FontMapper::FindSubstFontByUnicode(uint32_t dwUnicode,
                                                 uint32_t flags,
                                                 int weight,
                                                 int italic_angle) {
  if (!m_pFontInfo)
    return nullptr;

  bool bItalic = (flags & FXFONT_ITALIC) != 0;
  int PitchFamily = 0;
  UpdatePitchFamily(flags, PitchFamily);
  void* hFont =
      m_pFontInfo->MapFontByUnicode(dwUnicode, weight, bItalic, PitchFamily);
  if (!hFont)
    return nullptr;

  uint32_t ttc_size = m_pFontInfo->GetFontData(hFont, 0x74746366, nullptr, 0);
  uint32_t font_size = m_pFontInfo->GetFontData(hFont, 0, nullptr, 0);
  if (font_size == 0 && ttc_size == 0) {
    m_pFontInfo->DeleteFont(hFont);
    return nullptr;
  }
  FXFT_Face face = nullptr;
  if (ttc_size) {
    face = GetCachedTTCFace(hFont, 0x74746366, ttc_size, font_size);
  } else {
    CFX_ByteString SubstName;
    m_pFontInfo->GetFaceName(hFont, SubstName);
    face = GetCachedFace(hFont, SubstName, weight, bItalic, font_size);
  }
  m_pFontInfo->DeleteFont(hFont);
  return face;
}
#endif  // PDF_ENABLE_XFA

int CFX_FontMapper::GetFaceSize() const {
  return pdfium::CollectionSize<int>(m_FaceArray);
}

bool CFX_FontMapper::IsBuiltinFace(const FXFT_Face face) const {
  for (size_t i = 0; i < MM_FACE_COUNT; ++i) {
    if (m_MMFaces[i] == face)
      return true;
  }
  for (size_t i = 0; i < FOXIT_FACE_COUNT; ++i) {
    if (m_FoxitFaces[i] == face)
      return true;
  }
  return false;
}

FXFT_Face CFX_FontMapper::GetCachedTTCFace(void* hFont,
                                           const uint32_t tableTTCF,
                                           uint32_t ttc_size,
                                           uint32_t font_size) {
  FXFT_Face face;
  uint8_t buffer[1024];
  m_pFontInfo->GetFontData(hFont, tableTTCF, buffer, FX_ArraySize(buffer));
  uint32_t* pBuffer = reinterpret_cast<uint32_t*>(buffer);
  uint32_t checksum = 0;
  for (int i = 0; i < 256; i++)
    checksum += pBuffer[i];
  uint8_t* pFontData;
  face = m_pFontMgr->GetCachedTTCFace(ttc_size, checksum, ttc_size - font_size,
                                      pFontData);
  if (!face) {
    pFontData = FX_Alloc(uint8_t, ttc_size);
    m_pFontInfo->GetFontData(hFont, tableTTCF, pFontData, ttc_size);
    face = m_pFontMgr->AddCachedTTCFace(ttc_size, checksum, pFontData, ttc_size,
                                        ttc_size - font_size);
  }
  return face;
}

FXFT_Face CFX_FontMapper::GetCachedFace(void* hFont,
                                        CFX_ByteString SubstName,
                                        int weight,
                                        bool bItalic,
                                        uint32_t font_size) {
  FXFT_Face face;
  uint8_t* pFontData;
  face = m_pFontMgr->GetCachedFace(SubstName, weight, bItalic, pFontData);
  if (!face) {
    pFontData = FX_Alloc(uint8_t, font_size);
    m_pFontInfo->GetFontData(hFont, 0, pFontData, font_size);
    face =
        m_pFontMgr->AddCachedFace(SubstName, weight, bItalic, pFontData,
                                  font_size, m_pFontInfo->GetFaceIndex(hFont));
  }
  return face;
}

int PDF_GetStandardFontName(CFX_ByteString* name) {
  AltFontName* found = static_cast<AltFontName*>(
      FXSYS_bsearch(name->c_str(), g_AltFontNames, FX_ArraySize(g_AltFontNames),
                    sizeof(AltFontName), CompareString));
  if (!found)
    return -1;

  *name = g_Base14FontNames[found->m_Index];
  return found->m_Index;
}
