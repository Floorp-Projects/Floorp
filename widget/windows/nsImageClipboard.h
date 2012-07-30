/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  nsImageToClipboard ( imgIContainer* inImage );
  ~nsImageToClipboard();

    // Call to get the actual bits that go on the clipboard. If |nullptr|, the
    // setup operations have failed.
    //
    // NOTE: The caller owns the handle and must delete it with ::GlobalRelease()
  nsresult GetPicture ( HANDLE* outBits ) ;

private:

    // Computes # of bytes needed by a bitmap with the specified attributes.
  PRInt32 CalcSize(PRInt32 aHeight, PRInt32 aColors, WORD aBitsPerPixel, PRInt32 aSpanBytes);
  PRInt32 CalcSpanLength(PRUint32 aWidth, PRUint32 aBitCount);

    // Do the work
  nsresult CreateFromImage ( imgIContainer* inImage, HANDLE* outBitmap );

  nsCOMPtr<imgIContainer> mImage;            // the image we're working with

}; // class nsImageToClipboard


struct bitFields {
    PRUint32 red;
    PRUint32 green;
    PRUint32 blue;
    PRUint8 redLeftShift;
    PRUint8 redRightShift;
    PRUint8 greenLeftShift;
    PRUint8 greenRightShift;
    PRUint8 blueLeftShift;
    PRUint8 blueRightShift;
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

  void InvertRows(unsigned char * aInitialBuffer, PRUint32 aSizeOfBuffer, PRUint32 aNumBytesPerRow);
  nsresult ConvertColorBitMap(unsigned char * aInputBuffer, PBITMAPINFO pBitMapInfo, unsigned char * aOutBuffer);
  void CalcBitmask(PRUint32 aMask, PRUint8& aBegin, PRUint8& aLength);
  void CalcBitShift(bitFields * aColorMask);

}; // nsImageFromClipboard
