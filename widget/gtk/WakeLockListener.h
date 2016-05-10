/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>

#ifndef __WakeLockListener_h__
#define __WakeLockListener_h__

#include "nsHashKeys.h"
#include "nsClassHashtable.h"

#include "nsIDOMWakeLockListener.h"

#ifdef MOZ_ENABLE_DBUS
#include "mozilla/ipc/DBusConnectionRefPtr.h"
#endif

class WakeLockTopic;

/**
 * Receives WakeLock events and simply passes it on to the right WakeLockTopic
 * to inhibit the screensaver.
 */
class WakeLockListener final : public nsIDOMMozWakeLockListener
{
public:
  NS_DECL_ISUPPORTS;

  static WakeLockListener* GetSingleton(bool aCreate = true);
  static void Shutdown();

  virtual nsresult Callback(const nsAString& topic,
                            const nsAString& state) override;

private:
  WakeLockListener();
  ~WakeLockListener() = default;

  static WakeLockListener* sSingleton;

#ifdef MOZ_ENABLE_DBUS
  RefPtr<DBusConnection> mConnection;
#endif
  // Map of topic names to |WakeLockTopic|s.
  // We assume a small, finite-sized set of topics.
  nsClassHashtable<nsStringHashKey, WakeLockTopic> mTopics;
};

#endif // __WakeLockListener_h__
