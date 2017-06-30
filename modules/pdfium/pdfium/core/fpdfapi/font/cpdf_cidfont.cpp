// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/cpdf_cidfont.h"

#include <algorithm>
#include <vector>

#include "core/fpdfapi/cmaps/cmap_int.h"
#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/font/cpdf_fontencoding.h"
#include "core/fpdfapi/font/font_int.h"
#include "core/fpdfapi/font/ttgsubtable.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "third_party/base/numerics/safe_math.h"
#include "third_party/base/ptr_util.h"

namespace {

const uint16_t g_CharsetCPs[CIDSET_NUM_SETS] = {0, 936, 950, 932, 949, 1200};

const struct CIDTransform {
  uint16_t cid;
  uint8_t a;
  uint8_t b;
  uint8_t c;
  uint8_t d;
  uint8_t e;
  uint8_t f;
} g_Japan1_VertCIDs[] = {
    {97, 129, 0, 0, 127, 55, 0},     {7887, 127, 0, 0, 127, 76, 89},
    {7888, 127, 0, 0, 127, 79, 94},  {7889, 0, 129, 127, 0, 17, 127},
    {7890, 0, 129, 127, 0, 17, 127}, {7891, 0, 129, 127, 0, 17, 127},
    {7892, 0, 129, 127, 0, 17, 127}, {7893, 0, 129, 127, 0, 17, 127},
    {7894, 0, 129, 127, 0, 17, 127}, {7895, 0, 129, 127, 0, 17, 127},
    {7896, 0, 129, 127, 0, 17, 127}, {7897, 0, 129, 127, 0, 17, 127},
    {7898, 0, 129, 127, 0, 17, 127}, {7899, 0, 129, 127, 0, 17, 104},
    {7900, 0, 129, 127, 0, 17, 127}, {7901, 0, 129, 127, 0, 17, 104},
    {7902, 0, 129, 127, 0, 17, 127}, {7903, 0, 129, 127, 0, 17, 127},
    {7904, 0, 129, 127, 0, 17, 127}, {7905, 0, 129, 127, 0, 17, 114},
    {7906, 0, 129, 127, 0, 17, 127}, {7907, 0, 129, 127, 0, 17, 127},
    {7908, 0, 129, 127, 0, 17, 127}, {7909, 0, 129, 127, 0, 17, 127},
    {7910, 0, 129, 127, 0, 17, 127}, {7911, 0, 129, 127, 0, 17, 127},
    {7912, 0, 129, 127, 0, 17, 127}, {7913, 0, 129, 127, 0, 17, 127},
    {7914, 0, 129, 127, 0, 17, 127}, {7915, 0, 129, 127, 0, 17, 114},
    {7916, 0, 129, 127, 0, 17, 127}, {7917, 0, 129, 127, 0, 17, 127},
    {7918, 127, 0, 0, 127, 18, 25},  {7919, 127, 0, 0, 127, 18, 25},
    {7920, 127, 0, 0, 127, 18, 25},  {7921, 127, 0, 0, 127, 18, 25},
    {7922, 127, 0, 0, 127, 18, 25},  {7923, 127, 0, 0, 127, 18, 25},
    {7924, 127, 0, 0, 127, 18, 25},  {7925, 127, 0, 0, 127, 18, 25},
    {7926, 127, 0, 0, 127, 18, 25},  {7927, 127, 0, 0, 127, 18, 25},
    {7928, 127, 0, 0, 127, 18, 25},  {7929, 127, 0, 0, 127, 18, 25},
    {7930, 127, 0, 0, 127, 18, 25},  {7931, 127, 0, 0, 127, 18, 25},
    {7932, 127, 0, 0, 127, 18, 25},  {7933, 127, 0, 0, 127, 18, 25},
    {7934, 127, 0, 0, 127, 18, 25},  {7935, 127, 0, 0, 127, 18, 25},
    {7936, 127, 0, 0, 127, 18, 25},  {7937, 127, 0, 0, 127, 18, 25},
    {7938, 127, 0, 0, 127, 18, 25},  {7939, 127, 0, 0, 127, 18, 25},
    {8720, 0, 129, 127, 0, 19, 102}, {8721, 0, 129, 127, 0, 13, 127},
    {8722, 0, 129, 127, 0, 19, 108}, {8723, 0, 129, 127, 0, 19, 102},
    {8724, 0, 129, 127, 0, 19, 102}, {8725, 0, 129, 127, 0, 19, 102},
    {8726, 0, 129, 127, 0, 19, 102}, {8727, 0, 129, 127, 0, 19, 102},
    {8728, 0, 129, 127, 0, 19, 114}, {8729, 0, 129, 127, 0, 19, 114},
    {8730, 0, 129, 127, 0, 38, 108}, {8731, 0, 129, 127, 0, 13, 108},
    {8732, 0, 129, 127, 0, 19, 108}, {8733, 0, 129, 127, 0, 19, 108},
    {8734, 0, 129, 127, 0, 19, 108}, {8735, 0, 129, 127, 0, 19, 108},
    {8736, 0, 129, 127, 0, 19, 102}, {8737, 0, 129, 127, 0, 19, 102},
    {8738, 0, 129, 127, 0, 19, 102}, {8739, 0, 129, 127, 0, 19, 102},
    {8740, 0, 129, 127, 0, 19, 102}, {8741, 0, 129, 127, 0, 19, 102},
    {8742, 0, 129, 127, 0, 19, 102}, {8743, 0, 129, 127, 0, 19, 102},
    {8744, 0, 129, 127, 0, 19, 102}, {8745, 0, 129, 127, 0, 19, 102},
    {8746, 0, 129, 127, 0, 19, 114}, {8747, 0, 129, 127, 0, 19, 114},
    {8748, 0, 129, 127, 0, 19, 102}, {8749, 0, 129, 127, 0, 19, 102},
    {8750, 0, 129, 127, 0, 19, 102}, {8751, 0, 129, 127, 0, 19, 102},
    {8752, 0, 129, 127, 0, 19, 102}, {8753, 0, 129, 127, 0, 19, 102},
    {8754, 0, 129, 127, 0, 19, 102}, {8755, 0, 129, 127, 0, 19, 102},
    {8756, 0, 129, 127, 0, 19, 102}, {8757, 0, 129, 127, 0, 19, 102},
    {8758, 0, 129, 127, 0, 19, 102}, {8759, 0, 129, 127, 0, 19, 102},
    {8760, 0, 129, 127, 0, 19, 102}, {8761, 0, 129, 127, 0, 19, 102},
    {8762, 0, 129, 127, 0, 19, 102}, {8763, 0, 129, 127, 0, 19, 102},
    {8764, 0, 129, 127, 0, 19, 102}, {8765, 0, 129, 127, 0, 19, 102},
    {8766, 0, 129, 127, 0, 19, 102}, {8767, 0, 129, 127, 0, 19, 102},
    {8768, 0, 129, 127, 0, 19, 102}, {8769, 0, 129, 127, 0, 19, 102},
    {8770, 0, 129, 127, 0, 19, 102}, {8771, 0, 129, 127, 0, 19, 102},
    {8772, 0, 129, 127, 0, 19, 102}, {8773, 0, 129, 127, 0, 19, 102},
    {8774, 0, 129, 127, 0, 19, 102}, {8775, 0, 129, 127, 0, 19, 102},
    {8776, 0, 129, 127, 0, 19, 102}, {8777, 0, 129, 127, 0, 19, 102},
    {8778, 0, 129, 127, 0, 19, 102}, {8779, 0, 129, 127, 0, 19, 114},
    {8780, 0, 129, 127, 0, 19, 108}, {8781, 0, 129, 127, 0, 19, 114},
    {8782, 0, 129, 127, 0, 13, 114}, {8783, 0, 129, 127, 0, 19, 108},
    {8784, 0, 129, 127, 0, 13, 114}, {8785, 0, 129, 127, 0, 19, 108},
    {8786, 0, 129, 127, 0, 19, 108}, {8787, 0, 129, 127, 0, 19, 108},
    {8788, 0, 129, 127, 0, 19, 108}, {8789, 0, 129, 127, 0, 19, 108},
    {8790, 0, 129, 127, 0, 19, 108}, {8791, 0, 129, 127, 0, 19, 108},
    {8792, 0, 129, 127, 0, 19, 108}, {8793, 0, 129, 127, 0, 19, 108},
    {8794, 0, 129, 127, 0, 19, 108}, {8795, 0, 129, 127, 0, 19, 108},
    {8796, 0, 129, 127, 0, 19, 108}, {8797, 0, 129, 127, 0, 19, 108},
    {8798, 0, 129, 127, 0, 19, 108}, {8799, 0, 129, 127, 0, 19, 108},
    {8800, 0, 129, 127, 0, 19, 108}, {8801, 0, 129, 127, 0, 19, 108},
    {8802, 0, 129, 127, 0, 19, 108}, {8803, 0, 129, 127, 0, 19, 108},
    {8804, 0, 129, 127, 0, 19, 108}, {8805, 0, 129, 127, 0, 19, 108},
    {8806, 0, 129, 127, 0, 19, 108}, {8807, 0, 129, 127, 0, 19, 108},
    {8808, 0, 129, 127, 0, 19, 108}, {8809, 0, 129, 127, 0, 19, 108},
    {8810, 0, 129, 127, 0, 19, 108}, {8811, 0, 129, 127, 0, 19, 114},
    {8812, 0, 129, 127, 0, 19, 102}, {8813, 0, 129, 127, 0, 19, 114},
    {8814, 0, 129, 127, 0, 76, 102}, {8815, 0, 129, 127, 0, 13, 121},
    {8816, 0, 129, 127, 0, 19, 114}, {8817, 0, 129, 127, 0, 19, 127},
    {8818, 0, 129, 127, 0, 19, 114}, {8819, 0, 129, 127, 0, 218, 108},
};

CPDF_FontGlobals* GetFontGlobals() {
  return CPDF_ModuleMgr::Get()->GetPageModule()->GetFontGlobals();
}

#if _FXM_PLATFORM_ != _FXM_PLATFORM_WINDOWS_

bool IsValidEmbeddedCharcodeFromUnicodeCharset(CIDSet charset) {
  switch (charset) {
    case CIDSET_GB1:
    case CIDSET_CNS1:
    case CIDSET_JAPAN1:
    case CIDSET_KOREA1:
      return true;

    default:
      return false;
  }
}

FX_WCHAR EmbeddedUnicodeFromCharcode(const FXCMAP_CMap* pEmbedMap,
                                     CIDSet charset,
                                     uint32_t charcode) {
  if (!IsValidEmbeddedCharcodeFromUnicodeCharset(charset))
    return 0;

  uint16_t cid = FPDFAPI_CIDFromCharCode(pEmbedMap, charcode);
  const auto& codes = GetFontGlobals()->m_EmbeddedToUnicodes[charset];
  if (codes.m_pMap && cid && cid < codes.m_Count)
    return codes.m_pMap[cid];
  return 0;
}

uint32_t EmbeddedCharcodeFromUnicode(const FXCMAP_CMap* pEmbedMap,
                                     CIDSet charset,
                                     FX_WCHAR unicode) {
  if (!IsValidEmbeddedCharcodeFromUnicodeCharset(charset))
    return 0;

  const auto& codes = GetFontGlobals()->m_EmbeddedToUnicodes[charset];
  const uint16_t* pCodes = codes.m_pMap;
  if (!pCodes)
    return 0;

  for (uint32_t i = 0; i < codes.m_Count; ++i) {
    if (pCodes[i] == unicode) {
      uint32_t CharCode = FPDFAPI_CharCodeFromCID(pEmbedMap, i);
      if (CharCode)
        return CharCode;
    }
  }
  return 0;
}

#endif  // _FXM_PLATFORM_ != _FXM_PLATFORM_WINDOWS_

void FT_UseCIDCharmap(FXFT_Face face, int coding) {
  int encoding;
  switch (coding) {
    case CIDCODING_GB:
      encoding = FXFT_ENCODING_GB2312;
      break;
    case CIDCODING_BIG5:
      encoding = FXFT_ENCODING_BIG5;
      break;
    case CIDCODING_JIS:
      encoding = FXFT_ENCODING_SJIS;
      break;
    case CIDCODING_KOREA:
      encoding = FXFT_ENCODING_JOHAB;
      break;
    default:
      encoding = FXFT_ENCODING_UNICODE;
  }
  int err = FXFT_Select_Charmap(face, encoding);
  if (err)
    err = FXFT_Select_Charmap(face, FXFT_ENCODING_UNICODE);
  if (err && FXFT_Get_Face_Charmaps(face))
    FXFT_Set_Charmap(face, *FXFT_Get_Face_Charmaps(face));
}

bool IsMetricForCID(const uint32_t* pEntry, uint16_t CID) {
  return pEntry[0] <= CID && pEntry[1] >= CID;
}

}  // namespace

CPDF_CIDFont::CPDF_CIDFont()
    : m_pCID2UnicodeMap(nullptr),
      m_bCIDIsGID(false),
      m_bAnsiWidthsFixed(false),
      m_bAdobeCourierStd(false) {
  for (size_t i = 0; i < FX_ArraySize(m_CharBBox); ++i)
    m_CharBBox[i] = FX_RECT(-1, -1, -1, -1);
}

CPDF_CIDFont::~CPDF_CIDFont() {}

bool CPDF_CIDFont::IsCIDFont() const {
  return true;
}

const CPDF_CIDFont* CPDF_CIDFont::AsCIDFont() const {
  return this;
}

CPDF_CIDFont* CPDF_CIDFont::AsCIDFont() {
  return this;
}

uint16_t CPDF_CIDFont::CIDFromCharCode(uint32_t charcode) const {
  return m_pCMap ? m_pCMap->CIDFromCharCode(charcode)
                 : static_cast<uint16_t>(charcode);
}

bool CPDF_CIDFont::IsVertWriting() const {
  return m_pCMap && m_pCMap->IsVertWriting();
}

CFX_WideString CPDF_CIDFont::UnicodeFromCharCode(uint32_t charcode) const {
  CFX_WideString str = CPDF_Font::UnicodeFromCharCode(charcode);
  if (!str.IsEmpty())
    return str;
  FX_WCHAR ret = GetUnicodeFromCharCode(charcode);
  return ret ? ret : CFX_WideString();
}

FX_WCHAR CPDF_CIDFont::GetUnicodeFromCharCode(uint32_t charcode) const {
  switch (m_pCMap->m_Coding) {
    case CIDCODING_UCS2:
    case CIDCODING_UTF16:
      return static_cast<FX_WCHAR>(charcode);
    case CIDCODING_CID:
      if (!m_pCID2UnicodeMap || !m_pCID2UnicodeMap->IsLoaded())
        return 0;
      return m_pCID2UnicodeMap->UnicodeFromCID(static_cast<uint16_t>(charcode));
  }
  if (m_pCID2UnicodeMap && m_pCID2UnicodeMap->IsLoaded() && m_pCMap->IsLoaded())
    return m_pCID2UnicodeMap->UnicodeFromCID(CIDFromCharCode(charcode));

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  FX_WCHAR unicode;
  int charsize = 1;
  if (charcode > 255) {
    charcode = (charcode % 256) * 256 + (charcode / 256);
    charsize = 2;
  }
  int ret = FXSYS_MultiByteToWideChar(
      g_CharsetCPs[m_pCMap->m_Coding], 0,
      reinterpret_cast<const FX_CHAR*>(&charcode), charsize, &unicode, 1);
  return ret == 1 ? unicode : 0;
#else
  if (!m_pCMap->m_pEmbedMap)
    return 0;
  return EmbeddedUnicodeFromCharcode(m_pCMap->m_pEmbedMap, m_pCMap->m_Charset,
                                     charcode);
#endif
}

uint32_t CPDF_CIDFont::CharCodeFromUnicode(FX_WCHAR unicode) const {
  uint32_t charcode = CPDF_Font::CharCodeFromUnicode(unicode);
  if (charcode)
    return charcode;
  switch (m_pCMap->m_Coding) {
    case CIDCODING_UNKNOWN:
      return 0;
    case CIDCODING_UCS2:
    case CIDCODING_UTF16:
      return unicode;
    case CIDCODING_CID: {
      if (!m_pCID2UnicodeMap || !m_pCID2UnicodeMap->IsLoaded())
        return 0;
      uint32_t CID = 0;
      while (CID < 65536) {
        FX_WCHAR this_unicode =
            m_pCID2UnicodeMap->UnicodeFromCID(static_cast<uint16_t>(CID));
        if (this_unicode == unicode)
          return CID;
        CID++;
      }
      break;
    }
  }

  if (unicode < 0x80)
    return static_cast<uint32_t>(unicode);
  if (m_pCMap->m_Coding == CIDCODING_CID)
    return 0;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  uint8_t buffer[32];
  int ret = FXSYS_WideCharToMultiByte(
      g_CharsetCPs[m_pCMap->m_Coding], 0, &unicode, 1,
      reinterpret_cast<char*>(buffer), 4, nullptr, nullptr);
  if (ret == 1)
    return buffer[0];
  if (ret == 2)
    return buffer[0] * 256 + buffer[1];
#else
  if (m_pCMap->m_pEmbedMap) {
    return EmbeddedCharcodeFromUnicode(m_pCMap->m_pEmbedMap, m_pCMap->m_Charset,
                                       unicode);
  }
#endif
  return 0;
}

bool CPDF_CIDFont::Load() {
  if (m_pFontDict->GetStringFor("Subtype") == "TrueType") {
    LoadGB2312();
    return true;
  }

  CPDF_Array* pFonts = m_pFontDict->GetArrayFor("DescendantFonts");
  if (!pFonts || pFonts->GetCount() != 1)
    return false;

  CPDF_Dictionary* pCIDFontDict = pFonts->GetDictAt(0);
  if (!pCIDFontDict)
    return false;

  m_BaseFont = pCIDFontDict->GetStringFor("BaseFont");
  if ((m_BaseFont.Compare("CourierStd") == 0 ||
       m_BaseFont.Compare("CourierStd-Bold") == 0 ||
       m_BaseFont.Compare("CourierStd-BoldOblique") == 0 ||
       m_BaseFont.Compare("CourierStd-Oblique") == 0) &&
      !IsEmbedded()) {
    m_bAdobeCourierStd = true;
  }
  CPDF_Dictionary* pFontDesc = pCIDFontDict->GetDictFor("FontDescriptor");
  if (pFontDesc)
    LoadFontDescriptor(pFontDesc);

  CPDF_Object* pEncoding = m_pFontDict->GetDirectObjectFor("Encoding");
  if (!pEncoding)
    return false;

  CFX_ByteString subtype = pCIDFontDict->GetStringFor("Subtype");
  m_bType1 = (subtype == "CIDFontType0");

  CPDF_CMapManager& manager = GetFontGlobals()->m_CMapManager;
  if (pEncoding->IsName()) {
    CFX_ByteString cmap = pEncoding->GetString();
    bool bPromptCJK = m_pFontFile && m_bType1;
    m_pCMap = manager.GetPredefinedCMap(cmap, bPromptCJK);
    if (!m_pCMap)
      return false;
  } else if (CPDF_Stream* pStream = pEncoding->AsStream()) {
    m_pCMap = pdfium::MakeUnique<CPDF_CMap>();
    CPDF_StreamAcc acc;
    acc.LoadAllData(pStream, false);
    m_pCMap->LoadEmbedded(acc.GetData(), acc.GetSize());
  } else {
    return false;
  }

  m_Charset = m_pCMap->m_Charset;
  if (m_Charset == CIDSET_UNKNOWN) {
    CPDF_Dictionary* pCIDInfo = pCIDFontDict->GetDictFor("CIDSystemInfo");
    if (pCIDInfo) {
      m_Charset =
          CharsetFromOrdering(pCIDInfo->GetStringFor("Ordering").AsStringC());
    }
  }
  if (m_Charset != CIDSET_UNKNOWN) {
    bool bPromptCJK = !m_pFontFile && (m_pCMap->m_Coding == CIDCODING_CID ||
                                       pCIDFontDict->KeyExist("W"));
    m_pCID2UnicodeMap = manager.GetCID2UnicodeMap(m_Charset, bPromptCJK);
  }
  if (m_Font.GetFace()) {
    if (m_bType1)
      FXFT_Select_Charmap(m_Font.GetFace(), FXFT_ENCODING_UNICODE);
    else
      FT_UseCIDCharmap(m_Font.GetFace(), m_pCMap->m_Coding);
  }
  m_DefaultWidth = pCIDFontDict->GetIntegerFor("DW", 1000);
  CPDF_Array* pWidthArray = pCIDFontDict->GetArrayFor("W");
  if (pWidthArray)
    LoadMetricsArray(pWidthArray, &m_WidthList, 1);
  if (!IsEmbedded())
    LoadSubstFont();

  if (m_pFontFile || (GetSubstFont()->m_SubstFlags & FXFONT_SUBST_EXACT)) {
    CPDF_Object* pmap = pCIDFontDict->GetDirectObjectFor("CIDToGIDMap");
    if (pmap) {
      if (CPDF_Stream* pStream = pmap->AsStream()) {
        m_pStreamAcc = pdfium::MakeUnique<CPDF_StreamAcc>();
        m_pStreamAcc->LoadAllData(pStream, false);
      } else if (pmap->GetString() == "Identity") {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
        if (m_pFontFile)
          m_bCIDIsGID = true;
#else
        m_bCIDIsGID = true;
#endif
      }
    }
  }

  CheckFontMetrics();
  if (IsVertWriting()) {
    pWidthArray = pCIDFontDict->GetArrayFor("W2");
    if (pWidthArray)
      LoadMetricsArray(pWidthArray, &m_VertMetrics, 3);
    CPDF_Array* pDefaultArray = pCIDFontDict->GetArrayFor("DW2");
    if (pDefaultArray) {
      m_DefaultVY = pDefaultArray->GetIntegerAt(0);
      m_DefaultW1 = pDefaultArray->GetIntegerAt(1);
    } else {
      m_DefaultVY = 880;
      m_DefaultW1 = -1000;
    }
  }
  return true;
}

FX_RECT CPDF_CIDFont::GetCharBBox(uint32_t charcode) {
  if (charcode < 256 && m_CharBBox[charcode].right != -1)
    return m_CharBBox[charcode];

  FX_RECT rect;
  bool bVert = false;
  int glyph_index = GlyphFromCharCode(charcode, &bVert);
  FXFT_Face face = m_Font.GetFace();
  if (face) {
    if (FXFT_Is_Face_Tricky(face)) {
      int err = FXFT_Load_Glyph(face, glyph_index,
                                FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH);
      if (!err) {
        FXFT_BBox cbox;
        FXFT_Glyph glyph;
        err = FXFT_Get_Glyph(((FXFT_Face)face)->glyph, &glyph);
        if (!err) {
          FXFT_Glyph_Get_CBox(glyph, FXFT_GLYPH_BBOX_PIXELS, &cbox);
          int pixel_size_x = ((FXFT_Face)face)->size->metrics.x_ppem;
          int pixel_size_y = ((FXFT_Face)face)->size->metrics.y_ppem;
          if (pixel_size_x == 0 || pixel_size_y == 0) {
            rect = FX_RECT(cbox.xMin, cbox.yMax, cbox.xMax, cbox.yMin);
          } else {
            rect = FX_RECT(cbox.xMin * 1000 / pixel_size_x,
                           cbox.yMax * 1000 / pixel_size_y,
                           cbox.xMax * 1000 / pixel_size_x,
                           cbox.yMin * 1000 / pixel_size_y);
          }
          rect.top = std::min(rect.top,
                              static_cast<int>(FXFT_Get_Face_Ascender(face)));
          rect.bottom = std::max(
              rect.bottom, static_cast<int>(FXFT_Get_Face_Descender(face)));
          FXFT_Done_Glyph(glyph);
        }
      }
    } else {
      int err = FXFT_Load_Glyph(face, glyph_index, FXFT_LOAD_NO_SCALE);
      if (err == 0) {
        rect = FX_RECT(TT2PDF(FXFT_Get_Glyph_HoriBearingX(face), face),
                       TT2PDF(FXFT_Get_Glyph_HoriBearingY(face), face),
                       TT2PDF(FXFT_Get_Glyph_HoriBearingX(face) +
                                  FXFT_Get_Glyph_Width(face),
                              face),
                       TT2PDF(FXFT_Get_Glyph_HoriBearingY(face) -
                                  FXFT_Get_Glyph_Height(face),
                              face));
        rect.top += rect.top / 64;
      }
    }
  }
  if (!m_pFontFile && m_Charset == CIDSET_JAPAN1) {
    uint16_t CID = CIDFromCharCode(charcode);
    const uint8_t* pTransform = GetCIDTransform(CID);
    if (pTransform && !bVert) {
      CFX_Matrix matrix(CIDTransformToFloat(pTransform[0]),
                        CIDTransformToFloat(pTransform[1]),
                        CIDTransformToFloat(pTransform[2]),
                        CIDTransformToFloat(pTransform[3]),
                        CIDTransformToFloat(pTransform[4]) * 1000,
                        CIDTransformToFloat(pTransform[5]) * 1000);
      CFX_FloatRect rect_f(rect);
      matrix.TransformRect(rect_f);
      rect = rect_f.GetOuterRect();
    }
  }
  if (charcode < 256)
    m_CharBBox[charcode] = rect;

  return rect;
}

int CPDF_CIDFont::GetCharWidthF(uint32_t charcode) {
  if (charcode < 0x80 && m_bAnsiWidthsFixed)
    return (charcode >= 32 && charcode < 127) ? 500 : 0;

  uint16_t cid = CIDFromCharCode(charcode);
  size_t size = m_WidthList.size();
  const uint32_t* pList = m_WidthList.data();
  for (size_t i = 0; i < size; i += 3) {
    const uint32_t* pEntry = pList + i;
    if (IsMetricForCID(pEntry, cid))
      return static_cast<int>(pEntry[2]);
  }
  return m_DefaultWidth;
}

short CPDF_CIDFont::GetVertWidth(uint16_t CID) const {
  size_t vertsize = m_VertMetrics.size() / 5;
  if (vertsize) {
    const uint32_t* pTable = m_VertMetrics.data();
    for (size_t i = 0; i < vertsize; i++) {
      const uint32_t* pEntry = pTable + (i * 5);
      if (IsMetricForCID(pEntry, CID))
        return static_cast<short>(pEntry[2]);
    }
  }
  return m_DefaultW1;
}

void CPDF_CIDFont::GetVertOrigin(uint16_t CID, short& vx, short& vy) const {
  size_t vertsize = m_VertMetrics.size() / 5;
  if (vertsize) {
    const uint32_t* pTable = m_VertMetrics.data();
    for (size_t i = 0; i < vertsize; i++) {
      const uint32_t* pEntry = pTable + (i * 5);
      if (IsMetricForCID(pEntry, CID)) {
        vx = static_cast<short>(pEntry[3]);
        vy = static_cast<short>(pEntry[4]);
        return;
      }
    }
  }
  uint32_t dwWidth = m_DefaultWidth;
  size_t size = m_WidthList.size();
  const uint32_t* pList = m_WidthList.data();
  for (size_t i = 0; i < size; i += 3) {
    const uint32_t* pEntry = pList + i;
    if (IsMetricForCID(pEntry, CID)) {
      dwWidth = pEntry[2];
      break;
    }
  }
  vx = static_cast<short>(dwWidth) / 2;
  vy = m_DefaultVY;
}

int CPDF_CIDFont::GetGlyphIndex(uint32_t unicode, bool* pVertGlyph) {
  if (pVertGlyph)
    *pVertGlyph = false;

  FXFT_Face face = m_Font.GetFace();
  int index = FXFT_Get_Char_Index(face, unicode);
  if (unicode == 0x2502)
    return index;

  if (!index || !IsVertWriting())
    return index;

  if (m_pTTGSUBTable)
    return GetVerticalGlyph(index, pVertGlyph);

  if (!m_Font.GetSubData()) {
    unsigned long length = 0;
    int error = FXFT_Load_Sfnt_Table(face, FT_MAKE_TAG('G', 'S', 'U', 'B'), 0,
                                     nullptr, &length);
    if (!error)
      m_Font.SetSubData(FX_Alloc(uint8_t, length));
  }
  int error = FXFT_Load_Sfnt_Table(face, FT_MAKE_TAG('G', 'S', 'U', 'B'), 0,
                                   m_Font.GetSubData(), nullptr);
  if (error || !m_Font.GetSubData())
    return index;

  m_pTTGSUBTable = pdfium::MakeUnique<CFX_CTTGSUBTable>();
  m_pTTGSUBTable->LoadGSUBTable((FT_Bytes)m_Font.GetSubData());
  return GetVerticalGlyph(index, pVertGlyph);
}

int CPDF_CIDFont::GetVerticalGlyph(int index, bool* pVertGlyph) {
  uint32_t vindex = 0;
  m_pTTGSUBTable->GetVerticalGlyph(index, &vindex);
  if (!vindex)
    return index;

  index = vindex;
  if (pVertGlyph)
    *pVertGlyph = true;
  return index;
}

int CPDF_CIDFont::GlyphFromCharCode(uint32_t charcode, bool* pVertGlyph) {
  if (pVertGlyph)
    *pVertGlyph = false;

  if (!m_pFontFile && !m_pStreamAcc) {
    uint16_t cid = CIDFromCharCode(charcode);
    FX_WCHAR unicode = 0;
    if (m_bCIDIsGID) {
#if _FXM_PLATFORM_ != _FXM_PLATFORM_APPLE_
      return cid;
#else
      if (m_Flags & FXFONT_SYMBOLIC)
        return cid;

      CFX_WideString uni_str = UnicodeFromCharCode(charcode);
      if (uni_str.IsEmpty())
        return cid;

      unicode = uni_str.GetAt(0);
#endif
    } else {
      if (cid && m_pCID2UnicodeMap && m_pCID2UnicodeMap->IsLoaded())
        unicode = m_pCID2UnicodeMap->UnicodeFromCID(cid);
      if (unicode == 0)
        unicode = GetUnicodeFromCharCode(charcode);
      if (unicode == 0) {
        CFX_WideString unicode_str = UnicodeFromCharCode(charcode);
        if (!unicode_str.IsEmpty())
          unicode = unicode_str.GetAt(0);
      }
    }
    FXFT_Face face = m_Font.GetFace();
    if (unicode == 0) {
      if (!m_bAdobeCourierStd)
        return charcode ? static_cast<int>(charcode) : -1;

      charcode += 31;
      bool bMSUnicode = FT_UseTTCharmap(face, 3, 1);
      bool bMacRoman = !bMSUnicode && FT_UseTTCharmap(face, 1, 0);
      int iBaseEncoding = PDFFONT_ENCODING_STANDARD;
      if (bMSUnicode)
        iBaseEncoding = PDFFONT_ENCODING_WINANSI;
      else if (bMacRoman)
        iBaseEncoding = PDFFONT_ENCODING_MACROMAN;
      const FX_CHAR* name = GetAdobeCharName(
          iBaseEncoding, std::vector<CFX_ByteString>(), charcode);
      if (!name)
        return charcode ? static_cast<int>(charcode) : -1;

      int index = 0;
      uint16_t name_unicode = PDF_UnicodeFromAdobeName(name);
      if (!name_unicode)
        return charcode ? static_cast<int>(charcode) : -1;

      if (iBaseEncoding == PDFFONT_ENCODING_STANDARD)
        return FXFT_Get_Char_Index(face, name_unicode);

      if (iBaseEncoding == PDFFONT_ENCODING_WINANSI) {
        index = FXFT_Get_Char_Index(face, name_unicode);
      } else {
        ASSERT(iBaseEncoding == PDFFONT_ENCODING_MACROMAN);
        uint32_t maccode =
            FT_CharCodeFromUnicode(FXFT_ENCODING_APPLE_ROMAN, name_unicode);
        index = maccode ? FXFT_Get_Char_Index(face, maccode)
                        : FXFT_Get_Name_Index(face, const_cast<char*>(name));
      }
      if (index == 0 || index == 0xffff)
        return charcode ? static_cast<int>(charcode) : -1;
      return index;
    }
    if (m_Charset == CIDSET_JAPAN1) {
      if (unicode == '\\') {
        unicode = '/';
#if _FXM_PLATFORM_ != _FXM_PLATFORM_APPLE_
      } else if (unicode == 0xa5) {
        unicode = 0x5c;
#endif
      }
    }
    if (!face)
      return unicode;

    int err = FXFT_Select_Charmap(face, FXFT_ENCODING_UNICODE);
    if (err) {
      int i;
      for (i = 0; i < FXFT_Get_Face_CharmapCount(face); i++) {
        uint32_t ret = FT_CharCodeFromUnicode(
            FXFT_Get_Charmap_Encoding(FXFT_Get_Face_Charmaps(face)[i]),
            static_cast<FX_WCHAR>(charcode));
        if (ret == 0)
          continue;
        FXFT_Set_Charmap(face, FXFT_Get_Face_Charmaps(face)[i]);
        unicode = static_cast<FX_WCHAR>(ret);
        break;
      }
      if (i == FXFT_Get_Face_CharmapCount(face) && i) {
        FXFT_Set_Charmap(face, FXFT_Get_Face_Charmaps(face)[0]);
        unicode = static_cast<FX_WCHAR>(charcode);
      }
    }
    if (FXFT_Get_Face_Charmap(face)) {
      int index = GetGlyphIndex(unicode, pVertGlyph);
      return index != 0 ? index : -1;
    }
    return unicode;
  }

  if (!m_Font.GetFace())
    return -1;

  uint16_t cid = CIDFromCharCode(charcode);
  if (!m_pStreamAcc) {
    if (m_bType1)
      return cid;

    if (m_pFontFile && !m_pCMap->m_pMapping)
      return cid;
    if (m_pCMap->m_Coding == CIDCODING_UNKNOWN ||
        !FXFT_Get_Face_Charmap(m_Font.GetFace())) {
      return cid;
    }
    if (FXFT_Get_Charmap_Encoding(FXFT_Get_Face_Charmap(m_Font.GetFace())) ==
        FXFT_ENCODING_UNICODE) {
      CFX_WideString unicode_str = UnicodeFromCharCode(charcode);
      if (unicode_str.IsEmpty())
        return -1;

      charcode = unicode_str.GetAt(0);
    }
    return GetGlyphIndex(charcode, pVertGlyph);
  }
  uint32_t byte_pos = cid * 2;
  if (byte_pos + 2 > m_pStreamAcc->GetSize())
    return -1;

  const uint8_t* pdata = m_pStreamAcc->GetData() + byte_pos;
  return pdata[0] * 256 + pdata[1];
}

uint32_t CPDF_CIDFont::GetNextChar(const FX_CHAR* pString,
                                   int nStrLen,
                                   int& offset) const {
  return m_pCMap->GetNextChar(pString, nStrLen, offset);
}

int CPDF_CIDFont::GetCharSize(uint32_t charcode) const {
  return m_pCMap->GetCharSize(charcode);
}

int CPDF_CIDFont::CountChar(const FX_CHAR* pString, int size) const {
  return m_pCMap->CountChar(pString, size);
}

int CPDF_CIDFont::AppendChar(FX_CHAR* str, uint32_t charcode) const {
  return m_pCMap->AppendChar(str, charcode);
}

bool CPDF_CIDFont::IsUnicodeCompatible() const {
  if (m_pCID2UnicodeMap && m_pCID2UnicodeMap->IsLoaded() && m_pCMap->IsLoaded())
    return true;
  return m_pCMap->m_Coding != CIDCODING_UNKNOWN;
}

void CPDF_CIDFont::LoadSubstFont() {
  pdfium::base::CheckedNumeric<int> safeStemV(m_StemV);
  safeStemV *= 5;
  m_Font.LoadSubst(m_BaseFont, !m_bType1, m_Flags,
                   safeStemV.ValueOrDefault(FXFONT_FW_NORMAL), m_ItalicAngle,
                   g_CharsetCPs[m_Charset], IsVertWriting());
}

void CPDF_CIDFont::LoadMetricsArray(CPDF_Array* pArray,
                                    std::vector<uint32_t>* result,
                                    int nElements) {
  int width_status = 0;
  int iCurElement = 0;
  int first_code = 0;
  int last_code = 0;
  for (size_t i = 0; i < pArray->GetCount(); i++) {
    CPDF_Object* pObj = pArray->GetDirectObjectAt(i);
    if (!pObj)
      continue;

    if (CPDF_Array* pObjArray = pObj->AsArray()) {
      if (width_status != 1)
        return;

      for (size_t j = 0; j < pObjArray->GetCount(); j += nElements) {
        result->push_back(first_code);
        result->push_back(first_code);
        for (int k = 0; k < nElements; k++)
          result->push_back(pObjArray->GetIntegerAt(j + k));
        first_code++;
      }
      width_status = 0;
    } else {
      if (width_status == 0) {
        first_code = pObj->GetInteger();
        width_status = 1;
      } else if (width_status == 1) {
        last_code = pObj->GetInteger();
        width_status = 2;
        iCurElement = 0;
      } else {
        if (!iCurElement) {
          result->push_back(first_code);
          result->push_back(last_code);
        }
        result->push_back(pObj->GetInteger());
        iCurElement++;
        if (iCurElement == nElements)
          width_status = 0;
      }
    }
  }
}

// static
FX_FLOAT CPDF_CIDFont::CIDTransformToFloat(uint8_t ch) {
  return (ch < 128 ? ch : ch - 255) * (1.0f / 127);
}

void CPDF_CIDFont::LoadGB2312() {
  m_BaseFont = m_pFontDict->GetStringFor("BaseFont");
  CPDF_Dictionary* pFontDesc = m_pFontDict->GetDictFor("FontDescriptor");
  if (pFontDesc)
    LoadFontDescriptor(pFontDesc);

  m_Charset = CIDSET_GB1;
  m_bType1 = false;
  CPDF_CMapManager& manager = GetFontGlobals()->m_CMapManager;
  m_pCMap = manager.GetPredefinedCMap("GBK-EUC-H", false);
  m_pCID2UnicodeMap = manager.GetCID2UnicodeMap(m_Charset, false);
  if (!IsEmbedded())
    LoadSubstFont();

  CheckFontMetrics();
  m_DefaultWidth = 1000;
  m_bAnsiWidthsFixed = true;
}

const uint8_t* CPDF_CIDFont::GetCIDTransform(uint16_t CID) const {
  if (m_Charset != CIDSET_JAPAN1 || m_pFontFile)
    return nullptr;

  const auto* pEnd = g_Japan1_VertCIDs + FX_ArraySize(g_Japan1_VertCIDs);
  const auto* pTransform = std::lower_bound(
      g_Japan1_VertCIDs, pEnd, CID,
      [](const CIDTransform& entry, uint16_t cid) { return entry.cid < cid; });
  return (pTransform < pEnd && CID == pTransform->cid) ? &pTransform->a
                                                       : nullptr;
}
