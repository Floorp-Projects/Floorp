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
 * Copyright (C) 2002 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef nsJPEGDecoder_h__
#define nsJPEGDecoder_h__

#include "imgIDecoder.h"

#include "nsCOMPtr.h"

#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "imgIDecoderObserver.h"
#include "imgILoad.h"
#include "nsIInputStream.h"
#include "nsIPipe.h"

#include "ijl.h"

#define NS_JPEGDECODER_CID \
{ /* 7e309b88-1dd2-11b2-815c-e8f8b7653393 */         \
     0x7e309b88,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x81, 0x5c, 0xe8, 0xf8, 0xb7, 0x65, 0x33, 0x93} \
}

class nsJPEGDecoder : public imgIDecoder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODER

  nsJPEGDecoder();
  virtual ~nsJPEGDecoder();

public:
  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<imgILoad> mImageLoad;
  nsCOMPtr<gfxIImageFrame> mFrame;

  nsCOMPtr<imgIDecoderObserver> mObserver;

  JPEG_CORE_PROPERTIES mJPEG;
};

#endif // nsJPEGDecoder_h__
