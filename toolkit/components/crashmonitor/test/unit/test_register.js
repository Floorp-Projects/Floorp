/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that CrashMonitor.jsm is correctly loaded from XPCOM component
 */
add_task(function test_register() {
  let cm = Components.classes["@mozilla.org/toolkit/crashmonitor;1"]
                             .createInstance(Components.interfaces.nsIObserver);

  // Send "profile-after-change" to trigger the initialization
  cm.observe(null, "profile-after-change", null);

  // If CrashMonitor was initialized properly a new call to |init|
  // should fail
  try {
    CrashMonitor.init();
    do_check_true(false);
  } catch (ex) {
    do_check_true(true);
  }
});
