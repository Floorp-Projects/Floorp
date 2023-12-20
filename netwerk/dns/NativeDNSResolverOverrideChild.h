/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NativeDNSResolverOverrideChild_h
#define mozilla_net_NativeDNSResolverOverrideChild_h

#include "mozilla/net/PNativeDNSResolverOverrideChild.h"
#include "nsINativeDNSResolverOverride.h"

namespace mozilla {
namespace net {

class NativeDNSResolverOverrideChild : public PNativeDNSResolverOverrideChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(NativeDNSResolverOverrideChild, override)

  NativeDNSResolverOverrideChild();

  mozilla::ipc::IPCResult RecvAddIPOverride(const nsCString& aHost,
                                            const nsCString& aIPLiteral);
  mozilla::ipc::IPCResult RecvAddHTTPSRecordOverride(const nsCString& aHost,
                                                     nsTArray<uint8_t>&& aData);
  mozilla::ipc::IPCResult RecvSetCnameOverride(const nsCString& aHost,
                                               const nsCString& aCNAME);
  mozilla::ipc::IPCResult RecvClearHostOverride(const nsCString& aHost);
  mozilla::ipc::IPCResult RecvClearOverrides();

 private:
  virtual ~NativeDNSResolverOverrideChild() = default;

  nsCOMPtr<nsINativeDNSResolverOverride> mOverrideService;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_NativeDNSResolverOverrideChild_h
