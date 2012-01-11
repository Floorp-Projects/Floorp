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

    // Call to get the actual bits that go on the clipboard. If |nsnull|, the
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
