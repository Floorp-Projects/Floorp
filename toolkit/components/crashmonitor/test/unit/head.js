/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

var sessionCheckpointsPath;
var CrashMonitor;

/**
 * Start the tasks of the different tests
 */
function run_test() {
  do_get_profile();
  sessionCheckpointsPath = OS.Path.join(
    OS.Constants.Path.profileDir,
    "sessionCheckpoints.json"
  );
  ({ CrashMonitor } = ChromeUtils.import(
    "resource://gre/modules/CrashMonitor.jsm"
  ));
  run_next_test();
}
