/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Mike Pinkerton <pinkerton@netscape.com>
 *   Gus Verdun <gustavoverdun@aol.com>
 */
 
 
#include "nsImageClipboard.h"
#include "nsGfxCIID.h"
#include "nsIImage.h"

#include <windows.h>


/* Things To Do 11/8/00

Check image metrics, can we support them? Do we need to?
Any other render format? HTML?

*/


//
// nsImageToClipboard ctor
//
// Given an nsIImage, convert it to a DIB that is ready to go on the win32 clipboard
//
nsImageToClipboard :: nsImageToClipboard ( nsIImage* inImage )
  : mHeader(nsnull), mBitmap(nsnull), mImage(inImage)
{
  // nothing to do here
}


//
// nsImageToClipboard dtor
//
// Clean up after ourselves. We know that we have created the bitmap
// successfully if we still have a pointer to the header.
//
nsImageToClipboard::~nsImageToClipboard()
{
  if ( mHeader )
    ::GlobalFree ( (HGLOBAL)mBitmap );
}


//
// GetPicture
//
// Call to get the actual bits that go on the clipboard. If an error 
// ocurred during conversion, |outBits| will be null.
//
// NOTE: This routine owns the handle that is returned. Do not release the
//        memory.
//
nsresult
nsImageToClipboard :: GetPicture ( HANDLE* outBits )
{
  NS_ASSERTION ( outBits, "Bad parameter" );

  nsresult res = CreateFromImage ( mImage );
  if ( NS_SUCCEEDED(res) )
    *outBits = mBitmap;
    
  return res;

} // GetPicture


//
// CalcSize
//
// Computes # of bytes needed by a bitmap with the specified attributes.
//
PRInt32 
nsImageToClipboard :: CalcSize ( PRInt32 aHeight, PRInt32 aColors, WORD aBitsPerPixel, PRInt32 aSpanBytes )
{
  PRInt32 HeaderMem = sizeof(BITMAPINFOHEADER);

  // add size of pallette to header size
  if (aBitsPerPixel < 16)
    HeaderMem += aColors * sizeof(RGBQUAD);

  if (aHeight < 0)
    aHeight = -aHeight;

  return (HeaderMem + (aHeight * aSpanBytes));
}


//
// CalcSpanLength
//
// Computes the span bytes for determining the overall size of the image
//
PRInt32 
nsImageToClipboard::CalcSpanLength(PRUint32 aWidth, PRUint32 aBitCount)
{
  PRInt32 spanBytes = (aWidth * aBitCount) >> 5;
  
  if ((aWidth * aBitCount) & 0x1F)
    spanBytes++;
  spanBytes <<= 2;

  return spanBytes;
}


//
// CreateFromImage
//
// Do the work to setup the bitmap header and copy the bits out of the
// image. 
//
nsresult
nsImageToClipboard::CreateFromIImage ( nsIImage* inImage )
{
  nsresult result = NS_OK;

/*
  //pHead = (BITMAPINFOHEADER*)pImage->GetBitInfo();
  //pImage->GetNativeData((void**)&hBitMap);
 if (hBitMap)
  {
         BITMAPINFO * pBMI;
         pBMI                    = (BITMAPINFO *)new BYTE[(sizeof(BITMAPINFOHEADER)+256*4)];
         BITMAPINFOHEADER * pBIH = (BITMAPINFOHEADER *)pBMI;
         pBIH->biSize            = sizeof(BITMAPINFOHEADER);
         pBIH->biBitCount        = 0;
         pBIH->biPlanes          = 1;
         pBIH->biSizeImage       = 0;
         pBIH->biXPelsPerMeter   = 0;
         pBIH->biYPelsPerMeter   = 0;
         pBIH->biClrUsed         = 0;                            // Always use the whole palette.

         pBIH->biClrImportant    = 0;

         // Get bitmap format.

         HDC hdc  =  ::GetDC(NULL);
         int rc   =  ::GetDIBits(hdc, hBitMap, 0, 0, NULL, pBMI, DIB_RGB_COLORS);
         NS_ASSERTION(rc, "rc returned false");

         nLength = CalcSize(pBIH->biHeight, pBIH->biClrUsed, pBIH->biBitCount, CalcSpanLength(pBIH->biWidth, pBIH->biBitCount));

         // alloc and lock

         mBitmap = (HANDLE)::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT, nLength);
         if (mBitmap && (mHeader = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL) mBitmap)) )
         {
                 result = TRUE;

                 pBits = (PBYTE)&mHeader[1];
                 memcpy(mHeader, pBIH, sizeof (BITMAPINFOHEADER));
                 rc = ::GetDIBits(hdc, hBitMap, 0, mHeader->biHeight, pBits, (BITMAPINFO *)mHeader, DIB_RGB_COLORS);
                 NS_ASSERTION(rc, "rc returned false");
                 delete[] (BYTE*)pBMI;
                 ::GlobalUnlock((HGLOBAL)mBitmap); // we use mHeader to tell if we have to free it later.

         }
         ::ReleaseDC(NULL, hdc);
  }
  else
  {
*/

//XXX Lock the bits!!!!
//XXX I guess i need a locker stack-based object here.

  if ( inImage->GetBits() ) {
    BITMAPINFOHEADER* imageHeader = NS_REINTERPRET_CAST(BITMAPINFOHEADER*, inImage->GetBitInfo());
    NS_ASSERTION ( imageHeader, "Can't get header for image" );
    if ( !imageHeader )
      return NS_ERROR_FAILURE;
      
    PRInt32 imageSize = CalcSize(imageHeader->biHeight, imageHeader->biClrUsed, imageHeader->biBitCount, imageHeader->GetLineStride());

    // Create the buffer where we'll copy the image bits (and header) into and lock it
    mBitmap = (HANDLE)::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT, imageSize);
    if (mBitmap && (mHeader = (BITMAPINFOHEADER*)::GlobalLock((HGLOBAL) mBitmap)) )
    {
      RGBQUAD *pRGBQUAD = (RGBQUAD *)&mHeader[1];
      PBYTE bits = (PBYTE)&pRGBQUAD[pHead->biClrUsed];

      // Fill in the header info.

      mHeader->biSize          = sizeof(BITMAPINFOHEADER);
      mHeader->biWidth         = pHead->biWidth;
      imageHeight             = mHeader->biHeight = imageHeader->biHeight;
      if ( imageHeight < 0 )
        imageHeight = -imageHeight;

      mHeader->biPlanes        = 1;
      mHeader->biBitCount      = imageHeader->biBitCount;
      mHeader->biCompression   = BI_RGB;               // No compression

      mHeader->biSizeImage     = 0;
      mHeader->biXPelsPerMeter = 0;
      mHeader->biYPelsPerMeter = 0;
      mHeader->biClrUsed       = imageHeader->biClrUsed;
      mHeader->biClrImportant  = 0;

      // Set up the color map.
      
      nsColorMap *colorMap = inImage->GetColorMap();
      if ( colorMap ) {
        PBYTE pClr = colorMap->Index;

        NS_ASSERTION(( ((DWORD)colorMap->NumColors) == mHeader->biClrUsed), "Color counts must match");
        for ( UINT i=0; i < mHeader->biClrUsed; ++i ) {
          NS_WARNING ( "Cool! You found a test case for this! Let Gus or Pink know" );
          
          // now verify that the order is correct
          pRGBQUAD[i].rgbBlue  = *pClr++;
          pRGBQUAD[i].rgbGreen = *pClr++;
          pRGBQUAD[i].rgbRed   = *pClr++;
        }
      }
      else
        NS_ASSERTION(mHeader->biClrUsed == 0, "Ok, now why are there colors and no table?");

      // Copy!!
      ::CopyMemory(bits, inImage->GetBits(), inImage->GetLineStride() * mHeader->biHeight);
      
      ::GlobalUnlock((HGLOBAL)mBitmap);
    }
    else
      result = NS_ERROR_FAILURE;
  } // if we can get the bits from the image
  
  return result;
}


#if XP_MAC
#pragma mark -
#endif


nsImageFromClipboard :: nsImageFromClipboard ( BITMAPV4HEADER* inHeader )
  : mHeader(inHeader)
{
  // nothing to do here
}


nsImageFromClipboard :: ~nsImageFromClipboard ( )
{
  // since the output is a COM object, it is refcounted and such we've got
  // nothing to do.
}


//
// GetImage
//
// Take the image data in the given buffer and create an nsIImage based
// off it
//
nsresult 
nsImageFromClipboard :: GetImage ( nsIImage** outImage )
{
  NS_ASSERTION ( outImage, "Bad parameter" );
  *outImage = nsnull;
  
  static NS_DEFINE_IID(kCImageCID, NS_IMAGE_CID);
  nsresult rv = nsComponentManager::CreateInstance(kCImageCID, nsnull, 
                                                    NS_GET_IID(nsIImage), 
                                                    (void**) outImage);
  if ( NS_SUCCEEDED(rv) ) {
    // pull the size informat out of the BITMAPINFO header and
    // initialize the image
    PRInt32 width  = mHeader->bV4Width;
    PRInt32 height = mHeader->bV4Height;
    PRInt32 depth  = mHeader->bV4BitCount;
    PRUint8 * bits = GetDIBBits((BITMAPINFO *)mHeader);
    if ( !bits )
      return NS_ERROR_FAILURE;
      
    // BUG 44369 notes problems with the GFX image code handling
    // depths other than 8 or 24 bits. Ensure that's what we have
    // before we try to work with the image. (pinkerton)
    if ( depth == 24 || depth == 8 ) {
      outImage->Init(width, height, depth, nsMaskRequirements_kNoMask);

      // Now, copy the image bits from the Dib into the nsIImage's buffer
      PRUint8 * imageBits = outImage->GetBits();
      depth = (depth >> 3);
      PRUint32 size = width * height * depth;
      ::CopyMemory(imageBits, bits, size);
    }
  }

  return rv;

} // GetImage


//
// GetDIBBits
//
// Compute the sizes of the various headers then return a ptr into the buffer
// that is at the start of the bits.
//
PRUint8*
nsImageFromClipboard :: GetDIBBits ( )
{
  BITMAPINFO * aBitmapInfo = (BITMAPINFO*) mHeader;
  DWORD   maskSize;
  DWORD   colorSize;  

  // Get the size of the information header
  DWORD headerSize = aBitmapInfo->bmiHeader.biSize ;
  if (headerSize != sizeof (BITMAPCOREHEADER) &&
      headerSize != sizeof (BITMAPINFOHEADER) &&
      //headerSize != sizeof (BITMAPV5HEADER) &&
      headerSize != sizeof (BITMAPV4HEADER))
    return NULL ;

  // Get the size of the color masks
  if (headerSize == sizeof (BITMAPINFOHEADER) &&
      aBitmapInfo->bmiHeader.biCompression == BI_BITFIELDS)
    maskSize = 3 * sizeof (DWORD);
  else
    maskSize = 0;

  // Get the size of the color table
  if (headerSize == sizeof (BITMAPCOREHEADER)) {
    int iBitCount = ((BITMAPCOREHEADER *) aBitmapInfo)->bcBitCount;
    if (iBitCount <= 8)
      colorSize = (1 << iBitCount) * sizeof (RGBTRIPLE);
    else
      colorSize = 0 ;
  } 
  else {         // All non-OS/2 compatible DIBs
    if (aBitmapInfo->bmiHeader.biClrUsed > 0)
      colorSize = aBitmapInfo->bmiHeader.biClrUsed * sizeof (RGBQUAD);
    else if (aBitmapInfo->bmiHeader.biBitCount <= 8)
      colorSize = (1 << aBitmapInfo->bmiHeader.biBitCount) * sizeof (RGBQUAD);
    else
      colorSize = 0;
  }

  // the bits are at an offset into the buffer that is the header size plus the
  // mask size plus the color table size. 
  return (BYTE *) aBitmapInfo + (headerSize + maskSize + colorSize);
  
} // GetDIBBits



