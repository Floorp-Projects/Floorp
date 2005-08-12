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
 *   Rich Walsh <dragtext@e-vertise.com>
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

//------------------------------------------------------------------------

#include "nsIconChannel.h"
#include "nsIIconURI.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsNetUtil.h"
#include "nsInt64.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsDirectoryServiceDefs.h"

#define INCL_PM
#include <os2.h>

//------------------------------------------------------------------------

// Due to byte swap, the second nibble is the first pixel of the pair
#define FIRSTPEL(x)  (0xF & (x >> 4))
#define SECONDPEL(x) (0xF & x)

// nbr of bytes per row, rounded up to the nearest dword boundary
#define ALIGNEDBPR(cx,bits) ((( ((cx)*(bits)) + 31) / 32) * 4)

// nbr of bytes per row, rounded up to the nearest byte boundary
#define UNALIGNEDBPR(cx,bits) (( ((cx)*(bits)) + 7) / 8)

// native icon functions
HPOINTER GetFileIcon(nsCString& file, PRBool fExists);

void    ConvertColorBitMap(PRUint8* inBuf, PBITMAPINFO2 pBMInfo, PRUint8* outBuf);
void    ShrinkColorBitMap(PRUint8* inBuf, PBITMAPINFO2 pBMInfo, PRUint8* outBuf);
void    ConvertMaskBitMap(PRUint8* inBuf, PBITMAPINFO2 pBMInfo, PRUint8* outBuf);
void    ShrinkMaskBitMap(PRUint8* inBuf, PBITMAPINFO2 pBMInfo, PRUint8* outBuf);

//------------------------------------------------------------------------
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

//------------------------------------------------------------------------
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

//------------------------------------------------------------------------
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

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsCOMPtr<nsIInputStream> inStream;
  nsresult rv = MakeInputStream(getter_AddRefs(inStream), PR_TRUE);
  if (NS_FAILED(rv))
    return rv;

  // Init our streampump
  rv = mPump->Init(inStream, nsInt64(-1), nsInt64(-1), 0, 0, PR_FALSE);
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

//------------------------------------------------------------------------

// retrieves a native icon with 16, 256, or 16M colors and converts it
// to 24-bit BGR with 1-bit alpha data (BGR_A1 format);  Note:  this
// implementation ignores the file's MIME-type because using it virtually
// guarantees we'll end up with an inappropriate icon (i.e. an .exe icon)

nsresult nsIconChannel::MakeInputStream(nsIInputStream** _retval, PRBool nonBlocking)
{
  // get some details about this icon
  nsCOMPtr<nsIFile> localFile;
  PRUint32 desiredImageSize;
  nsXPIDLCString contentType;
  nsCAutoString filePath;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(localFile), &desiredImageSize, contentType, filePath);
  NS_ENSURE_SUCCESS(rv, rv);

  // if the file exists, get its path
  PRBool fileExists = PR_FALSE;
  if (localFile) {
    localFile->GetNativePath(filePath);
    localFile->Exists(&fileExists);
  }

  // get the icon from the file
  HPOINTER hIcon = GetFileIcon(filePath, fileExists);
  if (hIcon == NULLHANDLE)
    return NS_ERROR_FAILURE;

  // get the color & mask bitmaps used by the icon
  POINTERINFO IconInfo;
  if (!WinQueryPointerInfo(hIcon, &IconInfo)) {
    WinFreeFileIcon( hIcon);
    return NS_ERROR_FAILURE;
  }

  // if we need a mini-icon, use those bitmaps if present;
  // otherwise, signal that the icon needs to be shrunk
  PRBool fShrink = FALSE;
  if (desiredImageSize <= 16) {
    if (IconInfo.hbmMiniPointer) {
      IconInfo.hbmColor = IconInfo.hbmMiniColor;
      IconInfo.hbmPointer = IconInfo.hbmMiniPointer;
    }
    else
      fShrink = TRUE;
  }

  // various resources to be allocated
  PBITMAPINFO2  pBMInfo = 0;
  PRUint8*      pInBuf  = 0;
  PRUint8*      pOutBuf = 0;
  HDC           hdc     = 0;
  HPS           hps     = 0;

  // using this dummy do{...}while(0) "loop" guarantees resources will
  // be deallocated, eliminates the need for nesting, and generally makes
  // testing for & dealing with errors pretty painless (just 'break')
  do {
    rv = NS_ERROR_FAILURE;
  
    // get the details for the color bitmap;  if there isn't one
    // or this is 1-bit color, exit
    BITMAPINFOHEADER2  BMHeader;
    BMHeader.cbFix = sizeof(BMHeader);
    if (!IconInfo.hbmColor ||
        !GpiQueryBitmapInfoHeader(IconInfo.hbmColor, &BMHeader) ||
        BMHeader.cBitCount == 1)
      break;
  
    // alloc space for the color bitmap's info, including its color table
    PRUint32 cbBMInfo = sizeof(BITMAPINFO2) + (sizeof(RGB2) * 255);
    pBMInfo = (PBITMAPINFO2)nsMemory::Alloc(cbBMInfo);
    if (!pBMInfo)
      break;
  
    // alloc space for the color bitmap data
    PRUint32 cbInRow = ALIGNEDBPR( BMHeader.cx, BMHeader.cBitCount);
    PRUint32 cbInBuf = cbInRow * BMHeader.cy;
    pInBuf = (PRUint8*)nsMemory::Alloc(cbInBuf);
    if (!pInBuf)
      break;
    memset( pInBuf, 0, cbInBuf);
  
    // alloc space for the 24-bit BGR_A1 bitmap we're creating
    PRUint32 cxOut    = (fShrink ? BMHeader.cx / 2 : BMHeader.cx);
    PRUint32 cyOut    = (fShrink ? BMHeader.cy / 2 : BMHeader.cy);
    PRUint32 cbColor  = ALIGNEDBPR( cxOut, 24) * cyOut;
    PRUint32 cbMask   = ALIGNEDBPR( cxOut,  1) * cyOut;
    PRUint32 cbOutBuf = 2 + cbColor + cbMask;
    pOutBuf = (PRUint8*)nsMemory::Alloc(cbOutBuf);
    if (!pOutBuf)
      break;
    memset( pOutBuf, 0, cbOutBuf);
  
    // create a DC and PS
    DEVOPENSTRUC  dop = {NULL, "DISPLAY", NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    hdc = DevOpenDC( (HAB)0, OD_MEMORY, "*", 5L, (PDEVOPENDATA)&dop, NULLHANDLE);
    if (!hdc)
      break;
  
    SIZEL sizel = {0,0};
    hps = GpiCreatePS((HAB)0, hdc, &sizel, GPIA_ASSOC | PU_PELS | GPIT_MICRO);
    if (!hps)
      break;
  
    // get the color bits
    memset( pBMInfo, 0, cbBMInfo);
    *((PBITMAPINFOHEADER2)pBMInfo ) = BMHeader;
    GpiSetBitmap(hps, IconInfo.hbmColor);
    if (GpiQueryBitmapBits( hps, 0L, (LONG)BMHeader.cy,
                            (BYTE*)pInBuf, pBMInfo) <= 0)
      break;
  
    // The first 2 bytes are the width & height of the icon in pixels
    PRUint8* outPtr = pOutBuf;
    *outPtr++ = (PRUint8)cxOut;
    *outPtr++ = (PRUint8)cyOut;
  
    // convert the color bitmap
    pBMInfo->cbImage = cbInBuf;
    if (fShrink)
      ShrinkColorBitMap( pInBuf, pBMInfo, outPtr);
    else
      ConvertColorBitMap( pInBuf, pBMInfo, outPtr);
    outPtr += cbColor;
  
    // now we need to tack on the alpha data
  
    // Get the mask info
    BMHeader.cbFix = sizeof(BMHeader);
    if (!GpiQueryBitmapInfoHeader(IconInfo.hbmPointer, &BMHeader))
      break;
  
    // if the existing input buffer isn't large enough, reallocate it
    cbInRow  = ALIGNEDBPR( BMHeader.cx, BMHeader.cBitCount);
    if ((cbInRow * BMHeader.cy) > cbInBuf)  // Need more for mask
    {
      cbInBuf = cbInRow * BMHeader.cy;
      nsMemory::Free(pInBuf);
      pInBuf  = (PRUint8*)nsMemory::Alloc(cbInBuf);
      memset( pInBuf, 0, cbInBuf);
    }
  
    // get the mask/alpha bits
    memset( pBMInfo, 0, cbBMInfo);
    *((PBITMAPINFOHEADER2)pBMInfo ) = BMHeader;
    GpiSetBitmap(hps, IconInfo.hbmPointer);
    if (GpiQueryBitmapBits( hps, 0L, (LONG)BMHeader.cy,
                            (BYTE*)pInBuf, pBMInfo) <= 0)
      break;

    // convert the mask/alpha bitmap
    pBMInfo->cbImage = cbInBuf;
    if (fShrink)
      ShrinkMaskBitMap( pInBuf, pBMInfo, outPtr);
    else
      ConvertMaskBitMap( pInBuf, pBMInfo, outPtr);
  
    // create a pipe
    nsCOMPtr<nsIInputStream> inStream;
    nsCOMPtr<nsIOutputStream> outStream;
    rv = NS_NewPipe(getter_AddRefs(inStream), getter_AddRefs(outStream),
                    cbOutBuf, cbOutBuf, nonBlocking);
    if (NS_FAILED(rv))
      break;
  
    // put our data into the pipe
    PRUint32 written;
    rv = outStream->Write( NS_REINTERPRET_CAST(const char*, pOutBuf),
                           cbOutBuf, &written);
    if (NS_FAILED(rv))
      break;
  
    // success! so addref the pipe
    NS_ADDREF(*_retval = inStream);
  
  } while (0);

  // free all the resources we allocated
  if (pOutBuf)
    nsMemory::Free( pOutBuf);
  if (pInBuf)
    nsMemory::Free( pInBuf);
  if (pBMInfo)
    nsMemory::Free( pBMInfo);
  if (hps) {
    GpiAssociate(hps, NULLHANDLE);
    GpiDestroyPS(hps);
  }
  if (hdc)
    DevCloseDC(hdc);
  if (hIcon)
    WinFreeFileIcon( hIcon);

  return rv;
}

//------------------------------------------------------------------------

// if the file exists, get its icon;  if it doesn't, create a dummy file
// with the same extension, then use whatever icon the system assigns it

HPOINTER    GetFileIcon(nsCString& file, PRBool fExists)
{

  if (fExists)
    return WinLoadFileIcon( file.get(), FALSE);

  nsCOMPtr<nsIFile> dummyPath;
  if (NS_FAILED(NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                       getter_AddRefs(dummyPath))))
    return 0;

  if (file.First() == '.')
    file.Insert("moztmp", 0);

  if (NS_FAILED(dummyPath->AppendNative(file)))
    return 0;

  nsCAutoString dummyFile;
  dummyPath->GetNativePath(dummyFile);

  HPOINTER  hRtn = 0;
  FILE*     fp = fopen( dummyFile.get(), "wb+");
  if (fp) {
    fclose( fp);
    hRtn = WinLoadFileIcon(dummyFile.get(), FALSE);
    remove(dummyFile.get());
  }

  return hRtn;
}

//------------------------------------------------------------------------

// converts 16, 256, & 16M color bitmaps to 24-bit BGR format;  since the
// scanlines in OS/2 bitmaps run bottom-to-top, it starts at the end of the
// input buffer & works its way back;  Note:  no output padding is needed
// for any expected color-depth or size;  only 4-bit, 20x20 icons contain
// input padding that has to be ignored

void    ConvertColorBitMap(PRUint8* inBuf, PBITMAPINFO2 pBMInfo, PRUint8* outBuf)
{

  PRUint32  bprIn = ALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
  PRUint8*  pIn   = inBuf + (pBMInfo->cy - 1) * bprIn;
  PRUint8*  pOut  = outBuf;
  PRGB2 	pColorTable = &pBMInfo->argbColor[0];

  if (pBMInfo->cBitCount == 4) {
    PRUint32  ubprIn = UNALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
    PRUint32  padIn  = bprIn - ubprIn;

    for (PRUint32 row = pBMInfo->cy; row > 0; row--) {
      for (PRUint32 ndx = 0; ndx < ubprIn; ndx++, pIn++) {
        pOut = 3 + (PRUint8*)memcpy( pOut, &pColorTable[FIRSTPEL(*pIn)], 3);
        pOut = 3 + (PRUint8*)memcpy( pOut, &pColorTable[SECONDPEL(*pIn)], 3);
      }
      pIn -= (2 * bprIn) - padIn;
    }
  }
  else
  if (pBMInfo->cBitCount == 8) {
    for (PRUint32 row = pBMInfo->cy; row > 0; row--) {
      for (PRUint32 ndx = 0; ndx < bprIn; ndx++, pIn++) {
        pOut = 3 + (PRUint8*)memcpy( pOut, &pColorTable[*pIn], 3);
      }
      pIn -= 2 * bprIn;
    }
  }
  else
  if (pBMInfo->cBitCount == 24) {
    for (PRUint32 row = pBMInfo->cy; row > 0; row--) {
      pOut = bprIn + (PRUint8*)memcpy( pOut, pIn, bprIn);
      pIn -= bprIn;
    }
  }

  return;
}

//------------------------------------------------------------------------

// similar to ConvertColorBitMap() except that it skips every other pixel
// horizontally, & every other line vertically;  this is the exact reverse
// of what GPI does when it expands a mini-icon to full-size

void    ShrinkColorBitMap(PRUint8* inBuf, PBITMAPINFO2 pBMInfo, PRUint8* outBuf)
{

  PRUint32  bprIn = ALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
  PRUint8*  pIn   = inBuf + (pBMInfo->cy - 1) * bprIn;
  PRUint8*  pOut  = outBuf;
  PRGB2 	pColorTable = &pBMInfo->argbColor[0];

  if (pBMInfo->cBitCount == 4) {
    PRUint32  ubprIn = UNALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
    PRUint32  padIn  = bprIn - ubprIn;

    for (PRUint32 row = pBMInfo->cy; row > 0; row -= 2) {
      for (PRUint32 ndx = 0; ndx < ubprIn; ndx++, pIn++) {
        pOut = 3 + (PRUint8*)memcpy( pOut, &pColorTable[FIRSTPEL(*pIn)], 3);
      }
      pIn -= (3 * bprIn) - padIn;
    }
  }
  else
  if (pBMInfo->cBitCount == 8) {
    for (PRUint32 row = pBMInfo->cy; row > 0; row -= 2) {
      for (PRUint32 ndx = 0; ndx < bprIn; ndx += 2, pIn += 2) {
        pOut = 3 + (PRUint8*)memcpy( pOut, &pColorTable[*pIn], 3);
      }
      pIn -= 3 * bprIn;
    }
  }
  else
  if (pBMInfo->cBitCount == 24) {
    for (PRUint32 row = pBMInfo->cy; row > 0; row -= 2) {
      for (PRUint32 ndx = 0; ndx < bprIn; ndx += 6, pIn += 3) {
        pOut = 3 + (PRUint8*)memcpy( pOut, pIn, 3);
        pIn += 3;
      }
      pIn -= 3 * bprIn;
    }
  }

  return;
}

//------------------------------------------------------------------------

// converts an icon's AND mask into 1-bit alpha data;  since the AND mask
// is the 2nd half of a pair of bitmaps & the scanlines run bottom-to-top,
// starting at the end & working back to the midpoint converts the entire
// bitmap;  Note: because the input is already padded to a dword boundary,
// the output will be padded automatically

void    ConvertMaskBitMap(PRUint8* inBuf, PBITMAPINFO2 pBMInfo, PRUint8* outBuf)
{

  PRUint32  bprIn  = ALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
  PRUint32  lprIn  = bprIn / 4;
  PRUint32* pOut32 = (PRUint32*)outBuf;
  PRUint32* pIn32  = (PRUint32*)(inBuf + (pBMInfo->cy - 1) * bprIn);

  for (PRUint32 row = pBMInfo->cy/2; row > 0; row--) {
    for (PRUint32 ndx = 0; ndx < lprIn; ndx++) {
        *pOut32++ = ~(*pIn32++);
    }
    pIn32 -= 2 * lprIn;
  }

  return;
}

//------------------------------------------------------------------------

// similar to ConvertMaskBitMap() except that it skips every other pixel
// horizontally, & every other line vertically;  Note:  this is the only
// one of these functions that may have to add padding to its output

void    ShrinkMaskBitMap(PRUint8* inBuf, PBITMAPINFO2 pBMInfo, PRUint8* outBuf)
{

  PRUint32  bprIn  = ALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
  PRUint32  padOut = (bprIn / 2) & 3;
  PRUint8*  pOut   = outBuf;
  PRUint8*  pIn    = inBuf + (pBMInfo->cy - 1) * bprIn;

  // for every other row
  for (PRUint32 row = pBMInfo->cy/2; row > 0; row -= 2) {
    PRUint8 dest = 0;
    PRUint8 destMask = 0x80;

    // for every byte in the row
    for (PRUint32 ndx = 0; ndx < bprIn; ndx++) {
      PRUint8 src = ~(*pIn++);
      PRUint8 srcMask = 0x80;

      // for every other bit in the current byte
      for (PRUint32 bitNdx = 0; bitNdx < 8; bitNdx += 2) {
        if (src & srcMask)
          dest |= destMask;
        srcMask >>= 2;
        destMask >>= 1;
      }

      // if we've filled an output byte from two input bytes, save it
      if (!destMask) {
        *pOut++ = dest;
        dest = 0;
        destMask = 0x80;
      }
    }

    // after we've processed every input byte in the row, 
    // does the output row need padding?
    if (padOut) {
      memset( pOut, 0, padOut);
      pOut += padOut;
    }

    pIn -= 3 * bprIn;
  }

  return;
}

//------------------------------------------------------------------------

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

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nsnull;

  return NS_OK;
}

//------------------------------------------------------------------------
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

//------------------------------------------------------------------------

