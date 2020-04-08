/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_content_large_annotation.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  // Try crashing with a runtime abort
  await do_content_crash(
    function() {
      crashType = CrashTestUtils.CRASH_MOZ_CRASH;
      crashReporter.annotateCrashReport("TestKey", "a".repeat(32768));
    },
    function(mdump, extra) {
      Assert.ok(
        extra.TestKey == "a".repeat(32768),
        "The TestKey annotation matches the expected value"
      );
    }
  );
});
