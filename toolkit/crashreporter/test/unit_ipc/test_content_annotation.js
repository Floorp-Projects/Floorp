/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Components.classes)) {
    dump("INFO | test_content_annotation.js | Can't test crashreporter in a non-libxul build.\n");
    return;
  }

  // Try crashing with a runtime abort
  do_content_crash(function() {
                     crashType = CrashTestUtils.CRASH_MOZ_CRASH;
                     crashReporter.annotateCrashReport("TestKey", "TestValue");
                     crashReporter.appendAppNotesToCrashReport("!!!foo!!!");
                   },
                   function(mdump, extra) {
                     Assert.equal(extra.TestKey, "TestValue");
                     Assert.ok("StartupTime" in extra);
                     Assert.ok("ProcessType" in extra);
                     Assert.notEqual(extra.Notes.indexOf("!!!foo!!!"), -1);
                   });
}
