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
#include "nsIFileChannel.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"

// we need windows.h to read out registry information...
#include <windows.h>
#include <shellapi.h>

// forward declarations of a couple of helper methods.
// Takes a bitmap from the windows registry and converts it into 4 byte RGB data.
void ConvertColorBitMap(unsigned char * aBitmapBuffer, PBITMAPINFO pBitMapInfo, nsCString& iconBuffer);
void ConvertMaskBitMap(unsigned char * aBitMaskBuffer, PBITMAPINFOHEADER pBitMapInfo, nsCString& iconBuffer);
PRUint32 CalcWordAlignedRowSpan(PRUint32  aWidth, PRUint32 aBitCount);

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

  nsMemory::Free(temporaryRowHolder);
}

nsresult nsIconChannel::ExtractIconInfoFromUrl(nsIFile ** aLocalFile, PRUint32 * aDesiredImageSize, char ** aContentType, char ** aFileExtension)
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

void ConvertColorBitMap(unsigned char * buffer, PBITMAPINFO pBitMapInfo, nsCString& iconBuffer)
{
  // windows invers the row order in their bitmaps. So we need to invert the rows back into a top-down order.

  PRUint32 bytesPerPixel = pBitMapInfo->bmiHeader.biBitCount / 8;
  PRUint32 unalignedBytesPerRow = pBitMapInfo->bmiHeader.biWidth * 3;
  InvertRows(buffer, pBitMapInfo->bmiHeader.biSizeImage, pBitMapInfo->bmiHeader.biWidth * bytesPerPixel);
  
  PRUint32 alignedBytesPerRow = CalcWordAlignedRowSpan(pBitMapInfo->bmiHeader.biWidth, 24);
  PRInt32 numBytesPaddingPerRow = alignedBytesPerRow - unalignedBytesPerRow;
  PRUint32 pos = 0;
  if (numBytesPaddingPerRow < 0)  // this should never happen.....
    numBytesPaddingPerRow = 0;

  PRUint32 index = 0;
  // if each pixel uses 16 bits to describe the color then each R, G, and B value uses 5 bites. Use some fancy
  // bit operations to blow up the 16 bit case into 1 byte per component color. Actually windows
  // is using a 5:6:5 scheme. so the green component gets 6 bits....
  if (pBitMapInfo->bmiHeader.biBitCount == 16)
  {
    PRUint8 redValue, greenValue, blueValue;
    while (index < pBitMapInfo->bmiHeader.biSizeImage)
    {                            
      DWORD dst=(DWORD) buffer[index];
      PRUint16 num = 0;
      num = (PRUint8) buffer[index+1];
      num <<= 8;
      num |= (PRUint8) buffer[index];
      // be sure to ignore the top bit
      //num &= 0x7FFF; // only want the low 15 bits.....not the 16th...

      // use num as an offset into the color table....get the RGBQuad entry and read out
      // the RGB values.
      //RGBQUAD rgbValues = pBitMapInfo->bmiColors[num];

      //redValue = ( (num & 0xf800) >> 11);
      //greenValue = ( (num & 0x07E0) >> 5);
      //blueValue = ( num & 0x001F);

      redValue = ((PRUint32) (((float)(num & 0xf800) / 0xf800) * 0xFF0000) & 0xFF0000)>> 16;
      greenValue =  ((PRUint32)(((float)(num & 0x07E0) / 0x07E0) * 0x00FF00) & 0x00FF00)>> 8;
      blueValue =  ((PRUint32)(((float)(num & 0x001F) / 0x001F) * 0x0000FF) & 0x0000FF);

      // now we have the right RGB values...
      iconBuffer.Append((char) blueValue);
      iconBuffer.Append((char) greenValue);
      iconBuffer.Append((char) redValue);
      pos += 3;
      if (pos == unalignedBytesPerRow && numBytesPaddingPerRow) // if we have reached the end of a current row, add padding to force dword alignment
      {
        pos = 0;
        for (PRUint32 i = 0; i < numBytesPaddingPerRow; i++)
        {
          iconBuffer.Append((char) 0);
        }
      }
      index += bytesPerPixel;
    }
  }
  else // otherwise we must be using 32 bits per pixel so each component value is getting one byte...
  {
    while (index < pBitMapInfo->bmiHeader.biSizeImage)
    {
      iconBuffer.Append((char) buffer[index]);
      iconBuffer.Append((char) buffer[index+1]);
      iconBuffer.Append((char) buffer[index+2]);
      pos += 3;
      if (pos == unalignedBytesPerRow && numBytesPaddingPerRow) // if we have reached the end of a current row, add padding to force dword alignment
      {
        pos = 0;
        for (PRUint32 i = 0; i < numBytesPaddingPerRow; i++)
        {
          iconBuffer.Append((char) 0);
        }
      }
      index += bytesPerPixel;
    }
  }
}

PRUint32 CalcWordAlignedRowSpan(PRUint32  aWidth, PRUint32 aBitCount)
{
  PRUint32 spanBytes;

  spanBytes = (aWidth * aBitCount) >> 5;

  if (((PRUint32) aWidth * aBitCount) & 0x1F) 
    spanBytes++;

  spanBytes <<= 2;

  return spanBytes;
}

void ConvertMaskBitMap(unsigned char * aBitMaskBuffer, PBITMAPINFOHEADER pBitMapHeaderInfo, nsCString& iconBuffer)
{
  InvertRows(aBitMaskBuffer, pBitMapHeaderInfo->biSizeImage, 4);
  PRUint32 index = 0;
  // for some reason the bit mask on windows are flipped from the values we really want for transparency. 
  // So complement each byte in the bit mask.
  while (index < pBitMapHeaderInfo->biSizeImage)
  {
    aBitMaskBuffer[index]^=255;
    index += 1;
  }
  iconBuffer.Append((char *) aBitMaskBuffer, pBitMapHeaderInfo->biSizeImage);
}

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsXPIDLCString contentType;
  nsXPIDLCString filePath;
  nsCOMPtr<nsIFile> localFile; // file we want an icon for
  PRUint32 desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(localFile), &desiredImageSize, getter_Copies(contentType), getter_Copies(filePath));
  NS_ENSURE_SUCCESS(rv, rv);

  // if the file exists, we are going to use it's real attributes...otherwise we only want to use it for it's extension...
  SHFILEINFO      sfi;
  UINT infoFlags = SHGFI_ICON;
  
  PRBool fileExists = PR_FALSE;
 
  if (localFile)
  {
    localFile->GetPath(getter_Copies(filePath));
    localFile->Exists(&fileExists);
  }

  if (!fileExists)
   infoFlags |= SHGFI_USEFILEATTRIBUTES;

  if (desiredImageSize > 16)
    infoFlags |= SHGFI_LARGEICON;
  else
    infoFlags |= SHGFI_SMALLICON;

  if ( (!filePath.get()) && (contentType.get() && *contentType.get()) ) // if we have a content type without a file extension...then use it!
  {
    nsCOMPtr<nsIMIMEService> mimeService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    nsCOMPtr<nsIMIMEInfo> mimeObject;
    NS_ENSURE_SUCCESS(rv, rv);
     
    mimeService->GetFromMIMEType(contentType.get(), getter_AddRefs(mimeObject));
    if (mimeObject)
    {
      nsXPIDLCString fileExt;
      mimeObject->FirstExtension(getter_Copies(fileExt));
      // we need to insert a '.' b4 the extension...
      nsCAutoString formattedFileExt;
      formattedFileExt = ".";
      formattedFileExt.Append(fileExt.get());
      filePath.Adopt(ToNewCString(formattedFileExt));
    }
  }

  // (1) get an hIcon for the file
  LONG result= SHGetFileInfo(filePath.get(), FILE_ATTRIBUTE_ARCHIVE, &sfi, sizeof(sfi), infoFlags);
  if (result > 0 && sfi.hIcon)
  {
    // we got a handle to an icon. Now we want to get a bitmap for the icon using GetIconInfo....
    ICONINFO pIconInfo;
    result = GetIconInfo(sfi.hIcon, &pIconInfo);
    if (result > 0)
    {
      nsCString iconBuffer;

     HANDLE h_bmp_info = GlobalAlloc (GHND, sizeof (BITMAPINFO) + (sizeof (RGBQUAD) * 256));
     BITMAPINFO* pBitMapInfo = (BITMAPINFO *) GlobalLock (h_bmp_info);
     memset (pBitMapInfo, NULL, sizeof (BITMAPINFO) + (sizeof (RGBQUAD) * 255));
     pBitMapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
     pBitMapInfo->bmiHeader.biPlanes = 1;
     pBitMapInfo->bmiHeader.biBitCount = 0;
     pBitMapInfo->bmiHeader.biCompression = BI_RGB;

     HDC pDC = CreateCompatibleDC(NULL); // get a device context for the screen.
     LONG result = GetDIBits(pDC, pIconInfo.hbmColor, 0, 0, NULL, pBitMapInfo, DIB_RGB_COLORS);
     if (result > 0 && pBitMapInfo->bmiHeader.biSizeImage > 0)
     {
        // allocate a buffer to hold the bit map....this should be a buffer that's biSizeImage...
        unsigned char * buffer = (PRUint8 *) nsMemory::Alloc(pBitMapInfo->bmiHeader.biSizeImage);
        result = GetDIBits(pDC, pIconInfo.hbmColor, 0, pBitMapInfo->bmiHeader.biHeight, (void *) buffer, pBitMapInfo, DIB_RGB_COLORS);
        if (result > 0)
        { 
          // The first 2 bytes into our output buffer needs to be the width and the height (in pixels) of the icon
          // as specified by our data format.
          iconBuffer.Assign((char) pBitMapInfo->bmiHeader.biWidth);
          iconBuffer.Append((char) pBitMapInfo->bmiHeader.biHeight);

          ConvertColorBitMap(buffer, pBitMapInfo, iconBuffer);
          
          // now we need to tack on the alpha data...which is hbmMask
          memset (pBitMapInfo, NULL, sizeof (BITMAPINFO) + (sizeof (RGBQUAD) * 255));
          pBitMapInfo->bmiHeader.biBitCount = 0;
          pBitMapInfo->bmiHeader.biPlanes = 1;
          pBitMapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
          result = GetDIBits(pDC, pIconInfo.hbmMask, 0, 0, NULL, pBitMapInfo, DIB_RGB_COLORS);
          if (result > 0 && pBitMapInfo->bmiHeader.biSizeImage > 0)
          {
            // allocate a buffer to hold the bit map....this should be a buffer that's biSizeImage...
            unsigned char * maskBuffer = (PRUint8 *) nsMemory::Alloc(pBitMapInfo->bmiHeader.biSizeImage);
            result = GetDIBits(pDC, pIconInfo.hbmMask, 0, pBitMapInfo->bmiHeader.biHeight, (void *) maskBuffer, pBitMapInfo, DIB_RGB_COLORS);
            if (result > 0)          
               ConvertMaskBitMap(maskBuffer, &(pBitMapInfo->bmiHeader), iconBuffer);           
            nsMemory::Free(maskBuffer);

            // turn our nsString into a stream looking object...
            aListener->OnStartRequest(this, ctxt);

            // turn our string into a stream...and make the appropriate calls on our consumer
            nsCOMPtr<nsISupports> streamSupports;
            NS_NewByteInputStream(getter_AddRefs(streamSupports), iconBuffer.get(), iconBuffer.Length());
            nsCOMPtr<nsIInputStream> inputStr (do_QueryInterface(streamSupports));
            aListener->OnDataAvailable(this, ctxt, inputStr, 0, iconBuffer.Length());
            aListener->OnStopRequest(this, ctxt, NS_OK);
          } // if we have a mask buffer to apply
        } // if we got the color bit map

        nsMemory::Free(buffer);
      } // if we got color info

      GlobalUnlock(h_bmp_info);
      GlobalFree(h_bmp_info);
      DeleteDC(pDC);
    } // if we got icon info
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

