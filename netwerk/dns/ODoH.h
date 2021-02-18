/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ODoH_h
#define mozilla_net_ODoH_h

#include "TRR.h"

namespace mozilla {
namespace net {

class ODoH final : public TRR {
 public:
  explicit ODoH(AHostResolver* aResolver, nsHostRecord* aRec,
                enum TrrType aType)
      : TRR(aResolver, aRec, aType) {}
  // when following CNAMEs
  explicit ODoH(AHostResolver* aResolver, nsHostRecord* aRec, nsCString& aHost,
                enum TrrType& aType, unsigned int aLoopCount, bool aPB)
      : TRR(aResolver, aRec, aHost, aType, aLoopCount, aPB) {}
  NS_IMETHOD Run() override;
  // ODoH should not support push.
  NS_IMETHOD GetInterface(const nsIID&, void**) override {
    return NS_ERROR_NO_INTERFACE;
  }

 protected:
  virtual ~ODoH() = default;
  virtual DNSPacket* GetOrCreateDNSPacket() override;
  virtual nsresult CreateQueryURI(nsIURI** aOutURI) override;
  virtual const char* ContentType() const override {
    return "application/oblivious-dns-message";
  }
  virtual DNSResolverType ResolverType() const override {
    return DNSResolverType::ODoH;
  }
  virtual bool MaybeBlockRequest() override {
    // TODO: check excluded list
    return false;
  };
  virtual void RecordProcessingTime(nsIChannel* aChannel) override {
    // TODO: record processing time for ODoH.
  }
  virtual void ReportStatus(nsresult aStatusCode) override {
    // TODO: record status in ODoHService.
  }
  virtual void HandleTimeout() override;
  virtual void HandleEncodeError(nsresult aStatusCode) override;
  virtual void HandleDecodeError(nsresult aStatusCode) override;
};

}  // namespace net
}  // namespace mozilla

#endif
