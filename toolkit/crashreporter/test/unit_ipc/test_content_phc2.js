/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_content_phc.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  // For some unknown reason, having two do_content_crash() calls in a single
  // test doesn't work. That explains why this test exists separately from
  // test_content_phc.js.
  do_content_crash(
    function() {
      crashType = CrashTestUtils.CRASH_PHC_DOUBLE_FREE;
    },
    function(mdump, extra) {
      Assert.equal(extra.PHCKind, "FreedPage");

      // This is a string holding a decimal address.
      Assert.ok(/^\d+$/.test(extra.PHCBaseAddress));

      // CRASH_PHC_DOUBLE_FREE uses 64 for the size.
      Assert.equal(extra.PHCUsableSize, 64);

      // These are strings holding comma-separated lists of decimal addresses.
      // Sometimes on Mac they have a single entry.
      Assert.ok(/^(\d+,)*\d+$/.test(extra.PHCAllocStack));
      Assert.ok(/^(\d+,)*\d+$/.test(extra.PHCFreeStack));
    }
  );
}
