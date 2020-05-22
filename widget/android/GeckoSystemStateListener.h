/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoSystemStateListener_h
#define GeckoSystemStateListener_h

#include "mozilla/Assertions.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/java/GeckoSystemStateListenerNatives.h"

namespace mozilla {

class GeckoSystemStateListener final
    : public java::GeckoSystemStateListener::Natives<GeckoSystemStateListener> {
  GeckoSystemStateListener() = delete;

 public:
  static void OnDeviceChanged() {
    MOZ_ASSERT(NS_IsMainThread());
    mozilla::LookAndFeel::NotifyChangedAllWindows();
  }
};

}  // namespace mozilla

#endif  // GeckoSystemStateListener_h
