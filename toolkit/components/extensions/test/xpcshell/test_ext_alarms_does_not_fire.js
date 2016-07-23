/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_cleared_alarm_does_not_fire() {
  function backgroundScript() {
    let ALARM_NAME = "test_ext_alarms";

    browser.alarms.onAlarm.addListener(alarm => {
      browser.test.fail("cleared alarm does not fire");
      browser.test.notifyFail("alarm-cleared");
    });
    browser.alarms.create(ALARM_NAME, {when: Date.now() + 1000});

    browser.alarms.clear(ALARM_NAME).then(wasCleared => {
      browser.test.assertTrue(wasCleared, "alarm was cleared");
      setTimeout(() => {
        browser.test.notifyPass("alarm-cleared");
      }, 2000);
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("alarm-cleared");
  yield extension.unload();
});
