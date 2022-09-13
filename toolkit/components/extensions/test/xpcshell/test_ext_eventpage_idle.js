"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionPreferencesManager",
  "resource://gre/modules/ExtensionPreferencesManager.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

Services.prefs.setBoolPref("extensions.eventPages.enabled", true);
// Set minimum idle timeout for testing
Services.prefs.setIntPref("extensions.background.idle.timeout", 0);

// Expected rejection from the test cases defined in this file.
PromiseTestUtils.allowMatchingRejectionsGlobally(/expected-test-rejection/);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Actor 'Conduits' destroyed before query 'RunListener' was resolved/
);

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_eventpage_idle() {
  clearHistograms();

  assertHistogramEmpty(WEBEXT_EVENTPAGE_RUNNING_TIME_MS);
  assertKeyedHistogramEmpty(WEBEXT_EVENTPAGE_RUNNING_TIME_MS_BY_ADDONID);
  assertHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT);
  assertKeyedHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID);

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["browserSettings"],
      background: { persistent: false },
    },
    background() {
      browser.browserSettings.allowPopupsForUserEvents.onChange.addListener(
        () => {
          browser.test.sendMessage("allowPopupsForUserEvents");
        }
      );
      browser.runtime.onSuspend.addListener(async () => {
        let setting = await browser.browserSettings.allowPopupsForUserEvents.get(
          {}
        );
        browser.test.sendMessage("suspended", setting);
      });
    },
  });
  await extension.startup();
  assertPersistentListeners(
    extension,
    "browserSettings",
    "allowPopupsForUserEvents",
    {
      primed: false,
    }
  );

  info(`test idle timeout after startup`);
  await extension.awaitMessage("suspended");
  await promiseExtensionEvent(extension, "shutdown-background-script");
  assertPersistentListeners(
    extension,
    "browserSettings",
    "allowPopupsForUserEvents",
    {
      primed: true,
    }
  );
  ExtensionPreferencesManager.setSetting(
    extension.id,
    "allowPopupsForUserEvents",
    "click"
  );
  await extension.awaitMessage("allowPopupsForUserEvents");
  ok(true, "allowPopupsForUserEvents.onChange fired");

  // again after the event is fired
  info(`test idle timeout after wakeup`);
  let setting = await extension.awaitMessage("suspended");
  equal(setting.value, true, "verify simple async wait works in onSuspend");

  await promiseExtensionEvent(extension, "shutdown-background-script");
  assertPersistentListeners(
    extension,
    "browserSettings",
    "allowPopupsForUserEvents",
    {
      primed: true,
    }
  );
  ExtensionPreferencesManager.setSetting(
    extension.id,
    "allowPopupsForUserEvents",
    false
  );
  await extension.awaitMessage("allowPopupsForUserEvents");
  ok(true, "allowPopupsForUserEvents.onChange fired");

  const { id } = extension;
  await extension.unload();

  info("Verify eventpage telemetry recorded");

  assertHistogramSnapshot(
    WEBEXT_EVENTPAGE_RUNNING_TIME_MS,
    {
      keyed: false,
      processSnapshot: snapshot => snapshot.sum > 0,
      expectedValue: true,
    },
    `Expect stored values in the eventpage running time non-keyed histogram snapshot`
  );

  assertHistogramSnapshot(
    WEBEXT_EVENTPAGE_RUNNING_TIME_MS_BY_ADDONID,
    {
      keyed: true,
      processSnapshot: snapshot => snapshot[id]?.sum > 0,
      expectedValue: true,
    },
    `Expect stored values for addon with id ${id} in the eventpage running time keyed histogram snapshot`
  );

  assertHistogramCategoryNotEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT, {
    category: "suspend",
    categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
  });

  assertHistogramCategoryNotEmpty(
    WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID,
    {
      keyed: true,
      key: id,
      category: "suspend",
      categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
    }
  );
});

add_task(
  { pref_set: [["extensions.webextensions.runtime.timeout", 500]] },
  async function test_eventpage_runtime_onSuspend_timeout() {
    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      manifest: {
        background: { persistent: false },
      },
      background() {
        browser.runtime.onSuspend.addListener(() => {
          // return a promise that never resolves
          return new Promise(() => {});
        });
      },
    });
    await extension.startup();
    await promiseExtensionEvent(extension, "shutdown-background-script");
    ok(true, "onSuspend did not block background shutdown");
    await extension.unload();
  }
);

add_task(
  { pref_set: [["extensions.webextensions.runtime.timeout", 500]] },
  async function test_eventpage_runtime_onSuspend_reject() {
    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      manifest: {
        background: { persistent: false },
      },
      background() {
        browser.runtime.onSuspend.addListener(() => {
          // Raise an error to test error handling in onSuspend
          return Promise.reject("testing reject");
        });
      },
    });
    await extension.startup();
    await promiseExtensionEvent(extension, "shutdown-background-script");
    ok(true, "onSuspend did not block background shutdown");
    await extension.unload();
  }
);

add_task(
  { pref_set: [["extensions.webextensions.runtime.timeout", 1000]] },
  async function test_eventpage_runtime_onSuspend_canceled() {
    clearHistograms();

    assertHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT);
    assertKeyedHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID);

    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      manifest: {
        permissions: ["browserSettings"],
        background: { persistent: false },
      },
      background() {
        let resolveSuspend;
        browser.browserSettings.allowPopupsForUserEvents.onChange.addListener(
          () => {
            browser.test.sendMessage("allowPopupsForUserEvents");
          }
        );
        browser.runtime.onSuspend.addListener(() => {
          browser.test.sendMessage("suspending");
          return new Promise(resolve => {
            resolveSuspend = resolve;
          });
        });
        browser.runtime.onSuspendCanceled.addListener(() => {
          browser.test.sendMessage("suspendCanceled");
        });
        browser.test.onMessage.addListener(() => {
          resolveSuspend();
        });
      },
    });
    await extension.startup();
    await extension.awaitMessage("suspending");
    // While suspending, cause an event
    ExtensionPreferencesManager.setSetting(
      extension.id,
      "allowPopupsForUserEvents",
      "click"
    );
    extension.sendMessage("resolveSuspend");
    await extension.awaitMessage("allowPopupsForUserEvents");
    await extension.awaitMessage("suspendCanceled");
    ok(true, "event caused suspend-canceled");

    assertHistogramCategoryNotEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT, {
      category: "reset_event",
      categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
    });

    assertHistogramCategoryNotEmpty(
      WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID,
      {
        keyed: true,
        key: extension.id,
        category: "reset_event",
        categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
      }
    );

    await extension.awaitMessage("suspending");
    await promiseExtensionEvent(extension, "shutdown-background-script");
    await extension.unload();
  }
);

add_task(async function test_terminateBackground_after_extension_hasShutdown() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      background: { persistent: false },
    },
    async background() {
      browser.runtime.onSuspend.addListener(() => {
        browser.test.fail(
          `runtime.onSuspend listener should have not been called`
        );
      });

      // Call an API method implemented in the parent process (to be sure runtime.onSuspend
      // listener is going to be fully registered from a parent process perspective by the
      // time we will send the "bg-ready" test message).
      await browser.runtime.getBrowserInfo();

      browser.test.sendMessage("bg-ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg-ready");

  // Fake suspending event page on idle while the extension was being shutdown by manually
  // setting the hasShutdown flag to true on the Extension class instance object.
  extension.extension.hasShutdown = true;
  await extension.terminateBackground();
  extension.extension.hasShutdown = false;

  await extension.unload();
});

add_task(async function test_wakeupBackground_after_extension_hasShutdown() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      background: { persistent: false },
    },
    async background() {
      browser.test.sendMessage("bg-ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg-ready");
  await extension.terminateBackground();

  // Fake suspending event page on idle while the extension was being shutdown by manually
  // setting the hasShutdown flag to true on the Extension class instance object.
  extension.extension.hasShutdown = true;
  await Assert.rejects(
    extension.wakeupBackground(),
    /wakeupBackground called while the extension was already shutting down/,
    "Got the expected rejection when wakeupBackground is called after extension shutdown"
  );
  extension.extension.hasShutdown = false;

  await extension.unload();
});

async function testSuspendShutdownRace({ manifest_version }) {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version,
      background: manifest_version === 2 ? { persistent: false } : {},
      permissions: ["webRequest", "webRequestBlocking"],
      host_permissions: ["*://example.com/*"],
      granted_host_permissions: true,
    },
    // Define an empty background script.
    background() {},
  });

  await extension.startup();
  await extension.extension.promiseBackgroundStarted();
  const promiseTerminateBackground = extension.extension.terminateBackground();
  // Wait one tick to leave to terminateBackground async method time to get
  // past the first check that returns earlier if extension.hasShutdown is true.
  await Promise.resolve();
  const promiseUnload = extension.unload();

  await promiseUnload;
  try {
    await promiseTerminateBackground;
    ok(true, "extension.terminateBackground should not have been rejected");
  } catch (err) {
    ok(
      false,
      `extension.terminateBackground should not have been rejected: ${err} :: ${err.stack}`
    );
  }
}

add_task(function test_mv2_suspend_shutdown_race() {
  return testSuspendShutdownRace({ manifest_version: 2 });
});

add_task(
  {
    pref_set: [["extensions.manifestV3.enabled", true]],
  },
  function test_mv3_suspend_shutdown_race() {
    return testSuspendShutdownRace({ manifest_version: 3 });
  }
);

function createPendingListenerTestExtension() {
  return ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["browserSettings"],
      background: { persistent: false },
    },
    background() {
      let idx = 0;
      browser.browserSettings.allowPopupsForUserEvents.onChange.addListener(
        async () => {
          const currIdx = idx++;
          await new Promise((resolve, reject) => {
            browser.test.onMessage.addListener(msg => {
              switch (`${msg}-${currIdx}`) {
                case "unblock-promise-0":
                  resolve();
                  browser.test.sendMessage("allowPopupsForUserEvents:resolved");
                  break;
                case "unblock-promise-1":
                  reject(new Error("expected-test-rejection"));
                  browser.test.sendMessage("allowPopupsForUserEvents:rejected");
                  break;
                default:
                  browser.test.fail(`Unexpected test message: ${msg}`);
              }
            });
            browser.test.sendMessage("allowPopupsForUserEvents:awaiting");
          });
        }
      );

      browser.runtime.onSuspend.addListener(() => {
        // Raise an error to test error handling in onSuspend
        return browser.test.sendMessage("runtime-on-suspend");
      });

      browser.test.sendMessage("bg-script-ready");
    },
  });
}

add_task(
  { pref_set: [["extensions.background.idle.timeout", 500]] },
  async function test_eventpage_idle_reset_on_async_listener_unresolved() {
    clearHistograms();

    assertHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT);
    assertKeyedHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID);

    let extension = createPendingListenerTestExtension();
    await extension.startup();
    await extension.awaitMessage("bg-script-ready");

    info("Trigger the first API event listener call");
    ExtensionPreferencesManager.setSetting(
      extension.id,
      "allowPopupsForUserEvents",
      "click"
    );

    await extension.awaitMessage("allowPopupsForUserEvents:awaiting");

    info("Trigger the second API event listener call");
    ExtensionPreferencesManager.setSetting(
      extension.id,
      "allowPopupsForUserEvents",
      "click"
    );

    await extension.awaitMessage("allowPopupsForUserEvents:awaiting");

    info("Wait for suspend on idle to be reset");
    const [, resetIdleData] = await promiseExtensionEvent(
      extension,
      "background-script-reset-idle"
    );

    Assert.deepEqual(
      resetIdleData,
      {
        reason: "pendingListeners",
        pendingListeners: 2,
      },
      "Got the expected idle reset reason and pendingListeners count"
    );

    assertHistogramCategoryNotEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT, {
      category: "reset_listeners",
      categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
    });

    assertHistogramCategoryNotEmpty(
      WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID,
      {
        keyed: true,
        key: extension.id,
        category: "reset_listeners",
        categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
      }
    );

    info(
      "Resolve the async listener pending on a promise and expect the event page to suspend after the idle timeout"
    );
    extension.sendMessage("unblock-promise");
    // Expect the two promises to be resolved and rejected respectively.
    await extension.awaitMessage("allowPopupsForUserEvents:resolved");
    await extension.awaitMessage("allowPopupsForUserEvents:rejected");

    info("Await for the runtime.onSuspend event to be emitted");
    await extension.awaitMessage("runtime-on-suspend");
    await extension.unload();
  }
);

add_task(
  { pref_set: [["extensions.background.idle.timeout", 500]] },
  async function test_pending_async_listeners_promises_rejected_on_shutdown() {
    let extension = createPendingListenerTestExtension();
    await extension.startup();
    await extension.awaitMessage("bg-script-ready");

    info("Trigger the API event listener call");
    ExtensionPreferencesManager.setSetting(
      extension.id,
      "allowPopupsForUserEvents",
      "click"
    );

    await extension.awaitMessage("allowPopupsForUserEvents:awaiting");

    const { runListenerPromises } = extension.extension.backgroundContext;
    equal(
      runListenerPromises.size,
      1,
      "Got the expected number of pending runListener promises"
    );

    const pendingPromise = Array.from(runListenerPromises)[0];

    // Shutdown the extension while there is still a pending promises being tracked
    // to verify they gets rejected as expected when the background page browser element
    // is going to be destroyed.
    await extension.unload();
    equal(
      runListenerPromises.size,
      0,
      "Expect no remaining pending runListener promises"
    );

    await Assert.rejects(
      pendingPromise,
      /Actor 'Conduits' destroyed before query 'RunListener' was resolved/,
      "Previously pending runListener promise rejected with the expected error"
    );
  }
);

add_task(
  { pref_set: [["extensions.background.idle.timeout", 500]] },
  async function test_eventpage_idle_reset_once_on_pending_async_listeners() {
    let extension = createPendingListenerTestExtension();
    await extension.startup();
    await extension.awaitMessage("bg-script-ready");

    info("Trigger the API event listener call");
    ExtensionPreferencesManager.setSetting(
      extension.id,
      "allowPopupsForUserEvents",
      "click"
    );

    await extension.awaitMessage("allowPopupsForUserEvents:awaiting");

    info("Wait for suspend on the first idle timeout to be reset");
    const [, resetIdleData] = await promiseExtensionEvent(
      extension,
      "background-script-reset-idle"
    );

    Assert.deepEqual(
      resetIdleData,
      {
        reason: "pendingListeners",
        pendingListeners: 1,
      },
      "Got the expected idle reset reason and pendingListeners count"
    );

    info(
      "Await for the runtime.onSuspend event to be emitted on the second idle timeout hit"
    );
    // We expect this part of the test to trigger a uncaught rejection for the
    // "Actor 'Conduits' destroyed before query 'RunListener' was resolved" error,
    // due to the listener left purposely pending in this test
    // and so that expected rejection is ignored using PromiseTestUtils in the preamble
    // of this test file.
    await extension.awaitMessage("runtime-on-suspend");
    await extension.unload();
  }
);
