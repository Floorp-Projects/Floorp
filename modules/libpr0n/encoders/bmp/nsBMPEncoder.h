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
 * The Original Code is an implementation of a bitmap encoder.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian R. Bondy <netzen@gmail.com>
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

#include "imgIEncoder.h"
#include "BMPFileHeaders.h"

#include "mozilla/ReentrantMonitor.h"

#include "nsCOMPtr.h"

#define NS_BMPENCODER_CID \
{ /* 13a5320c-4c91-4FA4-bd16-b081a3ba8c0b */         \
     0x13a5320c,                                     \
     0x4c91,                                         \
     0x4fa4,                                         \
    {0xbd, 0x16, 0xb0, 0x81, 0xa3, 0Xba, 0x8c, 0x0b} \
}

// Provides BMP encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.

class nsBMPEncoder : public imgIEncoder
{
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsBMPEncoder();
  ~nsBMPEncoder();

protected:
  // See InitData in the cpp for valid parse options
  nsresult ParseOptions(const nsAString& aOptions, PRUint32* bpp);
  // Obtains data with no alpha in machine-independent byte order
  void ConvertHostARGBRow(const PRUint8* aSrc, PRUint8* aDest,
                          PRUint32 aPixelWidth);
  // Strips the alpha from aSrc and puts it in aDest
  void StripAlpha(const PRUint8* aSrc, PRUint8* aDest,
                  PRUint32 aPixelWidth);
  // Thread safe notify listener
  void NotifyListener();

  // Initializes the bitmap file header member mBMPFileHeader
  void InitFileHeader(PRUint32 aBPP, PRUint32 aWidth, PRUint32 aHeight);
  // Initializes the bitmap info header member mBMPInfoHeader
  void InitInfoHeader(PRUint32 aBPP, PRUint32 aWidth, PRUint32 aHeight);
  // Encodes the bitmap file header member mBMPFileHeader
  void EncodeFileHeader();
  // Encodes the bitmap info header member mBMPInfoHeader
  void EncodeInfoHeader();
  // Encodes a row of image data which does not have alpha data
  void EncodeImageDataRow24(const PRUint8* aData);
  // Encodes a row of image data which does have alpha data
  void EncodeImageDataRow32(const PRUint8* aData);
  // Obtains the current offset filled up to for the image buffer
  inline PRInt32 GetCurrentImageBufferOffset()
  {
    return static_cast<PRInt32>(mImageBufferCurr - mImageBufferStart);
  }

  // These headers will always contain endian independent stuff 
  // They store the BMP headers which will be encoded
  mozilla::imagelib::BMPFILEHEADER mBMPFileHeader;
  mozilla::imagelib::BMPINFOHEADER mBMPInfoHeader;

  // Keeps track of the start of the image buffer
  PRUint8* mImageBufferStart;
  // Keeps track of the current position in the image buffer
  PRUint8* mImageBufferCurr;
  // Keeps track of the image buffer size
  PRUint32 mImageBufferSize;
  // Keeps track of the number of bytes in the image buffer which are read
  PRUint32 mImageBufferReadPoint;
  // Stores PR_TRUE if the image is done being encoded
  PRPackedBool mFinished;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  PRUint32 mNotifyThreshold;
};
