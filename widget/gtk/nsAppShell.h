/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include <glib.h>
#include "nsBaseAppShell.h"
#include "nsCOMPtr.h"
#ifdef MOZ_ENABLE_DBUS
#  include <dbus/dbus-glib.h>
#endif

class nsAppShell : public nsBaseAppShell {
 public:
  nsAppShell() : mTag(0) { mPipeFDs[0] = mPipeFDs[1] = 0; }

  // nsBaseAppShell overrides:
  nsresult Init();
  virtual void ScheduleNativeEventCallback() override;
  virtual bool ProcessNextNativeEvent(bool mayWait) override;
#ifdef MOZ_ENABLE_DBUS
  void StartDBusListening();
  void StopDBusListening();
#endif

 private:
  virtual ~nsAppShell();

  static gboolean EventProcessorCallback(GIOChannel* source,
                                         GIOCondition condition, gpointer data);
  int mPipeFDs[2];
  unsigned mTag;

#ifdef MOZ_ENABLE_DBUS
  DBusGConnection* mDBusConnection = nullptr;
  DBusGProxy* mLogin1Proxy = nullptr;
#endif
};

#endif /* nsAppShell_h__ */
