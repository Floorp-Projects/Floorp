function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crashreporter.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  dump("INFO | test_crashreporter.js | Get crashreporter service.\n");
  var cr = Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
                     .getService(Components.interfaces.nsICrashReporter);
  do_check_neq(cr, null);

  /**********
   * Check behavior when disabled.
   * XXX: don't run these tests right now, there's a race condition in
   * Breakpad with the handler thread. See:
   * http://code.google.com/p/google-breakpad/issues/detail?id=334
   **********

  // Crash reporting is enabled by default in </testing/xpcshell/head.js>.
  dump("INFO | test_crashreporter.js | Disable crashreporter.\n");
  cr.enabled = false;
  do_check_false(cr.enabled);

  try {
    let su = cr.serverURL;
    do_throw("Getting serverURL when disabled should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    let mdp = cr.minidumpPath;
    do_throw("Getting minidumpPath when disabled should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    cr.annotateCrashReport(null, null);
    do_throw("Calling annotateCrashReport() when disabled should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  try {
    cr.appendAppNotesToCrashReport(null);
    do_throw("Calling appendAppNotesToCrashReport() when disabled should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_NOT_INITIALIZED);
  }

  /**********
   * Check behavior when enabled.
   **********

  dump("INFO | test_crashreporter.js | Re-enable crashreporter (in default state).\n");
  // check that we can enable the crashreporter
  cr.enabled = true;
*/
  do_check_true(cr.enabled);
  // ensure that double-enabling doesn't error
  cr.enabled = true;
  do_check_true(cr.enabled);

  try {
    let su = cr.serverURL;
    do_throw("Getting serverURL when not set should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_FAILURE);
  }

  // check setting/getting serverURL
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);

  // try it with two different URLs, just for kicks
  var testspecs = ["http://example.com/submit",
                   "https://example.org/anothersubmit"];
  for (var i = 0; i < testspecs.length; ++i) {
    cr.serverURL = ios.newURI(testspecs[i], null, null);
    do_check_eq(cr.serverURL.spec, testspecs[i]);
  }

  // should not allow setting non-http/https URLs
  try {
    cr.serverURL = ios.newURI("ftp://example.com/submit", null, null);
    do_throw("Setting serverURL to a non-http(s) URL should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }

  // check getting/setting minidumpPath
  // it should be $TEMP by default, but I'm not sure if we can exactly test that
  // this will at least test that it doesn't throw
  do_check_neq(cr.minidumpPath.path, "");
  var cwd = do_get_cwd();
  cr.minidumpPath = cwd;
  do_check_eq(cr.minidumpPath.path, cwd.path);

  try {
    cr.annotateCrashReport("equal=equal", "");
    do_throw("Calling annotateCrashReport() with an '=' in key should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }
  try {
    cr.annotateCrashReport("new\nline", "");
    do_throw("Calling annotateCrashReport() with a '\\n' in key should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }
  try {
    cr.annotateCrashReport("", "da\0ta");
    do_throw("Calling annotateCrashReport() with a '\\0' in data should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }
  cr.annotateCrashReport("testKey", "testData1");
  // Replace previous data.
  cr.annotateCrashReport("testKey", "testData2");

  try {
    cr.appendAppNotesToCrashReport("da\0ta");
    do_throw("Calling appendAppNotesToCrashReport() with a '\\0' in data should have thrown!");
  }
  catch (ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }
  cr.appendAppNotesToCrashReport("additional testData3");
  // Add more data.
  cr.appendAppNotesToCrashReport("additional testData4");

  /*
   * These tests are also disabled, see comment above.
  // check that we can disable the crashreporter
  cr.enabled = false;
  do_check_false(cr.enabled);
  // ensure that double-disabling doesn't error
  cr.enabled = false;
  do_check_false(cr.enabled);
*/

  /**********
   * Reset to initial state.
   **********/

  // leave it enabled at the end in case of shutdown crashes
  dump("INFO | test_crashreporter.js | Reset crashreporter to its initial state.\n");
  // (Values as initially set by </testing/xpcshell/head.js>.)
  cr.enabled = true;
  do_check_true(cr.enabled);
  cr.minidumpPath = cwd;
  do_check_eq(cr.minidumpPath.path, cwd.path);
}
