function check(extra, size) {
  Assert.equal(extra.PHCKind, "FreedPage");

  // This is a string holding a decimal address.
  Assert.ok(/^\d+$/.test(extra.PHCBaseAddress));

  Assert.equal(extra.PHCUsableSize, size);

  // These are strings holding comma-separated lists of decimal addresses.
  // Sometimes on Mac they have a single entry.
  Assert.ok(/^(\d+,)*\d+$/.test(extra.PHCAllocStack));
  Assert.ok(/^(\d+,)*\d+$/.test(extra.PHCFreeStack));
}

function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_phc.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_PHC_USE_AFTER_FREE;
    },
    function(mdump, extra) {
      // CRASH_PHC_USE_AFTER_FREE uses 32 for the size.
      check(extra, 32);
    },
    true
  );

  do_crash(
    function() {
      crashType = CrashTestUtils.CRASH_PHC_DOUBLE_FREE;
    },
    function(mdump, extra) {
      // CRASH_PHC_DOUBLE_FREE uses 64 for the size.
      check(extra, 64);
    },
    true
  );
}
