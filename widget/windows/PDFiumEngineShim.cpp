/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumEngineShim.h"


namespace mozilla {
namespace widget {

static PDFiumEngineShim* sPDFiumEngineShim;

/* static */
already_AddRefed<PDFiumEngineShim>
PDFiumEngineShim::GetInstanceOrNull()
{
  RefPtr<PDFiumEngineShim> inst = sPDFiumEngineShim;
  if (!inst) {
    inst = new PDFiumEngineShim();
    if (!inst->Init()) {
      inst = nullptr;
    }
    sPDFiumEngineShim = inst.get();
  }

  return inst.forget();
}

PDFiumEngineShim::PDFiumEngineShim()
  : mFPDF_InitLibrary(nullptr)
  , mFPDF_DestroyLibrary(nullptr)
  , mFPDF_CloseDocument(nullptr)
  , mFPDF_GetPageCount(nullptr)
  , mFPDF_LoadPage(nullptr)
  , mFPDF_ClosePage(nullptr)
  , mFPDF_RenderPage(nullptr)
#ifdef USE_EXTERNAL_PDFIUM
  , mPRLibrary(nullptr)
#endif
  , mInitialized(false)
{
}

PDFiumEngineShim::~PDFiumEngineShim()
{
  if (mInitialized) {
    mFPDF_DestroyLibrary();
  }

  sPDFiumEngineShim = nullptr;

#ifdef USE_EXTERNAL_PDFIUM
  if (mPRLibrary) {
    PR_UnloadLibrary(mPRLibrary);
  }
#endif
}

bool
PDFiumEngineShim::Init()
{
  if (mInitialized) {
    return true;
  }

#ifdef USE_EXTERNAL_PDFIUM
  nsAutoString PDFiumPath;
  mozilla::Preferences::GetString("print.load_external_pdfium", PDFiumPath);
  NS_ENSURE_FALSE(PDFiumPath.IsEmpty(), false);

  nsAutoCString filePath = NS_ConvertUTF16toUTF8(PDFiumPath);
  mPRLibrary = PR_LoadLibrary(filePath.get());
  NS_ENSURE_TRUE(mPRLibrary, false);

  mFPDF_InitLibrary = (FPDF_InitLibrary_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_InitLibrary");
  NS_ENSURE_TRUE(mFPDF_InitLibrary, false);

  mFPDF_DestroyLibrary = (FPDF_DestroyLibrary_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_DestroyLibrary");
  NS_ENSURE_TRUE(mFPDF_DestroyLibrary, false);

  mFPDF_LoadDocument = (FPDF_LoadDocument_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_LoadDocument");
  NS_ENSURE_TRUE(mFPDF_LoadDocument, false);

  mFPDF_CloseDocument = (FPDF_CloseDocument_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_CloseDocument");
  NS_ENSURE_TRUE(mFPDF_CloseDocument, false);

  mFPDF_GetPageCount = (FPDF_GetPageCount_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_GetPageCount");
  NS_ENSURE_TRUE(mFPDF_GetPageCount, false);

  mFPDF_LoadPage = (FPDF_LoadPage_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_LoadPage");
  NS_ENSURE_TRUE(mFPDF_LoadPage, false);

  mFPDF_ClosePage = (FPDF_ClosePage_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_ClosePage");
  NS_ENSURE_TRUE(mFPDF_ClosePage, false);

  mFPDF_RenderPage = (FPDF_RenderPage_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_RenderPage");
  NS_ENSURE_TRUE(mFPDF_RenderPage, false);

#else
  mFPDF_InitLibrary = (FPDF_InitLibrary_Pfn) FPDF_InitLibrary;
  mFPDF_DestroyLibrary = (FPDF_DestroyLibrary_Pfn) FPDF_DestroyLibrary;
  mFPDF_LoadDocument = (FPDF_LoadDocument_Pfn) FPDF_LoadDocument;
  mFPDF_CloseDocument = (FPDF_CloseDocument_Pfn) FPDF_CloseDocument;
  mFPDF_GetPageCount = (FPDF_GetPageCount_Pfn) FPDF_GetPageCount;
  mFPDF_LoadPage = (FPDF_LoadPage_Pfn) FPDF_LoadPage;
  mFPDF_ClosePage = (FPDF_ClosePage_Pfn) FPDF_ClosePage;
  mFPDF_RenderPage = (FPDF_RenderPage_Pfn) FPDF_RenderPage;
#endif

  mFPDF_InitLibrary();
  mInitialized = true;
  return true;
}

FPDF_DOCUMENT
PDFiumEngineShim::LoadDocument(FPDF_STRING file_path,
                               FPDF_BYTESTRING aPassword)
{
  MOZ_ASSERT(mInitialized);
  return mFPDF_LoadDocument(file_path, aPassword);
}

void
PDFiumEngineShim::CloseDocument(FPDF_DOCUMENT aDocument)
{
  MOZ_ASSERT(mInitialized);
  mFPDF_CloseDocument(aDocument);
}

int
PDFiumEngineShim::GetPageCount(FPDF_DOCUMENT aDocument)
{
  MOZ_ASSERT(mInitialized);
  return mFPDF_GetPageCount(aDocument);
}

FPDF_PAGE
PDFiumEngineShim::LoadPage(FPDF_DOCUMENT aDocument, int aPageIndex)
{
  MOZ_ASSERT(mInitialized);
  return mFPDF_LoadPage(aDocument, aPageIndex);
}

void
PDFiumEngineShim::ClosePage(FPDF_PAGE aPage)
{
  MOZ_ASSERT(mInitialized);
  mFPDF_ClosePage(aPage);
}

void
PDFiumEngineShim::RenderPage(HDC aDC, FPDF_PAGE aPage,
                             int aStartX, int aStartY,
                             int aSizeX, int aSizeY,
                             int aRotate, int aFlags)
{
  MOZ_ASSERT(mInitialized);
  mFPDF_RenderPage(aDC, aPage, aStartX, aStartY,
                   aSizeX, aSizeY, aRotate, aFlags);
}

} // namespace widget
} // namespace mozilla
