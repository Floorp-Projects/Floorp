function run_test()
{
  var isOSX = ("nsILocalFileMac" in Components.interfaces);
  if (isOSX) {
    dump("INFO | test_crashreporter.js | Skipping test on mac, bug 599475")
    return;
  }

  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crashreporter.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // try a basic crash
  do_crash(null, function(mdump, extra) {
             do_check_true(mdump.exists());
             do_check_true(mdump.fileSize > 0);
             do_check_true('StartupTime' in extra);
             do_check_true('CrashTime' in extra);
           });

  // check setting some basic data
  do_crash(function() {
             crashReporter.annotateCrashReport("TestKey", "TestValue");
             crashReporter.appendAppNotesToCrashReport("Junk");
             crashReporter.appendAppNotesToCrashReport("MoreJunk");
           },
           function(mdump, extra) {
             do_check_eq(extra.TestKey, "TestValue");
             do_check_eq(extra.Notes, "JunkMoreJunk");
           });
}
