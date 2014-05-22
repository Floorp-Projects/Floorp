function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crash_after_js_large_allocation_failure.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_MOZ_CRASH;
      crashReporter.annotateCrashReport("TestingOOMCrash", "Yes");
      Components.utils.getJSTestingFunctions().reportLargeAllocationFailure();
      Components.utils.forceGC();
    },
    function(mdump, extra) {
      do_check_eq(extra.TestingOOMCrash, "Yes");
      do_check_false("JSOutOfMemory" in extra);
      do_check_eq(extra.JSLargeAllocationFailure, "Recovered");
    },
    true);
}
