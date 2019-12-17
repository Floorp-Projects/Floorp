/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DocumentChannelParent_h
#define mozilla_net_DocumentChannelParent_h

#include "mozilla/net/PDocumentChannelParent.h"
#include "mozilla/net/DocumentLoadListener.h"

namespace mozilla {
namespace net {

/**
 * An implementation of ADocumentChannelBridge that forwards all changes across
 * to DocumentChannelChild, the nsIChannel implementation owned by a content
 * process docshell.
 */
class DocumentChannelParent final : public ADocumentChannelBridge,
                                    public PDocumentChannelParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(DocumentChannelParent, override);

  explicit DocumentChannelParent(const dom::PBrowserOrId& aIframeEmbedding,
                                 nsILoadContext* aLoadContext,
                                 PBOverrideStatus aOverrideStatus);

  bool Init(const DocumentChannelCreationArgs& aArgs);

  // PDocumentChannelParent
  bool RecvCancel(const nsresult& aStatus) {
    mParent->Cancel(aStatus);
    return true;
  }
  void ActorDestroy(ActorDestroyReason aWhy) override {
    mParent->DocumentChannelBridgeDisconnected();
    mParent = nullptr;
  }

 private:
  // DocumentChannelListener
  void DisconnectChildListeners(nsresult aStatus) override {
    if (CanSend()) {
      Unused << SendDisconnectChildListeners(aStatus);
    }
  }

  void Delete() override {
    if (CanSend()) {
      Unused << SendDeleteSelf();
    }
  }

  RefPtr<PDocumentChannelParent::ConfirmRedirectPromise> ConfirmRedirect(
      const LoadInfoArgs& aLoadInfo, nsIURI* aNewURI) override {
    return SendConfirmRedirect(aLoadInfo, aNewURI);
  }

  virtual ProcessId OtherPid() const override { return IProtocol::OtherPid(); }

  virtual bool AttachStreamFilter(
      Endpoint<mozilla::extensions::PStreamFilterParent>&& aEndpoint) override {
    return SendAttachStreamFilter(std::move(aEndpoint));
  }

  RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
  RedirectToRealChannel(uint32_t aRedirectFlags, uint32_t aLoadFlags) override;

  ~DocumentChannelParent();

  RefPtr<DocumentLoadListener> mParent;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DocumentChannelParent_h
