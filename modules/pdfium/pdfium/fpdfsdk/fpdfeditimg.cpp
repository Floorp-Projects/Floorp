// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_edit.h"

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "fpdfsdk/fsdk_define.h"
#include "third_party/base/ptr_util.h"

DLLEXPORT FPDF_PAGEOBJECT STDCALL
FPDFPageObj_NewImgeObj(FPDF_DOCUMENT document) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return nullptr;

  CPDF_ImageObject* pImageObj = new CPDF_ImageObject;
  pImageObj->SetOwnedImage(pdfium::MakeUnique<CPDF_Image>(pDoc));
  return pImageObj;
}

FPDF_BOOL FPDFImageObj_LoadJpegHelper(FPDF_PAGE* pages,
                                      int nCount,
                                      FPDF_PAGEOBJECT image_object,
                                      FPDF_FILEACCESS* fileAccess,
                                      bool inlineJpeg) {
  if (!image_object || !fileAccess || !pages)
    return false;

  CFX_RetainPtr<IFX_SeekableReadStream> pFile =
      MakeSeekableReadStream(fileAccess);
  CPDF_ImageObject* pImgObj = reinterpret_cast<CPDF_ImageObject*>(image_object);
  for (int index = 0; index < nCount; index++) {
    CPDF_Page* pPage = CPDFPageFromFPDFPage(pages[index]);
    if (pPage)
      pImgObj->GetImage()->ResetCache(pPage, nullptr);
  }

  if (inlineJpeg)
    pImgObj->GetImage()->SetJpegImageInline(pFile);
  else
    pImgObj->GetImage()->SetJpegImage(pFile);

  return true;
}

DLLEXPORT FPDF_BOOL STDCALL
FPDFImageObj_LoadJpegFile(FPDF_PAGE* pages,
                          int nCount,
                          FPDF_PAGEOBJECT image_object,
                          FPDF_FILEACCESS* fileAccess) {
  return FPDFImageObj_LoadJpegHelper(pages, nCount, image_object, fileAccess,
                                     false);
}

DLLEXPORT FPDF_BOOL STDCALL
FPDFImageObj_LoadJpegFileInline(FPDF_PAGE* pages,
                                int nCount,
                                FPDF_PAGEOBJECT image_object,
                                FPDF_FILEACCESS* fileAccess) {
  return FPDFImageObj_LoadJpegHelper(pages, nCount, image_object, fileAccess,
                                     true);
}

DLLEXPORT FPDF_BOOL STDCALL FPDFImageObj_SetMatrix(FPDF_PAGEOBJECT image_object,
                                                   double a,
                                                   double b,
                                                   double c,
                                                   double d,
                                                   double e,
                                                   double f) {
  if (!image_object)
    return false;

  CPDF_ImageObject* pImgObj = reinterpret_cast<CPDF_ImageObject*>(image_object);
  pImgObj->set_matrix(
      CFX_Matrix(static_cast<FX_FLOAT>(a), static_cast<FX_FLOAT>(b),
                 static_cast<FX_FLOAT>(c), static_cast<FX_FLOAT>(d),
                 static_cast<FX_FLOAT>(e), static_cast<FX_FLOAT>(f)));
  pImgObj->CalcBoundingBox();
  return true;
}

DLLEXPORT FPDF_BOOL STDCALL FPDFImageObj_SetBitmap(FPDF_PAGE* pages,
                                                   int nCount,
                                                   FPDF_PAGEOBJECT image_object,
                                                   FPDF_BITMAP bitmap) {
  if (!image_object || !bitmap || !pages)
    return false;

  CPDF_ImageObject* pImgObj = reinterpret_cast<CPDF_ImageObject*>(image_object);
  for (int index = 0; index < nCount; index++) {
    CPDF_Page* pPage = CPDFPageFromFPDFPage(pages[index]);
    if (pPage)
      pImgObj->GetImage()->ResetCache(pPage, nullptr);
  }
  pImgObj->GetImage()->SetImage(reinterpret_cast<CFX_DIBitmap*>(bitmap));
  pImgObj->CalcBoundingBox();
  return true;
}
