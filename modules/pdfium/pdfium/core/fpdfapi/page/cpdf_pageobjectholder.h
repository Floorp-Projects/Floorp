// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_PAGEOBJECTHOLDER_H_
#define CORE_FPDFAPI_PAGE_CPDF_PAGEOBJECTHOLDER_H_

#include <memory>

#include "core/fpdfapi/page/cpdf_pageobjectlist.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class IFX_Pause;
class CPDF_Dictionary;
class CPDF_Stream;
class CPDF_Document;
class CPDF_ContentParser;

#define PDFTRANS_GROUP 0x0100
#define PDFTRANS_ISOLATED 0x0200
#define PDFTRANS_KNOCKOUT 0x0400

class CPDF_PageObjectHolder {
 public:
  CPDF_PageObjectHolder();
  virtual ~CPDF_PageObjectHolder();

  void ContinueParse(IFX_Pause* pPause);
  bool IsParsed() const { return m_ParseState == CONTENT_PARSED; }

  CPDF_PageObjectList* GetPageObjectList() { return &m_PageObjectList; }
  const CPDF_PageObjectList* GetPageObjectList() const {
    return &m_PageObjectList;
  }

  bool BackgroundAlphaNeeded() const { return m_bBackgroundAlphaNeeded; }
  void SetBackgroundAlphaNeeded(bool needed) {
    m_bBackgroundAlphaNeeded = needed;
  }

  bool HasImageMask() const { return m_bHasImageMask; }
  void SetHasImageMask(bool value) { m_bHasImageMask = value; }

  void Transform(const CFX_Matrix& matrix);
  CFX_FloatRect CalcBoundingBox() const;

  CPDF_Dictionary* m_pFormDict;
  CPDF_Stream* m_pFormStream;
  CPDF_Document* m_pDocument;
  CPDF_Dictionary* m_pPageResources;
  CPDF_Dictionary* m_pResources;
  CFX_FloatRect m_BBox;
  int m_Transparency;

 protected:
  enum ParseState { CONTENT_NOT_PARSED, CONTENT_PARSING, CONTENT_PARSED };

  void LoadTransInfo();

  bool m_bBackgroundAlphaNeeded;
  bool m_bHasImageMask;
  ParseState m_ParseState;
  std::unique_ptr<CPDF_ContentParser> m_pParser;
  CPDF_PageObjectList m_PageObjectList;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_PAGEOBJECTHOLDER_H_
