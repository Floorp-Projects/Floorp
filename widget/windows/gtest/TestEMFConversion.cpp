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
#include "prlink.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsString.h"

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

// Convert PrinterTestPage.pdf to an EMF file
TEST(TestEMFConversion, SaveEMFtoFile)
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

  // Check the file size of EMF and make sure it is greater than zero
  RefPtr<nsLocalFile> savedFile = new nsLocalFile();
  savedFile->InitWithPath(emfPath);
  RefPtr<nsFileInputStream> inputStream = new nsFileInputStream();
  rv = inputStream->Init(savedFile,
                         /* ioFlags */ PR_RDONLY,
                         /* perm */ -1,
                         /* behaviorFlags */ 0);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  int64_t size = 0;
  inputStream->GetSize(&size);
  ASSERT_GT(size, 0);
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