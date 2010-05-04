function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crashreporter.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // Try crashing with a pure virtual call
  do_crash(function() {
             crashType = Components.interfaces.nsITestCrasher.CRASH_PURE_VIRTUAL_CALL;
             crashReporter.annotateCrashReport("TestKey", "TestValue");
           },
           function(mdump, extra) {
             do_check_eq(extra.TestKey, "TestValue");
           },
          // process will exit with a zero exit status
          true);
}
