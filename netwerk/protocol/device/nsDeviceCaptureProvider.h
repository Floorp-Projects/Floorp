/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceCaptureProvider_h_
#define nsDeviceCaptureProvider_h_

#include "nsIInputStream.h"

struct nsCaptureParams {
  bool captureAudio;
  bool captureVideo;
  PRUint32 frameRate;
  PRUint32 frameLimit;
  PRUint32 timeLimit;
  PRUint32 width;
  PRUint32 height;
  PRUint32 bpp;
  PRUint32 camera;
};

class nsDeviceCaptureProvider : public nsISupports
{
public:
  virtual nsresult Init(nsACString& aContentType,
                        nsCaptureParams* aParams,
                        nsIInputStream** aStream) = 0;
};

#endif
