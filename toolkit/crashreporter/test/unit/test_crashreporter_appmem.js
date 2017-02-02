function run_test() {
  do_crash(function() {
    let appAddr = CrashTestUtils.saveAppMemory();
    crashReporter.registerAppMemory(appAddr, 32);
  },
  function(mdump, extra) {
    do_check_true(mdump.exists());
    do_check_true(mdump.fileSize > 0);
    do_check_true(CrashTestUtils.dumpCheckMemory(mdump.path));
  });
}
