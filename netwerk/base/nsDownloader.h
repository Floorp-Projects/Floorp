/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDownloader_h__
#define nsDownloader_h__

#include "nsIDownloader.h"
#include "nsCOMPtr.h"

class nsIFile;
class nsIOutputStream;

class nsDownloader : public nsIDownloader {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsDownloader() : mLocationIsTemp(false) {}

 protected:
  virtual ~nsDownloader();

  static nsresult ConsumeData(nsIInputStream* in, void* closure,
                              const char* fromRawSegment, uint32_t toOffset,
                              uint32_t count, uint32_t* writeCount);

  nsCOMPtr<nsIDownloadObserver> mObserver;
  nsCOMPtr<nsIFile> mLocation;
  nsCOMPtr<nsIOutputStream> mSink;
  bool mLocationIsTemp;
};

#endif  // nsDownloader_h__
