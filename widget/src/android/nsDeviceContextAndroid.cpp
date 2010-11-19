/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsDeviceContextAndroid.h"
#include "nsString.h"
#include "nsILocalFile.h"
#include "nsIFileStreams.h"
#include "nsAutoPtr.h"
#include "gfxPDFSurface.h"
#include "nsIPrintSettings.h"
#include "nsDirectoryServiceDefs.h"

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecAndroid, nsIDeviceContextSpec)

NS_IMETHODIMP
nsDeviceContextSpecAndroid::GetSurfaceForPrinter(gfxASurface** aSurface)
{
  nsCAutoString tmpDir(getenv("TMPDIR"));
  nsresult rv = 
    NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mTempFile));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCAutoString filename("tmp-printing.pdf");  
  mTempFile->AppendNative(filename);
  rv = mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0660);
  NS_ENSURE_SUCCESS(rv, rv);  
  
  nsCOMPtr<nsIFileOutputStream> stream = do_CreateInstance("@mozilla.org/network/file-output-stream;1");
  rv = stream->Init(mTempFile, -1, -1, 0);
  NS_ENSURE_SUCCESS(rv, rv);  

  nsRefPtr<gfxASurface> surface;

  // XXX: what should we do hear for size? screen size?
  gfxSize surfaceSize(480, 800);

  surface = new gfxPDFSurface(stream, surfaceSize);
  
  
  NS_ABORT_IF_FALSE(surface, "valid address expected");
  surface.swap(*aSurface);
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecAndroid::Init(nsIWidget* aWidget,
                             nsIPrintSettings* aPS,
                             PRBool aIsPrintPreview)
{
  mPrintSettings = aPS;
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecAndroid::BeginDocument(PRUnichar* aTitle,
                                      PRUnichar* aPrintToFileName,
                                      PRInt32 aStartPage,
                                      PRInt32 aEndPage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecAndroid::EndDocument()
{
  nsXPIDLString targetPath;
  nsCOMPtr<nsILocalFile> destFile;
  mPrintSettings->GetToFileName(getter_Copies(targetPath));
  
  nsresult rv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(targetPath),
                                      PR_FALSE, getter_AddRefs(destFile));
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

NS_IMETHODIMP
nsDeviceContextSpecAndroid::GetPath (const char** aPath)
{
  return NS_OK;
}
