/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_content_phc.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  // For some unknown reason, having multiple do_content_crash() calls in a
  // single test doesn't work. That explains why this test exists separately
  // from test_content_phc.js.
  await do_content_crash(
    function() {
      crashType = CrashTestUtils.CRASH_PHC_BOUNDS_VIOLATION;
    },
    function(mdump, extra) {
      Assert.equal(extra.PHCKind, "GuardPage");

      // This is a string holding a decimal address.
      Assert.ok(/^\d+$/.test(extra.PHCBaseAddress));

      // CRASH_PHC_BOUNDS_VIOLATION uses 96 for the size.
      Assert.equal(extra.PHCUsableSize, 96);

      // This is a string holding a comma-separated list of decimal addresses.
      // Sometimes on Mac it has a single entry.
      Assert.ok(/^(\d+,)*\d+$/.test(extra.PHCAllocStack));

      Assert.ok(!extra.hasOwnProperty("PHCFreeStack"));
    }
  );
});
