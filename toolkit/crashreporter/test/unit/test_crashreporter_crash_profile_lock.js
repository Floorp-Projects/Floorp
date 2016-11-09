function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_crashreporter.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // lock a profile directory, crash, and ensure that
  // the profile lock signal handler doesn't interfere with
  // writing a minidump
  do_crash(function() {
             let env = Components.classes["@mozilla.org/process/environment;1"]
               .getService(Components.interfaces.nsIEnvironment);
             // the python harness sets this in the environment for us
             let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
             let dir = Components.classes["@mozilla.org/file/local;1"]
               .createInstance(Components.interfaces.nsILocalFile);
             dir.initWithPath(profd);
             CrashTestUtils.lockDir(dir);
             // when we crash, the lock file should be cleaned up
           },
           function(mdump, extra) {
             // if we got here, we have a minidump, so that's all we wanted
             do_check_true(true);
           });
}
