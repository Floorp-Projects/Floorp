function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crash_oom.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_OOM;
      crashReporter.annotateCrashReport("TestingOOMCrash", "Yes");
    },
    function(mdump, extra) {
      do_check_eq(extra.TestingOOMCrash, "Yes");
      do_check_true("OOMAllocationSize" in extra);
      do_check_true(Number(extra.OOMAllocationSize) > 0);
      do_check_true("SystemMemoryUsePercentage" in extra);
      do_check_true("TotalVirtualMemory" in extra);
      do_check_true("AvailableVirtualMemory" in extra);
      do_check_true("TotalPageFile" in extra);
      do_check_true("AvailablePageFile" in extra);
      do_check_true("TotalPhysicalMemory" in extra);
      do_check_true("AvailablePhysicalMemory" in extra);

    },
    true);
}
