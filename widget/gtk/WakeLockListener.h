/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __WakeLockListener_h__
#define __WakeLockListener_h__

#include <unistd.h>

#include "mozilla/StaticPtr.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"

#include "nsIDOMWakeLockListener.h"

#ifdef MOZ_ENABLE_DBUS
#  include "mozilla/DBusHelpers.h"
#endif

class WakeLockTopic;

/**
 * Receives WakeLock events and simply passes it on to the right WakeLockTopic
 * to inhibit the screensaver.
 */
class WakeLockListener final : public nsIDOMMozWakeLockListener {
 public:
  NS_DECL_ISUPPORTS;

  static WakeLockListener* GetSingleton(bool aCreate = true);
  static void Shutdown();

  virtual nsresult Callback(const nsAString& topic,
                            const nsAString& state) override;

 private:
  ~WakeLockListener() = default;

  static mozilla::StaticRefPtr<WakeLockListener> sSingleton;

  // Map of topic names to |WakeLockTopic|s.
  // We assume a small, finite-sized set of topics.
  nsRefPtrHashtable<nsStringHashKey, WakeLockTopic> mTopics;
};

#endif  // __WakeLockListener_h__
