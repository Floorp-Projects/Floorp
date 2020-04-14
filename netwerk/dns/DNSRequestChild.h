/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DNSRequestChild_h
#define mozilla_net_DNSRequestChild_h

#include "mozilla/net/DNSRequestBase.h"
#include "mozilla/net/PDNSRequestChild.h"

namespace mozilla {
namespace net {

class DNSRequestChild final : public DNSRequestActor, public PDNSRequestChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DNSRequestChild, override)
  friend class PDNSRequestChild;

  explicit DNSRequestChild(DNSRequestBase* aRequest);

  bool CanSend() const override { return PDNSRequestChild::CanSend(); }
  DNSRequestChild* AsDNSRequestChild() override { return this; }
  DNSRequestParent* AsDNSRequestParent() override { return nullptr; }

 private:
  virtual ~DNSRequestChild() = default;

  mozilla::ipc::IPCResult RecvCancelDNSRequest(
      const nsCString& hostName, const nsCString& trrServer,
      const uint16_t& type, const OriginAttributes& originAttributes,
      const uint32_t& flags, const nsresult& reason);
  mozilla::ipc::IPCResult RecvLookupCompleted(const DNSRequestResponse& reply);
  virtual void ActorDestroy(ActorDestroyReason why) override;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DNSRequestChild_h
