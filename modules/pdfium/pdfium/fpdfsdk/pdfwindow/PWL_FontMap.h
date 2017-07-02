// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PDFWINDOW_PWL_FONTMAP_H_
#define FPDFSDK_PDFWINDOW_PWL_FONTMAP_H_

#include <memory>
#include <vector>

#include "core/fpdfdoc/ipvt_fontmap.h"
#include "core/fxge/fx_font.h"
#include "fpdfsdk/fxedit/fx_edit.h"
#include "public/fpdf_sysfontinfo.h"

class CPDF_Document;
class CFX_SystemHandler;

struct CPWL_FontMap_Data {
  CPDF_Font* pFont;
  int32_t nCharset;
  CFX_ByteString sFontName;
};

struct CPWL_FontMap_Native {
  int32_t nCharset;
  CFX_ByteString sFontName;
};

class CPWL_FontMap : public IPVT_FontMap {
 public:
  explicit CPWL_FontMap(CFX_SystemHandler* pSystemHandler);
  ~CPWL_FontMap() override;

  // IPVT_FontMap
  CPDF_Font* GetPDFFont(int32_t nFontIndex) override;
  CFX_ByteString GetPDFFontAlias(int32_t nFontIndex) override;
  int32_t GetWordFontIndex(uint16_t word,
                           int32_t nCharset,
                           int32_t nFontIndex) override;
  int32_t CharCodeFromUnicode(int32_t nFontIndex, uint16_t word) override;
  int32_t CharSetFromUnicode(uint16_t word, int32_t nOldCharset) override;

  const CPWL_FontMap_Data* GetFontMapData(int32_t nIndex) const;
  static int32_t GetNativeCharset();
  CFX_ByteString GetNativeFontName(int32_t nCharset);

  static CFX_ByteString GetDefaultFontByCharset(int32_t nCharset);
  static const FPDF_CharsetFontMap defaultTTFMap[];

 protected:
  virtual void Initialize();
  virtual CPDF_Document* GetDocument();
  virtual CPDF_Font* FindFontSameCharset(CFX_ByteString& sFontAlias,
                                         int32_t nCharset);
  virtual void AddedFont(CPDF_Font* pFont, const CFX_ByteString& sFontAlias);

  bool KnowWord(int32_t nFontIndex, uint16_t word);

  void Empty();
  int32_t GetFontIndex(const CFX_ByteString& sFontName,
                       int32_t nCharset,
                       bool bFind);
  int32_t AddFontData(CPDF_Font* pFont,
                      const CFX_ByteString& sFontAlias,
                      int32_t nCharset = FXFONT_DEFAULT_CHARSET);

  CFX_ByteString EncodeFontAlias(const CFX_ByteString& sFontName,
                                 int32_t nCharset);
  CFX_ByteString EncodeFontAlias(const CFX_ByteString& sFontName);

  std::vector<std::unique_ptr<CPWL_FontMap_Data>> m_Data;
  std::vector<std::unique_ptr<CPWL_FontMap_Native>> m_NativeFont;

 private:
  int32_t FindFont(const CFX_ByteString& sFontName,
                   int32_t nCharset = FXFONT_DEFAULT_CHARSET);

  CFX_ByteString GetNativeFont(int32_t nCharset);
  CPDF_Font* AddFontToDocument(CPDF_Document* pDoc,
                               CFX_ByteString& sFontName,
                               uint8_t nCharset);
  bool IsStandardFont(const CFX_ByteString& sFontName);
  CPDF_Font* AddStandardFont(CPDF_Document* pDoc, CFX_ByteString& sFontName);
  CPDF_Font* AddSystemFont(CPDF_Document* pDoc,
                           CFX_ByteString& sFontName,
                           uint8_t nCharset);

  std::unique_ptr<CPDF_Document> m_pPDFDoc;
  CFX_SystemHandler* const m_pSystemHandler;
};

#endif  // FPDFSDK_PDFWINDOW_PWL_FONTMAP_H_
