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

      var observerService = Components.classes["@mozilla.org/observer-service;1"]
        .getService(Components.interfaces.nsIObserverService);
      observerService.addObserver(crashWhileReporting, "memory-pressure", false);
      Components.utils.getJSTestingFunctions().reportLargeAllocationFailure();
    },
    function(mdump, extra) {
      do_check_eq(extra.TestingOOMCrash, "Yes");
      do_check_eq(extra.JSLargeAllocationFailure, "Reporting");
    },
    true);
}
