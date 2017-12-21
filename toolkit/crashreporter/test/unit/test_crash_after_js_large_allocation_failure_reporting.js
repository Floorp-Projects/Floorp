function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crash_after_js_oom_reporting.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_MOZ_CRASH;
      crashReporter.annotateCrashReport("TestingOOMCrash", "Yes");

      function crashWhileReporting() {
        CrashTestUtils.crash(crashType);
      }

      Services.obs.addObserver(crashWhileReporting, "memory-pressure");
      Components.utils.getJSTestingFunctions().reportLargeAllocationFailure();
    },
    function(mdump, extra) {
      Assert.equal(extra.TestingOOMCrash, "Yes");
      Assert.equal(extra.JSLargeAllocationFailure, "Reporting");
    },
    true);
}
