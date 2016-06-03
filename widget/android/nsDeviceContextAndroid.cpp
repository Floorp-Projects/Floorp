/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextAndroid.h"

#include "mozilla/gfx/PrintTargetThebes.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "gfxPDFSurface.h"
#include "nsIPrintSettings.h"
#include "nsDirectoryServiceDefs.h"

using namespace mozilla;
using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS(nsDeviceContextSpecAndroid, nsIDeviceContextSpec)

already_AddRefed<PrintTarget>
nsDeviceContextSpecAndroid::MakePrintTarget()
{
  nsresult rv =
    NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mTempFile));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsAutoCString filename("tmp-printing.pdf");
  mTempFile->AppendNative(filename);
  rv = mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0660);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIFileOutputStream> stream = do_CreateInstance("@mozilla.org/network/file-output-stream;1");
  rv = stream->Init(mTempFile, -1, -1, 0);
  NS_ENSURE_SUCCESS(rv, nullptr);

  // XXX: what should we do here for size? screen size?
  gfxSize size(480, 800);

  RefPtr<gfxASurface> surface = new gfxPDFSurface(stream, size);

  return PrintTargetThebes::CreateOrNull(surface);
}

NS_IMETHODIMP
nsDeviceContextSpecAndroid::Init(nsIWidget* aWidget,
                             nsIPrintSettings* aPS,
                             bool aIsPrintPreview)
{
  mPrintSettings = aPS;
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecAndroid::BeginDocument(const nsAString& aTitle,
                                          const nsAString& aPrintToFileName,
                                          int32_t aStartPage,
                                          int32_t aEndPage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecAndroid::EndDocument()
{
  nsXPIDLString targetPath;
  nsCOMPtr<nsIFile> destFile;
  mPrintSettings->GetToFileName(getter_Copies(targetPath));
  
  nsresult rv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(targetPath),
                                      false, getter_AddRefs(destFile));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsAutoString destLeafName;
  rv = destFile->GetLeafName(destLeafName);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIFile> destDir;
  rv = destFile->GetParent(getter_AddRefs(destDir));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mTempFile->MoveTo(destDir, destLeafName);
  NS_ENSURE_SUCCESS(rv, rv);
  
  destFile->SetPermissions(0666);
  return NS_OK;
}
