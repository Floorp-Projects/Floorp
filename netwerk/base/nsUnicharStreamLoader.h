/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicharStreamLoader_h__
#define nsUnicharStreamLoader_h__

#include "nsIChannel.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIUnicodeDecoder.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsIInputStream;

class nsUnicharStreamLoader : public nsIUnicharStreamLoader
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIUNICHARSTREAMLOADER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsUnicharStreamLoader() {}

  static nsresult Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
  virtual ~nsUnicharStreamLoader() {}

  nsresult DetermineCharset();

  /**
   * callback method used for ReadSegments
   */
  static nsresult WriteSegmentFun(nsIInputStream *, void *, const char *,
                                  uint32_t, uint32_t, uint32_t *);

  nsCOMPtr<nsIUnicharStreamLoaderObserver> mObserver;
  nsCOMPtr<nsIUnicodeDecoder>              mDecoder;
  nsCOMPtr<nsISupports>                    mContext;
  nsCOMPtr<nsIChannel>                     mChannel;
  nsCString                                mCharset;

  // This holds the first up-to-512 bytes of the raw stream.
  // It will be passed to the OnDetermineCharset callback.
  nsCString                                mRawData;

  // This holds the complete contents of the stream so far, after
  // decoding to UTF-16.  It will be passed to the OnStreamComplete
  // callback.
  nsString                                 mBuffer;
};

#endif // nsUnicharStreamLoader_h__
