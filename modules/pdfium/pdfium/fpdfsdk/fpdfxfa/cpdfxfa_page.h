// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FPDFXFA_CPDFXFA_PAGE_H_
#define FPDFSDK_FPDFXFA_CPDFXFA_PAGE_H_

#include <memory>

#include "core/fxcrt/fx_system.h"

class CFX_Matrix;
class CPDFXFA_Context;
class CPDF_Dictionary;
class CPDF_Page;
class CXFA_FFPageView;

class CPDFXFA_Page {
 public:
  CPDFXFA_Page(CPDFXFA_Context* pContext, int page_index);

  void Retain() { m_iRef++; }
  void Release() {
    if (--m_iRef <= 0)
      delete this;
  }

  bool LoadPage();
  bool LoadPDFPage(CPDF_Dictionary* pageDict);
  CPDFXFA_Context* GetContext() const { return m_pContext; }
  int GetPageIndex() const { return m_iPageIndex; }
  CPDF_Page* GetPDFPage() const { return m_pPDFPage.get(); }
  CXFA_FFPageView* GetXFAPageView() const { return m_pXFAPageView; }

  void SetXFAPageView(CXFA_FFPageView* pPageView) {
    m_pXFAPageView = pPageView;
  }

  FX_FLOAT GetPageWidth() const;
  FX_FLOAT GetPageHeight() const;

  void DeviceToPage(int start_x,
                    int start_y,
                    int size_x,
                    int size_y,
                    int rotate,
                    int device_x,
                    int device_y,
                    double* page_x,
                    double* page_y);
  void PageToDevice(int start_x,
                    int start_y,
                    int size_x,
                    int size_y,
                    int rotate,
                    double page_x,
                    double page_y,
                    int* device_x,
                    int* device_y);

  CFX_Matrix GetDisplayMatrix(int xPos,
                              int yPos,
                              int xSize,
                              int ySize,
                              int iRotate) const;

 protected:
  // Refcounted class.
  ~CPDFXFA_Page();

  bool LoadPDFPage();
  bool LoadXFAPageView();

 private:
  std::unique_ptr<CPDF_Page> m_pPDFPage;
  CXFA_FFPageView* m_pXFAPageView;
  CPDFXFA_Context* const m_pContext;
  const int m_iPageIndex;
  int m_iRef;
};

#endif  // FPDFSDK_FPDFXFA_CPDFXFA_PAGE_H_
