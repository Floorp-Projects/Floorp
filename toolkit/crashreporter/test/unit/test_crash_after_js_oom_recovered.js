add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_after_js_oom_recovered.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  await do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_MOZ_CRASH;
      crashReporter.annotateCrashReport("TestKey", "Yes");
      Cu.getJSTestingFunctions().reportOutOfMemory();
      Cu.forceGC();
    },
    function(mdump, extra) {
      Assert.equal(extra.TestKey, "Yes");
      Assert.equal(extra.JSOutOfMemory, "Recovered");
    },
    true
  );
});
