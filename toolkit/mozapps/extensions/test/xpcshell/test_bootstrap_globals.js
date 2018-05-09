/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that bootstrap.js has the expected globals defined
ChromeUtils.import("resource://gre/modules/Services.jsm");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const EXPECTED_GLOBALS = [
  ["Worker", "function"],
  ["ChromeWorker", "function"],
  ["console", "object"]
];

async function run_test() {
  do_test_pending();
  await promiseStartupManager();
  let sawGlobals = false;

  Services.obs.addObserver(function(subject) {
    subject.wrappedJSObject.expectedGlobals = EXPECTED_GLOBALS;
  }, "bootstrap-request-globals");

  Services.obs.addObserver(function({ wrappedJSObject: seenGlobals }) {
    for (let [name, ] of EXPECTED_GLOBALS)
      Assert.ok(seenGlobals.has(name));

    sawGlobals = true;
  }, "bootstrap-seen-globals");

  await promiseInstallAllFiles([do_get_addon("bootstrap_globals")]);
  Assert.ok(sawGlobals);
  await promiseShutdownManager();
  do_test_finished();
}
