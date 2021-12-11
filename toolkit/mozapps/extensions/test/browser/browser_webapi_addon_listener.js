const TESTPAGE = `${SECURE_TESTROOT}webapi_addon_listener.html`;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webapi.testing", true]],
  });
});

async function getListenerEvents(browser) {
  let result = await SpecialPowers.spawn(browser, [], async function() {
    return content.document.getElementById("result").textContent;
  });

  return result.split("\n").map(JSON.parse);
}

const RESTARTLESS_ID = "restartless@tests.mozilla.org";
const INSTALL_ID = "install@tests.mozilla.org";
const CANCEL_ID = "cancel@tests.mozilla.org";

let provider = new MockProvider();
provider.createAddons([
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
      { id: RESTARTLESS_ID, event: "onDisabling" },
      { id: RESTARTLESS_ID, event: "onDisabled" },
      { id: RESTARTLESS_ID, event: "onEnabling" },
      { id: RESTARTLESS_ID, event: "onEnabled" },
    ];
    Assert.deepEqual(events, expected, "Got expected disable/enable events");
  });
});

// Test install events
add_task(async function test_restartless() {
  await BrowserTestUtils.withNewTab(TESTPAGE, async function(browser) {
    let addon = new MockAddon(
      INSTALL_ID,
      "installme",
      null,
      AddonManager.OP_NEED_RESTART_NONE
    );
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
      { id: INSTALL_ID, event: "onInstalling" },
      { id: INSTALL_ID, event: "onInstalled" },
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
      { id: RESTARTLESS_ID, event: "onUninstalling" },
      { id: RESTARTLESS_ID, event: "onUninstalled" },
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
    let expected = [{ id: CANCEL_ID, event: "onUninstalling" }];
    Assert.deepEqual(events, expected, "Got expected uninstalling event");

    addon.cancelUninstall();
    events = await getListenerEvents(browser);
    expected.push({ id: CANCEL_ID, event: "onOperationCancelled" });
    Assert.deepEqual(events, expected, "Got expected cancel event");
  });
});
