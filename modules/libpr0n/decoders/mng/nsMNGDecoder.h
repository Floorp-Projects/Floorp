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

#ifndef _nsMNGDecoder_h_
#define _nsMNGDecoder_h_

#include "nsCOMPtr.h"
#include "imgIDecoder.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"
#include "imgIRequest.h"
#include "nsWeakReference.h"

#define NS_MNGDECODER_CID \
{ /* d407782c-1dd1-11b2-9b49-dbe684d09cd8 */         \
     0xd407782c,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0x9b, 0x49, 0xdb, 0xe6, 0x84, 0xd0, 0x9c, 0xd8} \
}

//////////////////////////////////////////////////////////////////////
// MNG Decoder Definition

class nsMNGDecoder : public imgIDecoder   
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODER
  NS_DECL_NSIOUTPUTSTREAM
  
  nsMNGDecoder();
  virtual ~nsMNGDecoder();

  nsCOMPtr<imgIContainer> mImageContainer;
  nsCOMPtr<gfxIImageFrame> mImageFrame;
  nsCOMPtr<imgIDecoderObserver> mObserver; // this is just qi'd from mRequest for speed
};
  
#endif
