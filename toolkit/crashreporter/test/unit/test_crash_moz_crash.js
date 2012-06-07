function run_test()
{
  // Try crashing with a runtime abort
  do_crash(function() {
             crashType = CrashTestUtils.CRASH_MOZ_CRASH;
             crashReporter.annotateCrashReport("TestKey", "TestValue");
           },
           function(mdump, extra) {
             do_check_eq(extra.TestKey, "TestValue");
             do_check_false("OOMAllocationSize" in extra);
           },
          // process will exit with a zero exit status
          true);
}
