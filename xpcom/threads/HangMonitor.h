/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HangMonitor_h
#define mozilla_HangMonitor_h

namespace mozilla { namespace HangMonitor {

/**
 * Start monitoring hangs. Should be called by the XPCOM startup process only.
 */
void Startup();

/**
 * Stop monitoring hangs and join the thread.
 */
void Shutdown();

/**
 * Notify the hang monitor of new activity which should reset its internal
 * timer.
 */
void NotifyActivity();

/**
 * Notify the hang monitor that the browser is now idle and no detection should
 * be done.
 */
void Suspend();

} } // namespace mozilla::HangMonitor

#endif // mozilla_HangMonitor_h
