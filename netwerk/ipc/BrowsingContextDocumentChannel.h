/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_BrowsingContextDocumentChannel_h
#define mozilla_net_BrowsingContextDocumentChannel_h

#include "mozilla/net/ADocumentChannelBridge.h"

class nsIBrowser;

namespace mozilla {
namespace dom {
class CanonicalBrowsingContext;
}  // namespace dom
namespace net {

class DocumentLoadListener;

// An ADocumentChannelBridge implementation for loads initiated by
// a CanonicalBrowsingContext, and don't start from a docshell.
class BrowsingContextDocumentChannel : public ADocumentChannelBridge {
 public:
  NS_INLINE_DECL_REFCOUNTING(BrowsingContextDocumentChannel, override)

  explicit BrowsingContextDocumentChannel(
      dom::CanonicalBrowsingContext* aBrowsingContext);

  // Creates the channel, and then calls AsyncOpen on it.
  bool Open(nsDocShellLoadState* aLoadState, uint64_t aOuterWindowId,
            bool aSetNavigating);

  void DisconnectChildListeners(nsresult aStatus, nsresult aLoadGroupStatus,
                                bool aSwitchingToNewProcess) override;

  void Delete() override;

  // We're not connected to an originating docshell, so we can't
  // support RedirectToRealChannel to issue a redirect on one.
  bool SupportsRedirectToRealChannel() override { return false; }

  RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
  RedirectToRealChannel(
      nsTArray<ipc::Endpoint<extensions::PStreamFilterParent>>&&
          aStreamFilterEndpoints,
      uint32_t aRedirectFlags, uint32_t aLoadFlags) override;

  base::ProcessId OtherPid() const override;

 private:
  virtual ~BrowsingContextDocumentChannel() = default;

  void FireStateChange(uint32_t aStateFlags, nsresult aStatus);
  void SetNavigating(bool aNavigating);

  already_AddRefed<nsIBrowser> GetBrowser();

  RefPtr<DocumentLoadListener> mParent;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_BrowsingContextDocumentChannel_h
