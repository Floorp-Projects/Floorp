add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_after_js_oom_reported.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  await do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_MOZ_CRASH;
      crashReporter.annotateCrashReport("TestKey", "Yes");

      // GC now to avoid having it happen randomly later, which would make the
      // test bogusly fail. See comment below.
      Cu.forceGC();

      Cu.getJSTestingFunctions().reportOutOfMemory();
    },
    function(mdump, extra) {
      Assert.equal(extra.TestKey, "Yes");

      // The JSOutOfMemory field is absent if the JS engine never reported OOM,
      // "Reported" if it did, and "Recovered" if it reported OOM but
      // subsequently completed a full GC cycle. Since this test calls
      // reportOutOfMemory() and then crashes, we expect "Reported".
      //
      // Theoretically, GC can happen any time, so it is just possible that
      // this property could be "Recovered" even if the implementation is
      // correct. More likely, though, that indicates a bug, so only accept
      // "Reported".
      Assert.equal(extra.JSOutOfMemory, "Reported");
    },
    true
  );
});
