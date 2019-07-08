/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://testing-common/AppData.jsm", this);

add_task(async function test_setup() {
  do_get_profile();
  await makeFakeAppDir();
});

add_task(async function test_main_process_crash() {
  let cm = Services.crashmanager;
  Assert.ok(cm, "CrashManager available.");

  let basename;
  let count = await new Promise((resolve, reject) => {
    do_crash(
      function() {
        // TelemetrySession setup will trigger the session annotation
        let scope = {};
        ChromeUtils.import(
          "resource://gre/modules/TelemetryController.jsm",
          scope
        );
        scope.TelemetryController.testSetup();
        crashType = CrashTestUtils.CRASH_MOZ_CRASH;
        crashReporter.annotateCrashReport("ShutdownProgress", "event-test");
      },
      (minidump, extra) => {
        basename = minidump.leafName;
        Object.defineProperty(cm, "_eventsDirs", { value: [getEventDir()] });
        cm.aggregateEventsFiles().then(resolve, reject);
      },
      true
    );
  });
  Assert.equal(count, 1, "A single crash event file was seen.");
  let crashes = await cm.getCrashes();
  Assert.equal(crashes.length, 1);
  let crash = crashes[0];
  Assert.ok(crash.isOfType(cm.PROCESS_TYPE_MAIN, cm.CRASH_TYPE_CRASH));
  Assert.equal(crash.id + ".dmp", basename, "ID recorded properly");
  Assert.equal(crash.metadata.ShutdownProgress, "event-test");
  Assert.ok("TelemetrySessionId" in crash.metadata);
  Assert.ok("UptimeTS" in crash.metadata);
  Assert.ok(
    /^[0-9a-f]{8}\-([0-9a-f]{4}\-){3}[0-9a-f]{12}$/.test(
      crash.metadata.TelemetrySessionId
    )
  );
  Assert.ok("CrashTime" in crash.metadata);
  Assert.ok(/^\d+$/.test(crash.metadata.CrashTime));
});
