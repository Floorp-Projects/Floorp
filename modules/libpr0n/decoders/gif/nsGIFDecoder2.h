/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   Chris Saari <saari@netscape.com>
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

#ifndef _nsGIFDecoder2_h
#define _nsGIFDecoder2_h

#include "nsCOMPtr.h"
#include "imgIDecoder.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"

#include "GIF2.h"

#define NS_GIFDECODER2_CID \
{ /* 797bec5a-1dd2-11b2-a7f8-ca397e0179c4 */         \
     0x797bec5a,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0xa7, 0xf8, 0xca, 0x39, 0x7e, 0x01, 0x79, 0xc4} \
}

//////////////////////////////////////////////////////////////////////
// nsGIFDecoder2 Definition

class nsGIFDecoder2 : public imgIDecoder   
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODER

  nsGIFDecoder2();
  ~nsGIFDecoder2();
  
  nsresult ProcessData(unsigned char *data, PRUint32 count, PRUint32 *_retval);

  NS_METHOD FlushImageData();

  /* These functions will be called when the decoder has a decoded
   * rows, frame size information, etc. */

  static int BeginGIF(
    void* aClientData,
    PRUint32 aLogicalScreenWidth, 
    PRUint32 aLogicalScreenHeight,
    PRUint8  aBackgroundRGBIndex);
    
  static int EndGIF(
    void*    aClientData,
    int      aAnimationLoopCount);
  
  static int BeginImageFrame(
    void*    aClientData,
    PRUint32 aFrameNumber,   /* Frame number, 1-n */
    PRUint32 aFrameXOffset,  /* X offset in logical screen */
    PRUint32 aFrameYOffset,  /* Y offset in logical screen */
    PRUint32 aFrameWidth,    
    PRUint32 aFrameHeight);
  
  static int EndImageFrame(
    void* aClientData,
    PRUint32 aFrameNumber,
    PRUint32 aDelayTimeout);
  
  static int HaveDecodedRow(
    void* aClientData,
    PRUint8* aRowBufPtr,   /* Pointer to single scanline temporary buffer */
    int aRow,              /* Row number? */
    int aDuplicateCount,   /* Number of times to duplicate the row? */
    int aInterlacePass);

private:
  nsCOMPtr<imgIContainer> mImageContainer;
  nsCOMPtr<gfxIImageFrame> mImageFrame;
  nsCOMPtr<imgIDecoderObserver> mObserver; // this is just qi'd from mRequest for speed
  PRInt32 mCurrentRow;
  PRInt32 mLastFlushedRow;

  gif_struct *mGIFStruct;

  PRUint8 *mRGBLine;
  PRUint32 mRGBLineMaxSize;
  PRUint8 mBackgroundRGBIndex;
  PRUint8 mCurrentPass;
  PRUint8 mLastFlushedPass;
  PRPackedBool mGIFOpen;
};

void nsGifShutdown();

#endif
