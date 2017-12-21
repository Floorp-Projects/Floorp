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
      Assert.equal(extra.TestingOOMCrash, "Yes");
      Assert.ok("OOMAllocationSize" in extra);
      Assert.ok(Number(extra.OOMAllocationSize) > 0);
    },
    true);
}
