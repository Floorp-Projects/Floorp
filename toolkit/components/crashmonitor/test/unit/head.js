/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

var sessionCheckpointsPath;
var CrashMonitor;

/**
 * Start the tasks of the different tests
 */
function run_test() {
  do_get_profile();
  sessionCheckpointsPath = PathUtils.join(
    PathUtils.profileDir,
    "sessionCheckpoints.json"
  );
  ({ CrashMonitor } = ChromeUtils.importESModule(
    "resource://gre/modules/CrashMonitor.sys.mjs"
  ));
  run_next_test();
}
