/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#include "nsImageClipboard.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/RefPtr.h"
#include "nsITransferable.h"
#include "nsGfxCIID.h"
#include "nsMemory.h"
#include "imgIEncoder.h"
#include "nsLiteralString.h"
#include "nsComponentManagerUtils.h"

#define BFH_LENGTH 14

using namespace mozilla;
using namespace mozilla::gfx;

/* Things To Do 11/8/00

Check image metrics, can we support them? Do we need to?
Any other render format? HTML?

*/


//
// nsImageToClipboard ctor
//
// Given an imgIContainer, convert it to a DIB that is ready to go on the win32 clipboard
//
nsImageToClipboard::nsImageToClipboard(imgIContainer* aInImage, bool aWantDIBV5)
  : mImage(aInImage)
  , mWantDIBV5(aWantDIBV5)
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
int32_t 
nsImageToClipboard :: CalcSize ( int32_t aHeight, int32_t aColors, WORD aBitsPerPixel, int32_t aSpanBytes )
{
  int32_t HeaderMem = sizeof(BITMAPINFOHEADER);

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
int32_t 
nsImageToClipboard::CalcSpanLength(uint32_t aWidth, uint32_t aBitCount)
{
  int32_t spanBytes = (aWidth * aBitCount) >> 5;
  
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
    nsresult rv;
    *outBitmap = nullptr;

    RefPtr<SourceSurface> surface =
      inImage->GetFrame(imgIContainer::FRAME_CURRENT,
                        imgIContainer::FLAG_SYNC_DECODE);
    NS_ENSURE_TRUE(surface, NS_ERROR_FAILURE);

    MOZ_ASSERT(surface->GetFormat() == SurfaceFormat::B8G8R8A8 ||
               surface->GetFormat() == SurfaceFormat::B8G8R8X8);

    RefPtr<DataSourceSurface> dataSurface;
    if (surface->GetFormat() == SurfaceFormat::B8G8R8A8) {
      dataSurface = surface->GetDataSurface();
    } else {
      // XXXjwatt Bug 995923 - get rid of this copy and handle B8G8R8X8
      // directly below once bug 995807 is fixed.
      dataSurface = gfxUtils::
        CopySurfaceToDataSourceSurfaceWithFormat(surface,
                                                 SurfaceFormat::B8G8R8A8);
    }
    NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance("@mozilla.org/image/encoder;2?type=image/bmp", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    uint32_t format;
    nsAutoString options;
    if (mWantDIBV5) {
      options.AppendLiteral("version=5;bpp=");
    } else {
      options.AppendLiteral("version=3;bpp=");
    }
    switch (dataSurface->GetFormat()) {
    case SurfaceFormat::B8G8R8A8:
        format = imgIEncoder::INPUT_FORMAT_HOSTARGB;
        options.AppendInt(32);
        break;
#if 0
    // XXXjwatt Bug 995923 - fix |format| and reenable once bug 995807 is fixed.
    case SurfaceFormat::B8G8R8X8:
        format = imgIEncoder::INPUT_FORMAT_RGB;
        options.AppendInt(24);
        break;
#endif
    default:
        MOZ_ASSERT_UNREACHABLE("Unexpected surface format");
        return NS_ERROR_INVALID_ARG;  
    }

    DataSourceSurface::MappedSurface map;
    bool mappedOK = dataSurface->Map(DataSourceSurface::MapType::READ, &map);
    NS_ENSURE_TRUE(mappedOK, NS_ERROR_FAILURE);

    rv = encoder->InitFromData(map.mData, 0,
                               dataSurface->GetSize().width,
                               dataSurface->GetSize().height,
                               map.mStride,
                               format, options);
    dataSurface->Unmap();
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t size;
    encoder->GetImageBufferUsed(&size);
    NS_ENSURE_TRUE(size > BFH_LENGTH, NS_ERROR_FAILURE);
    HGLOBAL glob = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT,
                                 size - BFH_LENGTH);
    if (!glob)
        return NS_ERROR_OUT_OF_MEMORY;

    char *dst = (char*) ::GlobalLock(glob);
    char *src;
    rv = encoder->GetImageBuffer(&src);
    NS_ENSURE_SUCCESS(rv, rv);

    ::CopyMemory(dst, src + BFH_LENGTH, size - BFH_LENGTH);
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
  *aInputStream = nullptr;

  // pull the size information out of the BITMAPINFO header and
  // initialize the image
  BITMAPINFO* header = (BITMAPINFO *) aClipboardData;
  int32_t width  = header->bmiHeader.biWidth;
  int32_t height = header->bmiHeader.biHeight;
  // neg. heights mean the Y axis is inverted and we don't handle that case
  NS_ENSURE_TRUE(height > 0, NS_ERROR_FAILURE); 

  unsigned char * rgbData = new unsigned char[width * height * 3 /* RGB */];

  if (rgbData) {
    BYTE  * pGlobal = (BYTE *) aClipboardData;
    // Convert the clipboard image into RGB packed pixel data
    rv = ConvertColorBitMap((unsigned char *) (pGlobal + header->bmiHeader.biSize), header, rgbData);
    // if that succeeded, encode the bitmap as aMIMEFormat data. Don't return early or we risk leaking rgbData
    if (NS_SUCCEEDED(rv)) {
      nsAutoCString encoderCID(NS_LITERAL_CSTRING("@mozilla.org/image/encoder;2?type="));

      // Map image/jpg to image/jpeg (which is how the encoder is registered).
      if (strcmp(aMIMEFormat, kJPGImageMime) == 0)
        encoderCID.AppendLiteral("image/jpeg");
      else
        encoderCID.Append(aMIMEFormat);
      nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get(), &rv);
      if (NS_SUCCEEDED(rv)){
        rv = encoder->InitFromData(rgbData, 0, width, height, 3 * width /* RGB * # pixels in a row */, 
                                   imgIEncoder::INPUT_FORMAT_RGB, EmptyString());
        if (NS_SUCCEEDED(rv)) {
          encoder.forget(aInputStream);
        }
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
nsImageFromClipboard::InvertRows(unsigned char * aInitialBuffer, uint32_t aSizeOfBuffer, uint32_t aNumBytesPerRow)
{
  if (!aNumBytesPerRow) 
    return; 

  uint32_t numRows = aSizeOfBuffer / aNumBytesPerRow;
  unsigned char * row = new unsigned char[aNumBytesPerRow];

  uint32_t currentRow = 0;
  uint32_t lastRow = (numRows - 1) * aNumBytesPerRow;
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
  uint8_t bitCount = pBitMapInfo->bmiHeader.biBitCount; 
  uint32_t imageSize = pBitMapInfo->bmiHeader.biSizeImage; // may be zero for BI_RGB bitmaps which means we need to calculate by hand
  uint32_t bytesPerPixel = bitCount / 8;
  
  if (bitCount <= 4)
    bytesPerPixel = 1;

  // rows are DWORD aligned. Calculate how many real bytes are in each row in the bitmap. This number won't 
  // correspond to biWidth.
  uint32_t rowSize = (bitCount * pBitMapInfo->bmiHeader.biWidth + 7) / 8; // +7 to round up
  if (rowSize % 4)
    rowSize += (4 - (rowSize % 4)); // Pad to DWORD Boundary
  
  // if our buffer includes a color map, skip over it 
  if (bitCount <= 8)
  {
    int32_t bytesToSkip = (pBitMapInfo->bmiHeader.biClrUsed ? pBitMapInfo->bmiHeader.biClrUsed : (1 << bitCount) ) * sizeof(RGBQUAD);
    aInputBuffer +=  bytesToSkip;
  }

  bitFields colorMasks; // only used if biCompression == BI_BITFIELDS

  if (pBitMapInfo->bmiHeader.biCompression == BI_BITFIELDS)
  {
    // color table consists of 3 DWORDS containing the color masks...
    colorMasks.red = (*((uint32_t*)&(pBitMapInfo->bmiColors[0]))); 
    colorMasks.green = (*((uint32_t*)&(pBitMapInfo->bmiColors[1]))); 
    colorMasks.blue = (*((uint32_t*)&(pBitMapInfo->bmiColors[2]))); 
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
    uint32_t index = 0;
    uint32_t writeIndex = 0;
     
    unsigned char redValue, greenValue, blueValue;
    uint8_t colorTableEntry = 0;
    int8_t bit; // used for grayscale bitmaps where each bit is a pixel
    uint32_t numPixelsLeftInRow = pBitMapInfo->bmiHeader.biWidth; // how many more pixels do we still need to read for the current row
    uint32_t pos = 0;

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
            uint16_t num = 0;
            num = (uint8_t) aInputBuffer[index+1];
            num <<= 8;
            num |= (uint8_t) aInputBuffer[index];

            redValue = ((uint32_t) (((float)(num & 0xf800) / 0xf800) * 0xFF0000) & 0xFF0000)>> 16;
            greenValue =  ((uint32_t)(((float)(num & 0x07E0) / 0x07E0) * 0x00FF00) & 0x00FF00)>> 8;
            blueValue =  ((uint32_t)(((float)(num & 0x001F) / 0x001F) * 0x0000FF) & 0x0000FF);

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
            uint32_t val = *((uint32_t*) (aInputBuffer + index) );
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

void nsImageFromClipboard::CalcBitmask(uint32_t aMask, uint8_t& aBegin, uint8_t& aLength)
{
  // find the rightmost 1
  uint8_t pos;
  bool started = false;
  aBegin = aLength = 0;
  for (pos = 0; pos <= 31; pos++) 
  {
    if (!started && (aMask & (1 << pos))) 
    {
      aBegin = pos;
      started = true;
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
  uint8_t begin, length;
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
