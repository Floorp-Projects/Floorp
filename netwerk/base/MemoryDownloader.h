/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_MemoryDownloader_h__
#define mozilla_net_MemoryDownloader_h__

#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsTArray.h"

/**
 * mozilla::net::MemoryDownloader
 *
 * This class is similar to nsIDownloader, but stores the downloaded
 * stream in memory instead of a file.  Ownership of the temporary
 * memory is transferred to the observer when download is complete;
 * there is no need to retain a reference to the downloader.
 */

namespace mozilla {
namespace net {

class MemoryDownloader final : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  typedef mozilla::UniquePtr<FallibleTArray<uint8_t>> Data;

  class IObserver : public nsISupports {
  public:
    // Note: aData may be null if (and only if) aStatus indicates failure.
    virtual void OnDownloadComplete(MemoryDownloader* aDownloader,
                                    nsIRequest* aRequest,
                                    nsISupports* aCtxt,
                                    nsresult aStatus,
                                    Data aData) = 0;
  };

  explicit MemoryDownloader(IObserver* aObserver);

private:
  virtual ~MemoryDownloader() = default;

  static nsresult ConsumeData(nsIInputStream *in,
                              void           *closure,
                              const char     *fromRawSegment,
                              uint32_t        toOffset,
                              uint32_t        count,
                              uint32_t       *writeCount);

  RefPtr<IObserver> mObserver;
  Data mData;
  nsresult mStatus;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_MemoryDownloader_h__
