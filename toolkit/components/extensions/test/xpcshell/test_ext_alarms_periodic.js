/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function test_periodic_alarm_fires() {
  function backgroundScript() {
    const ALARM_NAME = "test_ext_alarms";
    let count = 0;
    let timer;

    browser.alarms.onAlarm.addListener(alarm => {
      browser.test.assertEq(
        alarm.name,
        ALARM_NAME,
        "alarm has the expected name"
      );
      if (count++ === 3) {
        clearTimeout(timer);
        browser.alarms.clear(ALARM_NAME).then(wasCleared => {
          browser.test.assertTrue(wasCleared, "alarm was cleared");

          browser.test.notifyPass("alarm-periodic");
        });
      }
    });

    browser.alarms.create(ALARM_NAME, { periodInMinutes: 0.02 });

    timer = setTimeout(async () => {
      browser.test.fail("alarm fired expected number of times");

      let wasCleared = await browser.alarms.clear(ALARM_NAME);
      browser.test.assertTrue(wasCleared, "alarm was cleared");

      browser.test.notifyFail("alarm-periodic");
    }, 30000);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("alarm-periodic");
  await extension.unload();
});
