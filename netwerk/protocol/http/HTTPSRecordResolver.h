/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTTPSRecordResolver_h__
#define HTTPSRecordResolver_h__

#include "nsICancelable.h"
#include "nsIDNSListener.h"
#include "nsHttpConnectionInfo.h"

namespace mozilla {
namespace net {

class nsAHttpTransaction;

// This class is the place to put some common code about fetching HTTPS RR.
class HTTPSRecordResolver : public nsIDNSListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  explicit HTTPSRecordResolver(nsAHttpTransaction* aTransaction);
  nsresult FetchHTTPSRRInternal(nsIEventTarget* aTarget,
                                nsICancelable** aDNSRequest);
  void PrefetchAddrRecord(const nsACString& aTargetName, bool aRefreshDNS);

  void Close();

 protected:
  virtual ~HTTPSRecordResolver();

 private:
  RefPtr<nsAHttpTransaction> mTransaction;
  RefPtr<nsHttpConnectionInfo> mConnInfo;
  uint32_t mCaps;
};

}  // namespace net
}  // namespace mozilla

#endif  // HTTPSRecordResolver_h__
