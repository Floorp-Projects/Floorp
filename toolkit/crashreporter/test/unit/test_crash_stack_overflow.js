add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_stack_overflow.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  // Try crashing by overflowing a thread's stack
  await do_crash(
    function () {
      crashType = CrashTestUtils.CRASH_STACK_OVERFLOW;
      crashReporter.annotateCrashReport("TestKey", "TestValue");
    },
    async function (mdump, extra) {
      Assert.equal(extra.TestKey, "TestValue");
    },
    // process will exit with a zero exit status
    true
  );
});
