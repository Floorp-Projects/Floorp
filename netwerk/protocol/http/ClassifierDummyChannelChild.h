/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ClassifierDummyChannelChild_h
#define mozilla_net_ClassifierDummyChannelChild_h

#include "mozilla/net/PClassifierDummyChannelChild.h"
#include "nsCOMPtr.h"

#include <functional>

class nsIHttpChannel;
class nsIURI;

namespace mozilla {
namespace net {

class ClassifierDummyChannelChild final : public PClassifierDummyChannelChild {
  friend class PClassifierDummyChannelChild;

 public:
  static bool Create(nsIHttpChannel* aChannel, nsIURI* aURI,
                     const std::function<void(bool)>& aCallback);

  // Used by PNeckoChild only!
  ClassifierDummyChannelChild();
  ~ClassifierDummyChannelChild();

 private:
  void Initialize(nsIHttpChannel* aChannel, nsIURI* aURI, bool aIsThirdParty,
                  const std::function<void(bool)>& aCallback);

  mozilla::ipc::IPCResult Recv__delete__(
      const uint32_t& aClassificationFlags) override;

  nsCOMPtr<nsIHttpChannel> mChannel;
  nsCOMPtr<nsIURI> mURI;
  std::function<void(bool)> mCallback;
  bool mIsThirdParty;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ClassifierDummyChannelChild_h
