add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_thread_annotation.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  await do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_INVALID_POINTER_DEREF;
    },
    function(mdump, extra) {
      Assert.ok("ThreadIdNameMapping" in extra);
    },
    true
  );
});
