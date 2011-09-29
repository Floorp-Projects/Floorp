/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Pinkerton <pinkerton@netscape.com>
 *   Gus Verdun <gustavoverdun@aol.com>
 *   Kathleen Brade <brade@comcast.net>
 *   Mark Smith <mcs@pearlcrescent.com>
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
 
 
#include "nsITransferable.h"
#include "nsImageClipboard.h"
#include "nsGfxCIID.h"
#include "nsMemory.h"
#include "prmem.h"
#include "imgIEncoder.h"
#include "nsLiteralString.h"
#include "nsComponentManagerUtils.h"

/* Things To Do 11/8/00

Check image metrics, can we support them? Do we need to?
Any other render format? HTML?

*/


//
// nsImageToClipboard ctor
//
// Given an imgIContainer, convert it to a DIB that is ready to go on the win32 clipboard
//
nsImageToClipboard :: nsImageToClipboard ( imgIContainer* inImage )
  : mImage(inImage)
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
}


//
// GetPicture
//
// Call to get the actual bits that go on the clipboard. If an error 
// ocurred during conversion, |outBits| will be null.
//
// NOTE: The caller owns the handle and must delete it with ::GlobalRelease()
//
nsresult
nsImageToClipboard :: GetPicture ( HANDLE* outBits )
{
  NS_ASSERTION ( outBits, "Bad parameter" );

  return CreateFromImage ( mImage, outBits );

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
nsImageToClipboard::CreateFromImage ( imgIContainer* inImage, HANDLE* outBitmap )
{
    *outBitmap = nsnull;

    nsRefPtr<gfxImageSurface> frame;
    nsresult rv = inImage->CopyFrame(imgIContainer::FRAME_CURRENT,
                                     imgIContainer::FLAG_SYNC_DECODE,
                                     getter_AddRefs(frame));
    if (NS_FAILED(rv))
      return rv;

    const PRUint32 imageSize = frame->GetDataSize();
    const PRInt32 bitmapSize = sizeof(BITMAPINFOHEADER) + imageSize;

    HGLOBAL glob = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT, bitmapSize);
    if (!glob) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Create the buffer where we'll copy the image bits (and header) into and lock it
    void *data = (void*)::GlobalLock(glob);

    BITMAPINFOHEADER *header = (BITMAPINFOHEADER*)data;
    header->biSize          = sizeof(BITMAPINFOHEADER);
    header->biWidth         = frame->Width();
    header->biHeight        = frame->Height();

    header->biPlanes        = 1;
    if (frame->Format() == gfxASurface::ImageFormatARGB32)
        header->biBitCount  = 32;
    else if (frame->Format() == gfxASurface::ImageFormatRGB24)
        header->biBitCount  = 24;
    header->biCompression   = BI_RGB;
    header->biSizeImage     = imageSize;

    const PRUint32 bpr = frame->Stride();

    BYTE *dstBits = (BYTE*)data + sizeof(BITMAPINFOHEADER);
    BYTE *srcBits = frame->Data();
    for (PRInt32 i = 0; i < header->biHeight; ++i) {
        PRUint32 srcOffset = imageSize - (bpr * (i + 1));
        PRUint32 dstOffset = i * bpr;
        ::CopyMemory(dstBits + dstOffset, srcBits + srcOffset, bpr);
    }
    
    ::GlobalUnlock(glob);

    *outBitmap = (HANDLE)glob;

    return NS_OK;
}

nsImageFromClipboard :: nsImageFromClipboard ()
{
  // nothing to do here
}

nsImageFromClipboard :: ~nsImageFromClipboard ( )
{
}

//
// GetEncodedImageStream
//
// Take the raw clipboard image data and convert it to aMIMEFormat in the form of a nsIInputStream
//
nsresult 
nsImageFromClipboard ::GetEncodedImageStream (unsigned char * aClipboardData, const char * aMIMEFormat, nsIInputStream** aInputStream )
{
  NS_ENSURE_ARG_POINTER (aInputStream);
  NS_ENSURE_ARG_POINTER (aMIMEFormat);
  nsresult rv;
  *aInputStream = nsnull;

  // pull the size information out of the BITMAPINFO header and
  // initialize the image
  BITMAPINFO* header = (BITMAPINFO *) aClipboardData;
  PRInt32 width  = header->bmiHeader.biWidth;
  PRInt32 height = header->bmiHeader.biHeight;
  // neg. heights mean the Y axis is inverted and we don't handle that case
  NS_ENSURE_TRUE(height > 0, NS_ERROR_FAILURE); 

  unsigned char * rgbData = new unsigned char[width * height * 3 /* RGB */];

  if (rgbData) {
    BYTE  * pGlobal = (BYTE *) aClipboardData;
    // Convert the clipboard image into RGB packed pixel data
    rv = ConvertColorBitMap((unsigned char *) (pGlobal + header->bmiHeader.biSize), header, rgbData);
    // if that succeeded, encode the bitmap as aMIMEFormat data. Don't return early or we risk leaking rgbData
    if (NS_SUCCEEDED(rv)) {
      nsCAutoString encoderCID(NS_LITERAL_CSTRING("@mozilla.org/image/encoder;2?type="));

      // Map image/jpg to image/jpeg (which is how the encoder is registered).
      if (strcmp(aMIMEFormat, kJPEGImageMime) == 0)
        encoderCID.Append("image/jpeg");
      else
        encoderCID.Append(aMIMEFormat);
      nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get(), &rv);
      if (NS_SUCCEEDED(rv)){
        rv = encoder->InitFromData(rgbData, 0, width, height, 3 * width /* RGB * # pixels in a row */, 
                                   imgIEncoder::INPUT_FORMAT_RGB, EmptyString());
        if (NS_SUCCEEDED(rv))
          encoder->QueryInterface(NS_GET_IID(nsIInputStream), (void **) aInputStream);
      }
    }
    delete [] rgbData;
  } 
  else 
    rv = NS_ERROR_OUT_OF_MEMORY;

  return rv;
} // GetImage

//
// InvertRows
//
// Take the image data from the clipboard and invert the rows. Modifying aInitialBuffer in place.
//
void
nsImageFromClipboard::InvertRows(unsigned char * aInitialBuffer, PRUint32 aSizeOfBuffer, PRUint32 aNumBytesPerRow)
{
  if (!aNumBytesPerRow) 
    return; 

  PRUint32 numRows = aSizeOfBuffer / aNumBytesPerRow;
  unsigned char * row = new unsigned char[aNumBytesPerRow];

  PRUint32 currentRow = 0;
  PRUint32 lastRow = (numRows - 1) * aNumBytesPerRow;
  while (currentRow < lastRow)
  {
    // store the current row into a temporary buffer
    memcpy(row, &aInitialBuffer[currentRow], aNumBytesPerRow);
    memcpy(&aInitialBuffer[currentRow], &aInitialBuffer[lastRow], aNumBytesPerRow);
    memcpy(&aInitialBuffer[lastRow], row, aNumBytesPerRow);
    lastRow -= aNumBytesPerRow;
    currentRow += aNumBytesPerRow;
  }

  delete[] row;
}

//
// ConvertColorBitMap
//
// Takes the clipboard bitmap and converts it into a RGB packed pixel values.
//
nsresult 
nsImageFromClipboard::ConvertColorBitMap(unsigned char * aInputBuffer, PBITMAPINFO pBitMapInfo, unsigned char * aOutBuffer)
{
  PRUint8 bitCount = pBitMapInfo->bmiHeader.biBitCount; 
  PRUint32 imageSize = pBitMapInfo->bmiHeader.biSizeImage; // may be zero for BI_RGB bitmaps which means we need to calculate by hand
  PRUint32 bytesPerPixel = bitCount / 8;
  
  if (bitCount <= 4)
    bytesPerPixel = 1;

  // rows are DWORD aligned. Calculate how many real bytes are in each row in the bitmap. This number won't 
  // correspond to biWidth.
  PRUint32 rowSize = (bitCount * pBitMapInfo->bmiHeader.biWidth + 7) / 8; // +7 to round up
  if (rowSize % 4)
    rowSize += (4 - (rowSize % 4)); // Pad to DWORD Boundary
  
  // if our buffer includes a color map, skip over it 
  if (bitCount <= 8)
  {
    PRInt32 bytesToSkip = (pBitMapInfo->bmiHeader.biClrUsed ? pBitMapInfo->bmiHeader.biClrUsed : (1 << bitCount) ) * sizeof(RGBQUAD);
    aInputBuffer +=  bytesToSkip;
  }

  bitFields colorMasks; // only used if biCompression == BI_BITFIELDS

  if (pBitMapInfo->bmiHeader.biCompression == BI_BITFIELDS)
  {
    // color table consists of 3 DWORDS containing the color masks...
    colorMasks.red = (*((PRUint32*)&(pBitMapInfo->bmiColors[0]))); 
    colorMasks.green = (*((PRUint32*)&(pBitMapInfo->bmiColors[1]))); 
    colorMasks.blue = (*((PRUint32*)&(pBitMapInfo->bmiColors[2]))); 
    CalcBitShift(&colorMasks);
    aInputBuffer += 3 * sizeof(DWORD);
  } 
  else if (pBitMapInfo->bmiHeader.biCompression == BI_RGB && !imageSize)  // BI_RGB can have a size of zero which means we figure it out
  {
    // XXX: note use rowSize here and not biWidth. rowSize accounts for the DWORD padding for each row
    imageSize = rowSize * pBitMapInfo->bmiHeader.biHeight;
  }

  // The windows clipboard image format inverts the rows 
  InvertRows(aInputBuffer, imageSize, rowSize);

  if (!pBitMapInfo->bmiHeader.biCompression || pBitMapInfo->bmiHeader.biCompression == BI_BITFIELDS) 
  {  
    PRUint32 index = 0;
    PRUint32 writeIndex = 0;
     
    unsigned char redValue, greenValue, blueValue;
    PRUint8 colorTableEntry = 0;
    PRInt8 bit; // used for grayscale bitmaps where each bit is a pixel
    PRUint32 numPixelsLeftInRow = pBitMapInfo->bmiHeader.biWidth; // how many more pixels do we still need to read for the current row
    PRUint32 pos = 0;

    while (index < imageSize)
    {
      switch (bitCount) 
      {
        case 1:
          for (bit = 7; bit >= 0 && numPixelsLeftInRow; bit--)
          {
            colorTableEntry = (aInputBuffer[index] >> bit) & 1;
            aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[colorTableEntry].rgbRed;
            aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[colorTableEntry].rgbGreen;
            aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[colorTableEntry].rgbBlue;
            numPixelsLeftInRow--;
          }
          pos += 1;
          break;
        case 4:
          {
            // each aInputBuffer[index] entry contains data for two pixels.
            // read the first pixel
            colorTableEntry = aInputBuffer[index] >> 4;
            aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[colorTableEntry].rgbRed;
            aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[colorTableEntry].rgbGreen;
            aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[colorTableEntry].rgbBlue;
            numPixelsLeftInRow--;

            if (numPixelsLeftInRow) // now read the second pixel
            {
              colorTableEntry = aInputBuffer[index] & 0xF;
              aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[colorTableEntry].rgbRed;
              aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[colorTableEntry].rgbGreen;
              aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[colorTableEntry].rgbBlue;
              numPixelsLeftInRow--;
            }
            pos += 1;
          }
          break;
        case 8:
          aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[aInputBuffer[index]].rgbRed;
          aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[aInputBuffer[index]].rgbGreen;
          aOutBuffer[writeIndex++] = pBitMapInfo->bmiColors[aInputBuffer[index]].rgbBlue;
          numPixelsLeftInRow--;
          pos += 1;    
          break;
        case 16:
          {
            PRUint16 num = 0;
            num = (PRUint8) aInputBuffer[index+1];
            num <<= 8;
            num |= (PRUint8) aInputBuffer[index];

            redValue = ((PRUint32) (((float)(num & 0xf800) / 0xf800) * 0xFF0000) & 0xFF0000)>> 16;
            greenValue =  ((PRUint32)(((float)(num & 0x07E0) / 0x07E0) * 0x00FF00) & 0x00FF00)>> 8;
            blueValue =  ((PRUint32)(((float)(num & 0x001F) / 0x001F) * 0x0000FF) & 0x0000FF);

            // now we have the right RGB values...
            aOutBuffer[writeIndex++] = redValue;
            aOutBuffer[writeIndex++] = greenValue;
            aOutBuffer[writeIndex++] = blueValue;
            numPixelsLeftInRow--;
            pos += 2;          
          }
          break;
        case 32:
        case 24:
          if (pBitMapInfo->bmiHeader.biCompression == BI_BITFIELDS)
          {
            PRUint32 val = *((PRUint32*) (aInputBuffer + index) );
            aOutBuffer[writeIndex++] = (val & colorMasks.red) >> colorMasks.redRightShift << colorMasks.redLeftShift;
            aOutBuffer[writeIndex++] =  (val & colorMasks.green) >> colorMasks.greenRightShift << colorMasks.greenLeftShift;
            aOutBuffer[writeIndex++] = (val & colorMasks.blue) >> colorMasks.blueRightShift << colorMasks.blueLeftShift;
            numPixelsLeftInRow--;
            pos += 4; // we read in 4 bytes of data in order to process this pixel
          }
          else
          {
            aOutBuffer[writeIndex++] = aInputBuffer[index+2];
            aOutBuffer[writeIndex++] =  aInputBuffer[index+1];
            aOutBuffer[writeIndex++] = aInputBuffer[index];
            numPixelsLeftInRow--;
            pos += bytesPerPixel; // 3 bytes for 24 bit data, 4 bytes for 32 bit data (we skip over the 4th byte)...
          }
          break;
        default:
          // This is probably the wrong place to check this...
          return NS_ERROR_FAILURE;
      }

      index += bytesPerPixel; // increment our loop counter

      if (!numPixelsLeftInRow)
      {
        if (rowSize != pos)
        {
          // advance index to skip over remaining padding bytes
          index += (rowSize - pos);
        }
        numPixelsLeftInRow = pBitMapInfo->bmiHeader.biWidth;
        pos = 0; 
      }

    } // while we still have bytes to process
  }

  return NS_OK;
}

void nsImageFromClipboard::CalcBitmask(PRUint32 aMask, PRUint8& aBegin, PRUint8& aLength)
{
  // find the rightmost 1
  PRUint8 pos;
  bool started = false;
  aBegin = aLength = 0;
  for (pos = 0; pos <= 31; pos++) 
  {
    if (!started && (aMask & (1 << pos))) 
    {
      aBegin = pos;
      started = PR_TRUE;
    }
    else if (started && !(aMask & (1 << pos))) 
    {
      aLength = pos - aBegin;
      break;
    }
  }
}

void nsImageFromClipboard::CalcBitShift(bitFields * aColorMask)
{
  PRUint8 begin, length;
  // red
  CalcBitmask(aColorMask->red, begin, length);
  aColorMask->redRightShift = begin;
  aColorMask->redLeftShift = 8 - length;
  // green
  CalcBitmask(aColorMask->green, begin, length);
  aColorMask->greenRightShift = begin;
  aColorMask->greenLeftShift = 8 - length;
  // blue
  CalcBitmask(aColorMask->blue, begin, length);
  aColorMask->blueRightShift = begin;
  aColorMask->blueLeftShift = 8 - length;
}
