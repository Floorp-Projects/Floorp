// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_EDIT_CPDF_PAGECONTENTGENERATOR_H_
#define CORE_FPDFAPI_EDIT_CPDF_PAGECONTENTGENERATOR_H_

#include <vector>

#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Document;
class CPDF_ImageObject;
class CPDF_Page;
class CPDF_PageObject;
class CPDF_PathObject;
class CPDF_TextObject;

class CPDF_PageContentGenerator {
 public:
  explicit CPDF_PageContentGenerator(CPDF_Page* pPage);
  ~CPDF_PageContentGenerator();

  void GenerateContent();

 private:
  friend class CPDF_PageContentGeneratorTest;

  void ProcessPath(CFX_ByteTextBuf* buf, CPDF_PathObject* pPathObj);
  void ProcessImage(CFX_ByteTextBuf* buf, CPDF_ImageObject* pImageObj);
  void ProcessGraphics(CFX_ByteTextBuf* buf, CPDF_PageObject* pPageObj);
  void ProcessText(CFX_ByteTextBuf* buf, CPDF_TextObject* pTextObj);
  CFX_ByteString RealizeResource(uint32_t dwResourceObjNum,
                                 const CFX_ByteString& bsType);

  CPDF_Page* const m_pPage;
  CPDF_Document* const m_pDocument;
  std::vector<CPDF_PageObject*> m_pageObjects;
};

#endif  // CORE_FPDFAPI_EDIT_CPDF_PAGECONTENTGENERATOR_H_
