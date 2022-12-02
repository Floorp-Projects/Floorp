/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DocumentChannelParent_h
#define mozilla_net_DocumentChannelParent_h

#include "mozilla/net/DocumentLoadListener.h"
#include "mozilla/net/PDocumentChannelParent.h"

namespace mozilla {
namespace dom {
class CanonicalBrowsingContext;
}
namespace net {

/**
 * An actor that forwards all changes across to DocumentChannelChild, the
 * nsIChannel implementation owned by a content process docshell.
 */
class DocumentChannelParent final
    : public PDocumentChannelParent,
      public DocumentLoadListener::ObjectUpgradeHandler {
 public:
  NS_INLINE_DECL_REFCOUNTING(DocumentChannelParent, override);

  explicit DocumentChannelParent();

  bool Init(dom::CanonicalBrowsingContext* aContext,
            const DocumentChannelCreationArgs& aArgs);

  // PDocumentChannelParent
  ipc::IPCResult RecvCancel(const nsresult& aStatus, const nsCString& aReason) {
    if (mDocumentLoadListener) {
      mDocumentLoadListener->Cancel(aStatus, aReason);
    }
    return IPC_OK();
  }
  void ActorDestroy(ActorDestroyReason aWhy) override {
    if (mDocumentLoadListener) {
      mDocumentLoadListener->Cancel(NS_BINDING_ABORTED,
                                    "DocumentChannelParent::ActorDestroy"_ns);
    }
  }

 private:
  RefPtr<ObjectUpgradePromise> UpgradeObjectLoad() override;

  RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
  RedirectToRealChannel(
      nsTArray<ipc::Endpoint<extensions::PStreamFilterParent>>&&
          aStreamFilterEndpoints,
      uint32_t aRedirectFlags, uint32_t aLoadFlags);

  virtual ~DocumentChannelParent();

  RefPtr<DocumentLoadListener> mDocumentLoadListener;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DocumentChannelParent_h
