add_task(async function run_test() {
  // Ensure that attempting to override the exception handler doesn't cause
  // us to lose our exception handler.
  await do_crash(
    function() {
      CrashTestUtils.TryOverrideExceptionHandler();
    },
    function(mdump, extra) {},
    true
  );
});
