// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/font_int.h"

#include <memory>
#include <utility>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_simple_parser.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/fx_ext.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxge/fx_freetype.h"
#include "third_party/base/numerics/safe_conversions.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

int TT2PDF(int m, FXFT_Face face) {
  int upm = FXFT_Get_Face_UnitsPerEM(face);
  if (upm == 0)
    return m;
  return pdfium::base::checked_cast<int>(
      (static_cast<double>(m) * 1000 + upm / 2) / upm);
}

bool FT_UseTTCharmap(FXFT_Face face, int platform_id, int encoding_id) {
  auto* pCharMap = FXFT_Get_Face_Charmaps(face);
  for (int i = 0; i < FXFT_Get_Face_CharmapCount(face); i++) {
    if (FXFT_Get_Charmap_PlatformID(pCharMap[i]) == platform_id &&
        FXFT_Get_Charmap_EncodingID(pCharMap[i]) == encoding_id) {
      FXFT_Set_Charmap(face, pCharMap[i]);
      return true;
    }
  }
  return false;
}

CFX_StockFontArray::CFX_StockFontArray() {}

CFX_StockFontArray::~CFX_StockFontArray() {
  for (size_t i = 0; i < FX_ArraySize(m_StockFonts); ++i) {
    if (m_StockFonts[i])
      delete m_StockFonts[i]->GetFontDict();
  }
}

CPDF_Font* CFX_StockFontArray::GetFont(uint32_t index) const {
  if (index >= FX_ArraySize(m_StockFonts))
    return nullptr;
  return m_StockFonts[index].get();
}

CPDF_Font* CFX_StockFontArray::SetFont(uint32_t index,
                                       std::unique_ptr<CPDF_Font> pFont) {
  CPDF_Font* result = pFont.get();
  if (index < FX_ArraySize(m_StockFonts))
    m_StockFonts[index] = std::move(pFont);
  return result;
}

CPDF_FontGlobals::CPDF_FontGlobals() {
  FXSYS_memset(m_EmbeddedCharsets, 0, sizeof(m_EmbeddedCharsets));
  FXSYS_memset(m_EmbeddedToUnicodes, 0, sizeof(m_EmbeddedToUnicodes));
}

CPDF_FontGlobals::~CPDF_FontGlobals() {}

CPDF_Font* CPDF_FontGlobals::Find(CPDF_Document* pDoc, uint32_t index) {
  auto it = m_StockMap.find(pDoc);
  if (it == m_StockMap.end())
    return nullptr;
  return it->second ? it->second->GetFont(index) : nullptr;
}

CPDF_Font* CPDF_FontGlobals::Set(CPDF_Document* pDoc,
                                 uint32_t index,
                                 std::unique_ptr<CPDF_Font> pFont) {
  if (!pdfium::ContainsKey(m_StockMap, pDoc))
    m_StockMap[pDoc] = pdfium::MakeUnique<CFX_StockFontArray>();
  return m_StockMap[pDoc]->SetFont(index, std::move(pFont));
}

void CPDF_FontGlobals::Clear(CPDF_Document* pDoc) {
  m_StockMap.erase(pDoc);
}

CFX_WideString CPDF_ToUnicodeMap::Lookup(uint32_t charcode) const {
  auto it = m_Map.find(charcode);
  if (it != m_Map.end()) {
    uint32_t value = it->second;
    FX_WCHAR unicode = (FX_WCHAR)(value & 0xffff);
    if (unicode != 0xffff) {
      return unicode;
    }
    const FX_WCHAR* buf = m_MultiCharBuf.GetBuffer();
    uint32_t buf_len = m_MultiCharBuf.GetLength();
    if (!buf || buf_len == 0) {
      return CFX_WideString();
    }
    uint32_t index = value >> 16;
    if (index >= buf_len) {
      return CFX_WideString();
    }
    uint32_t len = buf[index];
    if (index + len < index || index + len >= buf_len) {
      return CFX_WideString();
    }
    return CFX_WideString(buf + index + 1, len);
  }
  if (m_pBaseMap) {
    return m_pBaseMap->UnicodeFromCID((uint16_t)charcode);
  }
  return CFX_WideString();
}

uint32_t CPDF_ToUnicodeMap::ReverseLookup(FX_WCHAR unicode) const {
  for (const auto& pair : m_Map) {
    if (pair.second == static_cast<uint32_t>(unicode))
      return pair.first;
  }
  return 0;
}

// Static.
uint32_t CPDF_ToUnicodeMap::StringToCode(const CFX_ByteStringC& str) {
  int len = str.GetLength();
  if (len == 0)
    return 0;

  uint32_t result = 0;
  if (str[0] == '<') {
    for (int i = 1; i < len && std::isxdigit(str[i]); ++i)
      result = result * 16 + FXSYS_toHexDigit(str.CharAt(i));
    return result;
  }

  for (int i = 0; i < len && std::isdigit(str[i]); ++i)
    result = result * 10 + FXSYS_toDecimalDigit(str.CharAt(i));

  return result;
}

static CFX_WideString StringDataAdd(CFX_WideString str) {
  CFX_WideString ret;
  int len = str.GetLength();
  FX_WCHAR value = 1;
  for (int i = len - 1; i >= 0; --i) {
    FX_WCHAR ch = str[i] + value;
    if (ch < str[i]) {
      ret.Insert(0, 0);
    } else {
      ret.Insert(0, ch);
      value = 0;
    }
  }
  if (value) {
    ret.Insert(0, value);
  }
  return ret;
}

// Static.
CFX_WideString CPDF_ToUnicodeMap::StringToWideString(
    const CFX_ByteStringC& str) {
  int len = str.GetLength();
  if (len == 0)
    return CFX_WideString();

  CFX_WideString result;
  if (str[0] == '<') {
    int byte_pos = 0;
    FX_WCHAR ch = 0;
    for (int i = 1; i < len && std::isxdigit(str[i]); ++i) {
      ch = ch * 16 + FXSYS_toHexDigit(str[i]);
      byte_pos++;
      if (byte_pos == 4) {
        result += ch;
        byte_pos = 0;
        ch = 0;
      }
    }
    return result;
  }
  return result;
}

CPDF_ToUnicodeMap::CPDF_ToUnicodeMap() : m_pBaseMap(nullptr) {}

CPDF_ToUnicodeMap::~CPDF_ToUnicodeMap() {}

uint32_t CPDF_ToUnicodeMap::GetUnicode() {
  FX_SAFE_UINT32 uni = m_MultiCharBuf.GetLength();
  uni = uni * 0x10000 + 0xffff;
  return uni.ValueOrDefault(0);
}

void CPDF_ToUnicodeMap::Load(CPDF_Stream* pStream) {
  CIDSet cid_set = CIDSET_UNKNOWN;
  CPDF_StreamAcc stream;
  stream.LoadAllData(pStream, false);
  CPDF_SimpleParser parser(stream.GetData(), stream.GetSize());
  while (1) {
    CFX_ByteStringC word = parser.GetWord();
    if (word.IsEmpty()) {
      break;
    }
    if (word == "beginbfchar") {
      while (1) {
        word = parser.GetWord();
        if (word.IsEmpty() || word == "endbfchar") {
          break;
        }
        uint32_t srccode = StringToCode(word);
        word = parser.GetWord();
        CFX_WideString destcode = StringToWideString(word);
        int len = destcode.GetLength();
        if (len == 0) {
          continue;
        }
        if (len == 1) {
          m_Map[srccode] = destcode.GetAt(0);
        } else {
          m_Map[srccode] = GetUnicode();
          m_MultiCharBuf.AppendChar(destcode.GetLength());
          m_MultiCharBuf << destcode;
        }
      }
    } else if (word == "beginbfrange") {
      while (1) {
        CFX_ByteString low, high;
        low = parser.GetWord();
        if (low.IsEmpty() || low == "endbfrange") {
          break;
        }
        high = parser.GetWord();
        uint32_t lowcode = StringToCode(low.AsStringC());
        uint32_t highcode =
            (lowcode & 0xffffff00) | (StringToCode(high.AsStringC()) & 0xff);
        if (highcode == (uint32_t)-1) {
          break;
        }
        CFX_ByteString start(parser.GetWord());
        if (start == "[") {
          for (uint32_t code = lowcode; code <= highcode; code++) {
            CFX_ByteString dest(parser.GetWord());
            CFX_WideString destcode = StringToWideString(dest.AsStringC());
            int len = destcode.GetLength();
            if (len == 0) {
              continue;
            }
            if (len == 1) {
              m_Map[code] = destcode.GetAt(0);
            } else {
              m_Map[code] = GetUnicode();
              m_MultiCharBuf.AppendChar(destcode.GetLength());
              m_MultiCharBuf << destcode;
            }
          }
          parser.GetWord();
        } else {
          CFX_WideString destcode = StringToWideString(start.AsStringC());
          int len = destcode.GetLength();
          uint32_t value = 0;
          if (len == 1) {
            value = StringToCode(start.AsStringC());
            for (uint32_t code = lowcode; code <= highcode; code++) {
              m_Map[code] = value++;
            }
          } else {
            for (uint32_t code = lowcode; code <= highcode; code++) {
              CFX_WideString retcode;
              if (code == lowcode) {
                retcode = destcode;
              } else {
                retcode = StringDataAdd(destcode);
              }
              m_Map[code] = GetUnicode();
              m_MultiCharBuf.AppendChar(retcode.GetLength());
              m_MultiCharBuf << retcode;
              destcode = retcode;
            }
          }
        }
      }
    } else if (word == "/Adobe-Korea1-UCS2") {
      cid_set = CIDSET_KOREA1;
    } else if (word == "/Adobe-Japan1-UCS2") {
      cid_set = CIDSET_JAPAN1;
    } else if (word == "/Adobe-CNS1-UCS2") {
      cid_set = CIDSET_CNS1;
    } else if (word == "/Adobe-GB1-UCS2") {
      cid_set = CIDSET_GB1;
    }
  }
  if (cid_set) {
    m_pBaseMap = CPDF_ModuleMgr::Get()
                     ->GetPageModule()
                     ->GetFontGlobals()
                     ->m_CMapManager.GetCID2UnicodeMap(cid_set, false);
  } else {
    m_pBaseMap = nullptr;
  }
}
