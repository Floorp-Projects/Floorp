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
 */


#include "nsIconChannel.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsIStringStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

// we need windows.h to read out registry information...
#include <windows.h>
#include <shellapi.h>

// nsIconChannel methods
nsIconChannel::nsIconChannel()
{
  NS_INIT_REFCNT();
  mStatus = NS_OK;
}

nsIconChannel::~nsIconChannel() 
{}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsIconChannel, 
                              nsIChannel, 
                              nsIRequest)

nsresult nsIconChannel::Init(nsIURI* uri)
{
  nsresult rv;
  NS_ASSERTION(uri, "no uri");
  mUrl = uri;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP nsIconChannel::GetName(PRUnichar* *result)
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

NS_IMETHODIMP nsIconChannel::SetURI(nsIURI* aURI)
{
  mUrl = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream **_retval)
{
  return NS_ERROR_FAILURE;
}

void InvertRows(unsigned char * aInitialBuffer, PRUint32 sizeOfBuffer, PRUint32 numBytesPerRow)
{
  PRUint32 numRows = sizeOfBuffer / numBytesPerRow;
  void * temporaryRowHolder = (void *) nsMemory::Alloc(numBytesPerRow);

  PRUint32 currentRow = 0;
  PRUint32 lastRow = (numRows - 1) * numBytesPerRow;
  while (currentRow < lastRow)
  {
    // store the current row into a temporary buffer
    nsCRT::memcpy(temporaryRowHolder, (void *) &aInitialBuffer[currentRow], numBytesPerRow);
    nsCRT::memcpy((void *) &aInitialBuffer[currentRow], (void *)&aInitialBuffer[lastRow], numBytesPerRow);
    nsCRT::memcpy((void *) &aInitialBuffer[lastRow], temporaryRowHolder, numBytesPerRow);
    lastRow -= numBytesPerRow;
    currentRow += numBytesPerRow;
  }
}

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  // get the file name from the url
  nsXPIDLCString fileName; // will contain a dummy file we'll use to figure out the type of icon desired.
  nsXPIDLCString filePath; // will contain an optional parameter for small vs. large icon. default is small
  mUrl->GetHost(getter_Copies(fileName));
  nsCOMPtr<nsIURL> url (do_QueryInterface(mUrl));
  if (url)
    url->GetFileBaseName(getter_Copies(filePath));

  
  // 1) get a hIcon for the file.
  SHFILEINFO      sfi;
  UINT infoFlags = SHGFI_USEFILEATTRIBUTES | SHGFI_ICON;
  if (filePath && !nsCRT::strcmp(filePath, "large"))
    infoFlags |= SHGFI_LARGEICON;
  else // default to small
    infoFlags |= SHGFI_SMALLICON;

  LONG result= SHGetFileInfo(fileName, FILE_ATTRIBUTE_ARCHIVE, &sfi, sizeof(sfi), infoFlags);
  if (result > 0 && sfi.hIcon)
  {
    // we got a handle to an icon. Now we want to get a bitmap for the icon using GetIconInfo....
    ICONINFO pIconInfo;
    result = GetIconInfo(sfi.hIcon, &pIconInfo);
    if (result > 0)
    {
      // now we have the bit map we need to get info about the bitmap
      BITMAPINFO pBitMapInfo;
      BITMAPINFOHEADER pBitMapInfoHeader;
      pBitMapInfo.bmiHeader.biBitCount = 0;
      pBitMapInfo.bmiHeader.biSize = sizeof(pBitMapInfoHeader);

      HDC pDC = CreateCompatibleDC(NULL); // get a device context for the screen.
      result = GetDIBits(pDC, pIconInfo.hbmColor, 0, 0, NULL, &pBitMapInfo, DIB_RGB_COLORS);
      if (result > 0 && pBitMapInfo.bmiHeader.biSizeImage > 0)
      {
        // allocate a buffer to hold the bit map....this should be a buffer that's biSizeImage...
        unsigned char * buffer = (PRUint8 *) nsMemory::Alloc(pBitMapInfo.bmiHeader.biSizeImage);
        result = GetDIBits(pDC, pIconInfo.hbmColor, 0, pBitMapInfo.bmiHeader.biHeight, (void *) buffer, &pBitMapInfo, DIB_RGB_COLORS);
        if (result > 0)
        {

          InvertRows(buffer, pBitMapInfo.bmiHeader.biSizeImage, pBitMapInfo.bmiHeader.biWidth * (pBitMapInfo.bmiHeader.biBitCount / 8));
          // Convert our little icon buffer which is padded to 4 bytes per pixel into a nice 3 byte per pixel
          // description.
          nsCString iconBuffer;
          iconBuffer.Assign((char) pBitMapInfo.bmiHeader.biWidth);
          iconBuffer.Append((char) pBitMapInfo.bmiHeader.biHeight);
          PRInt32 index = 0;
          while (index <pBitMapInfo.bmiHeader.biSizeImage)
          {
            iconBuffer.Append((char) buffer[index]);
            iconBuffer.Append((char) buffer[index+1]);
            iconBuffer.Append((char) buffer[index+2]);
            index += 4;
          }

          // now we need to tack on the alpha data...which is hbmMask
          pBitMapInfo.bmiHeader.biBitCount = 0;
          pBitMapInfo.bmiHeader.biSize = sizeof(pBitMapInfoHeader);
          result = GetDIBits(pDC, pIconInfo.hbmMask, 0, 0, NULL, &pBitMapInfo, DIB_RGB_COLORS);
          if (result > 0 && pBitMapInfo.bmiHeader.biSizeImage > 0)
          {
            // allocate a buffer to hold the bit map....this should be a buffer that's biSizeImage...
            unsigned char * maskBuffer = (PRUint8 *) nsMemory::Alloc(pBitMapInfo.bmiHeader.biSizeImage);
            result = GetDIBits(pDC, pIconInfo.hbmMask, 0, pBitMapInfo.bmiHeader.biHeight, (void *) maskBuffer, &pBitMapInfo, DIB_RGB_COLORS);
            if (result > 0)
            {
              InvertRows(maskBuffer, pBitMapInfo.bmiHeader.biSizeImage, 4);
              index = 0;
              // for some reason the bit mask on windows are flipped from the values we really want for transparency. 
              // So complement each byte in the bit mask.
              while (index < pBitMapInfo.bmiHeader.biSizeImage)
              {
                maskBuffer[index]^=255;
                index += 1;
              }
              iconBuffer.Append((char *) maskBuffer, pBitMapInfo.bmiHeader.biSizeImage);
            }

            nsMemory::Free(maskBuffer);
          } // if we have a mask buffer to apply

          // turn our nsString into a stream looking object...
          aListener->OnStartRequest(this, ctxt);

          // turn our string into a stream...
          nsCOMPtr<nsISupports> streamSupports;
          NS_NewByteInputStream(getter_AddRefs(streamSupports), iconBuffer.get(), iconBuffer.Length());

          nsCOMPtr<nsIInputStream> inputStr (do_QueryInterface(streamSupports));
          aListener->OnDataAvailable(this, ctxt, inputStr, 0, iconBuffer.Length());
          aListener->OnStopRequest(this, ctxt, NS_OK, nsnull);

        } // if we got valid bits for the main bitmap mask
        
        nsMemory::Free(buffer);

      }

      DeleteDC(pDC);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
  *aLoadAttributes = mLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
  mLoadAttributes = aLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetContentType(char* *aContentType) 
{
  if (!aContentType) return NS_ERROR_NULL_POINTER;

  *aContentType = nsCRT::strdup("image/icon");
  if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const char *aContentType)
{
  //It doesn't make sense to set the content-type on this type
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

