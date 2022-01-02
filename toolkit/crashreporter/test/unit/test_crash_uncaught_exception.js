/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function run_test() {
  // Try crashing with an uncaught exception.
  await do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_UNCAUGHT_EXCEPTION;
      crashReporter.annotateCrashReport("TestKey", "TestValue");
    },
    function(mdump, extra) {
      Assert.equal(extra.TestKey, "TestValue");
    },
    // process will exit with a zero exit status
    true
  );
});
