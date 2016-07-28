/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_alarm_without_permissions() {
  function backgroundScript() {
    browser.test.assertTrue(!browser.alarms,
                            "alarm API is not available when the alarm permission is not required");
    browser.test.notifyPass("alarms_permission");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: [],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("alarms_permission");
  yield extension.unload();
});


add_task(function* test_alarm_fires() {
  function backgroundScript() {
    let ALARM_NAME = "test_ext_alarms";
    let timer;

    browser.alarms.onAlarm.addListener(alarm => {
      browser.test.assertEq(ALARM_NAME, alarm.name, "alarm has the correct name");
      clearTimeout(timer);
      browser.test.notifyPass("alarm-fires");
    });

    browser.alarms.create(ALARM_NAME, {delayInMinutes: 0.02});

    timer = setTimeout(() => {
      browser.test.fail("alarm fired within expected time");
      browser.alarms.clear(ALARM_NAME).then(wasCleared => {
        browser.test.assertTrue(wasCleared, "alarm was cleared");
      });
      browser.test.notifyFail("alarm-fires");
    }, 10000);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("alarm-fires");
  yield extension.unload();
});


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


add_task(function* test_alarm_fires_with_when() {
  function backgroundScript() {
    let ALARM_NAME = "test_ext_alarms";
    let timer;

    browser.alarms.onAlarm.addListener(alarm => {
      browser.test.assertEq(ALARM_NAME, alarm.name, "alarm has the expected name");
      clearTimeout(timer);
      browser.test.notifyPass("alarm-when");
    });

    browser.alarms.create(ALARM_NAME, {when: Date.now() + 1000});

    timer = setTimeout(() => {
      browser.test.fail("alarm fired within expected time");
      browser.alarms.clear(ALARM_NAME).then(wasCleared => {
        browser.test.assertTrue(wasCleared, "alarm was cleared");
      });
      browser.test.notifyFail("alarm-when");
    }, 10000);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("alarm-when");
  yield extension.unload();
});


add_task(function* test_alarm_clear_non_matching_name() {
  function backgroundScript() {
    let ALARM_NAME = "test_ext_alarms";

    browser.alarms.create(ALARM_NAME, {when: Date.now() + 2000});

    browser.alarms.clear(ALARM_NAME + "1").then(wasCleared => {
      browser.test.assertFalse(wasCleared, "alarm was not cleared");
      return browser.alarms.getAll();
    }).then(alarms => {
      browser.test.assertEq(1, alarms.length, "alarm was not removed");
      browser.test.notifyPass("alarm-clear");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("alarm-clear");
  yield extension.unload();
});


add_task(function* test_alarm_get_and_clear_single_argument() {
  function backgroundScript() {
    browser.alarms.create({when: Date.now() + 2000});

    browser.alarms.get().then(alarm => {
      browser.test.assertEq("", alarm.name, "expected alarm returned");
      return browser.alarms.clear();
    }).then(wasCleared => {
      browser.test.assertTrue(wasCleared, "alarm was cleared");
      return browser.alarms.getAll();
    }).then(alarms => {
      browser.test.assertEq(0, alarms.length, "alarm was removed");
      browser.test.notifyPass("alarm-single-arg");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("alarm-single-arg");
  yield extension.unload();
});


add_task(function* test_periodic_alarm_fires() {
  function backgroundScript() {
    const ALARM_NAME = "test_ext_alarms";
    let count = 0;
    let timer;

    browser.alarms.onAlarm.addListener(alarm => {
      browser.test.assertEq(alarm.name, ALARM_NAME, "alarm has the expected name");
      if (count++ === 3) {
        clearTimeout(timer);
        browser.alarms.clear(ALARM_NAME).then(wasCleared => {
          browser.test.assertTrue(wasCleared, "alarm was cleared");
          browser.test.notifyPass("alarm-periodic");
        });
      }
    });

    browser.alarms.create(ALARM_NAME, {periodInMinutes: 0.02});

    timer = setTimeout(() => {
      browser.test.fail("alarm fired expected number of times");
      browser.alarms.clear(ALARM_NAME).then(wasCleared => {
        browser.test.assertTrue(wasCleared, "alarm was cleared");
      });
      browser.test.notifyFail("alarm-periodic");
    }, 30000);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("alarm-periodic");
  yield extension.unload();
});


add_task(function* test_get_get_all_clear_all_alarms() {
  function backgroundScript() {
    const ALARM_NAME = "test_alarm";

    let suffixes = [0, 1, 2];

    for (let suffix of suffixes) {
      browser.alarms.create(ALARM_NAME + suffix, {when: Date.now() + (suffix + 1) * 10000});
    }

    browser.alarms.getAll().then(alarms => {
      browser.test.assertEq(suffixes.length, alarms.length, "expected number of alarms were found");
      alarms.forEach((alarm, index) => {
        browser.test.assertEq(ALARM_NAME + index, alarm.name, "alarm has the expected name");
      });

      return Promise.all(
        suffixes.map(suffix => {
          return browser.alarms.get(ALARM_NAME + suffix).then(alarm => {
            browser.test.assertEq(ALARM_NAME + suffix, alarm.name, "alarm has the expected name");
            browser.test.sendMessage(`get-${suffix}`);
          });
        })
      );
    }).then(() => {
      return browser.alarms.clear(ALARM_NAME + suffixes[0]);
    }).then(wasCleared => {
      browser.test.assertTrue(wasCleared, "alarm was cleared");

      return browser.alarms.getAll();
    }).then(alarms => {
      browser.test.assertEq(2, alarms.length, "alarm was removed");

      return browser.alarms.get(ALARM_NAME + suffixes[0]);
    }).then(alarm => {
      browser.test.assertEq(undefined, alarm, "non-existent alarm is undefined");
      browser.test.sendMessage(`get-invalid`);

      return browser.alarms.clearAll();
    }).then(wasCleared => {
      browser.test.assertTrue(wasCleared, "alarms were cleared");

      return browser.alarms.getAll();
    }).then(alarms => {
      browser.test.assertEq(0, alarms.length, "no alarms exist");
      browser.test.sendMessage("clearAll");
      browser.test.sendMessage("clear");
      browser.test.sendMessage("getAll");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  yield Promise.all([
    extension.startup(),
    extension.awaitMessage("getAll"),
    extension.awaitMessage("get-0"),
    extension.awaitMessage("get-1"),
    extension.awaitMessage("get-2"),
    extension.awaitMessage("clear"),
    extension.awaitMessage("get-invalid"),
    extension.awaitMessage("clearAll"),
  ]);
  yield extension.unload();
});

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
