function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crash_runtimeabort.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // Try crashing with a runtime abort
  do_crash(function() {
             crashType = CrashTestUtils.CRASH_RUNTIMEABORT;
             crashReporter.annotateCrashReport("TestKey", "TestValue");
           },
           function(mdump, extra) {
             do_check_eq(extra.TestKey, "TestValue");
             do_check_true(/xpcom_runtime_abort/.test(extra.Notes));
             do_check_false("OOMAllocationSize" in extra);
             do_check_true(/Intentional crash/.test(extra.AbortMessage));
           },
          // process will exit with a zero exit status
          true);
}
