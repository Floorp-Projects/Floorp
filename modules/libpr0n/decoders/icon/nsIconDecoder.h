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
 *   Scott MacGregor <mscott@netscape.com>
 */

#ifndef nsIconDecoder_h__
#define nsIconDecoder_h__

#include "imgIDecoder.h"

#include "nsCOMPtr.h"

#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"

#define NS_ICONDECODER_CID                           \
{ /* FFC08380-256C-11d5-9905-001083010E9B */         \
     0xffc08380,                                     \
     0x256c,                                         \
     0x11d5,                                         \
    { 0x99, 0x5, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b }   \
}


//////////////////////////////////////////////////////////////////////////////////////////////
// The icon decoder is a decoder specifically tailored for loading icons 
// from the OS. We've defined our own little format to represent these icons
// and this decoder takes that format and converts it into 24-bit RGB with alpha channel
// support. It was modeled a bit off the PPM decoder.
//
// Assumptions about the decoder:
// (1) We receive ALL of the data from the icon channel in one OnDataAvailable call. We don't
//     support multiple ODA calls yet.
// (2) the format of the incoming data is as follows:
//     The first two bytes contain the width and the height of the icon. 
//     Followed by 3 bytes per pixel for the color bitmap row after row. (for heigh * width * 3 bytes)
//     Followed by bit mask data (used for transparency on the alpha channel). 
//
//
//////////////////////////////////////////////////////////////////////////////////////////////

class nsIconDecoder : public imgIDecoder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODER
  NS_DECL_NSIOUTPUTSTREAM

  nsIconDecoder();
  virtual ~nsIconDecoder();

private:
  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<gfxIImageFrame> mFrame;
  nsCOMPtr<imgIDecoderObserver> mObserver; // this is just qi'd from mRequest for speed
};

#endif // nsIconDecoder_h__
