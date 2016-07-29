/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


add_task(function* test_duplicate_alarm_name_replaces_alarm() {
  function backgroundScript() {
    let count = 0;

    browser.alarms.onAlarm.addListener(alarm => {
      if (alarm.name === "master alarm") {
        browser.alarms.create("child alarm", {delayInMinutes: 0.05});
        browser.alarms.getAll().then(results => {
          browser.test.assertEq(2, results.length, "exactly two alarms exist");
          browser.test.assertEq("master alarm", results[0].name, "first alarm has the expected name");
          browser.test.assertEq("child alarm", results[1].name, "second alarm has the expected name");
        }).then(() => {
          if (count++ === 3) {
            browser.alarms.clear("master alarm").then(wasCleared => {
              return browser.alarms.clear("child alarm");
            }).then(wasCleared => {
              browser.test.notifyPass("alarm-duplicate");
            });
          }
        });
      } else {
        browser.test.fail("duplicate named alarm replaced existing alarm");
        browser.test.notifyFail("alarm-duplicate");
      }
    });

    browser.alarms.create("master alarm", {delayInMinutes: 0.025, periodInMinutes: 0.025});
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("alarm-duplicate");
  yield extension.unload();
});
