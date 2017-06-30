// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FONT_CPDF_CIDFONT_H_
#define CORE_FPDFAPI_FONT_CPDF_CIDFONT_H_

#include <memory>
#include <vector>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fxcrt/cfx_maybe_owned.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

enum CIDSet {
  CIDSET_UNKNOWN,
  CIDSET_GB1,
  CIDSET_CNS1,
  CIDSET_JAPAN1,
  CIDSET_KOREA1,
  CIDSET_UNICODE,
  CIDSET_NUM_SETS
};

class CFX_CTTGSUBTable;
class CPDF_Array;
class CPDF_CID2UnicodeMap;
class CPDF_CMap;
class CPDF_StreamAcc;

class CPDF_CIDFont : public CPDF_Font {
 public:
  CPDF_CIDFont();
  ~CPDF_CIDFont() override;

  static FX_FLOAT CIDTransformToFloat(uint8_t ch);

  // CPDF_Font:
  bool IsCIDFont() const override;
  const CPDF_CIDFont* AsCIDFont() const override;
  CPDF_CIDFont* AsCIDFont() override;
  int GlyphFromCharCode(uint32_t charcode, bool* pVertGlyph) override;
  int GetCharWidthF(uint32_t charcode) override;
  FX_RECT GetCharBBox(uint32_t charcode) override;
  uint32_t GetNextChar(const FX_CHAR* pString,
                       int nStrLen,
                       int& offset) const override;
  int CountChar(const FX_CHAR* pString, int size) const override;
  int AppendChar(FX_CHAR* str, uint32_t charcode) const override;
  bool IsVertWriting() const override;
  bool IsUnicodeCompatible() const override;
  bool Load() override;
  CFX_WideString UnicodeFromCharCode(uint32_t charcode) const override;
  uint32_t CharCodeFromUnicode(FX_WCHAR Unicode) const override;

  uint16_t CIDFromCharCode(uint32_t charcode) const;
  const uint8_t* GetCIDTransform(uint16_t CID) const;
  short GetVertWidth(uint16_t CID) const;
  void GetVertOrigin(uint16_t CID, short& vx, short& vy) const;
  int GetCharSize(uint32_t charcode) const;

 protected:
  void LoadGB2312();
  int GetGlyphIndex(uint32_t unicodeb, bool* pVertGlyph);
  int GetVerticalGlyph(int index, bool* pVertGlyph);
  void LoadMetricsArray(CPDF_Array* pArray,
                        std::vector<uint32_t>* result,
                        int nElements);
  void LoadSubstFont();
  FX_WCHAR GetUnicodeFromCharCode(uint32_t charcode) const;

  CFX_MaybeOwned<CPDF_CMap> m_pCMap;
  CPDF_CID2UnicodeMap* m_pCID2UnicodeMap;
  CIDSet m_Charset;
  bool m_bType1;
  bool m_bCIDIsGID;
  uint16_t m_DefaultWidth;
  std::unique_ptr<CPDF_StreamAcc> m_pStreamAcc;
  bool m_bAnsiWidthsFixed;
  FX_RECT m_CharBBox[256];
  std::vector<uint32_t> m_WidthList;
  short m_DefaultVY;
  short m_DefaultW1;
  std::vector<uint32_t> m_VertMetrics;
  bool m_bAdobeCourierStd;
  std::unique_ptr<CFX_CTTGSUBTable> m_pTTGSUBTable;
};

#endif  // CORE_FPDFAPI_FONT_CPDF_CIDFONT_H_
