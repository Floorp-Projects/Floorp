/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ClassifierDummyChannelParent_h
#define mozilla_net_ClassifierDummyChannelParent_h

#include "mozilla/net/PClassifierDummyChannelParent.h"
#include "nsISupportsImpl.h"

class nsILoadInfo;
class nsIURI;

namespace mozilla {
namespace net {

class ClassifierDummyChannelParent final
    : public PClassifierDummyChannelParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(ClassifierDummyChannelParent)

  ClassifierDummyChannelParent();

  void Init(nsIURI* aURI, nsIURI* aTopWindowURI,
            nsIPrincipal* aContentBlockingAllowListPrincipal,
            nsresult aTopWindowURIResult, nsILoadInfo* aLoadInfo);

 private:
  ~ClassifierDummyChannelParent();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  bool mIPCActive;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ClassifierDummyChannelParent_h
