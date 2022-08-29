/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

add_task(
  {
    // TODO(Bug 1725478): remove the skip if once webidl API bindings will be hidden based on permissions.
    skip_if: () => ExtensionTestUtils.isInBackgroundServiceWorkerTests(),
  },
  async function test_alarm_without_permissions() {
    function backgroundScript() {
      browser.test.assertTrue(
        !browser.alarms,
        "alarm API is not available when the alarm permission is not required"
      );
      browser.test.notifyPass("alarms_permission");
    }

    let extension = ExtensionTestUtils.loadExtension({
      background: `(${backgroundScript})()`,
      manifest: {
        permissions: [],
      },
    });

    await extension.startup();
    await extension.awaitFinish("alarms_permission");
    await extension.unload();
  }
);

add_task(async function test_alarm_clear_non_matching_name() {
  async function backgroundScript() {
    let ALARM_NAME = "test_ext_alarms";

    browser.alarms.create(ALARM_NAME, { when: Date.now() + 2000000 });

    let wasCleared = await browser.alarms.clear(ALARM_NAME + "1");
    browser.test.assertFalse(wasCleared, "alarm was not cleared");

    let alarms = await browser.alarms.getAll();
    browser.test.assertEq(1, alarms.length, "alarm was not removed");
    browser.test.notifyPass("alarm-clear");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("alarm-clear");
  await extension.unload();
});

add_task(async function test_alarm_get_and_clear_single_argument() {
  async function backgroundScript() {
    browser.alarms.create({ when: Date.now() + 2000000 });

    let alarm = await browser.alarms.get();
    browser.test.assertEq("", alarm.name, "expected alarm returned");

    let wasCleared = await browser.alarms.clear();
    browser.test.assertTrue(wasCleared, "alarm was cleared");

    let alarms = await browser.alarms.getAll();
    browser.test.assertEq(0, alarms.length, "alarm was removed");

    browser.test.notifyPass("alarm-single-arg");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("alarm-single-arg");
  await extension.unload();
});

// This test case covers the behavior of browser.alarms.create when the
// first optional argument (the alarm name) is passed explicitly as null
// or undefined instead of being omitted.
add_task(async function test_alarm_name_arg_null_or_undefined() {
  async function backgroundScript(alarmName) {
    browser.alarms.create(alarmName, { when: Date.now() + 2000000 });

    let alarm = await browser.alarms.get();
    browser.test.assertTrue(alarm, "got an alarm");
    browser.test.assertEq("", alarm.name, "expected alarm returned");

    let wasCleared = await browser.alarms.clear();
    browser.test.assertTrue(wasCleared, "alarm was cleared");

    let alarms = await browser.alarms.getAll();
    browser.test.assertEq(0, alarms.length, "alarm was removed");

    browser.test.notifyPass("alarm-test-done");
  }

  for (const alarmName of [null, undefined]) {
    info(`Test alarm.create with alarm name ${alarmName}`);
    let extension = ExtensionTestUtils.loadExtension({
      background: `(${backgroundScript})(${alarmName})`,
      manifest: {
        permissions: ["alarms"],
      },
    });
    await extension.startup();
    await extension.awaitFinish("alarm-test-done");
    await extension.unload();
  }
});

add_task(async function test_get_get_all_clear_all_alarms() {
  async function backgroundScript() {
    const ALARM_NAME = "test_alarm";

    let suffixes = [0, 1, 2];

    for (let suffix of suffixes) {
      browser.alarms.create(ALARM_NAME + suffix, {
        when: Date.now() + (suffix + 1) * 10000,
      });
    }

    let alarms = await browser.alarms.getAll();
    browser.test.assertEq(
      suffixes.length,
      alarms.length,
      "expected number of alarms were found"
    );
    alarms.forEach((alarm, index) => {
      browser.test.assertEq(
        ALARM_NAME + index,
        alarm.name,
        "alarm has the expected name"
      );
    });

    for (let suffix of suffixes) {
      let alarm = await browser.alarms.get(ALARM_NAME + suffix);
      browser.test.assertEq(
        ALARM_NAME + suffix,
        alarm.name,
        "alarm has the expected name"
      );
      browser.test.sendMessage(`get-${suffix}`);
    }

    let wasCleared = await browser.alarms.clear(ALARM_NAME + suffixes[0]);
    browser.test.assertTrue(wasCleared, "alarm was cleared");

    alarms = await browser.alarms.getAll();
    browser.test.assertEq(2, alarms.length, "alarm was removed");

    let alarm = await browser.alarms.get(ALARM_NAME + suffixes[0]);
    browser.test.assertEq(undefined, alarm, "non-existent alarm is undefined");
    browser.test.sendMessage(`get-invalid`);

    wasCleared = await browser.alarms.clearAll();
    browser.test.assertTrue(wasCleared, "alarms were cleared");

    alarms = await browser.alarms.getAll();
    browser.test.assertEq(0, alarms.length, "no alarms exist");
    browser.test.sendMessage("clearAll");
    browser.test.sendMessage("clear");
    browser.test.sendMessage("getAll");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${backgroundScript})()`,
    manifest: {
      permissions: ["alarms"],
    },
  });

  await Promise.all([
    extension.startup(),
    extension.awaitMessage("getAll"),
    extension.awaitMessage("get-0"),
    extension.awaitMessage("get-1"),
    extension.awaitMessage("get-2"),
    extension.awaitMessage("clear"),
    extension.awaitMessage("get-invalid"),
    extension.awaitMessage("clearAll"),
  ]);
  await extension.unload();
});

function getAlarmExtension(alarmCreateOptions, extOpts = {}) {
  info(
    `Test alarms.create fires with options: ${JSON.stringify(
      alarmCreateOptions
    )}`
  );

  function backgroundScript(createOptions) {
    let ALARM_NAME = "test_ext_alarms";
    let timer;

    browser.alarms.onAlarm.addListener(alarm => {
      browser.test.assertEq(
        ALARM_NAME,
        alarm.name,
        "alarm has the expected name"
      );
      clearTimeout(timer);
      browser.test.sendMessage("alarms-create-with-options");
    });

    browser.alarms.create(ALARM_NAME, createOptions);

    timer = setTimeout(async () => {
      browser.test.fail("alarm fired within expected time");
      let wasCleared = await browser.alarms.clear(ALARM_NAME);
      browser.test.assertTrue(wasCleared, "alarm was cleared");
      browser.test.sendMessage("alarms-create-with-options");
    }, 10000);
  }

  let { persistent, useAddonManager } = extOpts;
  return ExtensionTestUtils.loadExtension({
    useAddonManager,
    // Pass the alarms.create options to the background page.
    background: `(${backgroundScript})(${JSON.stringify(alarmCreateOptions)})`,
    manifest: {
      permissions: ["alarms"],
      background: { persistent },
    },
  });
}

async function test_alarm_fires_with_options(alarmCreateOptions) {
  let extension = getAlarmExtension(alarmCreateOptions);

  await extension.startup();
  await extension.awaitMessage("alarms-create-with-options");

  // Defer unloading the extension so the asynchronous event listener
  // reply finishes.
  await new Promise(resolve => setTimeout(resolve, 0));
  await extension.unload();
}

add_task(async function test_alarm_fires() {
  Services.prefs.setBoolPref(
    "privacy.resistFingerprinting.reduceTimerPrecision.jitter",
    false
  );

  await test_alarm_fires_with_options({ delayInMinutes: 0.01 });
  await test_alarm_fires_with_options({ when: Date.now() + 1000 });
  await test_alarm_fires_with_options({ delayInMinutes: -10 });
  await test_alarm_fires_with_options({ when: Date.now() - 1000 });

  Services.prefs.clearUserPref(
    "privacy.resistFingerprinting.reduceTimerPrecision.jitter"
  );
});

function trackEvents(wrapper) {
  let events = new Map();
  for (let event of ["background-script-event", "start-background-script"]) {
    events.set(event, false);
    wrapper.extension.once(event, () => events.set(event, true));
  }
  return events;
}

add_task(
  {
    // TODO(Bug 1748665): remove the skip once background service worker is also
    // woken up by persistent listeners.
    skip_if: () => ExtensionTestUtils.isInBackgroundServiceWorkerTests(),
    pref_set: [
      ["privacy.resistFingerprinting.reduceTimerPrecision.jitter", false],
      ["extensions.eventPages.enabled", true],
    ],
  },
  async function test_alarm_persists() {
    await AddonTestUtils.promiseStartupManager();

    let extension = getAlarmExtension(
      { periodInMinutes: 0.01 },
      { useAddonManager: "permanent", persistent: false }
    );
    info(`wait startup`);
    await extension.startup();
    assertPersistentListeners(extension, "alarms", "onAlarm", {
      primed: false,
    });
    info(`wait first alarm`);
    await extension.awaitMessage("alarms-create-with-options");

    await extension.terminateBackground({ disableResetIdleForTest: true });
    ok(
      !extension.extension.backgroundContext,
      "Background Extension context should have been destroyed"
    );

    assertPersistentListeners(extension, "alarms", "onAlarm", {
      primed: true,
    });

    // Test an early startup event
    let events = trackEvents(extension);
    ok(
      !events.get("background-script-event"),
      "Should not have received a background script event"
    );
    ok(
      !events.get("start-background-script"),
      "Background script should not be started"
    );

    info("waiting for alarm to wake background");
    await extension.awaitMessage("alarms-create-with-options");
    ok(
      events.get("background-script-event"),
      "Should have received a background script event"
    );
    ok(
      events.get("start-background-script"),
      "Background script should be started"
    );

    await extension.unload();
    await AddonTestUtils.promiseShutdownManager();
  }
);
