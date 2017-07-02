// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pdfwindow/PWL_FontMap.h"

#include <utility>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/font/cpdf_fontencoding.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfdoc/ipvt_fontmap.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

namespace {

const char kDefaultFontName[] = "Helvetica";

const char* const g_sDEStandardFontName[] = {"Courier",
                                             "Courier-Bold",
                                             "Courier-BoldOblique",
                                             "Courier-Oblique",
                                             "Helvetica",
                                             "Helvetica-Bold",
                                             "Helvetica-BoldOblique",
                                             "Helvetica-Oblique",
                                             "Times-Roman",
                                             "Times-Bold",
                                             "Times-Italic",
                                             "Times-BoldItalic",
                                             "Symbol",
                                             "ZapfDingbats"};

}  // namespace

CPWL_FontMap::CPWL_FontMap(CFX_SystemHandler* pSystemHandler)
    : m_pSystemHandler(pSystemHandler) {
  ASSERT(m_pSystemHandler);
}

CPWL_FontMap::~CPWL_FontMap() {
  Empty();
}

CPDF_Document* CPWL_FontMap::GetDocument() {
  if (!m_pPDFDoc) {
    if (CPDF_ModuleMgr::Get()) {
      m_pPDFDoc = pdfium::MakeUnique<CPDF_Document>(nullptr);
      m_pPDFDoc->CreateNewDoc();
    }
  }

  return m_pPDFDoc.get();
}

CPDF_Font* CPWL_FontMap::GetPDFFont(int32_t nFontIndex) {
  if (nFontIndex >= 0 && nFontIndex < pdfium::CollectionSize<int32_t>(m_Data)) {
    if (m_Data[nFontIndex])
      return m_Data[nFontIndex]->pFont;
  }
  return nullptr;
}

CFX_ByteString CPWL_FontMap::GetPDFFontAlias(int32_t nFontIndex) {
  if (nFontIndex >= 0 && nFontIndex < pdfium::CollectionSize<int32_t>(m_Data)) {
    if (m_Data[nFontIndex])
      return m_Data[nFontIndex]->sFontName;
  }
  return CFX_ByteString();
}

bool CPWL_FontMap::KnowWord(int32_t nFontIndex, uint16_t word) {
  if (nFontIndex >= 0 && nFontIndex < pdfium::CollectionSize<int32_t>(m_Data)) {
    if (m_Data[nFontIndex])
      return CharCodeFromUnicode(nFontIndex, word) >= 0;
  }
  return false;
}

int32_t CPWL_FontMap::GetWordFontIndex(uint16_t word,
                                       int32_t nCharset,
                                       int32_t nFontIndex) {
  if (nFontIndex > 0) {
    if (KnowWord(nFontIndex, word))
      return nFontIndex;
  } else {
    if (const CPWL_FontMap_Data* pData = GetFontMapData(0)) {
      if (nCharset == FXFONT_DEFAULT_CHARSET ||
          pData->nCharset == FXFONT_SYMBOL_CHARSET ||
          nCharset == pData->nCharset) {
        if (KnowWord(0, word))
          return 0;
      }
    }
  }

  int32_t nNewFontIndex =
      GetFontIndex(GetNativeFontName(nCharset), nCharset, true);
  if (nNewFontIndex >= 0) {
    if (KnowWord(nNewFontIndex, word))
      return nNewFontIndex;
  }
  nNewFontIndex =
      GetFontIndex("Arial Unicode MS", FXFONT_DEFAULT_CHARSET, false);
  if (nNewFontIndex >= 0) {
    if (KnowWord(nNewFontIndex, word))
      return nNewFontIndex;
  }
  return -1;
}

int32_t CPWL_FontMap::CharCodeFromUnicode(int32_t nFontIndex, uint16_t word) {
  if (nFontIndex < 0 || nFontIndex >= pdfium::CollectionSize<int32_t>(m_Data))
    return -1;

  CPWL_FontMap_Data* pData = m_Data[nFontIndex].get();
  if (!pData || !pData->pFont)
    return -1;

  if (pData->pFont->IsUnicodeCompatible())
    return pData->pFont->CharCodeFromUnicode(word);

  return word < 0xFF ? word : -1;
}

CFX_ByteString CPWL_FontMap::GetNativeFontName(int32_t nCharset) {
  for (const auto& pData : m_NativeFont) {
    if (pData && pData->nCharset == nCharset)
      return pData->sFontName;
  }

  CFX_ByteString sNew = GetNativeFont(nCharset);
  if (sNew.IsEmpty())
    return CFX_ByteString();

  auto pNewData = pdfium::MakeUnique<CPWL_FontMap_Native>();
  pNewData->nCharset = nCharset;
  pNewData->sFontName = sNew;
  m_NativeFont.push_back(std::move(pNewData));
  return sNew;
}

void CPWL_FontMap::Empty() {
  m_Data.clear();
  m_NativeFont.clear();
}

void CPWL_FontMap::Initialize() {
  GetFontIndex(kDefaultFontName, FXFONT_ANSI_CHARSET, false);
}

bool CPWL_FontMap::IsStandardFont(const CFX_ByteString& sFontName) {
  for (size_t i = 0; i < FX_ArraySize(g_sDEStandardFontName); ++i) {
    if (sFontName == g_sDEStandardFontName[i])
      return true;
  }

  return false;
}

int32_t CPWL_FontMap::FindFont(const CFX_ByteString& sFontName,
                               int32_t nCharset) {
  int32_t i = 0;
  for (const auto& pData : m_Data) {
    if (pData &&
        (nCharset == FXFONT_DEFAULT_CHARSET || nCharset == pData->nCharset) &&
        (sFontName.IsEmpty() || pData->sFontName == sFontName)) {
      return i;
    }
    ++i;
  }
  return -1;
}

int32_t CPWL_FontMap::GetFontIndex(const CFX_ByteString& sFontName,
                                   int32_t nCharset,
                                   bool bFind) {
  int32_t nFontIndex = FindFont(EncodeFontAlias(sFontName, nCharset), nCharset);
  if (nFontIndex >= 0)
    return nFontIndex;

  CFX_ByteString sAlias;
  CPDF_Font* pFont = nullptr;
  if (bFind)
    pFont = FindFontSameCharset(sAlias, nCharset);

  if (!pFont) {
    CFX_ByteString sTemp = sFontName;
    pFont = AddFontToDocument(GetDocument(), sTemp, nCharset);
    sAlias = EncodeFontAlias(sTemp, nCharset);
  }
  AddedFont(pFont, sAlias);
  return AddFontData(pFont, sAlias, nCharset);
}

CPDF_Font* CPWL_FontMap::FindFontSameCharset(CFX_ByteString& sFontAlias,
                                             int32_t nCharset) {
  return nullptr;
}

int32_t CPWL_FontMap::AddFontData(CPDF_Font* pFont,
                                  const CFX_ByteString& sFontAlias,
                                  int32_t nCharset) {
  auto pNewData = pdfium::MakeUnique<CPWL_FontMap_Data>();
  pNewData->pFont = pFont;
  pNewData->sFontName = sFontAlias;
  pNewData->nCharset = nCharset;
  m_Data.push_back(std::move(pNewData));
  return pdfium::CollectionSize<int32_t>(m_Data) - 1;
}

void CPWL_FontMap::AddedFont(CPDF_Font* pFont,
                             const CFX_ByteString& sFontAlias) {}

CFX_ByteString CPWL_FontMap::GetNativeFont(int32_t nCharset) {
  if (nCharset == FXFONT_DEFAULT_CHARSET)
    nCharset = GetNativeCharset();

  CFX_ByteString sFontName = GetDefaultFontByCharset(nCharset);
  if (!m_pSystemHandler->FindNativeTrueTypeFont(sFontName))
    return CFX_ByteString();

  return sFontName;
}

CPDF_Font* CPWL_FontMap::AddFontToDocument(CPDF_Document* pDoc,
                                           CFX_ByteString& sFontName,
                                           uint8_t nCharset) {
  if (IsStandardFont(sFontName))
    return AddStandardFont(pDoc, sFontName);

  return AddSystemFont(pDoc, sFontName, nCharset);
}

CPDF_Font* CPWL_FontMap::AddStandardFont(CPDF_Document* pDoc,
                                         CFX_ByteString& sFontName) {
  if (!pDoc)
    return nullptr;

  CPDF_Font* pFont = nullptr;

  if (sFontName == "ZapfDingbats") {
    pFont = pDoc->AddStandardFont(sFontName.c_str(), nullptr);
  } else {
    CPDF_FontEncoding fe(PDFFONT_ENCODING_WINANSI);
    pFont = pDoc->AddStandardFont(sFontName.c_str(), &fe);
  }

  return pFont;
}

CPDF_Font* CPWL_FontMap::AddSystemFont(CPDF_Document* pDoc,
                                       CFX_ByteString& sFontName,
                                       uint8_t nCharset) {
  if (!pDoc)
    return nullptr;

  if (sFontName.IsEmpty())
    sFontName = GetNativeFont(nCharset);
  if (nCharset == FXFONT_DEFAULT_CHARSET)
    nCharset = GetNativeCharset();

  return m_pSystemHandler->AddNativeTrueTypeFontToPDF(pDoc, sFontName,
                                                      nCharset);
}

CFX_ByteString CPWL_FontMap::EncodeFontAlias(const CFX_ByteString& sFontName,
                                             int32_t nCharset) {
  CFX_ByteString sPostfix;
  sPostfix.Format("_%02X", nCharset);
  return EncodeFontAlias(sFontName) + sPostfix;
}

CFX_ByteString CPWL_FontMap::EncodeFontAlias(const CFX_ByteString& sFontName) {
  CFX_ByteString sRet = sFontName;
  sRet.Remove(' ');
  return sRet;
}

const CPWL_FontMap_Data* CPWL_FontMap::GetFontMapData(int32_t nIndex) const {
  if (nIndex < 0 || nIndex >= pdfium::CollectionSize<int32_t>(m_Data))
    return nullptr;

  return m_Data[nIndex].get();
}

int32_t CPWL_FontMap::GetNativeCharset() {
  uint8_t nCharset = FXFONT_ANSI_CHARSET;
  int32_t iCodePage = FXSYS_GetACP();
  switch (iCodePage) {
    case 932:  // Japan
      nCharset = FXFONT_SHIFTJIS_CHARSET;
      break;
    case 936:  // Chinese (PRC, Singapore)
      nCharset = FXFONT_GB2312_CHARSET;
      break;
    case 950:  // Chinese (Taiwan; Hong Kong SAR, PRC)
      nCharset = FXFONT_GB2312_CHARSET;
      break;
    case 1252:  // Windows 3.1 Latin 1 (US, Western Europe)
      nCharset = FXFONT_ANSI_CHARSET;
      break;
    case 874:  // Thai
      nCharset = FXFONT_THAI_CHARSET;
      break;
    case 949:  // Korean
      nCharset = FXFONT_HANGUL_CHARSET;
      break;
    case 1200:  // Unicode (BMP of ISO 10646)
      nCharset = FXFONT_ANSI_CHARSET;
      break;
    case 1250:  // Windows 3.1 Eastern European
      nCharset = FXFONT_EASTEUROPE_CHARSET;
      break;
    case 1251:  // Windows 3.1 Cyrillic
      nCharset = FXFONT_RUSSIAN_CHARSET;
      break;
    case 1253:  // Windows 3.1 Greek
      nCharset = FXFONT_GREEK_CHARSET;
      break;
    case 1254:  // Windows 3.1 Turkish
      nCharset = FXFONT_TURKISH_CHARSET;
      break;
    case 1255:  // Hebrew
      nCharset = FXFONT_HEBREW_CHARSET;
      break;
    case 1256:  // Arabic
      nCharset = FXFONT_ARABIC_CHARSET;
      break;
    case 1257:  // Baltic
      nCharset = FXFONT_BALTIC_CHARSET;
      break;
    case 1258:  // Vietnamese
      nCharset = FXFONT_VIETNAMESE_CHARSET;
      break;
    case 1361:  // Korean(Johab)
      nCharset = FXFONT_JOHAB_CHARSET;
      break;
  }
  return nCharset;
}

const FPDF_CharsetFontMap CPWL_FontMap::defaultTTFMap[] = {
    {FXFONT_ANSI_CHARSET, "Helvetica"},
    {FXFONT_GB2312_CHARSET, "SimSun"},
    {FXFONT_CHINESEBIG5_CHARSET, "MingLiU"},
    {FXFONT_SHIFTJIS_CHARSET, "MS Gothic"},
    {FXFONT_HANGUL_CHARSET, "Batang"},
    {FXFONT_RUSSIAN_CHARSET, "Arial"},
#if _FXM_PLATFORM_ == _FXM_PLATFORM_LINUX_ || \
    _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    {FXFONT_EASTEUROPE_CHARSET, "Arial"},
#else
    {FXFONT_EASTEUROPE_CHARSET, "Tahoma"},
#endif
    {FXFONT_ARABIC_CHARSET, "Arial"},
    {-1, nullptr}};

CFX_ByteString CPWL_FontMap::GetDefaultFontByCharset(int32_t nCharset) {
  int i = 0;
  while (defaultTTFMap[i].charset != -1) {
    if (nCharset == defaultTTFMap[i].charset)
      return defaultTTFMap[i].fontname;
    ++i;
  }
  return "";
}

int32_t CPWL_FontMap::CharSetFromUnicode(uint16_t word, int32_t nOldCharset) {
  // to avoid CJK Font to show ASCII
  if (word < 0x7F)
    return FXFONT_ANSI_CHARSET;
  // follow the old charset
  if (nOldCharset != FXFONT_DEFAULT_CHARSET)
    return nOldCharset;

  // find new charset
  if ((word >= 0x4E00 && word <= 0x9FA5) ||
      (word >= 0xE7C7 && word <= 0xE7F3) ||
      (word >= 0x3000 && word <= 0x303F) ||
      (word >= 0x2000 && word <= 0x206F)) {
    return FXFONT_GB2312_CHARSET;
  }

  if (((word >= 0x3040) && (word <= 0x309F)) ||
      ((word >= 0x30A0) && (word <= 0x30FF)) ||
      ((word >= 0x31F0) && (word <= 0x31FF)) ||
      ((word >= 0xFF00) && (word <= 0xFFEF))) {
    return FXFONT_SHIFTJIS_CHARSET;
  }

  if (((word >= 0xAC00) && (word <= 0xD7AF)) ||
      ((word >= 0x1100) && (word <= 0x11FF)) ||
      ((word >= 0x3130) && (word <= 0x318F))) {
    return FXFONT_HANGUL_CHARSET;
  }

  if (word >= 0x0E00 && word <= 0x0E7F)
    return FXFONT_THAI_CHARSET;

  if ((word >= 0x0370 && word <= 0x03FF) || (word >= 0x1F00 && word <= 0x1FFF))
    return FXFONT_GREEK_CHARSET;

  if ((word >= 0x0600 && word <= 0x06FF) || (word >= 0xFB50 && word <= 0xFEFC))
    return FXFONT_ARABIC_CHARSET;

  if (word >= 0x0590 && word <= 0x05FF)
    return FXFONT_HEBREW_CHARSET;

  if (word >= 0x0400 && word <= 0x04FF)
    return FXFONT_RUSSIAN_CHARSET;

  if (word >= 0x0100 && word <= 0x024F)
    return FXFONT_EASTEUROPE_CHARSET;

  if (word >= 0x1E00 && word <= 0x1EFF)
    return FXFONT_VIETNAMESE_CHARSET;

  return FXFONT_ANSI_CHARSET;
}
