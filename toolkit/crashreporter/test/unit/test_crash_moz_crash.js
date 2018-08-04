function run_test() {
  // Try crashing with a runtime abort
  do_crash(function() {
             crashType = CrashTestUtils.CRASH_MOZ_CRASH;
             crashReporter.annotateCrashReport("TestKey", "TestValue");
           },
           function(mdump, extra) {
             Assert.equal(extra.TestKey, "TestValue");
             Assert.equal(false, "OOMAllocationSize" in extra);
             Assert.equal(false, "JSOutOfMemory" in extra);
             Assert.equal(false, "JSLargeAllocationFailure" in extra);
           },
          // process will exit with a zero exit status
          true);
}
