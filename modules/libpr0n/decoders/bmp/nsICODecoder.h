/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
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
 * The Original Code is the Mozilla ICO Decoder.
 *
 * The Initial Developer of the Original Code is
 * Netscape.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com> (Original Author)
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


#ifndef _nsICODecoder_h
#define _nsICODecoder_h

#include "nsCOMPtr.h"
#include "imgIDecoder.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"
#include "nsBMPDecoder.h"

// {CB3EDE1A-0FA5-4e27-AAFE-0F7801E5A1F1}
#define NS_ICODECODER_CID \
{ 0xcb3ede1a, 0xfa5, 0x4e27, { 0xaa, 0xfe, 0xf, 0x78, 0x1, 0xe5, 0xa1, 0xf1 } }

#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
#define GFXFORMATALPHA8 gfxIFormats::BGR_A8
#else
#define USE_RGBA1
#define GFXFORMATALPHA8 gfxIFormats::RGB_A8
#endif

struct IconDirEntry
{
  PRUint8   mWidth;
  PRUint8   mHeight;
  PRUint8   mColorCount;
  PRUint8   mReserved;
  union {
    PRUint16 mPlanes;   // ICO
    PRUint16 mXHotspot; // CUR
  };
  union {
    PRUint16 mBitCount; // ICO
    PRUint16 mYHotspot; // CUR
  };
  PRUint32  mBytesInRes;
  PRUint32  mImageOffset;
};

class nsICODecoder : public imgIDecoder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODER

  nsICODecoder();
  virtual ~nsICODecoder();

private:
  /** Callback for ReadSegments to avoid copying the data */
  static NS_METHOD ReadSegCb(nsIInputStream* aIn, void* aClosure,
                             const char* aFromRawSegment, PRUint32 aToOffset,
                             PRUint32 aCount, PRUint32 *aWriteCount);

  // Private helper methods
  nsresult ProcessData(const char* aBuffer, PRUint32 aCount);
  void ProcessDirEntry(IconDirEntry& aTarget);
  void ProcessInfoHeader();

  nsresult SetImageData();

  PRUint32 CalcAlphaRowSize();

private:
  nsCOMPtr<imgIDecoderObserver> mObserver;
  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<gfxIImageFrame> mFrame;
  
  PRUint32 mPos;
  PRUint16 mNumIcons;
  PRUint16 mCurrIcon;
  PRUint32 mImageOffset;

  char mDirEntryArray[16];
  IconDirEntry mDirEntry;

  char mBIHraw[40];
  BMPINFOHEADER mBIH;

  PRUint32 mNumColors;
  colorTable* mColors;

  PRUint8* mRow; // Holds one raw line of the image
  PRUint32 mRowBytes; // How many bytes of the row were already received
  PRInt32 mCurLine;

  nsresult mStatus;

  PRUint8* mDecodedBuffer;

  PRUint8 mHaveAlphaData;
  PRPackedBool mIsCursor;
  PRPackedBool mDecodingAndMask;
};


#endif
