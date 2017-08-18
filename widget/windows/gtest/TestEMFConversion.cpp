/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFViaEMFPrintHelper.h"
#include "WindowsEMF.h"
#include "gtest/gtest.h"
#include "nsLocalFile.h"
#include "nsFileStreams.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"

typedef void (__stdcall *FPDF_InitLibrary_fn)();
typedef void (__stdcall *FPDF_DestroyLibrary_fn)();

typedef FPDF_DOCUMENT (__stdcall *FPDF_LoadDocument_fn)(FPDF_STRING file_path,
                                                     FPDF_BYTESTRING password);

typedef void(__stdcall *FPDF_CloseDocument_fn)(FPDF_DOCUMENT aDocument);

typedef int (__stdcall *FPDF_GetPageCount_fn)(FPDF_DOCUMENT aDocument);

typedef FPDF_PAGE (__stdcall *FPDF_LoadPage_fn)(FPDF_DOCUMENT aDocument,
                                                int aPageIndex);
typedef void (__stdcall *FPDF_ClosePage_fn)(FPDF_PAGE aPage);
typedef void (__stdcall *FPDF_RenderPage_fn)(HDC aDC,
                                             FPDF_PAGE aPage,
                                             int aStartX,
                                             int aStartY,
                                             int aSizeX,
                                             int aSizeY,
                                             int aRotate,
                                             int aFlags);


using namespace mozilla;
namespace mozilla {
namespace widget {

already_AddRefed<nsIFile>
GetFileViaSpecialDirectory(const char* aSpecialDirName,
                           const char* aFileName)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(aSpecialDirName, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = file->AppendNative(nsDependentCString(aFileName));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return file.forget();
}

NS_IMETHODIMP
GetFilePathViaSpecialDirectory(const char* aSpecialDirName,
                               const char* aFileName,
                               nsAutoString& aPath)
{
  nsCOMPtr<nsIFile> file = GetFileViaSpecialDirectory(aSpecialDirName,
                                                      aFileName);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  nsAutoCString path;
  nsresult rv  = file->GetNativePath(path);
  if (NS_SUCCEEDED(rv)) {
    aPath = NS_ConvertUTF8toUTF16(path);
  }
  return rv;
}

UniquePtr<PDFViaEMFPrintHelper>
CreatePrintHelper(const char* aFileName)
{
  nsCOMPtr<nsIFile> file = GetFileViaSpecialDirectory(NS_OS_CURRENT_WORKING_DIR,
                                                      aFileName);
  NS_ENSURE_TRUE(file, nullptr);

  UniquePtr<PDFViaEMFPrintHelper> PDFPrintHelper =
    MakeUnique<PDFViaEMFPrintHelper>();
  nsresult rv = PDFPrintHelper->OpenDocument(file);
  NS_ENSURE_SUCCESS(rv, nullptr);

  NS_ENSURE_TRUE(PDFPrintHelper->GetPageCount() > 0, nullptr);

  return PDFPrintHelper;
}

uint32_t
GetFileContents(const nsAutoString aFile, Vector<char>& aBuf)
{
  RefPtr<nsLocalFile> file = new nsLocalFile();
  file->InitWithPath(aFile);
  RefPtr<nsFileInputStream> inputStream = new nsFileInputStream();
  nsresult rv = inputStream->Init(file,
                                  /* ioFlags */ PR_RDONLY,
                                  /* perm */ -1,
                                  /* behaviorFlags */ 0);
  if (NS_FAILED(rv)) {
    return 0;
  }

  int64_t size = 0;
  inputStream->GetSize(&size);
  if (size <= 0) {
    return 0;
  }

  aBuf.initCapacity(size);
  uint32_t len = static_cast<uint32_t>(size);
  uint32_t read = 0;
  uint32_t haveRead = 0;
  while (len > haveRead) {
    rv = inputStream->Read(aBuf.begin() + haveRead, len - haveRead, &read);
    if (NS_FAILED(rv)) {
      return 0;
    }
    haveRead += read;
  }
  return haveRead;
}

class EMFViaExtDLLHelper
{
public:
  EMFViaExtDLLHelper();
  ~EMFViaExtDLLHelper();

  bool OpenDocument(const char* aFileName);
  void CloseDocument();
  bool DrawPageToFile(const wchar_t* aFilePath,
                      unsigned int aPageIndex,
                      int aPageWidth, int aPageHeight);

private:
  bool Init();
  bool RenderPageToDC(HDC aDC, unsigned int aPageIndex,
                      int aPageWidth, int aPageHeight);
  float ComputeScaleFactor(int aDCWidth, int aDCHeight,
                           int aPageWidth, int aPageHeight);

  FPDF_InitLibrary_fn       mFPDF_InitLibrary;
  FPDF_DestroyLibrary_fn    mFPDF_DestroyLibrary;
  FPDF_LoadDocument_fn      mFPDF_LoadDocument;
  FPDF_CloseDocument_fn     mFPDF_CloseDocument;
  FPDF_GetPageCount_fn      mFPDF_GetPageCount;
  FPDF_LoadPage_fn          mFPDF_LoadPage;
  FPDF_ClosePage_fn         mFPDF_ClosePage;
  FPDF_RenderPage_fn        mFPDF_RenderPage;

  PRLibrary*                mPRLibrary;
  FPDF_DOCUMENT             mPDFDoc;
  bool                      mInitialized;
};

EMFViaExtDLLHelper::EMFViaExtDLLHelper()
  : mFPDF_InitLibrary(nullptr)
  , mFPDF_DestroyLibrary(nullptr)
  , mFPDF_CloseDocument(nullptr)
  , mFPDF_GetPageCount(nullptr)
  , mFPDF_LoadPage(nullptr)
  , mFPDF_ClosePage(nullptr)
  , mFPDF_RenderPage(nullptr)
  , mPRLibrary(nullptr)
  , mPDFDoc(nullptr)
  , mInitialized(false)
{
}

EMFViaExtDLLHelper::~EMFViaExtDLLHelper()
{
  CloseDocument();

  if (mInitialized) {
    mFPDF_DestroyLibrary();
  }

  if (mPRLibrary) {
    PR_UnloadLibrary(mPRLibrary);
  }
}

bool
EMFViaExtDLLHelper::Init()
{
  if (mInitialized) {
    return true;
  }
#ifdef _WIN64
  nsAutoCString externalDll("pdfium_ref_x64.dll");
#else
  nsAutoCString externalDll("pdfium_ref_x86.dll");
#endif
  mPRLibrary = PR_LoadLibrary(externalDll.get());
  NS_ENSURE_TRUE(mPRLibrary, false);

  mFPDF_InitLibrary = (FPDF_InitLibrary_fn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_InitLibrary");
  NS_ENSURE_TRUE(mFPDF_InitLibrary, false);

  mFPDF_DestroyLibrary = (FPDF_DestroyLibrary_fn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_DestroyLibrary");
  NS_ENSURE_TRUE(mFPDF_DestroyLibrary, false);

  mFPDF_LoadDocument = (FPDF_LoadDocument_fn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_LoadDocument");
  NS_ENSURE_TRUE(mFPDF_LoadDocument, false);

  mFPDF_CloseDocument = (FPDF_CloseDocument_fn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_CloseDocument");
  NS_ENSURE_TRUE(mFPDF_CloseDocument, false);

  mFPDF_GetPageCount = (FPDF_GetPageCount_fn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_GetPageCount");
  NS_ENSURE_TRUE(mFPDF_GetPageCount, false);

  mFPDF_LoadPage = (FPDF_LoadPage_fn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_LoadPage");
  NS_ENSURE_TRUE(mFPDF_LoadPage, false);

  mFPDF_ClosePage = (FPDF_ClosePage_fn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_ClosePage");
  NS_ENSURE_TRUE(mFPDF_ClosePage, false);

  mFPDF_RenderPage = (FPDF_RenderPage_fn)PR_FindFunctionSymbol(
    mPRLibrary, "FPDF_RenderPage");
  NS_ENSURE_TRUE(mFPDF_RenderPage, false);

  mFPDF_InitLibrary();
  mInitialized = true;
  return true;
}

bool
EMFViaExtDLLHelper::OpenDocument(const char* aFileName)
{
  if (!Init()) {
    return false;
  }

  mPDFDoc = mFPDF_LoadDocument(aFileName, NULL);
  NS_ENSURE_TRUE(mPDFDoc, false);

  NS_ENSURE_TRUE(mFPDF_GetPageCount(mPDFDoc) >= 1, false);
  return true;
}

void
EMFViaExtDLLHelper::CloseDocument()
{
  if (mPDFDoc) {
    mFPDF_CloseDocument(mPDFDoc);
    mPDFDoc = nullptr;
  }
}

float
EMFViaExtDLLHelper::ComputeScaleFactor(int aDCWidth, int aDCHeight,
                                       int aPageWidth, int aPageHeight)
{
  if (aPageWidth <= 0 || aPageHeight <= 0) {
    return 0.0;
  }

  float scaleFactor = 1.0;
  // If page fits DC - no scaling needed.
  if (aDCWidth < aPageWidth || aDCHeight < aPageHeight) {
    float xFactor =
      static_cast<float>(aDCWidth) / static_cast <float>(aPageWidth);
    float yFactor =
      static_cast<float>(aDCHeight) / static_cast <float>(aPageHeight);
    scaleFactor = std::min(xFactor, yFactor);
  }
  return scaleFactor;
}

bool
EMFViaExtDLLHelper::RenderPageToDC(HDC aDC, unsigned int aPageIndex,
                                  int aPageWidth, int aPageHeight)
{
  NS_ENSURE_TRUE(aDC, false);
  NS_ENSURE_TRUE(mPDFDoc, false);
  NS_ENSURE_TRUE(static_cast<int>(aPageIndex) < mFPDF_GetPageCount(mPDFDoc),
                 false);
  NS_ENSURE_TRUE((aPageWidth > 0) && (aPageHeight > 0), false);

  int dcWidth = ::GetDeviceCaps(aDC, HORZRES);
  int dcHeight = ::GetDeviceCaps(aDC, VERTRES);
  float scaleFactor = ComputeScaleFactor(dcWidth, dcHeight,
                                         aPageWidth, aPageHeight);
  if (scaleFactor <= 0.0) {
    return false;
  }

  int savedState = ::SaveDC(aDC);
  ::SetGraphicsMode(aDC, GM_ADVANCED);
  XFORM xform = { 0 };
  xform.eM11 = xform.eM22 = scaleFactor;
  ::ModifyWorldTransform(aDC, &xform, MWT_LEFTMULTIPLY);

  // The caller wanted all drawing to happen within the bounds specified.
  // Based on scale calculations, our destination rect might be larger
  // than the bounds. Set the clip rect to the bounds.
  ::IntersectClipRect(aDC, 0, 0, aPageWidth, aPageHeight);

  FPDF_PAGE pdfPage = mFPDF_LoadPage(mPDFDoc, aPageIndex);
  NS_ENSURE_TRUE(pdfPage, false);

  mFPDF_RenderPage(aDC, pdfPage, 0, 0, aPageWidth, aPageHeight,
                   0, FPDF_ANNOT | FPDF_PRINTING | FPDF_NO_CATCH);
  mFPDF_ClosePage(pdfPage);
  ::RestoreDC(aDC, savedState);

  return true;
}

bool
EMFViaExtDLLHelper::DrawPageToFile(const wchar_t* aFilePath,
                                  unsigned int aPageIndex,
                                  int aPageWidth, int aPageHeight)
{
  WindowsEMF emf;
  bool result = emf.InitForDrawing(aFilePath);
  NS_ENSURE_TRUE(result, false);

  result = RenderPageToDC(emf.GetDC(), aPageIndex, aPageWidth, aPageHeight);
  NS_ENSURE_TRUE(result, false);
  return emf.SaveToFile();
}

// Compare an EMF file with the reference which is generated by the external
// library.
TEST(TestEMFConversion, CompareEMFWithReference)
{
  UniquePtr<PDFViaEMFPrintHelper> PDFHelper =
    CreatePrintHelper("PrinterTestPage.pdf");
  ASSERT_NE(PDFHelper, nullptr);

  // Convert a PDF file to an EMF file(PrinterTestPage.pdf -> gtest.emf)
  nsAutoString emfPath;
  nsresult rv = GetFilePathViaSpecialDirectory(NS_OS_TEMP_DIR,
                                               "gtest.emf",
                                               emfPath);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  int pageWidth = 4961;
  int pageHeight = 7016;
  bool result = PDFHelper->DrawPageToFile(emfPath.get(), 0,
                                          pageWidth, pageHeight);
  ASSERT_TRUE(result);

  PDFHelper->CloseDocument();

  Vector<char> emfContents;
  uint32_t size = GetFileContents(emfPath, emfContents);
  ASSERT_GT(size, static_cast<uint32_t>(0));


  // Convert a PDF file to an EMF file by external library.
  // (PrinterTestPage.pdf -> gtestRef.emf)
  nsCOMPtr<nsIFile> file = GetFileViaSpecialDirectory(NS_OS_CURRENT_WORKING_DIR,
                                                      "PrinterTestPage.pdf");
  nsAutoCString PDFPath;
  rv = file->GetNativePath(PDFPath);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  UniquePtr<EMFViaExtDLLHelper> ExtHelper = MakeUnique<EMFViaExtDLLHelper>();
  result = ExtHelper->OpenDocument(PDFPath.get());
  ASSERT_TRUE(result);

  nsAutoString emfPathRef;
  rv = GetFilePathViaSpecialDirectory(NS_OS_TEMP_DIR,
                                      "gtestRef.emf", emfPathRef);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  result = ExtHelper->DrawPageToFile(emfPathRef.get(), 0,
                                     pageWidth, pageHeight);
  ASSERT_TRUE(result);

  ExtHelper->CloseDocument();

  Vector<char> emfContentsRef;
  uint32_t sizeRef = GetFileContents(emfPathRef, emfContentsRef);
  ASSERT_GT(sizeRef, static_cast<uint32_t>(0));

  ASSERT_EQ(size, sizeRef);

  // Compare gtest.emf and gtestRef.emf
  ASSERT_TRUE(memcmp(emfContents.begin(), emfContentsRef.begin(), size) == 0);
}

// Input a PDF file which does not exist
TEST(TestEMFConversion, TestInputNonExistingPDF)
{
  UniquePtr<PDFViaEMFPrintHelper> PDFHelper = CreatePrintHelper("null.pdf");
  ASSERT_EQ(PDFHelper, nullptr);
}

// Test insufficient width and height
TEST(TestEMFConversion, TestInsufficientWidthAndHeight)
{
  UniquePtr<PDFViaEMFPrintHelper> PDFHelper =
    CreatePrintHelper("PrinterTestPage.pdf");
  ASSERT_NE(PDFHelper, nullptr);

  nsAutoString emfPath;
  nsresult rv = GetFilePathViaSpecialDirectory(NS_OS_TEMP_DIR,
                                               "gtest.emf",
                                               emfPath);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  int pageWidth = 0;
  int pageHeight = 0;
  bool result = PDFHelper->DrawPageToFile(emfPath.get(), 0,
                                          pageWidth, pageHeight);
  ASSERT_FALSE(result);

  pageWidth = 100;
  pageHeight = -1;
  result = PDFHelper->DrawPageToFile(emfPath.get(), 0,
                                     pageWidth, pageHeight);
  ASSERT_FALSE(result);

  PDFHelper->CloseDocument();
}

} // namespace widget
} // namespace mozilla