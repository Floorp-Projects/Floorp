/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumEngineShim.h"


namespace mozilla {
namespace widget {

PDFiumEngineShim::PDFiumEngineShim(PRLibrary* aLibrary)
  : mPRLibrary(aLibrary)
  , mFPDF_InitLibrary(nullptr)
  , mFPDF_DestroyLibrary(nullptr)
  , mFPDF_LoadMemDocument(nullptr)
  , mFPDF_CloseDocument(nullptr)
  , mFPDF_GetPageCount(nullptr)
  , mFPDF_GetPageSizeByIndex(nullptr)
  , mFPDF_LoadPage(nullptr)
  , mFPDF_ClosePage(nullptr)
  , mFPDF_RenderPage(nullptr)
  , mFPDF_RenderPage_Close(nullptr)
  , mInitialized(false)
{
  MOZ_ASSERT(mPRLibrary);
}

PDFiumEngineShim::~PDFiumEngineShim()
{
}

bool
PDFiumEngineShim::InitSymbolsAndLibrary()
{
  if (mInitialized) {
    return true;
  }

  mFPDF_InitLibrary = (FPDF_InitLibrary_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_InitLibrary");
  if (!mFPDF_InitLibrary) {
    return false;
  }
  mFPDF_DestroyLibrary = (FPDF_DestroyLibrary_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_DestroyLibrary");
  if (!mFPDF_DestroyLibrary) {
    return false;
  }
  mFPDF_LoadMemDocument = (FPDF_LoadMemDocument_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_LoadMemDocument");
  if (!mFPDF_LoadMemDocument) {
    return false;
  }
  mFPDF_LoadDocument = (FPDF_LoadDocument_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_LoadDocument");
  if (!mFPDF_LoadDocument) {
    return false;
  }
  mFPDF_CloseDocument = (FPDF_CloseDocument_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_CloseDocument");
  if (!mFPDF_CloseDocument) {
    return false;
  }
  mFPDF_GetPageCount = (FPDF_GetPageCount_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_GetPageCount");
  if (!mFPDF_GetPageCount) {
    return false;
  }
  mFPDF_GetPageSizeByIndex = (FPDF_GetPageSizeByIndex_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_GetPageSizeByIndex");
  if (!mFPDF_GetPageSizeByIndex) {
    return false;
  }
  mFPDF_LoadPage = (FPDF_LoadPage_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_LoadPage");
  if (!mFPDF_LoadPage) {
    return false;
  }
  mFPDF_ClosePage = (FPDF_ClosePage_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_ClosePage");
  if (!mFPDF_ClosePage) {
    return false;
  }
  mFPDF_RenderPage = (FPDF_RenderPage_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_RenderPage");
  if (!mFPDF_RenderPage) {
    return false;
  }
  mFPDF_RenderPage_Close = (FPDF_RenderPage_Close_Pfn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_RenderPage_Close");
  if (!mFPDF_RenderPage_Close) {
    return false;
  }

  InitLibrary();
  mInitialized = true;
  return true;
}

void
PDFiumEngineShim::InitLibrary()
{
  mFPDF_InitLibrary();
}

void
PDFiumEngineShim::DestroyLibrary()
{
  mFPDF_DestroyLibrary();
}

FPDF_DOCUMENT
PDFiumEngineShim::LoadMemDocument(const void* aDataBuf,
                                  int aSize,
                                  FPDF_BYTESTRING aPassword)
{
  if (!mInitialized) {
    if (!InitSymbolsAndLibrary()) {
      return nullptr;
    }
  }

  return mFPDF_LoadMemDocument(aDataBuf, aSize, aPassword);
}

FPDF_DOCUMENT
PDFiumEngineShim::LoadDocument(FPDF_STRING file_path,
                               FPDF_BYTESTRING aPassword)
{
  if (!mInitialized) {
    if (!InitSymbolsAndLibrary()) {
      return nullptr;
    }
  }

  return mFPDF_LoadDocument(file_path, aPassword);
}

void
PDFiumEngineShim::CloseDocument(FPDF_DOCUMENT aDocument)
{
  MOZ_ASSERT(mInitialized);
  mFPDF_CloseDocument(aDocument);
  DestroyLibrary();
  mInitialized = false;
}

int
PDFiumEngineShim::GetPageCount(FPDF_DOCUMENT aDocument)
{
  MOZ_ASSERT(mInitialized);
  return mFPDF_GetPageCount(aDocument);
}

int
PDFiumEngineShim::GetPageSizeByIndex(FPDF_DOCUMENT aDocument,
                                     int aPageIndex,
                                     double* aWidth,
                                     double* aHeight)
{
  MOZ_ASSERT(mInitialized);
  return mFPDF_GetPageSizeByIndex(aDocument, aPageIndex, aWidth, aHeight);
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

void
PDFiumEngineShim::RenderPage_Close(FPDF_PAGE aPage)
{
  MOZ_ASSERT(mInitialized);
  mFPDF_RenderPage_Close(aPage);
}

} // namespace widget
} // namespace mozilla