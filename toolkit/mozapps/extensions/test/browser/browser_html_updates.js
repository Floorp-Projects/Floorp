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

async function setupExtensionWithUpdate(id) {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.checkUpdateSecurity", false]],
  });

  let server = AddonTestUtils.createHttpServer();
  let serverHost = `http://localhost:${server.identity.primaryPort}`;
  let updatesPath = `/ext-updates-${id}.json`;

  let baseManifest = {
    name: "Updates",
    applications: {
      gecko: {
        id,
        update_url: serverHost + updatesPath,
      },
    },
  };

  let updateXpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      ...baseManifest,
      version: "2",
    },
  });
  let xpiFilename = `/update-${id}.xpi`;
  server.registerFile(xpiFilename, updateXpi);
  AddonTestUtils.registerJSON(server, updatesPath, {
    addons: {
      [id]: {
        updates: [
          {version: "2", update_link: serverHost + xpiFilename},
        ],
      },
    },
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      ...baseManifest,
      version: "1",
    },
    // Use permanent so the add-on can be updated.
    useAddonManager: "permanent",
  });
  await extension.startup();
  return extension;
}

function disableAutoUpdates(card) {
  // Check button should be hidden.
  let updateCheckButton = card.querySelector('button[action="update-check"]');
  ok(updateCheckButton.hidden, "The button is initially hidden");

  // Disable updates, update check button is now visible.
  card.querySelector('input[name="autoupdate"][value="0"]').click();
  ok(!updateCheckButton.hidden, "The button is now visible");

  // There shouldn't be an update shown to the user.
  assertUpdateState({card, shown: false});
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

function assertUpdateState({card, shown, expanded = true}) {
  let menuButton = card.querySelector(".more-options-button");
  ok(menuButton.classList.contains("more-options-button-badged") == shown,
     "The menu button is badged");
  let installButton = card.querySelector('panel-item[action="install-update"]');
  ok(installButton.hidden != shown,
     `The install button is ${shown ? "hidden" : "shown"}`);
  if (expanded) {
    let updateCheckButton = card.querySelector('button[action="update-check"]');
    ok(updateCheckButton.hidden == shown,
      `The update check button is ${shown ? "hidden" : "shown"}`);
  }
}

add_task(async function testUpdateAvailable() {
  let id = "update@mochi.test";
  let extension = await setupExtensionWithUpdate(id);

  let win = await loadInitialView("extension");
  let doc = win.document;

  await loadDetailView(win, id);

  let card = doc.querySelector("addon-card");

  // Disable updates and then check.
  disableAutoUpdates(card);
  await checkForUpdate(card, "update-found");

  // There should now be an update.
  assertUpdateState({card, shown: true});

  // The version was 1.
  let versionRow = card.querySelector(".addon-detail-row-version");
  is(versionRow.lastChild.textContent, "1", "The version started as 1");

  await installUpdate(card, "update-installed");

  // The version is now 2.
  versionRow = card.querySelector(".addon-detail-row-version");
  is(versionRow.lastChild.textContent, "2", "The version has updated");

  // No update is shown again.
  assertUpdateState({card, shown: false});

  // Check for updates again, there shouldn't be an update.
  await checkForUpdate(card, "no-update");

  await closeView(win);
  await extension.unload();
});

add_task(async function testUpdateCancelled() {
  let id = "update@mochi.test";
  let extension = await setupExtensionWithUpdate(id);

  let win = await loadInitialView("extension");
  let doc = win.document;

  await loadDetailView(win, "update@mochi.test");
  let card = doc.querySelector("addon-card");

  // Disable updates and then check.
  disableAutoUpdates(card);
  await checkForUpdate(card, "update-found");

  // There should now be an update.
  assertUpdateState({card, shown: true});

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
  assertUpdateState({card, shown: false});

  await closeView(win);
  await extension.unload();
});

add_task(async function testAvailableUpdates() {
  let ids = ["update1@mochi.test", "update2@mochi.test", "update3@mochi.test"];
  let addons = await Promise.all(ids.map(id => setupExtensionWithUpdate(id)));

  // Disable global add-on updates.
  AddonManager.autoUpdateDefault = false;

  let win = await loadInitialView("extension");
  let doc = win.document;

  let managerDoc = win.managerWindow.document;
  let {gCategories} = win.managerWindow;
  let availableCat = gCategories.get("addons://updates/available");

  ok(availableCat.hidden, "Available updates is hidden");
  is(availableCat.badgeCount, 0, "There are no updates");

  // Check for all updates.
  let updatesFound = TestUtils.topicObserved("EM-update-check-finished");
  managerDoc.getElementById("utils-updateNow").doCommand();
  await updatesFound;

  // Wait for the available updates count to finalize, it's async.
  await BrowserTestUtils.waitForCondition(() => availableCat.badgeCount == 3);

  // The category shows the correct update count.
  ok(!availableCat.hidden, "Available updates is visible");
  is(availableCat.badgeCount, 3, "There are 3 updates");

  // Go to the available updates page.
  let loaded = waitForViewLoad(win);
  availableCat.click();
  await loaded;

  // Check the updates are shown.
  let cards = doc.querySelectorAll("addon-card");
  is(cards.length, 3, "There are 3 cards");

  // Each card should have an update.
  for (let card of cards) {
    assertUpdateState({card, shown: true, expanded: false});
  }

  // Check the detail page for the first add-on.
  await loadDetailView(win, ids[0]);
  is(gCategories.selected, "addons://list/extension",
     "The extensions category is selected");

  // Go back to the last view.
  loaded = waitForViewLoad(win);
  managerDoc.getElementById("go-back").click();
  await loaded;

  // We're back on the updates view.
  is(gCategories.selected, "addons://updates/available",
     "The available updates category is selected");

  // Find the cards again.
  cards = doc.querySelectorAll("addon-card");
  is(cards.length, 3, "There are 3 cards");

  // Install the first update.
  await installUpdate(cards[0], "update-installed");
  assertUpdateState({card: cards[0], shown: false, expanded: false});

  // The count goes down but the card stays.
  is(availableCat.badgeCount, 2, "There are only 2 updates now");
  is(doc.querySelectorAll("addon-card").length, 3,
     "All 3 cards are still visible on the updates page");

  // Install the other two updates.
  await installUpdate(cards[1], "update-installed");
  assertUpdateState({card: cards[1], shown: false, expanded: false});
  await installUpdate(cards[2], "update-installed");
  assertUpdateState({card: cards[2], shown: false, expanded: false});

  // The count goes down but the card stays.
  is(availableCat.badgeCount, 0, "There are no more updates");
  is(doc.querySelectorAll("addon-card").length, 3,
     "All 3 cards are still visible on the updates page");

  // Enable global add-on updates again.
  AddonManager.autoUpdateDefault = true;

  await closeView(win);
  await Promise.all(addons.map(addon => addon.unload()));
});
