/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Brian Ryner.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Neil Rashbrook <neil@parkwaycc.co.uk>
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#define INCL_WINWORKPLACE
#define INCL_WIN
#define INCL_PM
#define INCL_DEV
#define INCL_GPI
#include <os2.h>
#include <stdlib.h> // for getenv

// From Windows
#define SHGFI_ICON               0x0001
#define SHGFI_USEFILEATTRIBUTES  0x0002
#define SHGFI_LARGEICON          0x0004
#define SHGFI_SMALLICON          0x0008

// Due to byte swap the second is first of the pair
#define FIRSTPEL(x) (0xF & (x >> 4))
#define SECONDPEL(x)  (0xF & x)

// forward declarations of a couple of helper methods.
// Takes a bitmap from the windows registry and converts it into 4 byte RGB data.
void ConvertColorBitMap(PBYTE aBitmapBuffer, PBITMAPINFO2 pBitMapInfo, nsCString& iconBuffer);
void ConvertMaskBitMap(PBYTE aBitMaskBuffer, PBITMAPINFO2 pBitMapInfo, nsCString& iconBuffer);
PRUint32 CalcWordAlignedRowSpan(PRUint32  aWidth, PRUint32 aBitCount);

// nsIconChannel methods
nsIconChannel::nsIconChannel()
{
}

nsIconChannel::~nsIconChannel()
{}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsIconChannel, 
                              nsIChannel,
                              nsIRequest,
                              nsIRequestObserver,
                              nsIStreamListener)

nsresult nsIconChannel::Init(nsIURI* uri)
{
  NS_ASSERTION(uri, "no uri");
  mUrl = uri;

  nsresult rv;
  mPump = do_CreateInstance(NS_INPUTSTREAMPUMP_CONTRACTID, &rv);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP nsIconChannel::GetName(nsACString &result)
{
  return mUrl->GetSpec(result);
}

NS_IMETHODIMP nsIconChannel::IsPending(PRBool *result)
{
  return mPump->IsPending(result);
}

NS_IMETHODIMP nsIconChannel::GetStatus(nsresult *status)
{
  return mPump->GetStatus(status);
}

NS_IMETHODIMP nsIconChannel::Cancel(nsresult status)
{
  return mPump->Cancel(status);
}

NS_IMETHODIMP nsIconChannel::Suspend(void)
{
  return mPump->Suspend();
}

NS_IMETHODIMP nsIconChannel::Resume(void)
{
  return mPump->Resume();
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

NS_IMETHODIMP nsIconChannel::GetLoadFlags(PRUint32 *aLoadAttributes)
{
  return mPump->GetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP nsIconChannel::SetLoadFlags(PRUint32 aLoadAttributes)
{
  return mPump->SetLoadFlags(aLoadAttributes);
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
  return MakeInputStream(_retval, PR_FALSE);
}

void InvertRows(PBYTE aInitialBuffer, PRUint32 sizeOfBuffer, PRUint32 numBytesPerRow)
{
  if (!numBytesPerRow) return;

  PRUint32 numRows = sizeOfBuffer / numBytesPerRow;
  void * temporaryRowHolder = (void *) nsMemory::Alloc(numBytesPerRow);

  PRUint32 currentRow = 0;
  PRUint32 lastRow = (numRows - 1) * numBytesPerRow;
  while (currentRow < lastRow)
  {
    // store the current row into a temporary buffer
    memcpy(temporaryRowHolder, (void *) &aInitialBuffer[currentRow], numBytesPerRow);
    memcpy((void *) &aInitialBuffer[currentRow], (void *)&aInitialBuffer[lastRow], numBytesPerRow);
    memcpy((void *) &aInitialBuffer[lastRow], temporaryRowHolder, numBytesPerRow);
    lastRow -= numBytesPerRow;
    currentRow += numBytesPerRow;
  }

  nsMemory::Free(temporaryRowHolder);
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

INT AddBGR(PRGB2 pColorTableEntry, nsCString& iconBuffer)
{
  iconBuffer.Append((char) pColorTableEntry->bBlue);
  iconBuffer.Append((char) pColorTableEntry->bGreen);
  iconBuffer.Append((char) pColorTableEntry->bRed);
  return 3;
}

void ConvertColorBitMap(PBYTE buffer, PBITMAPINFO2 pBitMapInfo, nsCString& iconBuffer)
{
  // windows inverts the row order in their bitmaps. So we need to invert the rows back into a top-down order.

  PRUint32 bytesPerPixel = pBitMapInfo->cBitCount / 8;
  PRUint32 unalignedBytesPerRowRGB = pBitMapInfo->cx * 3;
  PRInt32  iScanLineSize =  pBitMapInfo->cSize1;  // Stored early
  PRInt32  iIter;

  InvertRows(buffer, pBitMapInfo->cbImage, iScanLineSize);

  PRUint32 alignedBytesPerRowRGB = CalcWordAlignedRowSpan(pBitMapInfo->cx, 24);
  PRUint32 numBytesPaddingPerRowRGB = alignedBytesPerRowRGB - unalignedBytesPerRowRGB;
  PRUint32 pos = 0;
  if (numBytesPaddingPerRowRGB < 0)  // this should never happen.....
    numBytesPaddingPerRowRGB = 0;

  PRGB2 pColorTable = &pBitMapInfo->argbColor[0];  // Note Color tables are only valid for 1, 4, 8 BPP maps

  PRUint32 index = 0;

  // Many OS2 Icons are 16 colors or 4 BPP so we need to map the colors to RGB
  if (pBitMapInfo->cBitCount == 4 ||
      pBitMapInfo->cBitCount == 8  )
  {
    PBYTE pPelPair;
    PBYTE pPel;
    if (pBitMapInfo->cBitCount == 4)
    {
      iIter = (pBitMapInfo->cx + 1) / 2;  // Have 1/2 bytes as pels per row
    }
    else
    {
      iIter = pBitMapInfo->cx;  // Bytes = Pels
    }
    for (ULONG j = 0; j < pBitMapInfo->cy; j++)  //Number of rows
    {
      pPelPair = buffer;
      pPel     = (PBYTE)buffer;
      for(INT i = 0; i < iIter; i++)
      {

        if (pBitMapInfo->cBitCount == 4)
        {
          AddBGR(&pColorTable[FIRSTPEL(*pPelPair)], iconBuffer);
          AddBGR(&pColorTable[SECONDPEL(*pPelPair)], iconBuffer);
          pPelPair++;
        }
        else
        {
          AddBGR(&pColorTable[*pPel], iconBuffer);
          pPel++;
        }
      }
      if (numBytesPaddingPerRowRGB)
      {
        for (PRUint32 k = 0; k < numBytesPaddingPerRowRGB; k++)
        {
          iconBuffer.Append((char) 0);
        }
      }
      buffer += iScanLineSize;
    }
  }
//else
//// if each pixel uses 16 bits to describe the color then each R, G, and B value uses 5 bites. Use some fancy
//// bit operations to blow up the 16 bit case into 1 byte per component color. Actually windows
//// is using a 5:6:5 scheme. so the green component gets 6 bits....
//if (pBitMapInfo->cBitCount == 16)
//{
//  PRUint8 redValue, greenValue, blueValue;
//  while (index < pBitMapInfo->cbImage)
//  {
//    PRUint16 num = 0;
//    num = (PRUint8) buffer[index+1];
//    num <<= 8;
//    num |= (PRUint8) buffer[index];
//    // be sure to ignore the top bit
//    //num &= 0x7FFF; // only want the low 15 bits.....not the 16th...
//
//    // use num as an offset into the color table....get the RGBQuad entry and read out
//    // the RGB values.
//    //RGBQUAD rgbValues = pBitMapInfo->bmiColors[num];
//
//    //redValue = ( (num & 0xf800) >> 11);
//    //greenValue = ( (num & 0x07E0) >> 5);
//    //blueValue = ( num & 0x001F);
//
//    redValue = ((PRUint32) (((float)(num & 0xf800) / 0xf800) * 0xFF0000) & 0xFF0000)>> 16;
//    greenValue =  ((PRUint32)(((float)(num & 0x07E0) / 0x07E0) * 0x00FF00) & 0x00FF00)>> 8;
//    blueValue =  ((PRUint32)(((float)(num & 0x001F) / 0x001F) * 0x0000FF) & 0x0000FF);
//
//    // now we have the right RGB values...
//    iconBuffer.Append((char) blueValue);
//    iconBuffer.Append((char) greenValue);
//    iconBuffer.Append((char) redValue);
//    pos += 3;
//    if (pos == unalignedBytesPerRow && numBytesPaddingPerRow) // if we have reached the end of a current row, add padding to force dword alignment
//    {
//      pos = 0;
//      for (PRUint32 i = 0; i < numBytesPaddingPerRow; i++)
//      {
//        iconBuffer.Append((char) 0);
//      }
//    }
//    index += bytesPerPixel;
//  }
//}
  else // otherwise we must be using 32 bits per pixel so each component value is getting one byte...
  {
    while (index < pBitMapInfo->cbImage)
    {
      iconBuffer.Append((char) buffer[index]);
      iconBuffer.Append((char) buffer[index+1]);
      iconBuffer.Append((char) buffer[index+2]);
      pos += 3;
      if (pos == unalignedBytesPerRowRGB && numBytesPaddingPerRowRGB) // if we have reached the end of a current row, add padding to force dword alignment
      {
        pos = 0;
        for (PRUint32 i = 0; i < numBytesPaddingPerRowRGB; i++)
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

void ConvertMaskBitMap(PBYTE aBitMaskBuffer, PBITMAPINFO2 pBitMapInfo, nsCString& iconBuffer)
{
  PRInt32  iScanLineSize =  pBitMapInfo->cSize1;  // Stored early

  InvertRows(aBitMaskBuffer, pBitMapInfo->cbImage, iScanLineSize);
  PRUint32 index = 0;
  // for some reason the bit mask on windows are flipped from the values we really want for transparency.
  // So complement each byte in the bit mask.
  while (index < pBitMapInfo->cbImage)
  {
    aBitMaskBuffer[index]^=255;
    index += 1;
  }
  iconBuffer.Append((char *) aBitMaskBuffer, pBitMapInfo->cbImage);
}

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsCOMPtr<nsIInputStream> inStream;
  nsresult rv = MakeInputStream(getter_AddRefs(inStream), PR_TRUE);
  if (NS_FAILED(rv))
    return rv;

  // Init our streampump
  rv = mPump->Init(inStream, -1, -1, 0, 0, PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

  rv = mPump->AsyncRead(this, ctxt);
  if (NS_SUCCEEDED(rv)) {
    // Store our real listener
    mListener = aListener;
    // Add ourself to the load group, if available
    if (mLoadGroup)
      mLoadGroup->AddRequest(this, nsnull);
  }
  return rv;
}

nsresult nsIconChannel::MakeInputStream(nsIInputStream** _retval, PRBool nonBlocking)
{
  nsXPIDLCString contentType;
  nsCAutoString filePath;
  nsCOMPtr<nsIFile> localFile; // file we want an icon for
  PRUint32 desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(localFile), &desiredImageSize, contentType, filePath);
  NS_ENSURE_SUCCESS(rv, rv);

  // if the file exists, we are going to use it's real attributes...otherwise we only want to use it for it's extension...
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
    infoFlags |= SHGFI_LARGEICON;
  else
    infoFlags |= SHGFI_SMALLICON;

  // if we have a content type... then use it! but for existing files, we want
  // to show their real icon.
  if (!fileExists && !contentType.IsEmpty())
  {
    nsCOMPtr<nsIMIMEService> mimeService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString fileExt;
    mimeService->GetPrimaryExtension(contentType, EmptyCString(), fileExt);
    // If the mime service does not know about this mime type, we show
    // the generic icon.
    // In any case, we need to insert a '.' before the extension.
    filePath = NS_LITERAL_CSTRING(".") + fileExt;
  }

  // (1) get an hIcon for the file
  PSZ pszFileName = (PSZ)filePath.get();
  HPOINTER hIcon = WinLoadFileIcon(pszFileName, FALSE);
  if ((hIcon == NULLHANDLE) && (pszFileName[0] == '.')) {
    /* Just trying to get an icon for an extension */
    /* Create a temporary file to try to get an icon */
    char* tmpdir = getenv("TMP");
    if (tmpdir) {
      char tmpfile[CCHMAXPATH];
      strcpy(tmpfile, tmpdir);
      strcat(tmpfile, pszFileName);
      FILE* fp = fopen(tmpfile, "wb+");
      if (fp) {
        fclose(fp);
        hIcon = WinLoadFileIcon(tmpfile, FALSE);
        remove(tmpfile);
      }
    }
  }
  if (hIcon == NULLHANDLE)
    return NS_ERROR_FAILURE;
  // we got a handle to an icon. Now we want to get a bitmap for the icon using GetIconInfo....
  POINTERINFO IconInfo;
  BOOL fRC = WinQueryPointerInfo(hIcon, &IconInfo);
  if (fRC == 0) {
    WinFreeFileIcon(hIcon);
    return NS_ERROR_FAILURE;
  }
  nsCString iconBuffer;
  BITMAPINFOHEADER2  BMHeader;
  HBITMAP            hBitmap;
  HBITMAP            hBitmapMask;

  // Decide which icon to use
  if ( infoFlags & SHGFI_LARGEICON )
  {
    hBitmap = IconInfo.hbmColor;
    hBitmapMask = IconInfo.hbmPointer;
  }
  else
  {
    hBitmap = IconInfo.hbmMiniColor;
    hBitmapMask = IconInfo.hbmMiniPointer;
  }

  // Get the basic info
  BMHeader.cbFix = sizeof(BMHeader);
  fRC =  GpiQueryBitmapInfoHeader(hBitmap, &BMHeader);
  if (fRC == 0) {
    WinFreeFileIcon(hIcon);
    return NS_ERROR_FAILURE;
  }

///// // Calulate size of color table
///// LONG cbColorTable;
///// if ( BMHeader.cBitCount > 8 )
///// {
/////   cbColorTable = 0;
///// }
///// else
///// {
/////   cbColorTable = 1 << BMHeader.cBitCount;
///// }
  LONG cbBitMapInfo = sizeof(BITMAPINFO2) + (sizeof(RGB2) * 255); // Max possible
  LONG iScanLineSize =  ((BMHeader.cBitCount * BMHeader.cx + 31) / 32) * 4;
  LONG cbBuffer = iScanLineSize * BMHeader.cy;
  // Allocate buffers, fill w/ 0
  PBITMAPINFO2 pBitMapInfo = (PBITMAPINFO2)nsMemory::Alloc(cbBitMapInfo);
  memset(pBitMapInfo, 0, cbBitMapInfo);
  PBYTE buffer = (PBYTE)nsMemory::Alloc(cbBuffer);
  memset(buffer, 0, cbBuffer);
  // Copy over the header info
  *((PBITMAPINFOHEADER2)pBitMapInfo ) = BMHeader;

  // Create DC
  DEVOPENSTRUC  dop = {NULL, "DISPLAY", NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  HDC hdc = DevOpenDC( (HAB)0, OD_MEMORY, "*", 5L, (PDEVOPENDATA)&dop, NULLHANDLE);
  SIZEL sizel = {0,0};
  HPS hps = GpiCreatePS((HAB)0, hdc, &sizel, GPIA_ASSOC | PU_PELS | GPIT_MICRO);

  // Not sure if you need this but it is good form
  HBITMAP hOldBM = GpiSetBitmap(hps, hBitmap);

  // Get those bits
  LONG lScanLines = GpiQueryBitmapBits(hps, 0L, (LONG)BMHeader.cy, buffer, pBitMapInfo);
  if (lScanLines > 0)
  {
    // Set this since it is used all over
    pBitMapInfo->cbImage =  cbBuffer;
    pBitMapInfo->cSize1  =  iScanLineSize;

    // temporary hack alert...currently moz-icon urls only support 16, 24 and 32 bit color. we don't support
    // 8, 4 or 1 bit color yet. So convert OS/2 4 BPP to RGB

    // The first 2 bytes into our output buffer needs to be the width and the height (in pixels) of the icon
    // as specified by our data format.
    iconBuffer.Assign((char) pBitMapInfo->cx);
    iconBuffer.Append((char) pBitMapInfo->cy);

    ConvertColorBitMap(buffer, pBitMapInfo, iconBuffer);

    // now we need to tack on the alpha data...which is hbmMask

    memset(pBitMapInfo, 0, cbBitMapInfo);
    BMHeader.cbFix = sizeof(BMHeader);
    fRC =  GpiQueryBitmapInfoHeader(hBitmapMask, &BMHeader);
    iScanLineSize =  ((BMHeader.cBitCount * BMHeader.cx + 31) / 32) * 4;
    LONG cbBufferMask = iScanLineSize * BMHeader.cy;
    if (cbBufferMask > cbBuffer)  // Need more for mask
    {
      nsMemory::Free(buffer);
      buffer = (PBYTE)nsMemory::Alloc(cbBufferMask);
      memset(buffer, 0, cbBufferMask);
    }

    *((PBITMAPINFOHEADER2)pBitMapInfo ) = BMHeader;
    hOldBM = GpiSetBitmap(hps, hBitmapMask);

    lScanLines = GpiQueryBitmapBits(hps, 0L, (LONG)BMHeader.cy, buffer, pBitMapInfo);
    if (lScanLines > 0)
    {
      pBitMapInfo->cbImage =  cbBufferMask;
      pBitMapInfo->cSize1  =  iScanLineSize;
      ConvertMaskBitMap(buffer, pBitMapInfo, iconBuffer);

      // Now, create a pipe and stuff our data into it
      nsCOMPtr<nsIInputStream> inStream;
      nsCOMPtr<nsIOutputStream> outStream;
      rv = NS_NewPipe(getter_AddRefs(inStream), getter_AddRefs(outStream),
                      iconBuffer.Length(), iconBuffer.Length(), nonBlocking);
      if (NS_SUCCEEDED(rv)) {
        PRUint32 written;
        rv = outStream->Write(iconBuffer.get(), iconBuffer.Length(), &written);
        if (NS_SUCCEEDED(rv)) {
          NS_ADDREF(*_retval = inStream);
        }
      }
    } // if we have a mask buffer to apply

  } // if we got color info
  nsMemory::Free(buffer);
  nsMemory::Free(pBitMapInfo);
  if (hps)
  {
    GpiAssociate(hps, NULLHANDLE);
    GpiDestroyPS(hps);
  }
  if (hdc)
  {
    DevCloseDC(hdc);
  }
  if (hIcon)
    WinFreeFileIcon(hIcon);

  return rv;
}

NS_IMETHODIMP nsIconChannel::GetContentType(nsACString &aContentType) 
{
  aContentType.AssignLiteral("image/icon");
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

// nsIRequestObserver methods
NS_IMETHODIMP nsIconChannel::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  if (mListener)
    return mListener->OnStartRequest(this, aContext);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext, nsresult aStatus)
{
  if (mListener) {
    mListener->OnStopRequest(this, aContext, aStatus);
    mListener = nsnull;
  }

  // Remove from load group
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nsnull, aStatus);

  return NS_OK;
}

// nsIStreamListener methods
NS_IMETHODIMP nsIconChannel::OnDataAvailable(nsIRequest* aRequest,
                                             nsISupports* aContext,
                                             nsIInputStream* aStream,
                                             PRUint32 aOffset,
                                             PRUint32 aCount)
{
  if (mListener)
    return mListener->OnDataAvailable(this, aContext, aStream, aOffset, aCount);
  return NS_OK;
}
