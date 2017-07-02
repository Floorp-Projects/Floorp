// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_page.h"

#include <set>
#include <utility>

#include "core/fpdfapi/cpdf_pagerendercontext.h"
#include "core/fpdfapi/page/cpdf_contentparser.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

CPDF_Page::CPDF_Page(CPDF_Document* pDocument,
                     CPDF_Dictionary* pPageDict,
                     bool bPageCache)
    : m_PageWidth(100),
      m_PageHeight(100),
      m_pView(nullptr),
      m_pPageRender(bPageCache ? new CPDF_PageRenderCache(this) : nullptr) {
  m_pFormDict = pPageDict;
  m_pDocument = pDocument;
  if (!pPageDict)
    return;

  CPDF_Object* pageAttr = GetPageAttr("Resources");
  m_pResources = pageAttr ? pageAttr->GetDict() : nullptr;
  m_pPageResources = m_pResources;
  CPDF_Object* pRotate = GetPageAttr("Rotate");
  int rotate = pRotate ? pRotate->GetInteger() / 90 % 4 : 0;
  if (rotate < 0)
    rotate += 4;

  CPDF_Array* pMediaBox = ToArray(GetPageAttr("MediaBox"));
  CFX_FloatRect mediabox;
  if (pMediaBox) {
    mediabox = pMediaBox->GetRect();
    mediabox.Normalize();
  }
  if (mediabox.IsEmpty())
    mediabox = CFX_FloatRect(0, 0, 612, 792);

  CPDF_Array* pCropBox = ToArray(GetPageAttr("CropBox"));
  if (pCropBox) {
    m_BBox = pCropBox->GetRect();
    m_BBox.Normalize();
  }
  if (m_BBox.IsEmpty())
    m_BBox = mediabox;
  else
    m_BBox.Intersect(mediabox);

  m_PageWidth = m_BBox.right - m_BBox.left;
  m_PageHeight = m_BBox.top - m_BBox.bottom;
  if (rotate % 2)
    std::swap(m_PageWidth, m_PageHeight);

  switch (rotate) {
    case 0:
      m_PageMatrix = CFX_Matrix(1.0f, 0, 0, 1.0f, -m_BBox.left, -m_BBox.bottom);
      break;
    case 1:
      m_PageMatrix =
          CFX_Matrix(0, -1.0f, 1.0f, 0, -m_BBox.bottom, m_BBox.right);
      break;
    case 2:
      m_PageMatrix = CFX_Matrix(-1.0f, 0, 0, -1.0f, m_BBox.right, m_BBox.top);
      break;
    case 3:
      m_PageMatrix = CFX_Matrix(0, 1.0f, -1.0f, 0, m_BBox.top, -m_BBox.left);
      break;
  }

  m_Transparency = PDFTRANS_ISOLATED;
  LoadTransInfo();
}

CPDF_Page::~CPDF_Page() {}

void CPDF_Page::StartParse() {
  if (m_ParseState == CONTENT_PARSED || m_ParseState == CONTENT_PARSING)
    return;

  m_pParser = pdfium::MakeUnique<CPDF_ContentParser>();
  m_pParser->Start(this);
  m_ParseState = CONTENT_PARSING;
}

void CPDF_Page::ParseContent() {
  StartParse();
  ContinueParse(nullptr);
}

void CPDF_Page::SetRenderContext(
    std::unique_ptr<CPDF_PageRenderContext> pContext) {
  m_pRenderContext = std::move(pContext);
}

CPDF_Object* CPDF_Page::GetPageAttr(const CFX_ByteString& name) const {
  CPDF_Dictionary* pPageDict = m_pFormDict;
  std::set<CPDF_Dictionary*> visited;
  while (1) {
    visited.insert(pPageDict);
    if (CPDF_Object* pObj = pPageDict->GetDirectObjectFor(name))
      return pObj;

    pPageDict = pPageDict->GetDictFor("Parent");
    if (!pPageDict || pdfium::ContainsKey(visited, pPageDict))
      break;
  }
  return nullptr;
}

CFX_Matrix CPDF_Page::GetDisplayMatrix(int xPos,
                                       int yPos,
                                       int xSize,
                                       int ySize,
                                       int iRotate) const {
  if (m_PageWidth == 0 || m_PageHeight == 0)
    return CFX_Matrix();

  float x0 = 0;
  float y0 = 0;
  float x1 = 0;
  float y1 = 0;
  float x2 = 0;
  float y2 = 0;
  iRotate %= 4;
  switch (iRotate) {
    case 0:
      x0 = xPos;
      y0 = yPos + ySize;
      x1 = xPos;
      y1 = yPos;
      x2 = xPos + xSize;
      y2 = yPos + ySize;
      break;
    case 1:
      x0 = xPos;
      y0 = yPos;
      x1 = xPos + xSize;
      y1 = yPos;
      x2 = xPos;
      y2 = yPos + ySize;
      break;
    case 2:
      x0 = xPos + xSize;
      y0 = yPos;
      x1 = xPos + xSize;
      y1 = yPos + ySize;
      x2 = xPos;
      y2 = yPos;
      break;
    case 3:
      x0 = xPos + xSize;
      y0 = yPos + ySize;
      x1 = xPos;
      y1 = yPos + ySize;
      x2 = xPos + xSize;
      y2 = yPos;
      break;
  }
  CFX_Matrix matrix = m_PageMatrix;
  matrix.Concat(CFX_Matrix((x2 - x0) / m_PageWidth, (y2 - y0) / m_PageWidth,
                           (x1 - x0) / m_PageHeight, (y1 - y0) / m_PageHeight,
                           x0, y0));
  return matrix;
}

bool GraphicsData::operator<(const GraphicsData& other) const {
  if (fillAlpha != other.fillAlpha)
    return fillAlpha < other.fillAlpha;
  return strokeAlpha < other.strokeAlpha;
}

bool FontData::operator<(const FontData& other) const {
  return baseFont < other.baseFont;
}
