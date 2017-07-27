/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function test_cleared_alarm_does_not_fire() {
  async function backgroundScript() {
    let ALARM_NAME = "test_ext_alarms";

    browser.alarms.onAlarm.addListener(alarm => {
      browser.test.fail("cleared alarm does not fire");
      browser.test.notifyFail("alarm-cleared");
    });
    browser.alarms.create(ALARM_NAME, {when: Date.now() + 1000});

    let wasCleared = await browser.alarms.clear(ALARM_NAME);
    browser.test.assertTrue(wasCleared, "alarm was cleared");

    await new Promise(resolve => setTimeout(resolve, 2000));

    browser.test.notifyPass("alarm-cleared");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("alarm-cleared");
  await extension.unload();
});
