/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Tim Rowley.
 * Portions created by Tim Rowley are 
 * Copyright (C) 2001 Tim Rowley.  Rights Reserved.
 * 
 * Contributor(s): 
 *   Tim Rowley <tor@cs.brown.edu>
 */

#ifndef _imgContainerMNG_h_
#define _imgContainerMNG_h_

#include "imgIContainerObserver.h"
#include "imgIContainer.h"
#include "nsSize.h"
#include "nsSupportsArray.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"
#include "nsWeakReference.h"
#include "nsIInputStream.h"
#include "nsMNGDecoder.h"
#include "libmng.h"

#define NS_MNGCONTAINER_CID \
{ /* 7fc64c50-1dd2-11b2-85fe-997f8a1263d6 */         \
     0x7fc64c50,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x85, 0xfe, 0x99, 0x7f, 0x8a, 0x12, 0x63, 0xd6} \
}

class imgContainerMNG : public imgIContainer
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINER

  imgContainerMNG();
  virtual ~imgContainerMNG();

  NS_IMETHODIMP WriteMNG(nsIInputStream *inStr, PRInt32 count, PRUint32 *_retval);
  void InitMNG(nsMNGDecoder *decoder);

  nsWeakPtr                      mObserver;
  nsMNGDecoder                  *mDecoder;
  nsCOMPtr<gfxIImageFrame>       mFrame;
  nsSize                         mSize;
  PRUint16                       mAnimationMode;

  // Unfortunately we need to keep a copy of the image because partial
  // reads in necko aren't guaranteed to work (darin 5/11/2001).
  // Brute force for now, keeping the entire stream around.

  PRUint8                       *mBuffer;       // copy of image stream
  PRUint32                       mBufferEnd;    // size
  PRUint32                       mBufferPtr;    // libmng current read location

  PRUint8                       *image;         // full image buffer
  PRUint8                       *alpha;         // full alpha buffer

  mng_handle                     mHandle;       // libmng handle
//  PRUint32                       mWidth;        // width (for offset calcs)
//  PRUint32                       mHeight;
  PRUint32 mByteWidth, mByteWidthAlpha;
  nsCOMPtr<nsITimer>             mTimer;

  PRPackedBool                   mResumeNeeded; // display_resume call needed?
  PRPackedBool                   mFrozen;       // animation frozen? 
  PRPackedBool                   mErrorPending; // decode error to report?
};

#endif
