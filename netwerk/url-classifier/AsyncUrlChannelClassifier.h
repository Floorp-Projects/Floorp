/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set expandtab ts=4 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_AsyncUrlChannelClassifier_h
#define mozilla_net_AsyncUrlChannelClassifier_h

#include "nsISupports.h"
#include <functional>

class nsIChannel;

namespace mozilla {
namespace net {

class AsyncUrlChannelClassifier final {
 public:
  static nsresult CheckChannel(nsIChannel* aChannel,
                               std::function<void()>&& aCallback);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_AsyncUrlChannelClassifier_h
