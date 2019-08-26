/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_content_phc.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  do_content_crash(
    function() {
      crashType = CrashTestUtils.CRASH_PHC_USE_AFTER_FREE;
    },
    function(mdump, extra) {
      Assert.equal(extra.PHCKind, "FreedPage");

      // This is a string holding a decimal address.
      Assert.ok(/^\d+$/.test(extra.PHCBaseAddress));

      // CRASH_PHC_USE_AFTER_FREE uses 32 for the size.
      Assert.equal(extra.PHCUsableSize, 32);

      // These are strings holding comma-separated lists of decimal addresses.
      // Sometimes on Mac they have a single entry.
      Assert.ok(/^(\d+,)*\d+$/.test(extra.PHCAllocStack));
      Assert.ok(/^(\d+,)*\d+$/.test(extra.PHCFreeStack));
    }
  );
}
