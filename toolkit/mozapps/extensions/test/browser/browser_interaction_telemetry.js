const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", {});

AddonTestUtils.initMochitest(this);

let gManagerWindow;
let gCategoryUtilities;
const TELEMETRY_CATEGORY = "addonsManager";
const TELEMETRY_METHODS = new Set(["action", "link", "view"]);
const addonId = "extension@mochi.test";

registerCleanupFunction(() => {
  // AddonTestUtils with open_manager cause this reference to be maintained and creates a leak.
  gManagerWindow = null;
});

async function init(startPage) {
  gManagerWindow = await open_manager(null);
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  // When about:addons initially loads it will load the last view that
  // was open. If that's different than startPage, then clear the events
  // so that we can reliably test them.
  if (gCategoryUtilities.selectedCategory != startPage) {
    Services.telemetry.clearEvents();
  }

  return gCategoryUtilities.openType(startPage);
}

function assertTelemetryMatches(events) {
  let snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);

  if (events.length == 0) {
    ok(!snapshot.parent || snapshot.parent.length == 0, "There are no telemetry events");
    return;
  }

  // Make sure we got some data.
  ok(snapshot.parent && snapshot.parent.length > 0, "Got parent telemetry events in the snapshot");

  // Only look at the related events after stripping the timestamp and category.
  let relatedEvents = snapshot.parent
    .filter(([timestamp, category, method]) =>
      category == TELEMETRY_CATEGORY && TELEMETRY_METHODS.has(method))
    .map(relatedEvent => relatedEvent.slice(2, 6));

  // Events are now [method, object, value, extra] as expected.
  Assert.deepEqual(relatedEvents, events, "The events are recorded correctly");
}

async function installTheme() {
  let id = "theme@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id}},
      manifest_version: 2,
      name: "atheme",
      description: "wow. such theme.",
      author: "Pixel Pusher",
      version: "1",
      theme: {},
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  return extension;
}

async function installExtension(manifest = {}) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id: addonId}},
      manifest_version: 2,
      name: "extension",
      description: "wow. such extension.",
      author: "Code Pusher",
      version: "1",
      chrome_url_overrides: {newtab: "new.html"},
      options_ui: {page: "options.html", open_in_tab: true},
      browser_action: {default_popup: "action.html"},
      page_action: {default_popup: "action.html"},
      ...manifest,
    },
    files: {
      "new.html": "<h1>yo</h1>",
      "options.html": "<h1>options</h1>",
      "action.html": "<h1>do something</h1>",
    },
    useAddonManager: "permanent",
  });
  await extension.startup();
  return extension;
}

add_task(function clearInitialTelemetry() {
  // Clear out any telemetry data that existed before this file is run.
  Services.telemetry.clearEvents();
});

add_task(async function testBasicViewTelemetry() {
  let addons = await Promise.all([
    installTheme(),
    installExtension(),
  ]);
  await init("discover");

  let doc = gManagerWindow.document;

  await gCategoryUtilities.openType("theme");
  doc.querySelector('.addon[value="theme@mochi.test"]').click();
  await wait_for_view_load(gManagerWindow);

  await gCategoryUtilities.openType("extension");
  doc.querySelector('.addon[value="extension@mochi.test"]').click();
  await wait_for_view_load(gManagerWindow);

  assertTelemetryMatches([
    ["view", "aboutAddons", "discover"],
    ["view", "aboutAddons", "list", {type: "theme"}],
    ["view", "aboutAddons", "detail", {type: "theme", addonId: "theme@mochi.test"}],
    ["view", "aboutAddons", "list", {type: "extension"}],
    ["view", "aboutAddons", "detail", {type: "extension", addonId: "extension@mochi.test"}],
  ]);

  await close_manager(gManagerWindow);
  await Promise.all(addons.map(addon => addon.unload()));
});

add_task(async function testExtensionEvents() {
  let addon = await installExtension();
  let type = "extension";
  await init("extension");

  let doc = gManagerWindow.document;
  let list = doc.getElementById("addon-list");
  let row = list.querySelector(`[value="${addonId}"]`);

  // Check/clear the current telemetry.
  assertTelemetryMatches([["view", "aboutAddons", "list", {type: "extension"}]]);

  // Check disable/enable.
  is(row.getAttribute("active"), "true", "The add-on is enabled");
  doc.getAnonymousElementByAttribute(row, "anonid", "disable-btn").click();
  await TestUtils.waitForCondition(() => row.getAttribute("active") == "false", "Wait for disable");
  doc.getAnonymousElementByAttribute(row, "anonid", "enable-btn").click();
  await TestUtils.waitForCondition(() => row.getAttribute("active") == "true", "Wait for enable");
  assertTelemetryMatches([
    ["action", "aboutAddons", null, {action: "disable", addonId, type, view: "list"}],
    ["action", "aboutAddons", null, {action: "enable", addonId, type, view: "list"}],
  ]);

  // Check remove/undo.
  is(row.getAttribute("status"), "installed", "The add-on is installed");
  ok(!row.hasAttribute("pending"), "The add-on is not pending");
  doc.getAnonymousElementByAttribute(row, "anonid", "remove-btn").click();
  await TestUtils.waitForCondition(() => row.getAttribute("pending") == "uninstall", "Wait for uninstall");
  // Find the row again since the binding changed.
  doc.getAnonymousElementByAttribute(row, "anonid", "undo-btn").click();
  await TestUtils.waitForCondition(() => !row.hasAttribute("pending"), "Wait for undo");
  assertTelemetryMatches([
    ["action", "aboutAddons", null, {action: "uninstall", addonId, type, view: "list"}],
    ["action", "aboutAddons", null, {action: "undo", addonId, type, view: "list"}],
  ]);

  // Open the preferences page.
  let waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  let prefsButton;
  await TestUtils.waitForCondition(() => {
    prefsButton = doc.getAnonymousElementByAttribute(row, "anonid", "preferences-btn");
    return prefsButton;
  });
  prefsButton.click();
  BrowserTestUtils.removeTab(await waitForNewTab);
  assertTelemetryMatches([
    ["action", "aboutAddons", "external", {action: "preferences", type, addonId, view: "list"}],
  ]);

  // Go to the detail view.
  row.click();
  await wait_for_view_load(gManagerWindow);
  assertTelemetryMatches([
    ["view", "aboutAddons", "detail", {type, addonId}],
  ]);

  // Check updates.
  let autoUpdate = doc.getElementById("detail-autoUpdate");
  is(autoUpdate.value, "1", "Use default is selected");
  // Turn off auto update.
  autoUpdate.querySelector('[value="0"]').click();
  // Check for updates.
  let checkForUpdates = doc.getElementById("detail-findUpdates-btn");
  is(checkForUpdates.hidden, false, "The check for updates button is visible");
  checkForUpdates.click();
  // Turn on auto update.
  autoUpdate.querySelector('[value="2"]').click();
  // Set auto update to default again.
  autoUpdate.querySelector('[value="1"]').click();
  assertTelemetryMatches([
    ["action", "aboutAddons", "", {action: "setAddonUpdate", type, addonId, view: "detail"}],
    ["action", "aboutAddons", null, {action: "checkForUpdate", type, addonId, view: "detail"}],
    ["action", "aboutAddons", "enabled", {action: "setAddonUpdate", type, addonId, view: "detail"}],
    ["action", "aboutAddons", "default", {action: "setAddonUpdate", type, addonId, view: "detail"}],
  ]);

  // Check links.
  let creator = doc.getElementById("detail-creator");
  let label = doc.getAnonymousElementByAttribute(creator, "anonid", "label");
  let link = doc.getAnonymousElementByAttribute(creator, "anonid", "creator-link");
  // Check that clicking the label doesn't trigger a telemetry event.
  label.click();
  assertTelemetryMatches([]);

  // These links don't actually have a URL, so they don't open a tab. They're only
  // shown when there is a URL though.
  link.click();
  doc.getElementById("detail-homepage").click();
  doc.getElementById("detail-reviews").click();

  // The support button will open a new tab.
  waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  doc.getElementById("helpButton").click();
  BrowserTestUtils.removeTab(await waitForNewTab);

  // Check that the preferences button includes the view.
  waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  let prefsBtn;
  await TestUtils.waitForCondition(() => {
    prefsBtn = doc.getElementById("detail-prefs-btn");
    return prefsBtn;
  });
  prefsBtn.click();
  BrowserTestUtils.removeTab(await waitForNewTab);

  assertTelemetryMatches([
    ["link", "aboutAddons", "author", {view: "detail"}],
    ["link", "aboutAddons", "homepage", {view: "detail"}],
    ["link", "aboutAddons", "rating", {view: "detail"}],
    ["link", "aboutAddons", "support", {view: "detail"}],
    ["action", "aboutAddons", "external", {action: "preferences", type, addonId, view: "detail"}],
  ]);

  // Update the preferences and check that inline changes.
  await gCategoryUtilities.openType("extension");
  let upgraded = await installExtension({options_ui: {page: "options.html"}, version: "2"});
  row = list.querySelector(`[value="${addonId}"]`);
  await TestUtils.waitForCondition(() => {
    prefsBtn = doc.getAnonymousElementByAttribute(row, "anonid", "preferences-btn");
    return prefsBtn;
  });
  prefsBtn.click();
  await wait_for_view_load(gManagerWindow);
  assertTelemetryMatches([
    ["view", "aboutAddons", "list", {type}],
    ["action", "aboutAddons", "inline", {action: "preferences", type, addonId, view: "list"}],
    ["view", "aboutAddons", "detail", {type: "extension", addonId: "extension@mochi.test"}],
  ]);

  await close_manager(gManagerWindow);
  await addon.unload();
  await upgraded.unload();
});

add_task(async function testGeneralActions() {
  await init("extension");

  let doc = gManagerWindow.document;
  let menu = doc.getElementById("utils-menu");
  let checkForUpdates = doc.getElementById("utils-updateNow");
  let recentUpdates = doc.getElementById("utils-viewUpdates");
  let debugAddons = doc.getElementById("utils-debugAddons");
  let updatePolicy = doc.getElementById("utils-autoUpdateDefault");
  let resetUpdatePolicy = doc.getElementById("utils-resetAddonUpdatesToAutomatic");
  let manageShortcuts = doc.getElementById("manage-shortcuts");

  async function clickInGearMenu(item) {
    let shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
    menu.openPopup();
    await shown;
    item.click();
    menu.hidePopup();
  }

  await clickInGearMenu(checkForUpdates);
  await clickInGearMenu(recentUpdates);
  await wait_for_view_load(gManagerWindow);
  await clickInGearMenu(updatePolicy);
  await clickInGearMenu(updatePolicy);
  await clickInGearMenu(resetUpdatePolicy);

  // Check shortcuts view.
  await clickInGearMenu(manageShortcuts);
  await wait_for_view_load(gManagerWindow);
  await clickInGearMenu(checkForUpdates);

  let waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  await clickInGearMenu(debugAddons);
  BrowserTestUtils.removeTab(await waitForNewTab);

  waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  let searchBox = doc.getElementById("header-search");
  searchBox.value = "something";
  searchBox.doCommand();
  BrowserTestUtils.removeTab(await waitForNewTab);

  assertTelemetryMatches([
    ["view", "aboutAddons", "list", {type: "extension"}],
    ["action", "aboutAddons", null, {action: "checkForUpdates", view: "list"}],
    ["view", "aboutAddons", "updates", {type: "recent"}],
    ["action", "aboutAddons", "default,enabled", {action: "setUpdatePolicy", view: "updates"}],
    ["action", "aboutAddons", "enabled", {action: "setUpdatePolicy", view: "updates"}],
    ["action", "aboutAddons", null, {action: "resetUpdatePolicy", view: "updates"}],
    ["view", "aboutAddons", "shortcuts"],
    ["action", "aboutAddons", null, {action: "checkForUpdates", view: "shortcuts"}],
    ["link", "aboutAddons", "about:debugging", {view: "shortcuts"}],
    ["link", "aboutAddons", "search", {view: "shortcuts", type: "shortcuts"}],
  ]);

  await close_manager(gManagerWindow);

  assertTelemetryMatches([]);
});

add_task(async function testPreferencesLink() {
  assertTelemetryMatches([]);

  await init("theme");

  let doc = gManagerWindow.document;

  // Open the about:preferences page from about:addons.
  let waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences");
  doc.getElementById("preferencesButton").click();
  let tab = await waitForNewTab;
  let getAddonsButton = () => tab.linkedBrowser.contentDocument.getElementById("addonsButton");

  // Wait for the page to load.
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Open the about:addons page from about:preferences.
  getAddonsButton().click();

  // Close the about:preferences tab.
  BrowserTestUtils.removeTab(tab);

  assertTelemetryMatches([
    ["view", "aboutAddons", "list", {type: "theme"}],
    ["link", "aboutAddons", "about:preferences", {view: "list"}],
    ["link", "aboutPreferences", "about:addons"],
  ]);

  await close_manager(gManagerWindow);
});
