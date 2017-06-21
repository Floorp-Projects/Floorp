/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Shutdown Terminator report errors correctly

function setup_crash() {
  Components.utils.import("resource://gre/modules/Services.jsm");

  Services.prefs.setBoolPref("toolkit.terminator.testing", true);
  Services.prefs.setIntPref("toolkit.asyncshutdown.crash_timeout", 10);

  // Initialize the terminator
  // (normally, this is done through the manifest file, but xpcshell
  // doesn't take them into account).
  let terminator = Components.classes["@mozilla.org/toolkit/shutdown-terminator;1"].
    createInstance(Components.interfaces.nsIObserver);
  terminator.observe(null, "profile-after-change", null);

  // Inform the terminator that shutdown has started
  // Pick an arbitrary notification
  terminator.observe(null, "xpcom-will-shutdown", null);
  terminator.observe(null, "profile-before-change", null);

  dump("Waiting (actively) for the crash\n");
  Services.tm.spinEventLoopUntil(() => false);
}


function after_crash(mdump, extra) {
  do_print("Crash signature: " + JSON.stringify(extra, null, "\t"));
  Assert.equal(extra.ShutdownProgress, "profile-before-change");
}

function run_test() {
  do_crash(setup_crash, after_crash);
}
