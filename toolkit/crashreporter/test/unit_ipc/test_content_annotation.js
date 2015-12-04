load("../unit/head_crashreporter.js");

function run_test()
{
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_content_annotation.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // Try crashing with a pure virtual call
  do_content_crash(function() {
                     crashType = CrashTestUtils.CRASH_RUNTIMEABORT;
                     crashReporter.annotateCrashReport("TestKey", "TestValue");
                     crashReporter.appendAppNotesToCrashReport("!!!foo!!!");
		   },
                   function(mdump, extra) {
                     do_check_eq(extra.TestKey, "TestValue");
                     do_check_true('StartupTime' in extra);
                     do_check_true('ProcessType' in extra);
                     do_check_neq(extra.Notes.indexOf("!!!foo!!!"), -1);
                   });
}