/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextAndroid.h"

#include "mozilla/gfx/PrintTargetPDF.h"
#include "mozilla/RefPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIPrintSettings.h"
#include "nsIFileStreams.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAnonymousTemporaryFile.h"

using namespace mozilla;
using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS(nsDeviceContextSpecAndroid, nsIDeviceContextSpec)

nsDeviceContextSpecAndroid::~nsDeviceContextSpecAndroid() {
  if (mTempFile) {
    mTempFile->Remove(false);
  }
}

already_AddRefed<PrintTarget> nsDeviceContextSpecAndroid::MakePrintTarget() {
  double width, height;
  mPrintSettings->GetEffectiveSheetSize(&width, &height);

  // convert twips to points
  width /= TWIPS_PER_POINT_FLOAT;
  height /= TWIPS_PER_POINT_FLOAT;

  auto stream = [&]() -> nsCOMPtr<nsIOutputStream> {
    if (mPrintSettings->GetOutputDestination() ==
        nsIPrintSettings::kOutputDestinationStream) {
      nsCOMPtr<nsIOutputStream> out;
      mPrintSettings->GetOutputStream(getter_AddRefs(out));
      return out;
    }
    if (NS_FAILED(
            NS_OpenAnonymousTemporaryNsIFile(getter_AddRefs(mTempFile)))) {
      return nullptr;
    }
    // Print to printer not supported...
    nsCOMPtr<nsIFileOutputStream> s =
        do_CreateInstance("@mozilla.org/network/file-output-stream;1");
    if (NS_FAILED(s->Init(mTempFile, -1, -1, 0))) {
      return nullptr;
    }
    return s;
  }();

  return PrintTargetPDF::CreateOrNull(stream, IntSize::Ceil(width, height));
}

NS_IMETHODIMP
nsDeviceContextSpecAndroid::Init(nsIPrintSettings* aPS, bool aIsPrintPreview) {
  mPrintSettings = aPS;
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecAndroid::BeginDocument(const nsAString& aTitle,
                                          const nsAString& aPrintToFileName,
                                          int32_t aStartPage,
                                          int32_t aEndPage) {
  return NS_OK;
}

RefPtr<PrintEndDocumentPromise> nsDeviceContextSpecAndroid::EndDocument() {
  return nsIDeviceContextSpec::EndDocumentPromiseFromResult(DoEndDocument(),
                                                            __func__);
}

NS_IMETHODIMP
nsDeviceContextSpecAndroid::DoEndDocument() {
  if (mPrintSettings->GetOutputDestination() ==
          nsIPrintSettings::kOutputDestinationFile &&
      mTempFile) {
    nsAutoString targetPath;
    mPrintSettings->GetToFileName(targetPath);
    nsCOMPtr<nsIFile> destFile;
    MOZ_TRY(NS_NewLocalFile(targetPath, false, getter_AddRefs(destFile)));
    nsAutoString destLeafName;
    MOZ_TRY(destFile->GetLeafName(destLeafName));

    nsCOMPtr<nsIFile> destDir;
    MOZ_TRY(destFile->GetParent(getter_AddRefs(destDir)));

    MOZ_TRY(mTempFile->MoveTo(destDir, destLeafName));
    destFile->SetPermissions(0666);

    mTempFile = nullptr;
  }
  return NS_OK;
}
