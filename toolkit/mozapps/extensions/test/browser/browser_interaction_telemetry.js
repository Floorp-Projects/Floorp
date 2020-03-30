const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm",
  {}
);

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
      applications: { gecko: { id } },
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
      applications: { gecko: { id: addonId } },
      manifest_version: 2,
      name: "extension",
      description: "wow. such extension.",
      author: "Code Pusher",
      version: "1",
      chrome_url_overrides: { newtab: "new.html" },
      options_ui: { page: "options.html", open_in_tab: true },
      browser_action: { default_popup: "action.html" },
      page_action: { default_popup: "action.html" },
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
  return doc.querySelector(`addon-card[addon-id="${id}"]`);
}

function openDetailView(doc, id) {
  let card = getAddonCard(doc, id);
  card.querySelector('[action="expand"]').click();
}

async function enableAndDisable(doc, row) {
  let toggleButton = row.querySelector('[action="toggle-disabled"]');
  let disabled = BrowserTestUtils.waitForEvent(row, "update");
  toggleButton.click();
  await disabled;
  let enabled = BrowserTestUtils.waitForEvent(row, "update");
  toggleButton.click();
  await enabled;
}

async function removeAddonAndUndo(doc, row) {
  let id = row.addon.id;
  let started = AddonTestUtils.promiseWebExtensionStartup(id);
  let removed = BrowserTestUtils.waitForEvent(row, "remove");
  row.querySelector('[action="remove"]').click();
  await removed;

  let undoBanner = doc.querySelector(`message-bar[addon-id="${row.addon.id}"]`);
  undoBanner.querySelector('[action="undo"]').click();
  await TestUtils.waitForCondition(() => getAddonCard(doc, row.addon.id));
  await started;
}

async function openPrefs(doc, row) {
  row.querySelector('[action="preferences"]').click();
}

function changeAutoUpdates(doc) {
  let row = doc.querySelector(".addon-detail-row-updates");
  let checked = row.querySelector(":checked");
  is(checked.value, "1", "Use default is selected");
  row.querySelector('[value="0"]').click();
  row.querySelector('[action="update-check"]').click();
  row.querySelector('[value="2"]').click();
  row.querySelector('[value="1"]').click();
}

function clickLinks(doc) {
  let links = ["author", "homepage", "rating"];
  for (let linkType of links) {
    doc.querySelector(`.addon-detail-row-${linkType} a`).click();
  }
}

async function init(startPage) {
  gManagerWindow = await open_manager(null);
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  // When about:addons initially loads it will load the last view that
  // was open. If that's different than startPage, then clear the events
  // so that we can reliably test them.
  if (gCategoryUtilities.selectedCategory != startPage) {
    Services.telemetry.clearEvents();
  }

  await gCategoryUtilities.openType(startPage);

  return gManagerWindow.document.getElementById("html-view-browser")
    .contentDocument;
}

/* Test functions start here. */

add_task(async function setup() {
  // Clear out any telemetry data that existed before this file is run.
  Services.telemetry.clearEvents();
});

add_task(async function testBasicViewTelemetry() {
  let addons = await Promise.all([installTheme(), installExtension()]);
  let doc = await init("discover");

  await gCategoryUtilities.openType("theme");
  openDetailView(doc, "theme@mochi.test");
  await wait_for_view_load(gManagerWindow);

  await gCategoryUtilities.openType("extension");
  openDetailView(doc, "extension@mochi.test");
  await wait_for_view_load(gManagerWindow);

  assertAboutAddonsTelemetryEvents(
    [
      ["addonsManager", "view", "aboutAddons", "discover"],
      ["addonsManager", "view", "aboutAddons", "list", { type: "theme" }],
      [
        "addonsManager",
        "view",
        "aboutAddons",
        "detail",
        { type: "theme", addonId: "theme@mochi.test" },
      ],
      ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
      [
        "addonsManager",
        "view",
        "aboutAddons",
        "detail",
        { type: "extension", addonId: "extension@mochi.test" },
      ],
    ],
    { methods: ["view"] }
  );

  await close_manager(gManagerWindow);
  await Promise.all(addons.map(addon => addon.unload()));
});

add_task(async function testExtensionEvents() {
  let addon = await installExtension();
  let type = "extension";
  let doc = await init("extension");

  // Check/clear the current telemetry.
  assertAboutAddonsTelemetryEvents(
    [["addonsManager", "view", "aboutAddons", "list", { type: "extension" }]],
    { methods: ["view"] }
  );

  let row = getAddonCard(doc, addonId);

  // Check disable/enable.
  await enableAndDisable(doc, row);
  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "disable", addonId, type, view: "list" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "enable", addonId, type, view: "list" },
      ],
    ],
    { methods: ["action"] }
  );

  // Check remove/undo.
  await removeAddonAndUndo(doc, row);
  let uninstallValue = "accepted";
  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        uninstallValue,
        { action: "uninstall", addonId, type, view: "list" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "undo", addonId, type, view: "list" },
      ],
    ],
    { methods: ["action"] }
  );

  // Open the preferences page.
  let waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  // Find the row again since it was modified on uninstall.
  row = getAddonCard(doc, addonId);
  await openPrefs(doc, row);
  BrowserTestUtils.removeTab(await waitForNewTab);
  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "external",
        { action: "preferences", type, addonId, view: "list" },
      ],
    ],
    { methods: ["action"] }
  );

  // Go to the detail view.
  openDetailView(doc, addonId);
  await wait_for_view_load(gManagerWindow);
  assertAboutAddonsTelemetryEvents(
    [["addonsManager", "view", "aboutAddons", "detail", { type, addonId }]],
    { methods: ["view"] }
  );

  // Check updates.
  changeAutoUpdates(doc);
  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "",
        { action: "setAddonUpdate", type, addonId, view: "detail" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "checkForUpdate", type, addonId, view: "detail" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "enabled",
        { action: "setAddonUpdate", type, addonId, view: "detail" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "default",
        { action: "setAddonUpdate", type, addonId, view: "detail" },
      ],
    ],
    { methods: ["action"] }
  );

  // These links don't actually have a URL, so they don't open a tab. They're only
  // shown when there is a URL though.
  clickLinks(doc);

  // The support button will open a new tab.
  waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  doc.getElementById("help-button").click();
  BrowserTestUtils.removeTab(await waitForNewTab);

  // Check that the preferences button includes the view.
  waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  row = getAddonCard(doc, addonId);
  await openPrefs(doc, row);
  BrowserTestUtils.removeTab(await waitForNewTab);

  assertAboutAddonsTelemetryEvents(
    [
      ["addonsManager", "link", "aboutAddons", "author", { view: "detail" }],
      ["addonsManager", "link", "aboutAddons", "homepage", { view: "detail" }],
      ["addonsManager", "link", "aboutAddons", "rating", { view: "detail" }],
      ["addonsManager", "link", "aboutAddons", "support", { view: "detail" }],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "external",
        { action: "preferences", type, addonId, view: "detail" },
      ],
    ],
    { methods: ["action", "link"] }
  );

  // Update the preferences and check that inline changes.
  await gCategoryUtilities.openType("extension");
  let upgraded = await installExtension({
    options_ui: { page: "options.html" },
    version: "2",
  });
  row = getAddonCard(doc, addonId);
  await openPrefs(doc, row);
  await wait_for_view_load(gManagerWindow);

  assertAboutAddonsTelemetryEvents(
    [
      ["addonsManager", "view", "aboutAddons", "list", { type }],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "inline",
        { action: "preferences", type, addonId, view: "list" },
      ],
      [
        "addonsManager",
        "view",
        "aboutAddons",
        "detail",
        { type: "extension", addonId: "extension@mochi.test" },
      ],
    ],
    { methods: ["action", "view"] }
  );

  await close_manager(gManagerWindow);
  await addon.unload();
  await upgraded.unload();
});

add_task(async function testGeneralActions() {
  let win = await loadInitialView("extension");
  info("Loaded");

  let doc = win.document;
  let pageOptionsButton = doc.querySelector('[action="page-options"]');
  let menu = doc.querySelector("#page-options panel-list");
  let checkForUpdates = menu.querySelector('[action="check-for-updates"]');
  let recentUpdates = menu.querySelector('[action="view-recent-updates"]');
  let updatePolicy = menu.querySelector('[action="set-update-automatically"]');
  let resetUpdatePolicy = menu.querySelector('[action="reset-update-states"]');
  let debugAddons = menu.querySelector('[action="debug-addons"]');
  let manageShortcuts = menu.querySelector('[action="manage-shortcuts"]');

  async function clickInGearMenu(item) {
    info(`Opening menu to click ${item.getAttribute("action")}`);
    let shown = BrowserTestUtils.waitForEvent(menu, "shown");
    // This should perform a click on the button to ensure that works. Other
    // tests might just open the menu directly, or click items when it's closed.
    EventUtils.synthesizeMouseAtCenter(pageOptionsButton, {}, win);
    await shown;
    info(`Clicking ${item.getAttribute("action")}`);
    item.click();
  }

  await clickInGearMenu(checkForUpdates);
  let recentUpdatesLoaded = waitForViewLoad(win);
  await clickInGearMenu(recentUpdates);
  await recentUpdatesLoaded;
  await clickInGearMenu(updatePolicy);
  await clickInGearMenu(updatePolicy);
  await clickInGearMenu(resetUpdatePolicy);

  // Check shortcuts view.
  let shortcutsLoaded = waitForViewLoad(win);
  await clickInGearMenu(manageShortcuts);
  await shortcutsLoaded;
  await clickInGearMenu(checkForUpdates);

  let waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  await clickInGearMenu(debugAddons);
  info("Waiting for about:debugging tab");
  let tab = await waitForNewTab;
  BrowserTestUtils.removeTab(tab);

  waitForNewTab = BrowserTestUtils.waitForNewTab(gBrowser);
  let searchBox = doc.querySelector("search-addons").input.inputField;
  searchBox.value = "something";
  searchBox.focus();
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  info("Waiting for AMO search tab");
  tab = await waitForNewTab;
  BrowserTestUtils.removeTab(tab);

  assertAboutAddonsTelemetryEvents(
    [
      ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "checkForUpdates", view: "list" },
      ],
      ["addonsManager", "view", "aboutAddons", "updates", { type: "recent" }],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "default,enabled",
        { action: "setUpdatePolicy", view: "updates" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "enabled",
        { action: "setUpdatePolicy", view: "updates" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "resetUpdatePolicy", view: "updates" },
      ],
      ["addonsManager", "view", "aboutAddons", "shortcuts"],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "checkForUpdates", view: "shortcuts" },
      ],
      [
        "addonsManager",
        "link",
        "aboutAddons",
        "about:debugging",
        { view: "shortcuts" },
      ],
      [
        "addonsManager",
        "link",
        "aboutAddons",
        "search",
        { view: "shortcuts", type: "shortcuts" },
      ],
    ],
    { methods: TELEMETRY_METHODS }
  );

  await closeView(win);

  assertAboutAddonsTelemetryEvents([]);
});

add_task(async function testPreferencesLink() {
  assertAboutAddonsTelemetryEvents([]);

  let doc = await init("theme");

  // Open the about:preferences page from about:addons.
  let waitForNewTab = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:preferences"
  );
  doc.getElementById("preferencesButton").click();
  let tab = await waitForNewTab;
  let getAddonsButton = () =>
    tab.linkedBrowser.contentDocument.getElementById("addonsButton");

  // Wait for the page to load.
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Open the about:addons page from about:preferences.
  getAddonsButton().click();

  // Close the about:preferences tab.
  BrowserTestUtils.removeTab(tab);

  TelemetryTestUtils.assertEvents(
    [
      ["addonsManager", "view", "aboutAddons", "list", { type: "theme" }],
      [
        "addonsManager",
        "link",
        "aboutAddons",
        "about:preferences",
        { view: "list" },
      ],
      ["addonsManager", "link", "aboutPreferences", "about:addons"],
    ],
    {
      category: "addonsManager",
      method: /^(link|view)$/,
    }
  );

  await close_manager(gManagerWindow);
});
