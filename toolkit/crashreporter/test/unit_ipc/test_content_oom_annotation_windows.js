load("../unit/head_crashreporter.js");

function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_content_annotation.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // Try crashing with an OOM
  do_content_crash(function() {
                     crashType = CrashTestUtils.CRASH_OOM;
                   },
                   function(mdump, extra) {
                     do_check_true("SystemMemoryUsePercentage" in extra);
                     do_check_true("TotalVirtualMemory" in extra);
                     do_check_true("AvailableVirtualMemory" in extra);
                     do_check_true("TotalPageFile" in extra);
                     do_check_true("AvailablePageFile" in extra);
                     do_check_true("TotalPhysicalMemory" in extra);
                     do_check_true("AvailablePhysicalMemory" in extra);
                   });
}
