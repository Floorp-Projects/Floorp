/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef nsPNGDecoder_h__
#define nsPNGDecoder_h__

#include "imgIDecoder.h"

#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"
#include "imgILoad.h"


#include "nsCOMPtr.h"

#include "png.h"

#define NS_PNGDECODER_CID \
{ /* 36fa00c2-1dd2-11b2-be07-d16eeb4c50ed */         \
     0x36fa00c2,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0xbe, 0x07, 0xd1, 0x6e, 0xeb, 0x4c, 0x50, 0xed} \
}

class nsPNGDecoder : public imgIDecoder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODER

  nsPNGDecoder();
  virtual ~nsPNGDecoder();

  inline nsresult ProcessData(unsigned char *data, PRUint32 count, PRUint32 *readCount);

public:
  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<gfxIImageFrame> mFrame;
  nsCOMPtr<imgILoad> mImageLoad;
  nsCOMPtr<imgIDecoderObserver> mObserver; // this is just qi'd from mRequest for speed

  png_structp mPNG;
  png_infop mInfo;
  PRUint8 *colorLine, *alphaLine;
  PRUint8 *interlacebuf;
  PRUint32 ibpr;
};

#endif // nsPNGDecoder_h__
