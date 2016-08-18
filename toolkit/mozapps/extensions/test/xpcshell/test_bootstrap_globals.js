/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that bootstrap.js has the expected globals defined
Components.utils.import("resource://gre/modules/Services.jsm");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const EXPECTED_GLOBALS = [
  ["Worker", "function"],
  ["ChromeWorker", "function"],
  ["console", "object"]
];

function run_test() {
  do_test_pending();
  startupManager();
  let sawGlobals = false;

  Services.obs.addObserver(function(subject) {
    subject.wrappedJSObject.expectedGlobals = EXPECTED_GLOBALS;
  }, "bootstrap-request-globals", false);

  Services.obs.addObserver(function({ wrappedJSObject: seenGlobals }) {
    for (let [name, ] of EXPECTED_GLOBALS)
      do_check_true(seenGlobals.has(name));

    sawGlobals = true;
  }, "bootstrap-seen-globals", false);

  installAllFiles([do_get_addon("bootstrap_globals")], function() {
    do_check_true(sawGlobals);
    shutdownManager();
    do_test_finished();
  });
}
