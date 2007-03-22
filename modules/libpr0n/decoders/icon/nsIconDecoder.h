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
 *   Scott MacGregor <mscott@netscape.com>
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
//     The remaining bytes contain the icon data, 4 bytes per pixel, in
//       ARGB order (platform endianness, A in highest bits, B in lowest
//       bits), row-primary, top-to-bottom, left-to-right, with
//       premultiplied alpha.
//
//
//////////////////////////////////////////////////////////////////////////////////////////////

class nsIconDecoder : public imgIDecoder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODER

  nsIconDecoder();
  virtual ~nsIconDecoder();

private:
  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<gfxIImageFrame> mFrame;
  nsCOMPtr<imgIDecoderObserver> mObserver; // this is just qi'd from mRequest for speed
};

#endif // nsIconDecoder_h__
