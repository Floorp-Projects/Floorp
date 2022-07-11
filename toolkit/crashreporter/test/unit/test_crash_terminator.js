/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Shutdown Terminator report errors correctly

function setup_crash() {
  Services.prefs.setBoolPref("toolkit.terminator.testing", true);
  Services.prefs.setIntPref("toolkit.asyncshutdown.crash_timeout", 150);

  // Initialize the terminator
  // (normally, this is done through the manifest file, but xpcshell
  // doesn't take them into account).
  let terminator = Cc[
    "@mozilla.org/toolkit/shutdown-terminator;1"
  ].createInstance(Ci.nsIObserver);
  terminator.observe(null, "terminator-test-profile-after-change", null);

  // Inform the terminator that shutdown has started
  // Pick an arbitrary notification
  terminator.observe(null, "terminator-test-profile-before-change", null);
  terminator.observe(null, "terminator-test-xpcom-will-shutdown", null);

  dump("Waiting (actively) for the crash\n");
  Services.tm.spinEventLoopUntil(
    "Test(test_crash_terminator.js:setup_crash())",
    () => false
  );
}

function after_crash(mdump, extra) {
  info("Crash signature: " + JSON.stringify(extra, null, "\t"));
  Assert.equal(extra.ShutdownProgress, "xpcom-will-shutdown");
}

add_task(async function run_test() {
  await do_crash(setup_crash, after_crash);
});
