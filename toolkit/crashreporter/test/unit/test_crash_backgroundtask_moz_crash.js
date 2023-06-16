/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function run_test() {
  if (!AppConstants.MOZ_BACKGROUNDTASKS) {
    return;
  }

  // Try crashing background task with a runtime abort
  await do_backgroundtask_crash(
    CrashTestUtils.CRASH_MOZ_CRASH,
    { TestKey: "TestValue" },
    function (mdump, extra) {
      Assert.equal(extra.TestKey, "TestValue");
      Assert.equal(extra.BackgroundTaskMode, "1");
      Assert.equal(extra.BackgroundTaskName, "crash");
      Assert.equal(extra.HeadlessMode, "1");
      Assert.equal(false, "OOMAllocationSize" in extra);
      Assert.equal(false, "JSOutOfMemory" in extra);
      Assert.equal(false, "JSLargeAllocationFailure" in extra);
    },
    // process will exit with a zero exit status
    true
  );
});
