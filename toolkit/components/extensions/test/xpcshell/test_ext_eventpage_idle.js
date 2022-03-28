"use strict";

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

function promiseExtensionEvent(extension, event) {
  return new Promise(resolve => extension.extension.once(event, resolve));
}

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_eventpage_idle() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["browserSettings"],
      background: { persistent: false },
    },
    background() {
      browser.browserSettings.homepageOverride.onChange.addListener(() => {
        browser.test.sendMessage("homepageOverride");
      });
      browser.runtime.onSuspend.addListener(async () => {
        let setting = await browser.browserSettings.homepageOverride.get({});
        browser.test.sendMessage("suspended", setting);
      });
    },
  });
  await extension.startup();
  assertPersistentListeners(extension, "browserSettings", "homepageOverride", {
    primed: false,
  });

  info(`test idle timeout after startup`);
  await extension.awaitMessage("suspended");
  await promiseExtensionEvent(extension, "shutdown-background-script");
  assertPersistentListeners(extension, "browserSettings", "homepageOverride", {
    primed: true,
  });
  Services.prefs.setStringPref(
    "browser.startup.homepage",
    "http://homepage.example.com"
  );
  await extension.awaitMessage("homepageOverride");
  ok(true, "homepageOverride.onChange fired");

  // again after the event is fired
  info(`test idle timeout after wakeup`);
  let setting = await extension.awaitMessage("suspended");
  equal(
    setting.value,
    "http://homepage.example.com",
    "verify simple async wait works in onSuspend"
  );

  await promiseExtensionEvent(extension, "shutdown-background-script");
  assertPersistentListeners(extension, "browserSettings", "homepageOverride", {
    primed: true,
  });
  Services.prefs.setStringPref(
    "browser.startup.homepage",
    "http://test.example.com"
  );
  await extension.awaitMessage("homepageOverride");
  ok(true, "homepageOverride.onChange fired");

  await extension.unload();
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
    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      manifest: {
        permissions: ["browserSettings"],
        background: { persistent: false },
      },
      background() {
        let resolveSuspend;
        browser.browserSettings.homepageOverride.onChange.addListener(() => {
          browser.test.sendMessage("homepageOverride");
        });
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
    Services.prefs.setStringPref(
      "browser.startup.homepage",
      "http://homepage.example.com"
    );
    extension.sendMessage("resolveSuspend");
    await extension.awaitMessage("homepageOverride");
    await extension.awaitMessage("suspendCanceled");
    ok(true, "event caused suspend-canceled");

    await extension.awaitMessage("suspending");
    await promiseExtensionEvent(extension, "shutdown-background-script");
    await extension.unload();
  }
);
