/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HangMonitor_h
#define mozilla_HangMonitor_h

namespace mozilla {
namespace HangMonitor {

/**
 * Signifies the type of activity in question
*/
enum ActivityType
{
  /* There is activity and it is known to be UI related activity. */
  kUIActivity,

  /* There is non UI activity and no UI activity is pending */
  kActivityNoUIAVail,

  /* There is non UI activity and UI activity is known to be pending */
  kActivityUIAVail,

  /* There is non UI activity and UI activity pending is unknown */
  kGeneralActivity
};

/**
 * Start monitoring hangs. Should be called by the XPCOM startup process only.
 */
void Startup();

/**
 * Stop monitoring hangs and join the thread.
 */
void Shutdown();

/**
 * Notify the hang monitor of activity which will reset its internal timer.
 * 
 * @param activityType The type of activity being reported.
 * @see ActivityType
 */
void NotifyActivity(ActivityType activityType = kGeneralActivity);

/*
 * Notify the hang monitor that the browser is now idle and no detection should
 * be done.
 */
void Suspend();

} // namespace HangMonitor
} // namespace mozilla

#endif // mozilla_HangMonitor_h
