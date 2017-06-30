// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/cpdf_font.h"

#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/font/cpdf_fontencoding.h"
#include "core/fpdfapi/font/cpdf_truetypefont.h"
#include "core/fpdfapi/font/cpdf_type1font.h"
#include "core/fpdfapi/font/cpdf_type3font.h"
#include "core/fpdfapi/font/font_int.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxge/fx_freetype.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

namespace {

const uint8_t kChineseFontNames[][5] = {{0xCB, 0xCE, 0xCC, 0xE5, 0x00},
                                        {0xBF, 0xAC, 0xCC, 0xE5, 0x00},
                                        {0xBA, 0xDA, 0xCC, 0xE5, 0x00},
                                        {0xB7, 0xC2, 0xCB, 0xCE, 0x00},
                                        {0xD0, 0xC2, 0xCB, 0xCE, 0x00}};

void GetPredefinedEncoding(const CFX_ByteString& value, int* basemap) {
  if (value == "WinAnsiEncoding")
    *basemap = PDFFONT_ENCODING_WINANSI;
  else if (value == "MacRomanEncoding")
    *basemap = PDFFONT_ENCODING_MACROMAN;
  else if (value == "MacExpertEncoding")
    *basemap = PDFFONT_ENCODING_MACEXPERT;
  else if (value == "PDFDocEncoding")
    *basemap = PDFFONT_ENCODING_PDFDOC;
}

}  // namespace

CPDF_Font::CPDF_Font()
    : m_pFontFile(nullptr),
      m_pFontDict(nullptr),
      m_bToUnicodeLoaded(false),
      m_Flags(0),
      m_StemV(0),
      m_Ascent(0),
      m_Descent(0),
      m_ItalicAngle(0) {}

CPDF_Font::~CPDF_Font() {
  if (m_pFontFile) {
    m_pDocument->GetPageData()->ReleaseFontFileStreamAcc(
        m_pFontFile->GetStream()->AsStream());
  }
}

bool CPDF_Font::IsType1Font() const {
  return false;
}

bool CPDF_Font::IsTrueTypeFont() const {
  return false;
}

bool CPDF_Font::IsType3Font() const {
  return false;
}

bool CPDF_Font::IsCIDFont() const {
  return false;
}

const CPDF_Type1Font* CPDF_Font::AsType1Font() const {
  return nullptr;
}

CPDF_Type1Font* CPDF_Font::AsType1Font() {
  return nullptr;
}

const CPDF_TrueTypeFont* CPDF_Font::AsTrueTypeFont() const {
  return nullptr;
}

CPDF_TrueTypeFont* CPDF_Font::AsTrueTypeFont() {
  return nullptr;
}

const CPDF_Type3Font* CPDF_Font::AsType3Font() const {
  return nullptr;
}

CPDF_Type3Font* CPDF_Font::AsType3Font() {
  return nullptr;
}

const CPDF_CIDFont* CPDF_Font::AsCIDFont() const {
  return nullptr;
}

CPDF_CIDFont* CPDF_Font::AsCIDFont() {
  return nullptr;
}

bool CPDF_Font::IsUnicodeCompatible() const {
  return false;
}

int CPDF_Font::CountChar(const FX_CHAR* pString, int size) const {
  return size;
}

int CPDF_Font::GlyphFromCharCodeExt(uint32_t charcode) {
  return GlyphFromCharCode(charcode, nullptr);
}

bool CPDF_Font::IsVertWriting() const {
  const CPDF_CIDFont* pCIDFont = AsCIDFont();
  return pCIDFont ? pCIDFont->IsVertWriting() : m_Font.IsVertical();
}

int CPDF_Font::AppendChar(FX_CHAR* buf, uint32_t charcode) const {
  *buf = static_cast<FX_CHAR>(charcode);
  return 1;
}

void CPDF_Font::AppendChar(CFX_ByteString& str, uint32_t charcode) const {
  char buf[4];
  int len = AppendChar(buf, charcode);
  if (len == 1) {
    str += buf[0];
  } else {
    str += CFX_ByteString(buf, len);
  }
}

CFX_WideString CPDF_Font::UnicodeFromCharCode(uint32_t charcode) const {
  if (!m_bToUnicodeLoaded)
    LoadUnicodeMap();

  return m_pToUnicodeMap ? m_pToUnicodeMap->Lookup(charcode) : CFX_WideString();
}

uint32_t CPDF_Font::CharCodeFromUnicode(FX_WCHAR unicode) const {
  if (!m_bToUnicodeLoaded)
    LoadUnicodeMap();

  return m_pToUnicodeMap ? m_pToUnicodeMap->ReverseLookup(unicode) : 0;
}

void CPDF_Font::LoadFontDescriptor(CPDF_Dictionary* pFontDesc) {
  m_Flags = pFontDesc->GetIntegerFor("Flags", FXFONT_NONSYMBOLIC);
  int ItalicAngle = 0;
  bool bExistItalicAngle = false;
  if (pFontDesc->KeyExist("ItalicAngle")) {
    ItalicAngle = pFontDesc->GetIntegerFor("ItalicAngle");
    bExistItalicAngle = true;
  }
  if (ItalicAngle < 0) {
    m_Flags |= FXFONT_ITALIC;
    m_ItalicAngle = ItalicAngle;
  }
  bool bExistStemV = false;
  if (pFontDesc->KeyExist("StemV")) {
    m_StemV = pFontDesc->GetIntegerFor("StemV");
    bExistStemV = true;
  }
  bool bExistAscent = false;
  if (pFontDesc->KeyExist("Ascent")) {
    m_Ascent = pFontDesc->GetIntegerFor("Ascent");
    bExistAscent = true;
  }
  bool bExistDescent = false;
  if (pFontDesc->KeyExist("Descent")) {
    m_Descent = pFontDesc->GetIntegerFor("Descent");
    bExistDescent = true;
  }
  bool bExistCapHeight = false;
  if (pFontDesc->KeyExist("CapHeight"))
    bExistCapHeight = true;
  if (bExistItalicAngle && bExistAscent && bExistCapHeight && bExistDescent &&
      bExistStemV) {
    m_Flags |= FXFONT_USEEXTERNATTR;
  }
  if (m_Descent > 10)
    m_Descent = -m_Descent;
  CPDF_Array* pBBox = pFontDesc->GetArrayFor("FontBBox");
  if (pBBox) {
    m_FontBBox.left = pBBox->GetIntegerAt(0);
    m_FontBBox.bottom = pBBox->GetIntegerAt(1);
    m_FontBBox.right = pBBox->GetIntegerAt(2);
    m_FontBBox.top = pBBox->GetIntegerAt(3);
  }

  CPDF_Stream* pFontFile = pFontDesc->GetStreamFor("FontFile");
  if (!pFontFile)
    pFontFile = pFontDesc->GetStreamFor("FontFile2");
  if (!pFontFile)
    pFontFile = pFontDesc->GetStreamFor("FontFile3");
  if (!pFontFile)
    return;

  m_pFontFile = m_pDocument->LoadFontFile(pFontFile);
  if (!m_pFontFile)
    return;

  const uint8_t* pFontData = m_pFontFile->GetData();
  uint32_t dwFontSize = m_pFontFile->GetSize();
  if (!m_Font.LoadEmbedded(pFontData, dwFontSize)) {
    m_pDocument->GetPageData()->ReleaseFontFileStreamAcc(
        m_pFontFile->GetStream()->AsStream());
    m_pFontFile = nullptr;
  }
}

void CPDF_Font::CheckFontMetrics() {
  if (m_FontBBox.top == 0 && m_FontBBox.bottom == 0 && m_FontBBox.left == 0 &&
      m_FontBBox.right == 0) {
    FXFT_Face face = m_Font.GetFace();
    if (face) {
      m_FontBBox.left = TT2PDF(FXFT_Get_Face_xMin(face), face);
      m_FontBBox.bottom = TT2PDF(FXFT_Get_Face_yMin(face), face);
      m_FontBBox.right = TT2PDF(FXFT_Get_Face_xMax(face), face);
      m_FontBBox.top = TT2PDF(FXFT_Get_Face_yMax(face), face);
      m_Ascent = TT2PDF(FXFT_Get_Face_Ascender(face), face);
      m_Descent = TT2PDF(FXFT_Get_Face_Descender(face), face);
    } else {
      bool bFirst = true;
      for (int i = 0; i < 256; i++) {
        FX_RECT rect = GetCharBBox(i);
        if (rect.left == rect.right) {
          continue;
        }
        if (bFirst) {
          m_FontBBox = rect;
          bFirst = false;
        } else {
          if (m_FontBBox.top < rect.top) {
            m_FontBBox.top = rect.top;
          }
          if (m_FontBBox.right < rect.right) {
            m_FontBBox.right = rect.right;
          }
          if (m_FontBBox.left > rect.left) {
            m_FontBBox.left = rect.left;
          }
          if (m_FontBBox.bottom > rect.bottom) {
            m_FontBBox.bottom = rect.bottom;
          }
        }
      }
    }
  }
  if (m_Ascent == 0 && m_Descent == 0) {
    FX_RECT rect = GetCharBBox('A');
    m_Ascent = rect.bottom == rect.top ? m_FontBBox.top : rect.top;
    rect = GetCharBBox('g');
    m_Descent = rect.bottom == rect.top ? m_FontBBox.bottom : rect.bottom;
  }
}

void CPDF_Font::LoadUnicodeMap() const {
  m_bToUnicodeLoaded = true;
  CPDF_Stream* pStream = m_pFontDict->GetStreamFor("ToUnicode");
  if (!pStream) {
    return;
  }
  m_pToUnicodeMap = pdfium::MakeUnique<CPDF_ToUnicodeMap>();
  m_pToUnicodeMap->Load(pStream);
}

int CPDF_Font::GetStringWidth(const FX_CHAR* pString, int size) {
  int offset = 0;
  int width = 0;
  while (offset < size) {
    uint32_t charcode = GetNextChar(pString, size, offset);
    width += GetCharWidthF(charcode);
  }
  return width;
}

CPDF_Font* CPDF_Font::GetStockFont(CPDF_Document* pDoc,
                                   const CFX_ByteStringC& name) {
  CFX_ByteString fontname(name);
  int font_id = PDF_GetStandardFontName(&fontname);
  if (font_id < 0)
    return nullptr;

  CPDF_FontGlobals* pFontGlobals =
      CPDF_ModuleMgr::Get()->GetPageModule()->GetFontGlobals();
  CPDF_Font* pFont = pFontGlobals->Find(pDoc, font_id);
  if (pFont)
    return pFont;

  CPDF_Dictionary* pDict = new CPDF_Dictionary(pDoc->GetByteStringPool());
  pDict->SetNewFor<CPDF_Name>("Type", "Font");
  pDict->SetNewFor<CPDF_Name>("Subtype", "Type1");
  pDict->SetNewFor<CPDF_Name>("BaseFont", fontname);
  pDict->SetNewFor<CPDF_Name>("Encoding", "WinAnsiEncoding");
  return pFontGlobals->Set(pDoc, font_id, CPDF_Font::Create(nullptr, pDict));
}

std::unique_ptr<CPDF_Font> CPDF_Font::Create(CPDF_Document* pDoc,
                                             CPDF_Dictionary* pFontDict) {
  CFX_ByteString type = pFontDict->GetStringFor("Subtype");
  std::unique_ptr<CPDF_Font> pFont;
  if (type == "TrueType") {
    CFX_ByteString tag = pFontDict->GetStringFor("BaseFont").Left(4);
    for (size_t i = 0; i < FX_ArraySize(kChineseFontNames); ++i) {
      if (tag == CFX_ByteString(kChineseFontNames[i], 4)) {
        CPDF_Dictionary* pFontDesc = pFontDict->GetDictFor("FontDescriptor");
        if (!pFontDesc || !pFontDesc->KeyExist("FontFile2"))
          pFont = pdfium::MakeUnique<CPDF_CIDFont>();
        break;
      }
    }
    if (!pFont)
      pFont = pdfium::MakeUnique<CPDF_TrueTypeFont>();
  } else if (type == "Type3") {
    pFont = pdfium::MakeUnique<CPDF_Type3Font>();
  } else if (type == "Type0") {
    pFont = pdfium::MakeUnique<CPDF_CIDFont>();
  } else {
    pFont = pdfium::MakeUnique<CPDF_Type1Font>();
  }
  pFont->m_pFontDict = pFontDict;
  pFont->m_pDocument = pDoc;
  pFont->m_BaseFont = pFontDict->GetStringFor("BaseFont");
  return pFont->Load() ? std::move(pFont) : nullptr;
}

uint32_t CPDF_Font::GetNextChar(const FX_CHAR* pString,
                                int nStrLen,
                                int& offset) const {
  if (offset < 0 || nStrLen < 1) {
    return 0;
  }
  uint8_t ch = offset < nStrLen ? pString[offset++] : pString[nStrLen - 1];
  return static_cast<uint32_t>(ch);
}

void CPDF_Font::LoadPDFEncoding(CPDF_Object* pEncoding,
                                int& iBaseEncoding,
                                std::vector<CFX_ByteString>* pCharNames,
                                bool bEmbedded,
                                bool bTrueType) {
  if (!pEncoding) {
    if (m_BaseFont == "Symbol") {
      iBaseEncoding = bTrueType ? PDFFONT_ENCODING_MS_SYMBOL
                                : PDFFONT_ENCODING_ADOBE_SYMBOL;
    } else if (!bEmbedded && iBaseEncoding == PDFFONT_ENCODING_BUILTIN) {
      iBaseEncoding = PDFFONT_ENCODING_WINANSI;
    }
    return;
  }
  if (pEncoding->IsName()) {
    if (iBaseEncoding == PDFFONT_ENCODING_ADOBE_SYMBOL ||
        iBaseEncoding == PDFFONT_ENCODING_ZAPFDINGBATS) {
      return;
    }
    if ((m_Flags & FXFONT_SYMBOLIC) && m_BaseFont == "Symbol") {
      if (!bTrueType)
        iBaseEncoding = PDFFONT_ENCODING_ADOBE_SYMBOL;
      return;
    }
    CFX_ByteString bsEncoding = pEncoding->GetString();
    if (bsEncoding.Compare("MacExpertEncoding") == 0) {
      bsEncoding = "WinAnsiEncoding";
    }
    GetPredefinedEncoding(bsEncoding, &iBaseEncoding);
    return;
  }

  CPDF_Dictionary* pDict = pEncoding->AsDictionary();
  if (!pDict)
    return;

  if (iBaseEncoding != PDFFONT_ENCODING_ADOBE_SYMBOL &&
      iBaseEncoding != PDFFONT_ENCODING_ZAPFDINGBATS) {
    CFX_ByteString bsEncoding = pDict->GetStringFor("BaseEncoding");
    if (bsEncoding.Compare("MacExpertEncoding") == 0 && bTrueType) {
      bsEncoding = "WinAnsiEncoding";
    }
    GetPredefinedEncoding(bsEncoding, &iBaseEncoding);
  }
  if ((!bEmbedded || bTrueType) && iBaseEncoding == PDFFONT_ENCODING_BUILTIN)
    iBaseEncoding = PDFFONT_ENCODING_STANDARD;

  CPDF_Array* pDiffs = pDict->GetArrayFor("Differences");
  if (!pDiffs)
    return;

  pCharNames->resize(256);
  uint32_t cur_code = 0;
  for (uint32_t i = 0; i < pDiffs->GetCount(); i++) {
    CPDF_Object* pElement = pDiffs->GetDirectObjectAt(i);
    if (!pElement)
      continue;

    if (CPDF_Name* pName = pElement->AsName()) {
      if (cur_code < 256)
        (*pCharNames)[cur_code] = pName->GetString();
      cur_code++;
    } else {
      cur_code = pElement->GetInteger();
    }
  }
}

bool CPDF_Font::IsStandardFont() const {
  if (!IsType1Font())
    return false;
  if (m_pFontFile)
    return false;
  if (AsType1Font()->GetBase14Font() < 0)
    return false;
  return true;
}

const FX_CHAR* CPDF_Font::GetAdobeCharName(
    int iBaseEncoding,
    const std::vector<CFX_ByteString>& charnames,
    int charcode) {
  if (charcode < 0 || charcode >= 256) {
    ASSERT(false);
    return nullptr;
  }

  if (!charnames.empty() && !charnames[charcode].IsEmpty())
    return charnames[charcode].c_str();

  const FX_CHAR* name = nullptr;
  if (iBaseEncoding)
    name = PDF_CharNameFromPredefinedCharSet(iBaseEncoding, charcode);
  return name && name[0] ? name : nullptr;
}

uint32_t CPDF_Font::FallbackFontFromCharcode(uint32_t charcode) {
  if (m_FontFallbacks.empty()) {
    m_FontFallbacks.push_back(pdfium::MakeUnique<CFX_Font>());
    m_FontFallbacks[0]->LoadSubst("Arial", IsTrueTypeFont(), m_Flags,
                                  m_StemV * 5, m_ItalicAngle, 0,
                                  IsVertWriting());
  }
  return 0;
}

int CPDF_Font::FallbackGlyphFromCharcode(int fallbackFont, uint32_t charcode) {
  if (fallbackFont < 0 ||
      fallbackFont >= pdfium::CollectionSize<int>(m_FontFallbacks)) {
    return -1;
  }
  int glyph =
      FXFT_Get_Char_Index(m_FontFallbacks[fallbackFont]->GetFace(), charcode);
  if (glyph == 0 || glyph == 0xffff)
    return -1;
  return glyph;
}
