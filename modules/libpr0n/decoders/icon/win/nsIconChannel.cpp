/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Brian Ryner.
 * Portions created by Brian Ryner are Copyright (C) 2000 Brian Ryner.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *   Scott MacGregor <mscott@netscape.com>
 *   Neil Rashbrook <neil@parkwaycc.co.uk>
 */


#include "nsIconChannel.h"
#include "nsIIconURI.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsIStringStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"

// we need windows.h to read out registry information...
#include <windows.h>
#include <shellapi.h>

struct ICONFILEHEADER {
  PRUint16 ifhReserved;
  PRUint16 ifhType;
  PRUint16 ifhCount;
};

struct ICONENTRY {
  PRInt8 ieWidth;
  PRInt8 ieHeight;
  PRUint8 ieColors;
  PRUint8 ieReserved;
  PRUint16 iePlanes;
  PRUint16 ieBitCount;
  PRUint32 ieSizeImage;
  PRUint32 ieFileOffset;
};

// nsIconChannel methods
nsIconChannel::nsIconChannel()
{
  mStatus = NS_OK;
}

nsIconChannel::~nsIconChannel() 
{}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsIconChannel, 
                              nsIChannel, 
                              nsIRequest)

nsresult nsIconChannel::Init(nsIURI* uri)
{
  NS_ASSERTION(uri, "no uri");
  mUrl = uri;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP nsIconChannel::GetName(nsACString &result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::IsPending(PRBool *result)
{
  NS_NOTREACHED("nsIconChannel::IsPending");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::GetStatus(nsresult *status)
{
  *status = mStatus;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::Cancel(nsresult status)
{
  NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
  nsresult rv = NS_ERROR_FAILURE;

  mStatus = status;
  return rv;
}

NS_IMETHODIMP nsIconChannel::Suspend(void)
{
  NS_NOTREACHED("nsIconChannel::Suspend");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::Resume(void)
{
  NS_NOTREACHED("nsIconChannel::Resume");
  return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP nsIconChannel::GetOriginalURI(nsIURI* *aURI)
{
  *aURI = mOriginalURI ? mOriginalURI : mUrl;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetOriginalURI(nsIURI* aURI)
{
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetURI(nsIURI* *aURI)
{
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream **_retval)
{
  return NS_ERROR_FAILURE;
}

nsresult nsIconChannel::ExtractIconInfoFromUrl(nsIFile ** aLocalFile, PRUint32 * aDesiredImageSize, nsACString &aContentType, nsACString &aFileExtension)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMozIconURI> iconURI (do_QueryInterface(mUrl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  iconURI->GetImageSize(aDesiredImageSize);
  iconURI->GetContentType(aContentType);
  iconURI->GetFileExtension(aFileExtension);

  nsCOMPtr<nsIURI> fileURI;
  rv = iconURI->GetIconFile(getter_AddRefs(fileURI));
  if (NS_FAILED(rv) || !fileURI) return NS_OK;

  nsCOMPtr<nsIFileURL>    fileURL = do_QueryInterface(fileURI, &rv);
  if (NS_FAILED(rv) || !fileURL) return NS_OK;

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv) || !file) return NS_OK;
  
  *aLocalFile = file;
  NS_IF_ADDREF(*aLocalFile);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsXPIDLCString contentType;
  nsCAutoString filePath;
  nsCOMPtr<nsIFile> localFile; // file we want an icon for
  PRUint32 desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(localFile), &desiredImageSize, contentType, filePath);
  NS_ENSURE_SUCCESS(rv, rv);

  // if the file exists, we are going to use it's real attributes...otherwise we only want to use it for it's extension...
  SHFILEINFO      sfi;
  UINT infoFlags = SHGFI_ICON;
  
  PRBool fileExists = PR_FALSE;
 
  if (localFile)
  {
    localFile->GetNativePath(filePath);
    localFile->Exists(&fileExists);
  }

  if (!fileExists)
   infoFlags |= SHGFI_USEFILEATTRIBUTES;

  if (desiredImageSize > 16)
    infoFlags |= SHGFI_SHELLICONSIZE;
  else
    infoFlags |= SHGFI_SMALLICON;

  if ( (filePath.IsEmpty()) && (contentType.get() && *contentType.get()) ) // if we have a content type without a file extension...then use it!
  {
    nsCOMPtr<nsIMIMEService> mimeService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    nsCOMPtr<nsIMIMEInfo> mimeObject;
    NS_ENSURE_SUCCESS(rv, rv);
     
    mimeService->GetFromTypeAndExtension(contentType.get(), nsnull, getter_AddRefs(mimeObject));
    if (mimeObject)
    {
      nsXPIDLCString fileExt;
      mimeObject->GetPrimaryExtension(getter_Copies(fileExt));
      // we need to insert a '.' b4 the extension...
      filePath = NS_LITERAL_CSTRING(".") + fileExt;
    }
  }

  // (1) get an hIcon for the file
  if (SHGetFileInfo(filePath.get(), FILE_ATTRIBUTE_ARCHIVE, &sfi, sizeof(sfi), infoFlags) && sfi.hIcon)
  {
    // we got a handle to an icon. Now we want to get a bitmap for the icon using GetIconInfo....
    ICONINFO iconInfo;
    if (GetIconInfo(sfi.hIcon, &iconInfo))
    {
      // we got the bitmaps, first find out their size
      HDC hDC = CreateCompatibleDC(NULL); // get a device context for the screen.
      BITMAPINFO maskInfo = {{sizeof(BITMAPINFOHEADER)}};
      if (GetDIBits(hDC, iconInfo.hbmMask, 0, 0, NULL, &maskInfo, DIB_RGB_COLORS) &&
          maskInfo.bmiHeader.biSizeImage > 0) {
        PRUint32 colorSize = (((maskInfo.bmiHeader.biWidth + 1) * 3) & ~3) * maskInfo.bmiHeader.biHeight;
        PRUint32 iconSize = sizeof(ICONFILEHEADER) + sizeof(ICONENTRY) + sizeof(BITMAPINFOHEADER) + colorSize + maskInfo.bmiHeader.biSizeImage;
        char *buffer = new char[iconSize];
        if (!buffer)
          rv = NS_ERROR_OUT_OF_MEMORY;
        else {
          // the data starts with an icon file header
          ICONFILEHEADER *iconHeader = (ICONFILEHEADER *)buffer;
          iconHeader->ifhReserved = 0;
          iconHeader->ifhType = 1;
          iconHeader->ifhCount = 1;
          // followed by the single icon entry
          ICONENTRY *iconEntry = (ICONENTRY *)(buffer + sizeof(ICONFILEHEADER));
          iconEntry->ieWidth = maskInfo.bmiHeader.biWidth;
          iconEntry->ieHeight = maskInfo.bmiHeader.biHeight;
          iconEntry->ieColors = 0;
          iconEntry->ieReserved = 0;
          iconEntry->iePlanes = 1;
          iconEntry->ieBitCount = 24;
          iconEntry->ieSizeImage = sizeof(BITMAPINFOHEADER) + colorSize + maskInfo.bmiHeader.biSizeImage;
          iconEntry->ieFileOffset = sizeof(ICONFILEHEADER) + sizeof(ICONENTRY);
          // followed by the bitmap info header and the bits
          LPBITMAPINFO lpBitmapInfo = (LPBITMAPINFO)(buffer + sizeof(ICONFILEHEADER) + sizeof(ICONENTRY));
          memcpy(lpBitmapInfo, &maskInfo.bmiHeader, sizeof(BITMAPINFOHEADER));
          if (GetDIBits(hDC, iconInfo.hbmMask, 0, maskInfo.bmiHeader.biHeight, buffer + sizeof(ICONFILEHEADER) + sizeof(ICONENTRY) + sizeof(BITMAPINFOHEADER) + colorSize, lpBitmapInfo, DIB_RGB_COLORS)) {
            lpBitmapInfo->bmiHeader.biBitCount = 24;
            lpBitmapInfo->bmiHeader.biSizeImage = colorSize;
            lpBitmapInfo->bmiHeader.biClrUsed = 0;
            lpBitmapInfo->bmiHeader.biClrImportant = 0;
            if (GetDIBits(hDC, iconInfo.hbmColor, 0, maskInfo.bmiHeader.biHeight, buffer + sizeof(ICONFILEHEADER) + sizeof(ICONENTRY) + sizeof(BITMAPINFOHEADER), lpBitmapInfo, DIB_RGB_COLORS)) {
              // doubling the height because icons have two bitmaps
              lpBitmapInfo->bmiHeader.biHeight *= 2;
              aListener->OnStartRequest(this, ctxt);

              // turn our buffer into a stream...and make the appropriate calls on our consumer
              nsCOMPtr<nsIInputStream> inputStr;
              rv = NS_NewByteInputStream(getter_AddRefs(inputStr), buffer, iconSize);
              if (NS_SUCCEEDED(rv))
                aListener->OnDataAvailable(this, ctxt, inputStr, 0, iconSize);
              aListener->OnStopRequest(this, ctxt, rv);
            } // if we got bitmap bits
          } // if we got mask bits
          delete [] buffer;
        } // if we allocated the buffer
      } // if we got mask size

      DeleteDC(hDC);
      DeleteObject(iconInfo.hbmColor);
      DeleteObject(iconInfo.hbmMask);
    } // if we got icon info
    DestroyIcon(sfi.hIcon);
  } // if we got sfi

  return rv;
}

NS_IMETHODIMP nsIconChannel::GetLoadFlags(PRUint32 *aLoadAttributes)
{
  *aLoadAttributes = mLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetLoadFlags(PRUint32 aLoadAttributes)
{
  mLoadAttributes = aLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetContentType(nsACString &aContentType) 
{
  aContentType = NS_LITERAL_CSTRING("image/x-icon");
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const nsACString &aContentType)
{
  // It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsIconChannel::GetContentCharset(nsACString &aContentCharset) 
{
  aContentCharset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentCharset(const nsACString &aContentCharset)
{
  // It doesn't make sense to set the content-charset on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsIconChannel::GetContentLength(PRInt32 *aContentLength)
{
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetContentLength(PRInt32 aContentLength)
{
  NS_NOTREACHED("nsIconChannel::SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetOwner(nsISupports* *aOwner)
{
  *aOwner = mOwner.get();
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetOwner(nsISupports* aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  *aSecurityInfo = nsnull;
  return NS_OK;
}

