function run_test() {
  dump("INFO | test_crashreporter.js | Get crashreporter service.\n");
  var cr = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
    Ci.nsICrashReporter
  );
  Assert.notEqual(cr, null);

  Assert.ok(cr.enabled);

  try {
    cr.serverURL;
    do_throw("Getting serverURL when not set should have thrown!");
  } catch (ex) {
    Assert.equal(ex.result, Cr.NS_ERROR_FAILURE);
  }

  // check setting/getting serverURL

  // try it with two different URLs, just for kicks
  var testspecs = [
    "http://example.com/submit",
    "https://example.org/anothersubmit",
  ];
  for (var i = 0; i < testspecs.length; ++i) {
    cr.serverURL = Services.io.newURI(testspecs[i]);
    Assert.equal(cr.serverURL.spec, testspecs[i]);
  }

  // should not allow setting non-http/https URLs
  try {
    cr.serverURL = Services.io.newURI("ftp://example.com/submit");
    do_throw("Setting serverURL to a non-http(s) URL should have thrown!");
  } catch (ex) {
    Assert.equal(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }

  // check getting/setting minidumpPath
  // it should be $TEMP by default, but I'm not sure if we can exactly test that
  // this will at least test that it doesn't throw
  Assert.notEqual(cr.minidumpPath.path, "");
  var cwd = do_get_cwd();
  cr.minidumpPath = cwd;
  Assert.equal(cr.minidumpPath.path, cwd.path);

  // Test annotateCrashReport()
  try {
    cr.annotateCrashReport(undefined, "");
    do_throw(
      "Calling annotateCrashReport() with an undefined key should have thrown!"
    );
  } catch (ex) {
    Assert.equal(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
  try {
    cr.annotateCrashReport("foobar", "");
    do_throw(
      "Calling annotateCrashReport() with a bogus key should have thrown!"
    );
  } catch (ex) {
    Assert.equal(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
  cr.annotateCrashReport("TestKey", "testData1");
  // Replace previous data.
  cr.annotateCrashReport("TestKey", "testData2");
  // Allow nul chars in annotations.
  cr.annotateCrashReport("TestKey", "da\0ta");

  cr.appendAppNotesToCrashReport("additional testData3");
  // Add more data.
  cr.appendAppNotesToCrashReport("additional testData4");

  // Test removeCrashReportAnnotation()
  try {
    cr.removeCrashReportAnnotation(undefined);
    do_throw(
      "Calling removeCrashReportAnnotation() with an undefined key should have thrown!"
    );
  } catch (ex) {
    Assert.equal(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
  try {
    cr.removeCrashReportAnnotation("foobar");
    do_throw(
      "Calling removeCrashReportAnnotation() with a bogus key should have thrown!"
    );
  } catch (ex) {
    Assert.equal(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
  cr.removeCrashReportAnnotation("TestKey");

  // Testing setting the minidumpPath field
  cr.minidumpPath = cwd;
  Assert.equal(cr.minidumpPath.path, cwd.path);
}
