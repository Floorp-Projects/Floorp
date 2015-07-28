/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSandboxViolationSink_h_
#define nsSandboxViolationSink_h_

#include <stdint.h>

// Class for tracking sandbox violations.  Currently it just logs them to
// stdout and the system console.  In the future it may do more.

// What makes this possible is the fact that Apple' sandboxd calls
// notify_post("com.apple.sandbox.violation.*") whenever it's notified by the
// Sandbox kernel extension of a sandbox violation.  We register to receive
// these notifications.  But the notifications are empty, and are sent for
// every violation in every process.  So we need to do more to get only "our"
// violations, and to find out what kind of violation they were.  See the
// implementation of nsSandboxViolationSink::ViolationHandler().

#define SANDBOX_VIOLATION_QUEUE_NAME "org.mozilla.sandbox.violation.queue"
#define SANDBOX_VIOLATION_NOTIFICATION_NAME "com.apple.sandbox.violation.*"

class nsSandboxViolationSink
{
public:
  static void Start();
  static void Stop();
private:
  static void ViolationHandler();
  static int mNotifyToken;
  static uint64_t mLastMsgReceived;
};

#endif // nsSandboxViolationSink_h_
