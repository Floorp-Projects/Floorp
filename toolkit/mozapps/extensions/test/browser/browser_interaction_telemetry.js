/**
 * These tests run in both the XUL and HTML version of about:addons. When adding
 * a new test it should be defined as a function that accepts a boolean isHtmlViews
 * and added to the testFns array.
 */

const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", {});

AddonTestUtils.initMochitest(this);

let gManagerWindow;
let gCategoryUtilities;

const TELEMETRY_METHODS = ["action", "link", "view"];
const addonId = "extension@mochi.test";

registerCleanupFunction(() => {
  // AddonTestUtils with open_manager cause this reference to be maintained and creates a leak.
  gManagerWindow = null;
});

add_task(function setupPromptService() {
  let promptService = mockPromptService();
  promptService._response = 0; // Accept dialogs.
});

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

function getAddonCard(doc, id) {
  if (doc.ownerGlobal != gManagerWindow) {
    return doc.querySelector(`addon-card[addon-id="${id}"]`);
  }
  return doc.querySelector(`.addon[value="${id}"]`);
}

function openDetailView(doc, id) {
  if (doc.ownerGlobal != gManagerWindow) {
    let card = getAddonCard(doc, id);
    card.querySelector('[action="expand"]').click();
  } else {
    getAddonCard(doc, id).click();
  }
}

async function enableAndDisable(doc, row) {
  if (doc.ownerGlobal != gManagerWindow) {
    let toggleButton = row.querySelector('[action="toggle-disabled"]');
    let disabled = BrowserTestUtils.waitForEvent(row, "update");
    toggleButton.click();
    await disabled;
    let enabled = BrowserTestUtils.waitForEvent(row, "update");
    toggleButton.click();
    await enabled;
  } else {
    is(row.getAttribute("active"), "true", "The add-on is enabled");
    doc.getAnonymousElementByAttribute(row, "anonid", "disable-btn").click();
    await TestUtils.waitForCondition(() => row.getAttribute("active") == "false", "Wait for disable");
    doc.getAnonymousElementByAttribute(row, "anonid", "enable-btn").click();
    await TestUtils.waitForCondition(() => row.getAttribute("active") == "true", "Wait for enable");
  }
}

async function removeAddonAndUndo(doc, row) {
  let isHtml = doc.ownerGlobal != gManagerWindow;
  let id = isHtml ? row.addon.id : row.mAddon.id;
  let started = AddonTestUtils.promiseWebExtensionStartup(id);
  if (isHtml) {
    let removed = BrowserTestUtils.waitForEvent(row, "remove");
    row.querySelector('[action="remove"]').click();
    await removed;

    let undoBanner = doc.querySelector(`message-bar[addon-id="${row.addon.id}"]`);
    undoBanner.querySelector('[action="undo"]').click();
    await TestUtils.waitForCondition(() => getAddonCard(doc, row.addon.id));
  } else {
    is(row.getAttribute("status"), "installed", "The add-on is installed");
    ok(!row.hasAttribute("pending"), "The add-on is not pending");
    doc.getAnonymousElementByAttribute(row, "anonid", "remove-btn").click();
    await TestUtils.waitForCondition(() => row.getAttribute("pending") == "uninstall", "Wait for uninstall");

    doc.getAnonymousElementByAttribute(row, "anonid", "undo-btn").click();
    await TestUtils.waitForCondition(() => !row.hasAttribute("pending"), "Wait for undo");
  }
  await started;
}

async function openPrefs(doc, row) {
  if (doc.ownerGlobal != gManagerWindow) {
    row.querySelector('[action="preferences"]').click();
  } else {
    let prefsButton;
    await TestUtils.waitForCondition(() => {
      prefsButton = doc.getAnonymousElementByAttribute(row, "anonid", "preferences-btn");
      return prefsButton;
    });
    prefsButton.click();
  }
}

function changeAutoUpdates(doc) {
  if (doc.ownerGlobal != gManagerWindow) {
    let row = doc.querySelector(".addon-detail-row-updates");
    let checked = row.querySelector(":checked");
    is(checked.value, "1", "Use default is selected");
    row.querySelector('[value="0"]').click();
    row.querySelector('[action="update-check"]').click();
    row.querySelector('[value="2"]').click();
    row.querySelector('[value="1"]').click();
  } else {
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
  }
}

function clickLinks(doc) {
  if (doc.ownerGlobal != gManagerWindow) {
    let links = ["author", "homepage", "rating"];
    for (let linkType of links) {
      doc.querySelector(`.addon-detail-row-${linkType} a`).click();
    }
  } else {
    // Check links.
    let creator = doc.getElementById("detail-creator");
    let label = doc.getAnonymousElementByAttribute(creator, "anonid", "label");
    let link = doc.getAnonymousElementByAttribute(creator, "anonid", "creator-link");
    // Check that clicking the label doesn't trigger a telemetry event.
    label.click();
    assertTelemetryMatches([]);
    link.click();
    doc.getElementById("detail-homepage").click();
    doc.getElementById("detail-reviews").click();
  }
}

async function init(startPage, isHtmlViews) {
  gManagerWindow = await open_manager(null);
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  // When about:addons initially loads it will load the last view that
  // was open. If that's different than startPage, then clear the events
  // so that we can reliably test them.
  if (gCategoryUtilities.selectedCategory != startPage) {
    Services.telemetry.clearEvents();
  }

  await gCategoryUtilities.openType(startPage);

  if (isHtmlViews) {
    return gManagerWindow.document.getElementById("html-view-browser").contentDocument;
  }
  return gManagerWindow.document;
}

/* Test functions start here. */

async function setup(isHtmlViews) {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", isHtmlViews]],
  });
  // Clear out any telemetry data that existed before this file is run.
  Services.telemetry.clearEvents();
}

async function testBasicViewTelemetry(isHtmlViews) {
  let addons = await Promise.all([
    installTheme(),
    installExtension(),
  ]);
  let doc = await init("discover", isHtmlViews);

  await gCategoryUtilities.openType("theme");
  openDetailView(doc, "theme@mochi.test");
  await wait_for_view_load(gManagerWindow);

  await gCategoryUtilities.openType("extension");
  openDetailView(doc, "extension@mochi.test");
  await wait_for_view_load(gManagerWindow);

  assertTelemetryMatches([
    ["view", "aboutAddons", "discover"],
    ["view", "aboutAddons", "list", {type: "theme"}],
    ["view", "aboutAddons", "detail", {type: "theme", addonId: "theme@mochi.test"}],
    ["view", "aboutAddons", "list", {type: "extension"}],
    ["view", "aboutAddons", "detail", {type: "extension", addonId: "extension@mochi.test"}],
  ], {filterMethods: ["view"]});

  await close_manager(gManagerWindow);
  await Promise.all(addons.map(addon => addon.unload()));
}

async function testExtensionEvents(isHtmlViews) {
  let addon = await installExtension();
  let type = "extension";
  let doc = await init("extension", isHtmlViews);

  // Check/clear the current telemetry.
  assertTelemetryMatches([["view", "aboutAddons", "list", {type: "extension"}]],
                          {filterMethods: ["view"]});

  let row = getAddonCard(doc, addonId);

  // Check disable/enable.
  await enableAndDisable(doc, row);
  assertTelemetryMatches([
    ["action", "aboutAddons", null, {action: "disable", addonId, type, view: "list"}],
    ["action", "aboutAddons", null, {action: "enable", addonId, type, view: "list"}],
  ], {filterMethods: ["action"]});

  // Check remove/undo.
  await removeAddonAndUndo(doc, row);
  let uninstallValue = isHtmlViews ? "accepted" : null;
  assertTelemetryMatches([
    ["action", "aboutAddons", uninstallValue, {action: "uninstall", addonId, type, view: "list"}],
    ["action", "aboutAddons", null, {action: "undo", addonId, type, view: "list"}],
  ], {filterMethods: ["action"]});

  // Open the preferences page.
  let waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  // Find the row again since it was modified on uninstall.
  row = getAddonCard(doc, addonId);
  await openPrefs(doc, row);
  BrowserTestUtils.removeTab(await waitForNewTab);
  assertTelemetryMatches([
    ["action", "aboutAddons", "external", {action: "preferences", type, addonId, view: "list"}],
  ], {filterMethods: ["action"]});

  // Go to the detail view.
  openDetailView(doc, addonId);
  await wait_for_view_load(gManagerWindow);
  assertTelemetryMatches([
    ["view", "aboutAddons", "detail", {type, addonId}],
  ], {filterMethods: ["view"]});

  // Check updates.
  changeAutoUpdates(doc);
  assertTelemetryMatches([
    ["action", "aboutAddons", "", {action: "setAddonUpdate", type, addonId, view: "detail"}],
    ["action", "aboutAddons", null, {action: "checkForUpdate", type, addonId, view: "detail"}],
    ["action", "aboutAddons", "enabled", {action: "setAddonUpdate", type, addonId, view: "detail"}],
    ["action", "aboutAddons", "default", {action: "setAddonUpdate", type, addonId, view: "detail"}],
  ], {filterMethods: ["action"]});

  // These links don't actually have a URL, so they don't open a tab. They're only
  // shown when there is a URL though.
  clickLinks(doc);

  // The support button will open a new tab.
  waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  gManagerWindow.document.getElementById("helpButton").click();
  BrowserTestUtils.removeTab(await waitForNewTab);

  // Check that the preferences button includes the view.
  waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  row = getAddonCard(doc, addonId);
  await openPrefs(doc, row);
  BrowserTestUtils.removeTab(await waitForNewTab);

  assertTelemetryMatches([
    ["link", "aboutAddons", "author", {view: "detail"}],
    ["link", "aboutAddons", "homepage", {view: "detail"}],
    ["link", "aboutAddons", "rating", {view: "detail"}],
    ["link", "aboutAddons", "support", {view: "detail"}],
    ["action", "aboutAddons", "external", {action: "preferences", type, addonId, view: "detail"}],
  ], {filterMethods: ["action", "link"]});

  // Update the preferences and check that inline changes.
  await gCategoryUtilities.openType("extension");
  let upgraded = await installExtension({options_ui: {page: "options.html"}, version: "2"});
  row = getAddonCard(doc, addonId);
  await openPrefs(doc, row);
  await wait_for_view_load(gManagerWindow);

  assertTelemetryMatches([
    ["view", "aboutAddons", "list", {type}],
    ["action", "aboutAddons", "inline", {action: "preferences", type, addonId, view: "list"}],
    ["view", "aboutAddons", "detail", {type: "extension", addonId: "extension@mochi.test"}],
  ], {filterMethods: ["action", "view"]});

  await close_manager(gManagerWindow);
  await addon.unload();
  await upgraded.unload();
}

async function testGeneralActions(isHtmlViews) {
  await init("extension", isHtmlViews);

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
  ], {filterMethods: TELEMETRY_METHODS});

  await close_manager(gManagerWindow);

  assertTelemetryMatches([]);
}

async function testPreferencesLink(isHtmlViews) {
  assertTelemetryMatches([]);

  await init("theme", isHtmlViews);

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
  ], {filterMethods: ["link", "view"]});

  await close_manager(gManagerWindow);
}

const testFns = [
  testBasicViewTelemetry,
  testExtensionEvents,
  testGeneralActions,
  testPreferencesLink,
];

/**
 * Setup the tasks. This will add tasks for each of testFns to run with the
 * XUL and HTML version of about:addons.
 *
 * To add a test, add it to the testFns array.
 */
function addTestTasks(isHtmlViews) {
  add_task(() => setup(isHtmlViews));

  for (let fn of testFns) {
    let localTestFnName = fn.name + (isHtmlViews ? "HTML" : "XUL");
    // Get an informative name for the function in stack traces.
    let obj = {[localTestFnName]: () => fn(isHtmlViews)};
    add_task(obj[localTestFnName]);
  }
}

addTestTasks(false);
addTestTasks(true);
