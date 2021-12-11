/* import-globals-from ../unit/head_crashreporter.js */
load("../unit/head_crashreporter.js");

add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_content_annotation.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  // TelemetrySession setup will trigger the session annotation
  let scope = {};
  ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", scope);
  scope.TelemetryController.testSetup();

  // Try crashing with a runtime abort
  await do_content_crash(
    function() {
      crashType = CrashTestUtils.CRASH_MOZ_CRASH;
      crashReporter.annotateCrashReport("TestKey", "TestValue");
      crashReporter.appendAppNotesToCrashReport("!!!foo!!!");
    },
    function(mdump, extra) {
      Assert.equal(extra.TestKey, "TestValue");
      Assert.ok("ProcessType" in extra);
      Assert.ok("StartupTime" in extra);
      Assert.ok("TelemetrySessionId" in extra);
      Assert.notEqual(extra.Notes.indexOf("!!!foo!!!"), -1);
    }
  );
});
