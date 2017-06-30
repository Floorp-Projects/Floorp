// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FONT_FONT_INT_H_
#define CORE_FPDFAPI_FONT_FONT_INT_H_

#include <map>
#include <memory>
#include <vector>

#include "core/fpdfapi/font/cpdf_cidfont.h"
#include "core/fxcrt/cfx_maybe_owned.h"
#include "core/fxcrt/fx_basic.h"

class CPDF_CID2UnicodeMap;
class CPDF_CMap;
class CPDF_Font;
class CPDF_Stream;

using FXFT_Library = void*;

int TT2PDF(int m, FXFT_Face face);
bool FT_UseTTCharmap(FXFT_Face face, int platform_id, int encoding_id);
CIDSet CharsetFromOrdering(const CFX_ByteStringC& ordering);

class CPDF_CMapManager {
 public:
  CPDF_CMapManager();
  ~CPDF_CMapManager();

  CFX_MaybeOwned<CPDF_CMap> GetPredefinedCMap(const CFX_ByteString& name,
                                              bool bPromptCJK);
  CPDF_CID2UnicodeMap* GetCID2UnicodeMap(CIDSet charset, bool bPromptCJK);

 private:
  std::unique_ptr<CPDF_CMap> LoadPredefinedCMap(const CFX_ByteString& name,
                                                bool bPromptCJK);
  std::unique_ptr<CPDF_CID2UnicodeMap> LoadCID2UnicodeMap(CIDSet charset,
                                                          bool bPromptCJK);

  std::map<CFX_ByteString, std::unique_ptr<CPDF_CMap>> m_CMaps;
  std::unique_ptr<CPDF_CID2UnicodeMap> m_CID2UnicodeMaps[6];
};

class CFX_StockFontArray {
 public:
  CFX_StockFontArray();
  ~CFX_StockFontArray();

  // Takes ownership of |pFont|, returns unowned pointer to it.
  CPDF_Font* SetFont(uint32_t index, std::unique_ptr<CPDF_Font> pFont);
  CPDF_Font* GetFont(uint32_t index) const;

 private:
  std::unique_ptr<CPDF_Font> m_StockFonts[14];
};

class CPDF_FontGlobals {
 public:
  CPDF_FontGlobals();
  ~CPDF_FontGlobals();

  void Clear(CPDF_Document* pDoc);
  CPDF_Font* Find(CPDF_Document* pDoc, uint32_t index);

  // Takes ownership of |pFont|, returns unowned pointer to it.
  CPDF_Font* Set(CPDF_Document* key,
                 uint32_t index,
                 std::unique_ptr<CPDF_Font> pFont);

  CPDF_CMapManager m_CMapManager;
  struct {
    const struct FXCMAP_CMap* m_pMapList;
    uint32_t m_Count;
  } m_EmbeddedCharsets[CIDSET_NUM_SETS];
  struct {
    const uint16_t* m_pMap;
    uint32_t m_Count;
  } m_EmbeddedToUnicodes[CIDSET_NUM_SETS];

 private:
  std::map<CPDF_Document*, std::unique_ptr<CFX_StockFontArray>> m_StockMap;
};

struct CMap_CodeRange {
  int m_CharSize;
  uint8_t m_Lower[4];
  uint8_t m_Upper[4];
};

class CPDF_CMapParser {
 public:
  CPDF_CMapParser();
  ~CPDF_CMapParser();
  void Initialize(CPDF_CMap* pMap);
  void ParseWord(const CFX_ByteStringC& str);
  CFX_BinaryBuf m_AddMaps;

 private:
  friend class fpdf_font_cid_CMap_GetCode_Test;
  friend class fpdf_font_cid_CMap_GetCodeRange_Test;

  static uint32_t CMap_GetCode(const CFX_ByteStringC& word);
  static bool CMap_GetCodeRange(CMap_CodeRange& range,
                                const CFX_ByteStringC& first,
                                const CFX_ByteStringC& second);

  CPDF_CMap* m_pCMap;
  int m_Status;
  int m_CodeSeq;
  uint32_t m_CodePoints[4];
  std::vector<CMap_CodeRange> m_CodeRanges;
  CFX_ByteString m_LastWord;
};

enum CIDCoding : uint8_t {
  CIDCODING_UNKNOWN = 0,
  CIDCODING_GB,
  CIDCODING_BIG5,
  CIDCODING_JIS,
  CIDCODING_KOREA,
  CIDCODING_UCS2,
  CIDCODING_CID,
  CIDCODING_UTF16,
};

class CPDF_CMap {
 public:
  enum CodingScheme : uint8_t {
    OneByte,
    TwoBytes,
    MixedTwoBytes,
    MixedFourBytes
  };

  CPDF_CMap();
  ~CPDF_CMap();

  void LoadPredefined(CPDF_CMapManager* pMgr,
                      const CFX_ByteString& name,
                      bool bPromptCJK);
  void LoadEmbedded(const uint8_t* pData, uint32_t dwSize);

  bool IsLoaded() const;
  bool IsVertWriting() const;
  uint16_t CIDFromCharCode(uint32_t charcode) const;
  int GetCharSize(uint32_t charcode) const;
  uint32_t GetNextChar(const FX_CHAR* pString, int nStrLen, int& offset) const;
  int CountChar(const FX_CHAR* pString, int size) const;
  int AppendChar(FX_CHAR* str, uint32_t charcode) const;

 private:
  friend class CPDF_CMapParser;
  friend class CPDF_CIDFont;

  CFX_ByteString m_PredefinedCMap;
  bool m_bVertical;
  CIDSet m_Charset;
  int m_Coding;
  CodingScheme m_CodingScheme;
  int m_nCodeRanges;
  uint8_t* m_pLeadingBytes;
  uint16_t* m_pMapping;
  uint8_t* m_pAddMapping;
  bool m_bLoaded;
  const FXCMAP_CMap* m_pEmbedMap;
};

class CPDF_CID2UnicodeMap {
 public:
  CPDF_CID2UnicodeMap();
  ~CPDF_CID2UnicodeMap();

  bool IsLoaded();
  void Load(CPDF_CMapManager* pMgr, CIDSet charset, bool bPromptCJK);
  FX_WCHAR UnicodeFromCID(uint16_t CID);

 private:
  CIDSet m_Charset;
  const uint16_t* m_pEmbeddedMap;
  uint32_t m_EmbeddedCount;
};

class CPDF_ToUnicodeMap {
 public:
  CPDF_ToUnicodeMap();
  ~CPDF_ToUnicodeMap();

  void Load(CPDF_Stream* pStream);

  CFX_WideString Lookup(uint32_t charcode) const;
  uint32_t ReverseLookup(FX_WCHAR unicode) const;

 private:
  friend class fpdf_font_StringToCode_Test;
  friend class fpdf_font_StringToWideString_Test;

  static uint32_t StringToCode(const CFX_ByteStringC& str);
  static CFX_WideString StringToWideString(const CFX_ByteStringC& str);

  uint32_t GetUnicode();

  std::map<uint32_t, uint32_t> m_Map;
  CPDF_CID2UnicodeMap* m_pBaseMap;
  CFX_WideTextBuf m_MultiCharBuf;
};

#endif  // CORE_FPDFAPI_FONT_FONT_INT_H_
