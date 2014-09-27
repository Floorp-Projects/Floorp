// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/ctypes.jsm");
Cu.import("resource://gre/modules/JNI.jsm");

add_task(function test_isUserRestricted() {
  // Make sure the parental controls service is available
  do_check_true("@mozilla.org/parental-controls-service;1" in Cc);
  
  let pc = Cc["@mozilla.org/parental-controls-service;1"].createInstance(Ci.nsIParentalControlsService);
  
  // In an admin profile, like the tests: enabled = false
  // In a restricted profile: enabled = true
  do_check_false(pc.parentalControlsEnabled);
  do_check_false(pc.blockFileDownloadsEnabled);

  do_check_true(pc.isAllowed(Ci.nsIParentalControlsService.DOWNLOAD));
  do_check_true(pc.isAllowed(Ci.nsIParentalControlsService.INSTALL_EXTENSION));
  do_check_true(pc.isAllowed(Ci.nsIParentalControlsService.INSTALL_APP));
  do_check_true(pc.isAllowed(Ci.nsIParentalControlsService.VISIT_FILE_URLS));
  do_check_true(pc.isAllowed(Ci.nsIParentalControlsService.SHARE));
  do_check_true(pc.isAllowed(Ci.nsIParentalControlsService.BOOKMARK));
  do_check_true(pc.isAllowed(Ci.nsIParentalControlsService.INSTALL_EXTENSION));
  do_check_true(pc.isAllowed(Ci.nsIParentalControlsService.MODIFY_ACCOUNTS));
});

add_task(function test_getUserRestrictions() {
  // In an admin profile, like the tests: {}
  // In a restricted profile: {"no_modify_accounts":true,"no_share_location":true}
  let restrictions = "{}";

  var jenv = null;
  try {
    jenv = JNI.GetForThread();
    var profile = JNI.LoadClass(jenv, "org.mozilla.gecko.RestrictedProfiles", {
      static_methods: [
        { name: "getUserRestrictions", sig: "()Ljava/lang/String;" },
      ],
    });
    restrictions = JNI.ReadString(jenv, profile.getUserRestrictions());
  } finally {
    if (jenv) {
      JNI.UnloadClasses(jenv);
    }
  }

  do_check_eq(restrictions, "{}");
});

run_next_test();
