function run_test() {
  dump("INFO | test_crashreporter.js | Get crashreporter service.\n");
  var cr = Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
                     .getService(Components.interfaces.nsICrashReporter);
  Assert.notEqual(cr, null);

  Assert.ok(cr.enabled);

  try {
    cr.serverURL;
    do_throw("Getting serverURL when not set should have thrown!");
  } catch (ex) {
    Assert.equal(ex.result, Components.results.NS_ERROR_FAILURE);
  }

  // check setting/getting serverURL

  // try it with two different URLs, just for kicks
  var testspecs = ["http://example.com/submit",
                   "https://example.org/anothersubmit"];
  for (var i = 0; i < testspecs.length; ++i) {
    cr.serverURL = Services.io.newURI(testspecs[i]);
    Assert.equal(cr.serverURL.spec, testspecs[i]);
  }

  // should not allow setting non-http/https URLs
  try {
    cr.serverURL = Services.io.newURI("ftp://example.com/submit");
    do_throw("Setting serverURL to a non-http(s) URL should have thrown!");
  } catch (ex) {
    Assert.equal(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }

  // check getting/setting minidumpPath
  // it should be $TEMP by default, but I'm not sure if we can exactly test that
  // this will at least test that it doesn't throw
  Assert.notEqual(cr.minidumpPath.path, "");
  var cwd = do_get_cwd();
  cr.minidumpPath = cwd;
  Assert.equal(cr.minidumpPath.path, cwd.path);

  try {
    cr.annotateCrashReport("equal=equal", "");
    do_throw("Calling annotateCrashReport() with an '=' in key should have thrown!");
  } catch (ex) {
    Assert.equal(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }
  try {
    cr.annotateCrashReport("new\nline", "");
    do_throw("Calling annotateCrashReport() with a '\\n' in key should have thrown!");
  } catch (ex) {
    Assert.equal(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }
  try {
    cr.annotateCrashReport("", "da\0ta");
    do_throw("Calling annotateCrashReport() with a '\\0' in data should have thrown!");
  } catch (ex) {
    Assert.equal(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }
  cr.annotateCrashReport("testKey", "testData1");
  // Replace previous data.
  cr.annotateCrashReport("testKey", "testData2");

  try {
    cr.appendAppNotesToCrashReport("da\0ta");
    do_throw("Calling appendAppNotesToCrashReport() with a '\\0' in data should have thrown!");
  } catch (ex) {
    Assert.equal(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }
  cr.appendAppNotesToCrashReport("additional testData3");
  // Add more data.
  cr.appendAppNotesToCrashReport("additional testData4");

  cr.minidumpPath = cwd;
  Assert.equal(cr.minidumpPath.path, cwd.path);
}
