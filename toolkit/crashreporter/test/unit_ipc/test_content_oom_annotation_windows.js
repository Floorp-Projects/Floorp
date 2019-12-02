/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_content_annotation.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  // Try crashing with an OOM
  await do_content_crash(
    function() {
      crashType = CrashTestUtils.CRASH_OOM;
    },
    function(mdump, extra) {
      Assert.ok("SystemMemoryUsePercentage" in extra);
      Assert.ok("TotalVirtualMemory" in extra);
      Assert.ok("AvailableVirtualMemory" in extra);
      Assert.ok("TotalPageFile" in extra);
      Assert.ok("AvailablePageFile" in extra);
      Assert.ok("TotalPhysicalMemory" in extra);
      Assert.ok("AvailablePhysicalMemory" in extra);
    }
  );
});
