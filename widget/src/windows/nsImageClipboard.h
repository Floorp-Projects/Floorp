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

/* Things To Do 11/8/00

Check image metrics, can we support them? Do we need to?
Any other render format? HTML?

*/

#include "nserror.h"
#include <windows.h>

#include "nsCOMPtr.h"
#include "nsIImage.h"


//
// nsImageToClipboard
//
// A utility class that takes an nsIImage and does all the bitmap magic
// to allow us to put it on the clipboard
//
class nsImageToClipboard
{
public:
  nsImageToClipboard ( nsIImage* inImage );
  ~nsImageToClipboard();

    // Call to get the actual bits that go on the clipboard. If |nsnull|, the
    // setup operations have failed.
    //
    // NOTE: This routine owns the handle that is returned. Do not release the
    //        memory.
  nsresult GetPicture ( HANDLE* outBits ) ;

private:

    // Computes # of bytes needed by a bitmap with the specified attributes.
  PRInt32 CalcSize(PRInt32 aHeight, PRInt32 aColors, WORD aBitsPerPixel, PRInt32 aSpanBytes);
  PRInt32 CalcSpanLength(PRUint32 aWidth, PRUint32 aBitCount);

    // Do the work
  nsresult CreateFromImage ( nsIImage* inImage );

  nsCOMPtr<nsIImage> mImage;            // the image we're working with
  HANDLE             mBitmap;           // the bitmap that will go on the clipboard
  BITMAPINFOHEADER*  mHeader;           // picture format information (bitmap header).

}; // class nsImageToClipboard


//
// nsImageFromClipboard
//
// A utility class that takes a DIB from the win32 clipboard and does
// all the bitmap magic to create a nsIImage
//
class nsImageFromClipboard
{
public:
  nsImageFromClipboard ( BITMAPV4HEADER* inHeader ) ;
  ~nsImageFromClipboard ( ) ;
  
    // Retrieve the newly created image
  nsresult GetImage ( nsIImage** outImage ) ;
  
private:

  PRUint8* GetDIBBits ( ) ;

  BITMAPV4HEADER* mHeader;

}; // nsImageFromClipboard
