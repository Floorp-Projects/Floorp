// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_docpagedata.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "core/fdrm/crypto/fx_crypt.h"
#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/font/cpdf_type1font.h"
#include "core/fpdfapi/font/font_int.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fpdfapi/page/cpdf_pattern.h"
#include "core/fpdfapi/page/cpdf_shadingpattern.h"
#include "core/fpdfapi/page/cpdf_tilingpattern.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "third_party/base/stl_util.h"

CPDF_DocPageData::CPDF_DocPageData(CPDF_Document* pPDFDoc)
    : m_pPDFDoc(pPDFDoc), m_bForceClear(false) {}

CPDF_DocPageData::~CPDF_DocPageData() {
  Clear(false);
  Clear(true);

  for (auto& it : m_PatternMap)
    delete it.second;
  m_PatternMap.clear();

  for (auto& it : m_FontMap)
    delete it.second;
  m_FontMap.clear();

  for (auto& it : m_ColorSpaceMap)
    delete it.second;
  m_ColorSpaceMap.clear();
}

void CPDF_DocPageData::Clear(bool bForceRelease) {
  m_bForceClear = bForceRelease;

  for (auto& it : m_PatternMap) {
    CPDF_CountedPattern* ptData = it.second;
    if (!ptData->get())
      continue;

    if (bForceRelease || ptData->use_count() < 2)
      ptData->clear();
  }

  for (auto& it : m_FontMap) {
    CPDF_CountedFont* fontData = it.second;
    if (!fontData->get())
      continue;

    if (bForceRelease || fontData->use_count() < 2) {
      fontData->clear();
    }
  }

  for (auto& it : m_ColorSpaceMap) {
    CPDF_CountedColorSpace* csData = it.second;
    if (!csData->get())
      continue;

    if (bForceRelease || csData->use_count() < 2) {
      csData->get()->Release();
      csData->reset(nullptr);
    }
  }

  for (auto it = m_IccProfileMap.begin(); it != m_IccProfileMap.end();) {
    auto curr_it = it++;
    CPDF_CountedIccProfile* ipData = curr_it->second;
    if (!ipData->get())
      continue;

    if (bForceRelease || ipData->use_count() < 2) {
      for (auto hash_it = m_HashProfileMap.begin();
           hash_it != m_HashProfileMap.end(); ++hash_it) {
        if (curr_it->first == hash_it->second) {
          m_HashProfileMap.erase(hash_it);
          break;
        }
      }
      delete ipData->get();
      delete ipData;
      m_IccProfileMap.erase(curr_it);
    }
  }

  for (auto it = m_FontFileMap.begin(); it != m_FontFileMap.end();) {
    auto curr_it = it++;
    CPDF_CountedStreamAcc* pCountedFont = curr_it->second;
    if (!pCountedFont->get())
      continue;

    if (bForceRelease || pCountedFont->use_count() < 2) {
      delete pCountedFont->get();
      delete pCountedFont;
      m_FontFileMap.erase(curr_it);
    }
  }

  for (auto it = m_ImageMap.begin(); it != m_ImageMap.end();) {
    auto curr_it = it++;
    CPDF_CountedImage* pCountedImage = curr_it->second;
    if (!pCountedImage->get())
      continue;

    if (bForceRelease || pCountedImage->use_count() < 2) {
      delete pCountedImage->get();
      delete pCountedImage;
      m_ImageMap.erase(curr_it);
    }
  }
}

CPDF_Font* CPDF_DocPageData::GetFont(CPDF_Dictionary* pFontDict) {
  if (!pFontDict)
    return nullptr;

  CPDF_CountedFont* pFontData = nullptr;
  auto it = m_FontMap.find(pFontDict);
  if (it != m_FontMap.end()) {
    pFontData = it->second;
    if (pFontData->get()) {
      return pFontData->AddRef();
    }
  }
  std::unique_ptr<CPDF_Font> pFont = CPDF_Font::Create(m_pPDFDoc, pFontDict);
  if (!pFont)
    return nullptr;

  if (pFontData) {
    pFontData->reset(std::move(pFont));
  } else {
    pFontData = new CPDF_CountedFont(std::move(pFont));
    m_FontMap[pFontDict] = pFontData;
  }
  return pFontData->AddRef();
}

CPDF_Font* CPDF_DocPageData::GetStandardFont(const CFX_ByteString& fontName,
                                             CPDF_FontEncoding* pEncoding) {
  if (fontName.IsEmpty())
    return nullptr;

  for (auto& it : m_FontMap) {
    CPDF_CountedFont* fontData = it.second;
    CPDF_Font* pFont = fontData->get();
    if (!pFont)
      continue;
    if (pFont->GetBaseFont() != fontName)
      continue;
    if (pFont->IsEmbedded())
      continue;
    if (!pFont->IsType1Font())
      continue;
    if (pFont->GetFontDict()->KeyExist("Widths"))
      continue;

    CPDF_Type1Font* pT1Font = pFont->AsType1Font();
    if (pEncoding && !pT1Font->GetEncoding()->IsIdentical(pEncoding))
      continue;

    return fontData->AddRef();
  }

  CPDF_Dictionary* pDict = m_pPDFDoc->NewIndirect<CPDF_Dictionary>();
  pDict->SetNewFor<CPDF_Name>("Type", "Font");
  pDict->SetNewFor<CPDF_Name>("Subtype", "Type1");
  pDict->SetNewFor<CPDF_Name>("BaseFont", fontName);
  if (pEncoding) {
    pDict->SetFor("Encoding",
                  pEncoding->Realize(m_pPDFDoc->GetByteStringPool()));
  }

  std::unique_ptr<CPDF_Font> pFont = CPDF_Font::Create(m_pPDFDoc, pDict);
  if (!pFont)
    return nullptr;

  CPDF_CountedFont* fontData = new CPDF_CountedFont(std::move(pFont));
  m_FontMap[pDict] = fontData;
  return fontData->AddRef();
}

void CPDF_DocPageData::ReleaseFont(const CPDF_Dictionary* pFontDict) {
  if (!pFontDict)
    return;

  auto it = m_FontMap.find(pFontDict);
  if (it == m_FontMap.end())
    return;

  CPDF_CountedFont* pFontData = it->second;
  if (!pFontData->get())
    return;

  pFontData->RemoveRef();
  if (pFontData->use_count() > 1)
    return;

  // We have font data only in m_FontMap cache. Clean it.
  pFontData->clear();
}

CPDF_ColorSpace* CPDF_DocPageData::GetColorSpace(
    CPDF_Object* pCSObj,
    const CPDF_Dictionary* pResources) {
  std::set<CPDF_Object*> visited;
  return GetColorSpaceImpl(pCSObj, pResources, &visited);
}

CPDF_ColorSpace* CPDF_DocPageData::GetColorSpaceImpl(
    CPDF_Object* pCSObj,
    const CPDF_Dictionary* pResources,
    std::set<CPDF_Object*>* pVisited) {
  if (!pCSObj)
    return nullptr;

  if (pdfium::ContainsKey(*pVisited, pCSObj))
    return nullptr;

  if (pCSObj->IsName()) {
    CFX_ByteString name = pCSObj->GetString();
    CPDF_ColorSpace* pCS = CPDF_ColorSpace::ColorspaceFromName(name);
    if (!pCS && pResources) {
      CPDF_Dictionary* pList = pResources->GetDictFor("ColorSpace");
      if (pList) {
        pdfium::ScopedSetInsertion<CPDF_Object*> insertion(pVisited, pCSObj);
        return GetColorSpaceImpl(pList->GetDirectObjectFor(name), nullptr,
                                 pVisited);
      }
    }
    if (!pCS || !pResources)
      return pCS;

    CPDF_Dictionary* pColorSpaces = pResources->GetDictFor("ColorSpace");
    if (!pColorSpaces)
      return pCS;

    CPDF_Object* pDefaultCS = nullptr;
    switch (pCS->GetFamily()) {
      case PDFCS_DEVICERGB:
        pDefaultCS = pColorSpaces->GetDirectObjectFor("DefaultRGB");
        break;
      case PDFCS_DEVICEGRAY:
        pDefaultCS = pColorSpaces->GetDirectObjectFor("DefaultGray");
        break;
      case PDFCS_DEVICECMYK:
        pDefaultCS = pColorSpaces->GetDirectObjectFor("DefaultCMYK");
        break;
    }
    if (!pDefaultCS)
      return pCS;

    pdfium::ScopedSetInsertion<CPDF_Object*> insertion(pVisited, pCSObj);
    return GetColorSpaceImpl(pDefaultCS, nullptr, pVisited);
  }

  CPDF_Array* pArray = pCSObj->AsArray();
  if (!pArray || pArray->IsEmpty())
    return nullptr;

  if (pArray->GetCount() == 1) {
    pdfium::ScopedSetInsertion<CPDF_Object*> insertion(pVisited, pCSObj);
    return GetColorSpaceImpl(pArray->GetDirectObjectAt(0), pResources,
                             pVisited);
  }

  CPDF_CountedColorSpace* csData = nullptr;
  auto it = m_ColorSpaceMap.find(pCSObj);
  if (it != m_ColorSpaceMap.end()) {
    csData = it->second;
    if (csData->get()) {
      return csData->AddRef();
    }
  }

  std::unique_ptr<CPDF_ColorSpace> pCS =
      CPDF_ColorSpace::Load(m_pPDFDoc, pArray);
  if (!pCS)
    return nullptr;

  if (csData) {
    csData->reset(std::move(pCS));
  } else {
    csData = new CPDF_CountedColorSpace(std::move(pCS));
    m_ColorSpaceMap[pCSObj] = csData;
  }
  return csData->AddRef();
}

CPDF_ColorSpace* CPDF_DocPageData::GetCopiedColorSpace(CPDF_Object* pCSObj) {
  if (!pCSObj)
    return nullptr;

  auto it = m_ColorSpaceMap.find(pCSObj);
  if (it != m_ColorSpaceMap.end())
    return it->second->AddRef();

  return nullptr;
}

void CPDF_DocPageData::ReleaseColorSpace(const CPDF_Object* pColorSpace) {
  if (!pColorSpace)
    return;

  auto it = m_ColorSpaceMap.find(pColorSpace);
  if (it == m_ColorSpaceMap.end())
    return;

  CPDF_CountedColorSpace* pCountedColorSpace = it->second;
  if (!pCountedColorSpace->get())
    return;

  pCountedColorSpace->RemoveRef();
  if (pCountedColorSpace->use_count() > 1)
    return;

  // We have item only in m_ColorSpaceMap cache. Clean it.
  pCountedColorSpace->get()->Release();
  pCountedColorSpace->reset(nullptr);
}

CPDF_Pattern* CPDF_DocPageData::GetPattern(CPDF_Object* pPatternObj,
                                           bool bShading,
                                           const CFX_Matrix& matrix) {
  if (!pPatternObj)
    return nullptr;

  CPDF_CountedPattern* ptData = nullptr;
  auto it = m_PatternMap.find(pPatternObj);
  if (it != m_PatternMap.end()) {
    ptData = it->second;
    if (ptData->get()) {
      return ptData->AddRef();
    }
  }
  std::unique_ptr<CPDF_Pattern> pPattern;
  if (bShading) {
    pPattern = pdfium::MakeUnique<CPDF_ShadingPattern>(m_pPDFDoc, pPatternObj,
                                                       true, matrix);
  } else {
    CPDF_Dictionary* pDict = pPatternObj ? pPatternObj->GetDict() : nullptr;
    if (pDict) {
      int type = pDict->GetIntegerFor("PatternType");
      if (type == CPDF_Pattern::TILING) {
        pPattern = pdfium::MakeUnique<CPDF_TilingPattern>(m_pPDFDoc,
                                                          pPatternObj, matrix);
      } else if (type == CPDF_Pattern::SHADING) {
        pPattern = pdfium::MakeUnique<CPDF_ShadingPattern>(
            m_pPDFDoc, pPatternObj, false, matrix);
      }
    }
  }
  if (!pPattern)
    return nullptr;

  if (ptData) {
    ptData->reset(std::move(pPattern));
  } else {
    ptData = new CPDF_CountedPattern(std::move(pPattern));
    m_PatternMap[pPatternObj] = ptData;
  }
  return ptData->AddRef();
}

void CPDF_DocPageData::ReleasePattern(const CPDF_Object* pPatternObj) {
  if (!pPatternObj)
    return;

  auto it = m_PatternMap.find(pPatternObj);
  if (it == m_PatternMap.end())
    return;

  CPDF_CountedPattern* pPattern = it->second;
  if (!pPattern->get())
    return;

  pPattern->RemoveRef();
  if (pPattern->use_count() > 1)
    return;

  // We have item only in m_PatternMap cache. Clean it.
  pPattern->clear();
}

CPDF_Image* CPDF_DocPageData::GetImage(uint32_t dwStreamObjNum) {
  ASSERT(dwStreamObjNum);
  auto it = m_ImageMap.find(dwStreamObjNum);
  if (it != m_ImageMap.end())
    return it->second->AddRef();

  CPDF_CountedImage* pCountedImage = new CPDF_CountedImage(
      pdfium::MakeUnique<CPDF_Image>(m_pPDFDoc, dwStreamObjNum));
  m_ImageMap[dwStreamObjNum] = pCountedImage;
  return pCountedImage->AddRef();
}

void CPDF_DocPageData::ReleaseImage(uint32_t dwStreamObjNum) {
  ASSERT(dwStreamObjNum);
  auto it = m_ImageMap.find(dwStreamObjNum);
  if (it == m_ImageMap.end())
    return;

  CPDF_CountedImage* pCountedImage = it->second;
  if (!pCountedImage)
    return;

  pCountedImage->RemoveRef();
  if (pCountedImage->use_count() > 1)
    return;

  // We have item only in m_ImageMap cache. Clean it.
  delete pCountedImage->get();
  delete pCountedImage;
  m_ImageMap.erase(it);
}

CPDF_IccProfile* CPDF_DocPageData::GetIccProfile(
    CPDF_Stream* pIccProfileStream) {
  if (!pIccProfileStream)
    return nullptr;

  auto it = m_IccProfileMap.find(pIccProfileStream);
  if (it != m_IccProfileMap.end())
    return it->second->AddRef();

  CPDF_StreamAcc stream;
  stream.LoadAllData(pIccProfileStream, false);
  uint8_t digest[20];
  CRYPT_SHA1Generate(stream.GetData(), stream.GetSize(), digest);
  CFX_ByteString bsDigest(digest, 20);
  auto hash_it = m_HashProfileMap.find(bsDigest);
  if (hash_it != m_HashProfileMap.end()) {
    auto it_copied_stream = m_IccProfileMap.find(hash_it->second);
    if (it_copied_stream != m_IccProfileMap.end())
      return it_copied_stream->second->AddRef();
  }
  CPDF_CountedIccProfile* ipData = new CPDF_CountedIccProfile(
      pdfium::MakeUnique<CPDF_IccProfile>(stream.GetData(), stream.GetSize()));
  m_IccProfileMap[pIccProfileStream] = ipData;
  m_HashProfileMap[bsDigest] = pIccProfileStream;
  return ipData->AddRef();
}

void CPDF_DocPageData::ReleaseIccProfile(const CPDF_IccProfile* pIccProfile) {
  ASSERT(pIccProfile);

  for (auto it = m_IccProfileMap.begin(); it != m_IccProfileMap.end(); ++it) {
    CPDF_CountedIccProfile* profile = it->second;
    if (profile->get() != pIccProfile)
      continue;

    profile->RemoveRef();
    if (profile->use_count() > 1)
      continue;
    // We have item only in m_IccProfileMap cache. Clean it.
    delete profile->get();
    delete profile;
    m_IccProfileMap.erase(it);
    return;
  }
}

CPDF_StreamAcc* CPDF_DocPageData::GetFontFileStreamAcc(
    CPDF_Stream* pFontStream) {
  ASSERT(pFontStream);

  auto it = m_FontFileMap.find(pFontStream);
  if (it != m_FontFileMap.end())
    return it->second->AddRef();

  CPDF_Dictionary* pFontDict = pFontStream->GetDict();
  int32_t org_size = pFontDict->GetIntegerFor("Length1") +
                     pFontDict->GetIntegerFor("Length2") +
                     pFontDict->GetIntegerFor("Length3");
  org_size = std::max(org_size, 0);

  auto pFontAcc = pdfium::MakeUnique<CPDF_StreamAcc>();
  pFontAcc->LoadAllData(pFontStream, false, org_size);

  CPDF_CountedStreamAcc* pCountedFont =
      new CPDF_CountedStreamAcc(std::move(pFontAcc));
  m_FontFileMap[pFontStream] = pCountedFont;
  return pCountedFont->AddRef();
}

void CPDF_DocPageData::ReleaseFontFileStreamAcc(
    const CPDF_Stream* pFontStream) {
  if (!pFontStream)
    return;

  auto it = m_FontFileMap.find(pFontStream);
  if (it == m_FontFileMap.end())
    return;

  CPDF_CountedStreamAcc* pCountedStream = it->second;
  if (!pCountedStream)
    return;

  pCountedStream->RemoveRef();
  if (pCountedStream->use_count() > 1)
    return;

  // We have item only in m_FontFileMap cache. Clean it.
  delete pCountedStream->get();
  delete pCountedStream;
  m_FontFileMap.erase(it);
}

CPDF_CountedColorSpace* CPDF_DocPageData::FindColorSpacePtr(
    CPDF_Object* pCSObj) const {
  if (!pCSObj)
    return nullptr;

  auto it = m_ColorSpaceMap.find(pCSObj);
  return it != m_ColorSpaceMap.end() ? it->second : nullptr;
}

CPDF_CountedPattern* CPDF_DocPageData::FindPatternPtr(
    CPDF_Object* pPatternObj) const {
  if (!pPatternObj)
    return nullptr;

  auto it = m_PatternMap.find(pPatternObj);
  return it != m_PatternMap.end() ? it->second : nullptr;
}
