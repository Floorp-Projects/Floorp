/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_duplicate_alarm_name_replaces_alarm() {
  function backgroundScript() {
    let count = 0;

    browser.alarms.onAlarm.addListener(async alarm => {
      browser.test.assertEq(
        "replaced alarm",
        alarm.name,
        "Expected last alarm"
      );
      browser.test.assertEq(
        0,
        count++,
        "duplicate named alarm replaced existing alarm"
      );
      let results = await browser.alarms.getAll();

      // "replaced alarm" is expected to be replaced with a non-repeating
      // alarm, so it should not appear in the list of alarms.
      browser.test.assertEq(1, results.length, "exactly one alarms exists");
      browser.test.assertEq(
        "unrelated alarm",
        results[0].name,
        "remaining alarm has the expected name"
      );

      browser.test.notifyPass("alarm-duplicate");
    });

    // Alarm that is so far in the future that it is never triggered.
    browser.alarms.create("unrelated alarm", { delayInMinutes: 60 });
    // Alarm that repeats.
    browser.alarms.create("replaced alarm", {
      delayInMinutes: 1 / 60,
      periodInMinutes: 1 / 60,
    });
    // Before the repeating alarm is triggered, it is immediately replaced with
    // a non-repeating alarm.
    browser.alarms.create("replaced alarm", { delayInMinutes: 3 / 60 });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("alarm-duplicate");
  await extension.unload();
});
