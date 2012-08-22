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
  uint32_t frameRate;
  uint32_t frameLimit;
  uint32_t timeLimit;
  uint32_t width;
  uint32_t height;
  uint32_t bpp;
  uint32_t camera;
};

class nsDeviceCaptureProvider : public nsISupports
{
public:
  virtual nsresult Init(nsACString& aContentType,
                        nsCaptureParams* aParams,
                        nsIInputStream** aStream) = 0;
};

#endif
