// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_DOCPAGEDATA_H_
#define CORE_FPDFAPI_PAGE_CPDF_DOCPAGEDATA_H_

#include <map>
#include <set>

#include "core/fpdfapi/page/cpdf_countedobject.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"

class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Font;
class CPDF_FontEncoding;
class CPDF_IccProfile;
class CPDF_Image;
class CPDF_Object;
class CPDF_Stream;
class CPDF_StreamAcc;

class CPDF_DocPageData {
 public:
  explicit CPDF_DocPageData(CPDF_Document* pPDFDoc);
  ~CPDF_DocPageData();

  void Clear(bool bRelease = false);
  CPDF_Font* GetFont(CPDF_Dictionary* pFontDict);
  CPDF_Font* GetStandardFont(const CFX_ByteString& fontName,
                             CPDF_FontEncoding* pEncoding);
  void ReleaseFont(const CPDF_Dictionary* pFontDict);
  CPDF_ColorSpace* GetColorSpace(CPDF_Object* pCSObj,
                                 const CPDF_Dictionary* pResources);
  CPDF_ColorSpace* GetCopiedColorSpace(CPDF_Object* pCSObj);
  void ReleaseColorSpace(const CPDF_Object* pColorSpace);
  CPDF_Pattern* GetPattern(CPDF_Object* pPatternObj,
                           bool bShading,
                           const CFX_Matrix& matrix);
  void ReleasePattern(const CPDF_Object* pPatternObj);
  CPDF_Image* GetImage(uint32_t dwStreamObjNum);
  void ReleaseImage(uint32_t dwStreamObjNum);
  CPDF_IccProfile* GetIccProfile(CPDF_Stream* pIccProfileStream);
  void ReleaseIccProfile(const CPDF_IccProfile* pIccProfile);
  CPDF_StreamAcc* GetFontFileStreamAcc(CPDF_Stream* pFontStream);
  void ReleaseFontFileStreamAcc(const CPDF_Stream* pFontStream);
  bool IsForceClear() const { return m_bForceClear; }
  CPDF_CountedColorSpace* FindColorSpacePtr(CPDF_Object* pCSObj) const;
  CPDF_CountedPattern* FindPatternPtr(CPDF_Object* pPatternObj) const;

 private:
  using CPDF_CountedFont = CPDF_CountedObject<CPDF_Font>;
  using CPDF_CountedIccProfile = CPDF_CountedObject<CPDF_IccProfile>;
  using CPDF_CountedImage = CPDF_CountedObject<CPDF_Image>;
  using CPDF_CountedStreamAcc = CPDF_CountedObject<CPDF_StreamAcc>;

  using CPDF_ColorSpaceMap =
      std::map<const CPDF_Object*, CPDF_CountedColorSpace*>;
  using CPDF_FontFileMap = std::map<const CPDF_Stream*, CPDF_CountedStreamAcc*>;
  using CPDF_FontMap = std::map<const CPDF_Dictionary*, CPDF_CountedFont*>;
  using CPDF_IccProfileMap =
      std::map<const CPDF_Stream*, CPDF_CountedIccProfile*>;
  using CPDF_ImageMap = std::map<uint32_t, CPDF_CountedImage*>;
  using CPDF_PatternMap = std::map<const CPDF_Object*, CPDF_CountedPattern*>;

  CPDF_ColorSpace* GetColorSpaceImpl(CPDF_Object* pCSObj,
                                     const CPDF_Dictionary* pResources,
                                     std::set<CPDF_Object*>* pVisited);

  CPDF_Document* const m_pPDFDoc;
  bool m_bForceClear;
  std::map<CFX_ByteString, CPDF_Stream*> m_HashProfileMap;
  CPDF_ColorSpaceMap m_ColorSpaceMap;
  CPDF_FontFileMap m_FontFileMap;
  CPDF_FontMap m_FontMap;
  CPDF_IccProfileMap m_IccProfileMap;
  CPDF_ImageMap m_ImageMap;
  CPDF_PatternMap m_PatternMap;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_DOCPAGEDATA_H_
