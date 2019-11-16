add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_oom.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  await do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_OOM;
      crashReporter.annotateCrashReport("TestKey", "Yes");
    },
    function(mdump, extra) {
      Assert.equal(extra.TestKey, "Yes");
      Assert.ok("OOMAllocationSize" in extra);
      Assert.ok(Number(extra.OOMAllocationSize) > 0);
    },
    true
  );
});
