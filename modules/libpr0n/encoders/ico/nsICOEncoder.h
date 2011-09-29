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
 * The Original Code is an implementation of an icon encoder.
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

#include "mozilla/ReentrantMonitor.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "ICOFileHeaders.h"

class nsBMPEncoder;
class nsPNGEncoder;

#define NS_ICOENCODER_CID \
{ /*92AE3AB2-8968-41B1-8709-B6123BCEAF21 */          \
     0x92ae3ab2,                                     \
     0x8968,                                         \
     0x41b1,                                         \
    {0x87, 0x09, 0xb6, 0x12, 0x3b, 0Xce, 0xaf, 0x21} \
}

// Provides ICO encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.

class nsICOEncoder : public imgIEncoder
{
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsICOEncoder();
  ~nsICOEncoder();
  
  // Obtains the width of the icon directory entry
  PRUint32 GetRealWidth() const
  {
    return mICODirEntry.mWidth == 0 ? 256 : mICODirEntry.mWidth; 
  }

  // Obtains the height of the icon directory entry
  PRUint32 GetRealHeight() const
  {
    return mICODirEntry.mHeight == 0 ? 256 : mICODirEntry.mHeight; 
  }

protected:
  nsresult ParseOptions(const nsAString& aOptions, PRUint32* bpp, 
                        bool *usePNG);
  void NotifyListener();

  // Initializes the icon file header mICOFileHeader
  void InitFileHeader();
  // Initializes the icon directory info header mICODirEntry
  void InitInfoHeader(PRUint32 aBPP, PRUint8 aWidth, PRUint8 aHeight);
  // Encodes the icon file header mICOFileHeader
  void EncodeFileHeader();
  // Encodes the icon directory info header mICODirEntry
  void EncodeInfoHeader();
  // Obtains the current offset filled up to for the image buffer
  inline PRInt32 GetCurrentImageBufferOffset()
  {
    return static_cast<PRInt32>(mImageBufferCurr - mImageBufferStart);
  }

  // Holds either a PNG or a BMP depending on the encoding options specified
  // or if no encoding options specified will use the default (PNG)
  nsCOMPtr<imgIEncoder> mContainedEncoder;

  // These headers will always contain endian independent stuff.
  // Don't trust the width and height of mICODirEntry directly,
  // instead use the accessors GetRealWidth() and GetRealHeight().
  mozilla::imagelib::IconFileHeader mICOFileHeader;
  mozilla::imagelib::IconDirEntry mICODirEntry;

  // Keeps track of the start of the image buffer
  PRUint8* mImageBufferStart;
  // Keeps track of the current position in the image buffer
  PRUint8* mImageBufferCurr;
  // Keeps track of the image buffer size
  PRUint32 mImageBufferSize;
  // Keeps track of the number of bytes in the image buffer which are read
  PRUint32 mImageBufferReadPoint;
  // Stores PR_TRUE if the image is done being encoded  
  bool mFinished;
  // Stores PR_TRUE if the contained image is a PNG
  bool mUsePNG;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  PRUint32 mNotifyThreshold;
};
