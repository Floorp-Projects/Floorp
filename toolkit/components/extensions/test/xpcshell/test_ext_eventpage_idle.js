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
    },
  });
  await extension.startup();
  assertPersistentListeners(extension, "browserSettings", "homepageOverride", {
    primed: false,
  });

  info(`test idle timeout after startup`);
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
