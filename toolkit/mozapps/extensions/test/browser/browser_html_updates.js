/* eslint max-len: ["error", 80] */

const {AddonTestUtils} =
  ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

AddonTestUtils.initMochitest(this);

add_task(async function enableHtmlViews() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
});

function loadDetailView(win, id) {
  let doc = win.document;
  let card = doc.querySelector(`addon-card[addon-id="${id}"]`);
  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(card, {clickCount: 1}, win);
  EventUtils.synthesizeMouseAtCenter(card, {clickCount: 2}, win);
  return loaded;
}

add_task(async function testChangeAutoUpdates() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test",
      applications: {gecko: {id: "test@mochi.test"}},
    },
    // Use permanent so the add-on can be updated.
    useAddonManager: "permanent",
  });

  await extension.startup();
  let addon = await AddonManager.getAddonByID("test@mochi.test");

  let win = await loadInitialView("extension");
  let doc = win.document;

  let getInputs = updateRow => ({
    default: updatesRow.querySelector('input[value="1"]'),
    on: updatesRow.querySelector('input[value="2"]'),
    off: updatesRow.querySelector('input[value="0"]'),
    checkForUpdate: updatesRow.querySelector('[action="update-check"]'),
  });

  await loadDetailView(win, "test@mochi.test");

  let card = doc.querySelector('addon-card[addon-id="test@mochi.test"]');
  ok(card.querySelector("addon-details"), "The card now has details");

  let updatesRow = card.querySelector(".addon-detail-row-updates");
  let inputs = getInputs(updatesRow);
  is(addon.applyBackgroundUpdates, 1, "Default is set");
  ok(inputs.default.checked, "The default option is selected");
  ok(inputs.checkForUpdate.hidden, "Update check is hidden");

  inputs.on.click();
  is(addon.applyBackgroundUpdates, 2, "Updates are now enabled");
  ok(inputs.on.checked, "The on option is selected");
  ok(inputs.checkForUpdate.hidden, "Update check is hidden");

  inputs.off.click();
  is(addon.applyBackgroundUpdates, 0, "Updates are now disabled");
  ok(inputs.off.checked, "The off option is selected");
  ok(!inputs.checkForUpdate.hidden, "Update check is visible");

  // Go back to the list view and check the details view again.
  let loaded = waitForViewLoad(win);
  win.managerWindow.document.getElementById("go-back").click();
  await loaded;

  // Load the detail view again.
  await loadDetailView(win, "test@mochi.test");

  card = doc.querySelector('addon-card[addon-id="test@mochi.test"]');
  updatesRow = card.querySelector(".addon-detail-row-updates");
  inputs = getInputs(updatesRow);

  ok(inputs.off.checked, "Off is still selected");

  // Disable global updates.
  let updated = BrowserTestUtils.waitForEvent(card, "update");
  AddonManager.autoUpdateDefault = false;
  await updated;

  // Updates are still the same.
  is(addon.applyBackgroundUpdates, 0, "Updates are now disabled");
  ok(inputs.off.checked, "The off option is selected");
  ok(!inputs.checkForUpdate.hidden, "Update check is visible");

  // Check default.
  inputs.default.click();
  is(addon.applyBackgroundUpdates, 1, "Default is set");
  ok(inputs.default.checked, "The default option is selected");
  ok(!inputs.checkForUpdate.hidden, "Update check is visible");

  inputs.on.click();
  is(addon.applyBackgroundUpdates, 2, "Updates are now enabled");
  ok(inputs.on.checked, "The on option is selected");
  ok(inputs.checkForUpdate.hidden, "Update check is hidden");

  // Enable updates again.
  updated = BrowserTestUtils.waitForEvent(card, "update");
  AddonManager.autoUpdateDefault = true;
  await updated;

  await closeView(win);
  await extension.unload();
});

async function setupExtensionWithUpdate() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.checkUpdateSecurity", false]],
  });

  let server = AddonTestUtils.createHttpServer();
  let serverHost = `http://localhost:${server.identity.primaryPort}`;

  let baseManifest = {
    name: "Updates",
    applications: {
      gecko: {
        id: "update@mochi.test",
        update_url: `${serverHost}/ext-updates.json`,
      },
    },
  };

  let updateXpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      ...baseManifest,
      version: "2",
    },
  });
  server.registerFile("/update-2.xpi", updateXpi);
  AddonTestUtils.registerJSON(server, "/ext-updates.json", {
    addons: {
      "update@mochi.test": {
        updates: [
          {version: "2", update_link: `${serverHost}/update-2.xpi`},
        ],
      },
    },
  });

  return ExtensionTestUtils.loadExtension({
    manifest: {
      ...baseManifest,
      version: "1",
    },
    // Use permanent so the add-on can be updated.
    useAddonManager: "permanent",
  });
}

function disableAutoUpdates(card) {
  // Check button should be hidden.
  let updateCheckButton = card.querySelector('button[action="update-check"]');
  ok(updateCheckButton.hidden, "The button is initially hidden");

  // Disable updates, update check button is now visible.
  card.querySelector('input[name="autoupdate"][value="0"]').click();
  ok(!updateCheckButton.hidden, "The button is now visible");

  // There shouldn't be an update shown to the user.
  assertUpdateState(card, false);
}

function checkForUpdate(card, expected) {
  let updateCheckButton = card.querySelector('button[action="update-check"]');
  let updateFound = BrowserTestUtils.waitForEvent(card, expected);
  updateCheckButton.click();
  return updateFound;
}

function installUpdate(card, expected) {
  // Install the update.
  let updateInstalled = BrowserTestUtils.waitForEvent(card, expected);
  let updated = BrowserTestUtils.waitForEvent(card, "update");
  card.querySelector('panel-item[action="install-update"]').click();
  return Promise.all([updateInstalled, updated]);
}

function assertUpdateState(card, shown) {
  let menuButton = card.querySelector(".more-options-button");
  ok(menuButton.classList.contains("more-options-button-badged") == shown,
     "The menu button is badged");
  let installButton = card.querySelector('panel-item[action="install-update"]');
  ok(installButton.hidden != shown,
     `The install button is ${shown ? "hidden" : "shown"}`);
  let updateCheckButton = card.querySelector('button[action="update-check"]');
  ok(updateCheckButton.hidden == shown,
     `The update check button is ${shown ? "hidden" : "shown"}`);
}

add_task(async function testUpdateAvailable() {
  let extension = await setupExtensionWithUpdate();
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  await loadDetailView(win, "update@mochi.test");

  let card = doc.querySelector("addon-card");

  // Disable updates and then check.
  disableAutoUpdates(card);
  await checkForUpdate(card, "update-found");

  // There should now be an update.
  assertUpdateState(card, true);

  // The version was 1.
  let versionRow = card.querySelector(".addon-detail-row-version");
  is(versionRow.lastChild.textContent, "1", "The version started as 1");

  await installUpdate(card, "update-installed");

  // The version is now 2.
  versionRow = card.querySelector(".addon-detail-row-version");
  is(versionRow.lastChild.textContent, "2", "The version has updated");

  // No update is shown again.
  assertUpdateState(card, false);

  // Check for updates again, there shouldn't be an update.
  await checkForUpdate(card, "no-update");

  await closeView(win);
  await extension.unload();
});

add_task(async function testUpdateCancelled() {
  let extension = await setupExtensionWithUpdate();
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  await loadDetailView(win, "update@mochi.test");
  let card = doc.querySelector("addon-card");

  // Disable updates and then check.
  disableAutoUpdates(card);
  await checkForUpdate(card, "update-found");

  // There should now be an update.
  assertUpdateState(card, true);

  // The add-on starts as version 1.
  let versionRow = card.querySelector(".addon-detail-row-version");
  is(versionRow.lastChild.textContent, "1", "The version started as 1");

  // Force the install to be cancelled.
  let install = card.updateInstall;
  ok(install, "There was an install found");
  install.promptHandler = Promise.reject().catch(() => {});

  await installUpdate(card, "update-cancelled");

  // The add-on is still version 1.
  versionRow = card.querySelector(".addon-detail-row-version");
  is(versionRow.lastChild.textContent, "1", "The version hasn't changed");

  // The update has been removed.
  assertUpdateState(card, false);

  await closeView(win);
  await extension.unload();
});
