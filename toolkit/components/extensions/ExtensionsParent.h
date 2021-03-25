/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionsParent_h
#define mozilla_extensions_ExtensionsParent_h

#include "mozilla/extensions/PExtensionsParent.h"
#include "nsISupportsImpl.h"

class extIWebNavigation;

namespace mozilla {
namespace extensions {

class ExtensionsParent final : public PExtensionsParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(ExtensionsParent, final)

  ExtensionsParent();

  ipc::IPCResult RecvDocumentChange(MaybeDiscardedBrowsingContext&& aBC,
                                    FrameTransitionData&& aTransitionData,
                                    nsIURI* aLocation);

  ipc::IPCResult RecvHistoryChange(MaybeDiscardedBrowsingContext&& aBC,
                                   FrameTransitionData&& aTransitionData,
                                   nsIURI* aLocation,
                                   bool aIsHistoryStateUpdated,
                                   bool aIsReferenceFragmentUpdated);

  ipc::IPCResult RecvStateChange(MaybeDiscardedBrowsingContext&& aBC,
                                 nsIURI* aRequestURI, nsresult aStatus,
                                 uint32_t aStateFlags);

  ipc::IPCResult RecvCreatedNavigationTarget(
      MaybeDiscardedBrowsingContext&& aBC,
      MaybeDiscardedBrowsingContext&& aSourceBC, const nsCString& aURI);

  ipc::IPCResult RecvDOMContentLoaded(MaybeDiscardedBrowsingContext&& aBC,
                                      nsIURI* aDocumentURI);

 private:
  ~ExtensionsParent();

  extIWebNavigation* WebNavigation();

  nsCOMPtr<extIWebNavigation> mWebNavigation;

 protected:
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionsParent_h
