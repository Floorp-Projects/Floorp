/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsImageClipboard_h
#define nsImageClipboard_h

/* Things To Do 11/8/00

Check image metrics, can we support them? Do we need to?
Any other render format? HTML?

*/

#include "nsError.h"
#include <windows.h>

#include "nsCOMPtr.h"
#include "imgIContainer.h"
#include "nsIInputStream.h"


//
// nsImageToClipboard
//
// A utility class that takes an imgIContainer and does all the bitmap magic
// to allow us to put it on the clipboard
//
class nsImageToClipboard
{
public:
  explicit nsImageToClipboard(imgIContainer* aInImage, bool aWantDIBV5 = true);
  ~nsImageToClipboard();

    // Call to get the actual bits that go on the clipboard. If |nullptr|, the
    // setup operations have failed.
    //
    // NOTE: The caller owns the handle and must delete it with ::GlobalRelease()
  nsresult GetPicture ( HANDLE* outBits ) ;

private:

    // Computes # of bytes needed by a bitmap with the specified attributes.
  int32_t CalcSize(int32_t aHeight, int32_t aColors, WORD aBitsPerPixel, int32_t aSpanBytes);
  int32_t CalcSpanLength(uint32_t aWidth, uint32_t aBitCount);

    // Do the work
  nsresult CreateFromImage ( imgIContainer* inImage, HANDLE* outBitmap );

  nsCOMPtr<imgIContainer> mImage;            // the image we're working with
  bool mWantDIBV5;

}; // class nsImageToClipboard


struct bitFields {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint8_t redLeftShift;
    uint8_t redRightShift;
    uint8_t greenLeftShift;
    uint8_t greenRightShift;
    uint8_t blueLeftShift;
    uint8_t blueRightShift;
};

//
// nsImageFromClipboard
//
// A utility class that takes a DIB from the win32 clipboard and does
// all the bitmap magic to convert it to a PNG or a JPEG in the form of a nsIInputStream
//
class nsImageFromClipboard
{
public:
  nsImageFromClipboard () ;
  ~nsImageFromClipboard ( ) ;
  
    // Retrieve the newly created image
  nsresult GetEncodedImageStream (unsigned char * aClipboardData, const char * aMIMEFormat, nsIInputStream** outImage);

private:

  void InvertRows(unsigned char * aInitialBuffer, uint32_t aSizeOfBuffer, uint32_t aNumBytesPerRow);
  nsresult ConvertColorBitMap(unsigned char * aInputBuffer, PBITMAPINFO pBitMapInfo, unsigned char * aOutBuffer);
  void CalcBitmask(uint32_t aMask, uint8_t& aBegin, uint8_t& aLength);
  void CalcBitShift(bitFields * aColorMask);

}; // nsImageFromClipboard

#endif
