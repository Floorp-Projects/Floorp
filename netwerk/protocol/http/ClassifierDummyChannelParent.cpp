/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClassifierDummyChannelParent.h"
#include "mozilla/net/AsyncUrlChannelClassifier.h"
#include "mozilla/Unused.h"
#include "nsIChannel.h"
#include "nsIPrincipal.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

ClassifierDummyChannelParent::ClassifierDummyChannelParent()
    : mIPCActive(true) {}

ClassifierDummyChannelParent::~ClassifierDummyChannelParent() = default;

void ClassifierDummyChannelParent::Init(
    nsIURI* aURI, nsIURI* aTopWindowURI,
    nsIPrincipal* aContentBlockingAllowListPrincipal,
    nsresult aTopWindowURIResult, nsILoadInfo* aLoadInfo) {
  MOZ_ASSERT(mIPCActive);

  RefPtr<ClassifierDummyChannelParent> self = this;
  auto onExit =
      MakeScopeExit([self] { Unused << Send__delete__(self, false); });

  if (!aURI) {
    return;
  }

  RefPtr<ClassifierDummyChannel> channel = new ClassifierDummyChannel(
      aURI, aTopWindowURI, aContentBlockingAllowListPrincipal,
      aTopWindowURIResult, aLoadInfo);

  bool willCallback = NS_SUCCEEDED(AsyncUrlChannelClassifier::CheckChannel(
      channel, [self = std::move(self), channel]() {
        if (self->mIPCActive) {
          Unused << Send__delete__(self, channel->ClassificationFlags());
        }
      }));

  if (willCallback) {
    onExit.release();
  }
}

void ClassifierDummyChannelParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCActive = false;
}

}  // namespace net
}  // namespace mozilla
