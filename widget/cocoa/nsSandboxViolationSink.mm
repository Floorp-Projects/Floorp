/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSandboxViolationSink.h"

#include <unistd.h>
#include <time.h>
#include <asl.h>
#include <dispatch/dispatch.h>
#include <notify.h>
#include "nsCocoaDebugUtils.h"
#include "mozilla/Preferences.h"

int nsSandboxViolationSink::mNotifyToken = 0;
uint64_t nsSandboxViolationSink::mLastMsgReceived = 0;

void
nsSandboxViolationSink::Start()
{
  if (mNotifyToken) {
    return;
  }
  notify_register_dispatch(SANDBOX_VIOLATION_NOTIFICATION_NAME,
                           &mNotifyToken,
                           dispatch_queue_create(SANDBOX_VIOLATION_QUEUE_NAME,
                                                 DISPATCH_QUEUE_SERIAL),
                           ^(int token) { ViolationHandler(); });
}

void
nsSandboxViolationSink::Stop()
{
  if (!mNotifyToken) {
    return;
  }
  notify_cancel(mNotifyToken);
  mNotifyToken = 0;
}

// We need to query syslogd to find out what violations occurred, and whether
// they were "ours".  We can use the Apple System Log facility to do this.
// Besides calling notify_post("com.apple.sandbox.violation.*"), Apple's
// sandboxd also reports all sandbox violations (sent to it by the Sandbox
// kernel extension) to syslogd, which stores them and makes them viewable
// in the system console.  This is the database we query.

// ViolationHandler() is always called on its own secondary thread.  This
// makes it unlikely it will interfere with other browser activity.

void
nsSandboxViolationSink::ViolationHandler()
{
  aslmsg query = asl_new(ASL_TYPE_QUERY);

  asl_set_query(query, ASL_KEY_FACILITY, "com.apple.sandbox",
                ASL_QUERY_OP_EQUAL);

  // Only get reports that were generated very recently.
  char query_time[30] = {0};
  snprintf(query_time, sizeof(query_time), "%li", time(NULL) - 2);
  asl_set_query(query, ASL_KEY_TIME, query_time,
                ASL_QUERY_OP_NUMERIC | ASL_QUERY_OP_GREATER_EQUAL);

  // This code is easier to test if we don't just track "our" violations,
  // which are (normally) few and far between.  For example (for the time
  // being at least) four appleeventsd sandbox violations happen every time
  // we start the browser in e10s mode.  But it makes sense to default to
  // only tracking "our" violations.
  if (mozilla::Preferences::GetBool(
      "security.sandbox.mac.track.violations.oursonly", true)) {
    // This makes each of our processes log its own violations.  It might
    // be better to make the chrome process log all the other processes'
    // violations.
    char query_pid[20] = {0};
    snprintf(query_pid, sizeof(query_pid), "%u", getpid());
    asl_set_query(query, ASL_KEY_REF_PID, query_pid, ASL_QUERY_OP_EQUAL);
  }

  aslresponse response = asl_search(nullptr, query);

  // Each time ViolationHandler() is called we grab as many messages as are
  // available.  Otherwise we might not get them all.
  if (response) {
    while (true) {
      aslmsg hit = nullptr;
      aslmsg found = nullptr;
      const char* id_str;

      while ((hit = aslresponse_next(response))) {
        // Record the message id to avoid logging the same violation more
        // than once.
        id_str = asl_get(hit, ASL_KEY_MSG_ID);
        uint64_t id_val = atoll(id_str);
        if (id_val <= mLastMsgReceived) {
          continue;
        }
        mLastMsgReceived = id_val;
        found = hit;
        break;
      }
      if (!found) {
        break;
      }

      const char* pid_str = asl_get(found, ASL_KEY_REF_PID);
      const char* message_str = asl_get(found, ASL_KEY_MSG);
      nsCocoaDebugUtils::DebugLog("nsSandboxViolationSink::ViolationHandler(): id %s, pid %s, message %s",
                                  id_str, pid_str, message_str);
    }
    aslresponse_free(response);
  }
}
