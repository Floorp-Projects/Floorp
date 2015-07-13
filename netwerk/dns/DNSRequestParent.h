/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DNSRequestParent_h
#define mozilla_net_DNSRequestParent_h

#include "mozilla/net/PDNSRequestParent.h"
#include "nsIDNSService.h"
#include "nsIDNSListener.h"

namespace mozilla {
namespace net {

class DNSRequestParent
  : public PDNSRequestParent
  , public nsIDNSListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  DNSRequestParent();

  void DoAsyncResolve(const nsACString  &hostname, uint32_t flags,
                      const nsACString  &networkInterface);

  // Pass args here rather than storing them in the parent; they are only
  // needed if the request is to be canceled.
  bool RecvCancelDNSRequest(const nsCString& hostName,
                            const uint32_t& flags,
                            const nsCString& networkInterface,
                            const nsresult& reason) override;
  bool Recv__delete__() override;

protected:
  virtual void ActorDestroy(ActorDestroyReason why) override;
private:
  virtual ~DNSRequestParent();

  uint32_t mFlags;
  bool mIPCClosed;  // true if IPDL channel has been closed (child crash)
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_DNSRequestParent_h
