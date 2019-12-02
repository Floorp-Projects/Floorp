add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_purevirtual.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  var isOSX = "nsILocalFileMac" in Ci;
  if (isOSX) {
    dump(
      "INFO | test_crash_purevirtual.js | TODO: purecalls not caught on OS X\n"
    );
    return;
  }

  // Try crashing with a pure virtual call
  await do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_PURE_VIRTUAL_CALL;
      crashReporter.annotateCrashReport("TestKey", "TestValue");
    },
    function(mdump, extra) {
      Assert.equal(extra.TestKey, "TestValue");
    },
    // process will exit with a zero exit status
    true
  );
});
