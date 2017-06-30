// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fpdfxfa/cpdfxfa_page.h"

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "fpdfsdk/fpdfxfa/cxfa_fwladaptertimermgr.h"
#include "fpdfsdk/fsdk_define.h"
#include "public/fpdf_formfill.h"
#include "third_party/base/ptr_util.h"
#include "xfa/fxfa/xfa_ffdocview.h"
#include "xfa/fxfa/xfa_ffpageview.h"

CPDFXFA_Page::CPDFXFA_Page(CPDFXFA_Context* pContext, int page_index)
    : m_pXFAPageView(nullptr),
      m_pContext(pContext),
      m_iPageIndex(page_index),
      m_iRef(1) {}

CPDFXFA_Page::~CPDFXFA_Page() {
  if (m_pContext)
    m_pContext->RemovePage(this);
}

bool CPDFXFA_Page::LoadPDFPage() {
  if (!m_pContext)
    return false;

  CPDF_Document* pPDFDoc = m_pContext->GetPDFDoc();
  if (!pPDFDoc)
    return false;

  CPDF_Dictionary* pDict = pPDFDoc->GetPage(m_iPageIndex);
  if (!pDict)
    return false;

  if (!m_pPDFPage || m_pPDFPage->m_pFormDict != pDict) {
    m_pPDFPage = pdfium::MakeUnique<CPDF_Page>(pPDFDoc, pDict, true);
    m_pPDFPage->ParseContent();
  }
  return true;
}

bool CPDFXFA_Page::LoadXFAPageView() {
  if (!m_pContext)
    return false;

  CXFA_FFDoc* pXFADoc = m_pContext->GetXFADoc();
  if (!pXFADoc)
    return false;

  CXFA_FFDocView* pXFADocView = m_pContext->GetXFADocView();
  if (!pXFADocView)
    return false;

  CXFA_FFPageView* pPageView = pXFADocView->GetPageView(m_iPageIndex);
  if (!pPageView)
    return false;

  m_pXFAPageView = pPageView;
  return true;
}

bool CPDFXFA_Page::LoadPage() {
  if (!m_pContext || m_iPageIndex < 0)
    return false;

  int iDocType = m_pContext->GetDocType();
  switch (iDocType) {
    case DOCTYPE_PDF:
    case DOCTYPE_STATIC_XFA: {
      return LoadPDFPage();
    }
    case DOCTYPE_DYNAMIC_XFA: {
      return LoadXFAPageView();
    }
    default:
      return false;
  }
}

bool CPDFXFA_Page::LoadPDFPage(CPDF_Dictionary* pageDict) {
  if (!m_pContext || m_iPageIndex < 0 || !pageDict)
    return false;

  m_pPDFPage =
      pdfium::MakeUnique<CPDF_Page>(m_pContext->GetPDFDoc(), pageDict, true);
  m_pPDFPage->ParseContent();
  return true;
}

FX_FLOAT CPDFXFA_Page::GetPageWidth() const {
  if (!m_pPDFPage && !m_pXFAPageView)
    return 0.0f;

  int nDocType = m_pContext->GetDocType();
  switch (nDocType) {
    case DOCTYPE_DYNAMIC_XFA: {
      if (m_pXFAPageView)
        return m_pXFAPageView->GetPageViewRect().width;
      break;
    }
    case DOCTYPE_STATIC_XFA:
    case DOCTYPE_PDF: {
      if (m_pPDFPage)
        return m_pPDFPage->GetPageWidth();
      break;
    }
    default:
      return 0.0f;
  }

  return 0.0f;
}

FX_FLOAT CPDFXFA_Page::GetPageHeight() const {
  if (!m_pPDFPage && !m_pXFAPageView)
    return 0.0f;

  int nDocType = m_pContext->GetDocType();
  switch (nDocType) {
    case DOCTYPE_PDF:
    case DOCTYPE_STATIC_XFA: {
      if (m_pPDFPage)
        return m_pPDFPage->GetPageHeight();
      break;
    }
    case DOCTYPE_DYNAMIC_XFA: {
      if (m_pXFAPageView)
        return m_pXFAPageView->GetPageViewRect().height;
      break;
    }
    default:
      return 0.0f;
  }

  return 0.0f;
}

void CPDFXFA_Page::DeviceToPage(int start_x,
                                int start_y,
                                int size_x,
                                int size_y,
                                int rotate,
                                int device_x,
                                int device_y,
                                double* page_x,
                                double* page_y) {
  if (!m_pPDFPage && !m_pXFAPageView)
    return;

  CFX_Matrix device2page;
  device2page.SetReverse(
      GetDisplayMatrix(start_x, start_y, size_x, size_y, rotate));

  CFX_PointF pos = device2page.Transform(CFX_PointF(
      static_cast<FX_FLOAT>(device_x), static_cast<FX_FLOAT>(device_y)));

  *page_x = pos.x;
  *page_y = pos.y;
}

void CPDFXFA_Page::PageToDevice(int start_x,
                                int start_y,
                                int size_x,
                                int size_y,
                                int rotate,
                                double page_x,
                                double page_y,
                                int* device_x,
                                int* device_y) {
  if (!m_pPDFPage && !m_pXFAPageView)
    return;

  CFX_Matrix page2device =
      GetDisplayMatrix(start_x, start_y, size_x, size_y, rotate);

  CFX_PointF pos = page2device.Transform(
      CFX_PointF(static_cast<FX_FLOAT>(page_x), static_cast<FX_FLOAT>(page_y)));

  *device_x = FXSYS_round(pos.x);
  *device_y = FXSYS_round(pos.y);
}

CFX_Matrix CPDFXFA_Page::GetDisplayMatrix(int xPos,
                                          int yPos,
                                          int xSize,
                                          int ySize,
                                          int iRotate) const {
  if (!m_pPDFPage && !m_pXFAPageView)
    return CFX_Matrix();

  int nDocType = m_pContext->GetDocType();
  switch (nDocType) {
    case DOCTYPE_DYNAMIC_XFA: {
      if (m_pXFAPageView) {
        return m_pXFAPageView->GetDisplayMatrix(
            CFX_Rect(xPos, yPos, xSize, ySize), iRotate);
      }
      break;
    }
    case DOCTYPE_PDF:
    case DOCTYPE_STATIC_XFA: {
      if (m_pPDFPage)
        return m_pPDFPage->GetDisplayMatrix(xPos, yPos, xSize, ySize, iRotate);
      break;
    }
    default:
      return CFX_Matrix();
  }
  return CFX_Matrix();
}
