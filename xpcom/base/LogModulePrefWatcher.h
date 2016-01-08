/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LogModulePrefWatcher_h
#define LogModulePrefWatcher_h

#include "nsIObserver.h"

namespace mozilla {

/**
 * Watches for changes to "logging.*" prefs and then updates the appropriate
 * LogModule's log level. Both the integer and string versions of the LogLevel
 * enum are supported.
 *
 * For example setting the pref "logging.Foo" to "Verbose" will set the
 * LogModule for "Foo" to the LogLevel::Verbose level. Setting "logging.Bar" to
 * 4 would set the LogModule for "Bar" to the LogLevel::Debug level.
 */
class LogModulePrefWatcher : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * Starts observing logging pref changes.
   */
  static void RegisterPrefWatcher();

private:
  LogModulePrefWatcher();
  virtual ~LogModulePrefWatcher()
  {
  }
};
} // namespace mozilla

#endif // LogModulePrefWatcher_h
