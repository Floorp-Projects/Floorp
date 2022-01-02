add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_after_js_oom_reported_2.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  await do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_MOZ_CRASH;
      crashReporter.annotateCrashReport("TestKey", "Yes");
      Cu.getJSTestingFunctions().reportOutOfMemory();
      Cu.forceGC(); // recover from first OOM
      Cu.getJSTestingFunctions().reportOutOfMemory();
    },
    function(mdump, extra) {
      Assert.equal(extra.TestKey, "Yes");

      // Technically, GC can happen at any time, but it would be really
      // peculiar for it to happen again heuristically right after a GC was
      // forced. If extra.JSOutOfMemory is "Recovered" here, that's most
      // likely a bug in the error reporting machinery.
      Assert.equal(extra.JSOutOfMemory, "Reported");
    },
    true
  );
});
