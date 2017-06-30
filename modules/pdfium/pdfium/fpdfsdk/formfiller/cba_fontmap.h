// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FORMFILLER_CBA_FONTMAP_H_
#define FPDFSDK_FORMFILLER_CBA_FONTMAP_H_

#include "fpdfsdk/pdfwindow/PWL_FontMap.h"

class CPDF_Dictionary;
class CPDFSDK_Annot;

class CBA_FontMap : public CPWL_FontMap {
 public:
  CBA_FontMap(CPDFSDK_Annot* pAnnot, CFX_SystemHandler* pSystemHandler);
  ~CBA_FontMap() override;

  void SetDefaultFont(CPDF_Font* pFont, const CFX_ByteString& sFontName);

  void Reset();
  void SetAPType(const CFX_ByteString& sAPType);

 private:
  // CPWL_FontMap:
  void Initialize() override;
  CPDF_Document* GetDocument() override;
  CPDF_Font* FindFontSameCharset(CFX_ByteString& sFontAlias,
                                 int32_t nCharset) override;
  void AddedFont(CPDF_Font* pFont, const CFX_ByteString& sFontAlias) override;

  CPDF_Font* FindResFontSameCharset(CPDF_Dictionary* pResDict,
                                    CFX_ByteString& sFontAlias,
                                    int32_t nCharset);
  CPDF_Font* GetAnnotDefaultFont(CFX_ByteString& csNameTag);
  void AddFontToAnnotDict(CPDF_Font* pFont, const CFX_ByteString& sAlias);

  CPDF_Document* m_pDocument;
  CPDF_Dictionary* m_pAnnotDict;
  CPDF_Font* m_pDefaultFont;
  CFX_ByteString m_sDefaultFontName;
  CFX_ByteString m_sAPType;
};

#endif  // FPDFSDK_FORMFILLER_CBA_FONTMAP_H_
