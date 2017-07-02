// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cba_fontmap.h"

#include <utility>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_simple_parser.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfdoc/cpdf_formfield.h"
#include "core/fxge/cfx_substfont.h"
#include "fpdfsdk/cpdfsdk_annot.h"

CBA_FontMap::CBA_FontMap(CPDFSDK_Annot* pAnnot,
                         CFX_SystemHandler* pSystemHandler)
    : CPWL_FontMap(pSystemHandler),
      m_pDocument(nullptr),
      m_pAnnotDict(nullptr),
      m_pDefaultFont(nullptr),
      m_sAPType("N") {
  CPDF_Page* pPage = pAnnot->GetPDFPage();

  m_pDocument = pPage->m_pDocument;
  m_pAnnotDict = pAnnot->GetPDFAnnot()->GetAnnotDict();
  Initialize();
}

CBA_FontMap::~CBA_FontMap() {}

void CBA_FontMap::Reset() {
  Empty();
  m_pDefaultFont = nullptr;
  m_sDefaultFontName = "";
}

void CBA_FontMap::Initialize() {
  int32_t nCharset = FXFONT_DEFAULT_CHARSET;

  if (!m_pDefaultFont) {
    m_pDefaultFont = GetAnnotDefaultFont(m_sDefaultFontName);
    if (m_pDefaultFont) {
      if (const CFX_SubstFont* pSubstFont = m_pDefaultFont->GetSubstFont()) {
        nCharset = pSubstFont->m_Charset;
      } else {
        if (m_sDefaultFontName == "Wingdings" ||
            m_sDefaultFontName == "Wingdings2" ||
            m_sDefaultFontName == "Wingdings3" ||
            m_sDefaultFontName == "Webdings")
          nCharset = FXFONT_SYMBOL_CHARSET;
        else
          nCharset = FXFONT_ANSI_CHARSET;
      }
      AddFontData(m_pDefaultFont, m_sDefaultFontName, nCharset);
      AddFontToAnnotDict(m_pDefaultFont, m_sDefaultFontName);
    }
  }

  if (nCharset != FXFONT_ANSI_CHARSET)
    CPWL_FontMap::Initialize();
}

void CBA_FontMap::SetDefaultFont(CPDF_Font* pFont,
                                 const CFX_ByteString& sFontName) {
  ASSERT(pFont);

  if (m_pDefaultFont)
    return;

  m_pDefaultFont = pFont;
  m_sDefaultFontName = sFontName;

  int32_t nCharset = FXFONT_DEFAULT_CHARSET;
  if (const CFX_SubstFont* pSubstFont = m_pDefaultFont->GetSubstFont())
    nCharset = pSubstFont->m_Charset;
  AddFontData(m_pDefaultFont, m_sDefaultFontName, nCharset);
}

CPDF_Font* CBA_FontMap::FindFontSameCharset(CFX_ByteString& sFontAlias,
                                            int32_t nCharset) {
  if (m_pAnnotDict->GetStringFor("Subtype") != "Widget")
    return nullptr;

  CPDF_Document* pDocument = GetDocument();
  CPDF_Dictionary* pRootDict = pDocument->GetRoot();
  if (!pRootDict)
    return nullptr;

  CPDF_Dictionary* pAcroFormDict = pRootDict->GetDictFor("AcroForm");
  if (!pAcroFormDict)
    return nullptr;

  CPDF_Dictionary* pDRDict = pAcroFormDict->GetDictFor("DR");
  if (!pDRDict)
    return nullptr;

  return FindResFontSameCharset(pDRDict, sFontAlias, nCharset);
}

CPDF_Document* CBA_FontMap::GetDocument() {
  return m_pDocument;
}

CPDF_Font* CBA_FontMap::FindResFontSameCharset(CPDF_Dictionary* pResDict,
                                               CFX_ByteString& sFontAlias,
                                               int32_t nCharset) {
  if (!pResDict)
    return nullptr;

  CPDF_Dictionary* pFonts = pResDict->GetDictFor("Font");
  if (!pFonts)
    return nullptr;

  CPDF_Document* pDocument = GetDocument();
  CPDF_Font* pFind = nullptr;
  for (const auto& it : *pFonts) {
    const CFX_ByteString& csKey = it.first;
    if (!it.second)
      continue;

    CPDF_Dictionary* pElement = ToDictionary(it.second->GetDirect());
    if (!pElement)
      continue;
    if (pElement->GetStringFor("Type") != "Font")
      continue;

    CPDF_Font* pFont = pDocument->LoadFont(pElement);
    if (!pFont)
      continue;
    const CFX_SubstFont* pSubst = pFont->GetSubstFont();
    if (!pSubst)
      continue;
    if (pSubst->m_Charset == nCharset) {
      sFontAlias = csKey;
      pFind = pFont;
    }
  }
  return pFind;
}

void CBA_FontMap::AddedFont(CPDF_Font* pFont,
                            const CFX_ByteString& sFontAlias) {
  AddFontToAnnotDict(pFont, sFontAlias);
}

void CBA_FontMap::AddFontToAnnotDict(CPDF_Font* pFont,
                                     const CFX_ByteString& sAlias) {
  if (!pFont)
    return;

  CPDF_Dictionary* pAPDict = m_pAnnotDict->GetDictFor("AP");
  if (!pAPDict)
    pAPDict = m_pAnnotDict->SetNewFor<CPDF_Dictionary>("AP");

  // to avoid checkbox and radiobutton
  CPDF_Object* pObject = pAPDict->GetObjectFor(m_sAPType);
  if (ToDictionary(pObject))
    return;

  CPDF_Stream* pStream = pAPDict->GetStreamFor(m_sAPType);
  if (!pStream) {
    pStream = m_pDocument->NewIndirect<CPDF_Stream>();
    pAPDict->SetNewFor<CPDF_Reference>(m_sAPType, m_pDocument,
                                       pStream->GetObjNum());
  }

  CPDF_Dictionary* pStreamDict = pStream->GetDict();
  if (!pStreamDict) {
    auto pOwnedDict =
        pdfium::MakeUnique<CPDF_Dictionary>(m_pDocument->GetByteStringPool());
    pStreamDict = pOwnedDict.get();
    pStream->InitStream(nullptr, 0, std::move(pOwnedDict));
  }

  CPDF_Dictionary* pStreamResList = pStreamDict->GetDictFor("Resources");
  if (!pStreamResList)
    pStreamResList = pStreamDict->SetNewFor<CPDF_Dictionary>("Resources");
  CPDF_Dictionary* pStreamResFontList = pStreamResList->GetDictFor("Font");
  if (!pStreamResFontList) {
    pStreamResFontList = m_pDocument->NewIndirect<CPDF_Dictionary>();
    pStreamResList->SetNewFor<CPDF_Reference>("Font", m_pDocument,
                                              pStreamResFontList->GetObjNum());
  }
  if (!pStreamResFontList->KeyExist(sAlias)) {
    pStreamResFontList->SetNewFor<CPDF_Reference>(
        sAlias, m_pDocument, pFont->GetFontDict()->GetObjNum());
  }
}

CPDF_Font* CBA_FontMap::GetAnnotDefaultFont(CFX_ByteString& sAlias) {
  CPDF_Dictionary* pAcroFormDict = nullptr;
  const bool bWidget = (m_pAnnotDict->GetStringFor("Subtype") == "Widget");
  if (bWidget) {
    if (CPDF_Dictionary* pRootDict = m_pDocument->GetRoot())
      pAcroFormDict = pRootDict->GetDictFor("AcroForm");
  }

  CFX_ByteString sDA;
  CPDF_Object* pObj = FPDF_GetFieldAttr(m_pAnnotDict, "DA");
  if (pObj)
    sDA = pObj->GetString();

  if (bWidget) {
    if (sDA.IsEmpty()) {
      pObj = FPDF_GetFieldAttr(pAcroFormDict, "DA");
      sDA = pObj ? pObj->GetString() : CFX_ByteString();
    }
  }
  if (sDA.IsEmpty())
    return nullptr;

  CPDF_SimpleParser syntax(sDA.AsStringC());
  syntax.FindTagParamFromStart("Tf", 2);
  CFX_ByteString sFontName(syntax.GetWord());
  sAlias = PDF_NameDecode(sFontName).Mid(1);
  CPDF_Dictionary* pFontDict = nullptr;

  if (CPDF_Dictionary* pAPDict = m_pAnnotDict->GetDictFor("AP")) {
    if (CPDF_Dictionary* pNormalDict = pAPDict->GetDictFor("N")) {
      if (CPDF_Dictionary* pNormalResDict =
              pNormalDict->GetDictFor("Resources")) {
        if (CPDF_Dictionary* pResFontDict = pNormalResDict->GetDictFor("Font"))
          pFontDict = pResFontDict->GetDictFor(sAlias);
      }
    }
  }

  if (bWidget && !pFontDict && pAcroFormDict) {
    if (CPDF_Dictionary* pDRDict = pAcroFormDict->GetDictFor("DR")) {
      if (CPDF_Dictionary* pDRFontDict = pDRDict->GetDictFor("Font"))
        pFontDict = pDRFontDict->GetDictFor(sAlias);
    }
  }

  return pFontDict ? m_pDocument->LoadFont(pFontDict) : nullptr;
}

void CBA_FontMap::SetAPType(const CFX_ByteString& sAPType) {
  m_sAPType = sAPType;

  Reset();
  Initialize();
}
