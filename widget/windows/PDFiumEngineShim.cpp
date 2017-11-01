/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumEngineShim.h"
#include "private/pprio.h"

typedef void (STDCALL *FPDF_InitLibrary_Pfn)();
typedef void (STDCALL *FPDF_DestroyLibrary_Pfn)();

typedef FPDF_DOCUMENT (STDCALL *FPDF_LoadDocument_Pfn)(FPDF_STRING file_path,
                                                      FPDF_BYTESTRING password);
typedef FPDF_DOCUMENT (STDCALL *FPDF_LoadCustomDocument_Pfn)(FPDF_FILEACCESS* pFileAccess,
                                                             FPDF_BYTESTRING password);
typedef void(STDCALL *FPDF_CloseDocument_Pfn)(FPDF_DOCUMENT aDocument);

typedef int (STDCALL *FPDF_GetPageCount_Pfn)(FPDF_DOCUMENT aDocument);

typedef FPDF_PAGE (STDCALL *FPDF_LoadPage_Pfn)(FPDF_DOCUMENT aDocument,
                                               int aPageIndex);
typedef void (STDCALL *FPDF_ClosePage_Pfn)(FPDF_PAGE aPage);
typedef void (STDCALL *FPDF_RenderPage_Pfn)(HDC aDC,
                                            FPDF_PAGE aPage,
                                            int aStartX,
                                            int aStartY,
                                            int aSizeX,
                                            int aSizeY,
                                            int aRotate,
                                            int aFlags);

namespace mozilla {
namespace widget {

static PDFiumEngineShim* sPDFiumEngineShim;

struct PDFFunctionPointerTable
{
  PDFFunctionPointerTable()
    : mFPDF_InitLibrary(nullptr)
    , mFPDF_DestroyLibrary(nullptr)
    , mFPDF_CloseDocument(nullptr)
    , mFPDF_GetPageCount(nullptr)
    , mFPDF_LoadPage(nullptr)
    , mFPDF_ClosePage(nullptr)
    , mFPDF_RenderPage(nullptr)
  {
  }

  FPDF_InitLibrary_Pfn        mFPDF_InitLibrary;
  FPDF_DestroyLibrary_Pfn     mFPDF_DestroyLibrary;
  FPDF_LoadDocument_Pfn       mFPDF_LoadDocument;
  FPDF_LoadCustomDocument_Pfn mFPDF_LoadCustomDocument;
  FPDF_CloseDocument_Pfn      mFPDF_CloseDocument;
  FPDF_GetPageCount_Pfn       mFPDF_GetPageCount;
  FPDF_LoadPage_Pfn           mFPDF_LoadPage;
  FPDF_ClosePage_Pfn          mFPDF_ClosePage;
  FPDF_RenderPage_Pfn         mFPDF_RenderPage;
};

/* static */
already_AddRefed<PDFiumEngineShim>
PDFiumEngineShim::GetInstanceOrNull()
{
  RefPtr<PDFiumEngineShim> inst = sPDFiumEngineShim;
  if (!inst) {
    inst = new PDFiumEngineShim();
    if (!inst->Init(nsCString("pdfium.dll"))) {
      inst = nullptr;
    }
    sPDFiumEngineShim = inst.get();
  }

  return inst.forget();
}

/* static */
already_AddRefed<PDFiumEngineShim>
PDFiumEngineShim::GetInstanceOrNull(const nsCString& aLibrary)
{
  RefPtr<PDFiumEngineShim> shim = new PDFiumEngineShim();
  if (!shim->Init(aLibrary)) {
    return nullptr;
  }

  return shim.forget();
}

PDFiumEngineShim::PDFiumEngineShim()
  : mTable(MakeUnique<PDFFunctionPointerTable>())
  , mInitialized(false)
  , mPRLibrary(nullptr)
{
}

PDFiumEngineShim::~PDFiumEngineShim()
{
  if (mInitialized) {
    mTable->mFPDF_DestroyLibrary();
  }

  sPDFiumEngineShim = nullptr;

  if (mPRLibrary) {
    PR_UnloadLibrary(mPRLibrary);
  }
}

bool
PDFiumEngineShim::Init(const nsCString& aLibrary)
{
  if (mInitialized) {
    return true;
  }

  mPRLibrary = PR_LoadLibrary(aLibrary.get());
  NS_ENSURE_TRUE(mPRLibrary, false);

  mTable->mFPDF_InitLibrary =
    (FPDF_InitLibrary_Pfn)PR_FindFunctionSymbol(mPRLibrary,
                                                "FPDF_InitLibrary");
  NS_ENSURE_TRUE(mTable->mFPDF_InitLibrary, false);

  mTable->mFPDF_DestroyLibrary =
    (FPDF_DestroyLibrary_Pfn)PR_FindFunctionSymbol(mPRLibrary,
                                                  "FPDF_DestroyLibrary");
  NS_ENSURE_TRUE(mTable->mFPDF_DestroyLibrary, false);

  mTable->mFPDF_LoadDocument =
    (FPDF_LoadDocument_Pfn)PR_FindFunctionSymbol(mPRLibrary,
                                                 "FPDF_LoadDocument");
  NS_ENSURE_TRUE(mTable->mFPDF_LoadDocument, false);

  mTable->mFPDF_LoadCustomDocument =
    (FPDF_LoadCustomDocument_Pfn)PR_FindFunctionSymbol(mPRLibrary,
                                                       "FPDF_LoadCustomDocument");
  NS_ENSURE_TRUE(mTable->mFPDF_LoadCustomDocument, false);

  mTable->mFPDF_CloseDocument =
    (FPDF_CloseDocument_Pfn)PR_FindFunctionSymbol(mPRLibrary,
                                                  "FPDF_CloseDocument");
  NS_ENSURE_TRUE(mTable->mFPDF_CloseDocument, false);

  mTable->mFPDF_GetPageCount =
    (FPDF_GetPageCount_Pfn)PR_FindFunctionSymbol(mPRLibrary,
                                                 "FPDF_GetPageCount");
  NS_ENSURE_TRUE(mTable->mFPDF_GetPageCount, false);

  mTable->mFPDF_LoadPage =
    (FPDF_LoadPage_Pfn)PR_FindFunctionSymbol(mPRLibrary, "FPDF_LoadPage");
  NS_ENSURE_TRUE(mTable->mFPDF_LoadPage, false);

  mTable->mFPDF_ClosePage =
    (FPDF_ClosePage_Pfn)PR_FindFunctionSymbol(mPRLibrary, "FPDF_ClosePage");
  NS_ENSURE_TRUE(mTable->mFPDF_ClosePage, false);

  mTable->mFPDF_RenderPage =
    (FPDF_RenderPage_Pfn)PR_FindFunctionSymbol(mPRLibrary, "FPDF_RenderPage");
  NS_ENSURE_TRUE(mTable->mFPDF_RenderPage, false);

  mTable->mFPDF_InitLibrary();
  mInitialized = true;
  return true;
}

FPDF_DOCUMENT
PDFiumEngineShim::LoadDocument(FPDF_STRING file_path,
                               FPDF_BYTESTRING aPassword)
{
  MOZ_ASSERT(mInitialized);
  return mTable->mFPDF_LoadDocument(file_path, aPassword);
}

FPDF_DOCUMENT
PDFiumEngineShim::LoadDocument(PRFileDesc* aPrfile,
                               FPDF_BYTESTRING aPassword)
{
  MOZ_ASSERT(mInitialized && aPrfile);

  PROffset32 fileLength = PR_Seek64(aPrfile, 0, PR_SEEK_END);
  if (fileLength == -1) {
    NS_WARNING("Failed to access the given FD.");
    return nullptr;
  }

  FPDF_FILEACCESS fileAccess;
  fileAccess.m_FileLen = static_cast<unsigned long>(fileLength);
  fileAccess.m_Param = reinterpret_cast<void*>(aPrfile);
  fileAccess.m_GetBlock =
    [](void* param, unsigned long pos, unsigned char* buf, unsigned long size)
    {
      PRFileDesc* prfile = reinterpret_cast<PRFileDesc*>(param);

      if (PR_Seek64(prfile, pos, PR_SEEK_SET) != pos) {
        return 0;
      }

      if (PR_Read(prfile, buf, size) <= 0) {
        return 0;
      }

      return 1;
    };

  return mTable->mFPDF_LoadCustomDocument(&fileAccess, aPassword);
}

void
PDFiumEngineShim::CloseDocument(FPDF_DOCUMENT aDocument)
{
  MOZ_ASSERT(mInitialized);
  mTable->mFPDF_CloseDocument(aDocument);
}

int
PDFiumEngineShim::GetPageCount(FPDF_DOCUMENT aDocument)
{
  MOZ_ASSERT(mInitialized);
  return mTable->mFPDF_GetPageCount(aDocument);
}

FPDF_PAGE
PDFiumEngineShim::LoadPage(FPDF_DOCUMENT aDocument, int aPageIndex)
{
  MOZ_ASSERT(mInitialized);
  return mTable->mFPDF_LoadPage(aDocument, aPageIndex);
}

void
PDFiumEngineShim::ClosePage(FPDF_PAGE aPage)
{
  MOZ_ASSERT(mInitialized);
  mTable->mFPDF_ClosePage(aPage);
}

void
PDFiumEngineShim::RenderPage(HDC aDC, FPDF_PAGE aPage,
                             int aStartX, int aStartY,
                             int aSizeX, int aSizeY,
                             int aRotate, int aFlags)
{
  MOZ_ASSERT(mInitialized);
  mTable->mFPDF_RenderPage(aDC, aPage, aStartX, aStartY,
                           aSizeX, aSizeY, aRotate, aFlags);
}

} // namespace widget
} // namespace mozilla
