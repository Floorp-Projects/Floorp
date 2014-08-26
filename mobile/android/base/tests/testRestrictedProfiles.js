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

  //run_next_test();
});
/*
// NOTE: JNI.jsm has no way to call a string method yet
add_task(function test_getUserRestrictions() {
  // In an admin profile, like the tests: {}
  // In a restricted profile: {"no_modify_accounts":true,"no_share_location":true}
  let restrictions = "{}";

  let jni = null;
  try {
    jni = new JNI();
    let cls = jni.findClass("org/mozilla/gecko/GeckoAppShell");
    let method = jni.getStaticMethodID(cls, "getUserRestrictions", "()Ljava/lang/String;");
    restrictions = jni.callStaticStringMethod(cls, method);
  } finally {
    if (jni != null) {
      jni.close();
    }
  }

  do_check_eq(restrictions, "{}");
});
*/
run_next_test();
