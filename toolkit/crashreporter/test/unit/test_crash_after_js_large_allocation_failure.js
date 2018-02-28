function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump("INFO | test_crash_after_js_large_allocation_failure.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_MOZ_CRASH;
      crashReporter.annotateCrashReport("TestingOOMCrash", "Yes");
      Cu.getJSTestingFunctions().reportLargeAllocationFailure();
      Cu.forceGC();
    },
    function(mdump, extra) {
      Assert.equal(extra.TestingOOMCrash, "Yes");
      Assert.equal(false, "JSOutOfMemory" in extra);
      Assert.equal(extra.JSLargeAllocationFailure, "Recovered");
    },
    true);
}
