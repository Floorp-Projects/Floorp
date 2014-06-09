function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crash_after_js_oom_reported_2.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_MOZ_CRASH;
      crashReporter.annotateCrashReport("TestingOOMCrash", "Yes");
      Components.utils.getJSTestingFunctions().reportOutOfMemory();
      Components.utils.forceGC();  // recover from first OOM
      Components.utils.getJSTestingFunctions().reportOutOfMemory();
    },
    function(mdump, extra) {
      do_check_eq(extra.TestingOOMCrash, "Yes");

      // Technically, GC can happen at any time, but it would be really
      // peculiar for it to happen again heuristically right after a GC was
      // forced. If extra.JSOutOfMemory is "Recovered" here, that's most
      // likely a bug in the error reporting machinery.
      do_check_eq(extra.JSOutOfMemory, "Reported");
    },
    true);
}
