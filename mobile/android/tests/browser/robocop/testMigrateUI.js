// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

add_task(function* test_migrateUI() {
  Services.prefs.clearUserPref("browser.migration.version");
  Services.prefs.setBoolPref("nglayout.debug.paint_flashing", true);

  let BrowserApp = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
  BrowserApp._migrateUI();

  // Check that migration version increased.
  do_check_eq(Services.prefs.getIntPref("browser.migration.version"), 1);

  // Check that user pref value was reset.
  do_check_eq(Services.prefs.prefHasUserValue("nglayout.debug.paint_flashing"), false);
});

run_next_test();
