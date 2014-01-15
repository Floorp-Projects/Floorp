/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

do_register_cleanup(function() {
  Services.prefs.setBoolPref("toolkit.osfile.log", false);
});

function run_test() {
  Services.prefs.setBoolPref("toolkit.osfile.log", true);

  run_next_test();
}

/**
 * Test OS.File.getAvailableFreeSpace
 */
add_task(function() {
  // Set up profile. We will use profile path to query for available free
  // space.
  do_get_profile();

  let dir = OS.Constants.Path.profileDir;

  // Sanity checking for the test
  do_check_true((yield OS.File.exists(dir)));

  // Query for available bytes for user
  let availableBytes = yield OS.File.getAvailableFreeSpace(dir);

  do_check_true(!!availableBytes);
  do_check_true(availableBytes > 0);
});
