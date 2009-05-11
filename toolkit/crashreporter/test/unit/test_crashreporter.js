function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    do_check_true(true, "Can't test this in a non-libxul build");
    return;
  }

  var cr = Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
                     .getService(Components.interfaces.nsICrashReporter);
  do_check_neq(cr, null);

  // check that we can enable the crashreporter
  cr.enabled = true;
  do_check_true(cr.enabled);
  // ensure that double-enabling doesn't error
  cr.enabled = true;

  // check setting/getting serverURL
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);

  // try it with two different URLs, just for kicks
  var testspecs = ["http://example.com/submit",
                   "https://example.org/anothersubmit"];
  for (var i=0; i<testspecs.length; i++) {
    var u = ios.newURI(testspecs[i], null, null);
    cr.serverURL = u;
    do_check_eq(cr.serverURL.spec, testspecs[i]);
  }

  // should not allow setting non-http/https URLs
  try {
    u = ios.newURI("ftp://example.com/submit", null, null);
    cr.serverURL = u;
    do_throw("Setting serverURL to a non-http URL should have thrown!");
  }
  catch(ex) {
    do_check_eq(ex.result, Components.results.NS_ERROR_INVALID_ARG);
  }

  // check getting/setting minidumpPath
  // it should be $TEMP initially, but I'm not sure if we can exactly test that
  // this will at least test that it doesn't throw
  do_check_neq(cr.minidumpPath.path, "");
  var cwd = do_get_cwd();
  cr.minidumpPath = cwd;
  do_check_eq(cr.minidumpPath.path, cwd.path);

  // check that we can disable the crashreporter
  cr.enabled = false;
  do_check_false(cr.enabled);
  // ensure that double-disabling doesn't error
  cr.enabled = false;
}
