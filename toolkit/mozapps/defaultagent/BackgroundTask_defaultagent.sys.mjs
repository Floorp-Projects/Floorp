/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Should be slightly longer than NOTIFICATION_WAIT_TIMEOUT_MS in
// Notification.cpp to not cause race between timeouts.
// 12 hours NOTIFICATION_WAIT_TIMEOUT_MS + 10 additional seconds.
export const backgroundTaskMinRuntimeMS = 12 * 60 * 60 * 1000 + 10 * 1000;

export async function runBackgroundTask(commandLine) {
  let defaultAgent = Cc["@mozilla.org/default-agent;1"].getService(
    Ci.nsIDefaultAgent
  );

  return defaultAgent.handleCommandLine(commandLine);
}
