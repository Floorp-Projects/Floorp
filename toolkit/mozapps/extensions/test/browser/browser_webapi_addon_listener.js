const TESTPAGE = `${SECURE_TESTROOT}webapi_addon_listener.html`;

Services.prefs.setBoolPref("extensions.webapi.testing", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("extensions.webapi.testing");
});


async function getListenerEvents(browser) {
  let result = await ContentTask.spawn(browser, null, async function() {
    return content.document.getElementById("result").textContent;
  });

  return result.split("\n").map(JSON.parse);
}

const RESTART_DISABLED_ID = "restart_disabled@tests.mozilla.org";
const RESTARTLESS_ID = "restartless@tests.mozilla.org";
const INSTALL_ID = "install@tests.mozilla.org";
const CANCEL_ID = "cancel@tests.mozilla.org";

let provider = new MockProvider(false);
provider.createAddons([
  {
    id: RESTART_DISABLED_ID,
    name: "Disabled add-on that requires restart",
    userDisabled: true,
  },
  {
    id: RESTARTLESS_ID,
    name: "Restartless add-on",
    operationsRequiringRestart: AddonManager.OP_NEED_RESTART_NONE,
  },
  {
    id: CANCEL_ID,
    name: "Add-on for uninstall cancel",
  },
]);

// Test enable of add-on requiring restart
add_task(async function test_enable() {
  await BrowserTestUtils.withNewTab(TESTPAGE, async function(browser) {
    let addon = await promiseAddonByID(RESTART_DISABLED_ID);
    is(addon.userDisabled, true, "addon is disabled");

    // enable it
    await addon.enable();
    is(addon.userDisabled, false, "addon was enabled successfully");

    let events = await getListenerEvents(browser);

    // Just a single onEnabling since restart is needed to complete
    let expected = [
      {id: RESTART_DISABLED_ID, needsRestart: true, event: "onEnabling"},
    ];
    Assert.deepEqual(events, expected, "Got expected enable event");
  });
});

// Test enable/disable events for restartless
add_task(async function test_restartless() {
  await BrowserTestUtils.withNewTab(TESTPAGE, async function(browser) {
    let addon = await promiseAddonByID(RESTARTLESS_ID);
    is(addon.userDisabled, false, "addon is enabled");

    // disable it
    await addon.disable();
    is(addon.userDisabled, true, "addon was disabled successfully");

    // re-enable it
    await addon.enable();
    is(addon.userDisabled, false, "addon was re-enabled successfuly");

    let events = await getListenerEvents(browser);
    let expected = [
      {id: RESTARTLESS_ID, needsRestart: false, event: "onDisabling"},
      {id: RESTARTLESS_ID, needsRestart: false, event: "onDisabled"},
      {id: RESTARTLESS_ID, needsRestart: false, event: "onEnabling"},
      {id: RESTARTLESS_ID, needsRestart: false, event: "onEnabled"},
    ];
    Assert.deepEqual(events, expected, "Got expected disable/enable events");
  });
});

// Test install events
add_task(async function test_restartless() {
  await BrowserTestUtils.withNewTab(TESTPAGE, async function(browser) {
    let addon = new MockAddon(INSTALL_ID, "installme", null,
                              AddonManager.OP_NEED_RESTART_NONE);
    let install = new MockInstall(null, null, addon);

    let installPromise = new Promise(resolve => {
      install.addTestListener({
        onInstallEnded: resolve,
      });
    });

    provider.addInstall(install);
    install.install();

    await installPromise;

    let events = await getListenerEvents(browser);
    let expected = [
      {id: INSTALL_ID, needsRestart: false, event: "onInstalling"},
      {id: INSTALL_ID, needsRestart: false, event: "onInstalled"},
    ];
    Assert.deepEqual(events, expected, "Got expected install events");
  });
});

// Test uninstall
add_task(async function test_uninstall() {
  await BrowserTestUtils.withNewTab(TESTPAGE, async function(browser) {
    let addon = await promiseAddonByID(RESTARTLESS_ID);
    isnot(addon, null, "Found add-on for uninstall");

    addon.uninstall();

    let events = await getListenerEvents(browser);
    let expected = [
      {id: RESTARTLESS_ID, needsRestart: false, event: "onUninstalling"},
      {id: RESTARTLESS_ID, needsRestart: false, event: "onUninstalled"},
    ];
    Assert.deepEqual(events, expected, "Got expected uninstall events");
  });
});

// Test cancel of uninstall.
add_task(async function test_cancel() {
  await BrowserTestUtils.withNewTab(TESTPAGE, async function(browser) {
    let addon = await promiseAddonByID(CANCEL_ID);
    isnot(addon, null, "Found add-on for cancelling uninstall");

    addon.uninstall();

    let events = await getListenerEvents(browser);
    let expected = [
      {id: CANCEL_ID, needsRestart: true, event: "onUninstalling"},
    ];
    Assert.deepEqual(events, expected, "Got expected uninstalling event");

    addon.cancelUninstall();
    events = await getListenerEvents(browser);
    expected.push({id: CANCEL_ID, needsRestart: false, event: "onOperationCancelled"});
    Assert.deepEqual(events, expected, "Got expected cancel event");
  });
});

