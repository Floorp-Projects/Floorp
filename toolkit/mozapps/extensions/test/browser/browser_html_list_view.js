/* eslint max-len: ["error", 80] */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

let promptService;

const SUPPORT_URL = Services.urlFormatter.formatURL(
  Services.prefs.getStringPref("app.support.baseURL")
);
const REMOVE_SUMO_URL = SUPPORT_URL + "cant-remove-addon";

function getTestCards(root) {
  return root.querySelectorAll('addon-card[addon-id$="@mochi.test"]');
}

function getCardByAddonId(root, id) {
  return root.querySelector(`addon-card[addon-id="${id}"]`);
}

function isEmpty(el) {
  return !el.children.length;
}

function waitForThemeChange(list) {
  // Wait for two move events. One theme will be enabled and another disabled.
  let moveCount = 0;
  return BrowserTestUtils.waitForEvent(list, "move", () => ++moveCount == 2);
}

let mockProvider;

add_setup(async function() {
  mockProvider = new MockProvider();
  promptService = mockPromptService();
  Services.telemetry.clearEvents();
});

let extensionsCreated = 0;

function createExtensions(manifestExtras) {
  return manifestExtras.map(extra =>
    ExtensionTestUtils.loadExtension({
      manifest: {
        name: "Test extension",
        applications: {
          gecko: { id: `test-${extensionsCreated++}@mochi.test` },
        },
        icons: {
          32: "test-icon.png",
        },
        ...extra,
      },
      useAddonManager: "temporary",
    })
  );
}

add_task(async function testExtensionList() {
  let id = "test@mochi.test";
  let headingId = "test_mochi_test-heading";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id } },
      icons: {
        32: "test-icon.png",
      },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let addon = await AddonManager.getAddonByID(id);
  ok(addon, "The add-on can be found");

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Find the addon-list to listen for events.
  let list = doc.querySelector("addon-list");

  // There shouldn't be any disabled extensions.
  let disabledSection = getSection(doc, "extension-disabled-section");
  ok(isEmpty(disabledSection), "The disabled section is empty");

  // The loaded extension should be in the enabled list.
  let enabledSection = getSection(doc, "extension-enabled-section");
  ok(
    enabledSection && !isEmpty(enabledSection),
    "The enabled section isn't empty"
  );
  let card = getCardByAddonId(enabledSection, id);
  ok(card, "The card is in the enabled section");

  // Check the properties of the card.
  is(card.addonNameEl.textContent, "Test extension", "The name is set");
  is(
    card.querySelector("h3").id,
    headingId,
    "The add-on name has the correct id"
  );
  is(
    card.querySelector(".card").getAttribute("aria-labelledby"),
    headingId,
    "The card is labelled by the heading"
  );
  let icon = card.querySelector(".addon-icon");
  ok(icon.src.endsWith("/test-icon.png"), "The icon is set");

  // Disable the extension.
  let disableToggle = card.querySelector('[action="toggle-disabled"]');
  ok(disableToggle.checked, "The disable toggle is checked");
  is(
    doc.l10n.getAttributes(disableToggle).id,
    "extension-enable-addon-button-label",
    "The toggle has the enable label"
  );
  ok(disableToggle.getAttribute("aria-label"), "There's an aria-label");
  ok(!disableToggle.hidden, "The toggle is visible");

  let disabled = BrowserTestUtils.waitForEvent(list, "move");
  disableToggle.click();
  await disabled;
  is(
    card.parentNode,
    disabledSection,
    "The card is now in the disabled section"
  );

  // The disable button is now enable.
  ok(!disableToggle.checked, "The disable toggle is unchecked");
  is(
    doc.l10n.getAttributes(disableToggle).id,
    "extension-enable-addon-button-label",
    "The button has the same enable label"
  );
  ok(disableToggle.getAttribute("aria-label"), "There's an aria-label");

  // Remove the add-on.
  let removeButton = card.querySelector('[action="remove"]');
  is(
    doc.l10n.getAttributes(removeButton).id,
    "remove-addon-button",
    "The button has the remove label"
  );
  // There is a support link when the add-on isn't removeable, verify we don't
  // always include one.
  ok(!removeButton.querySelector("a"), "There isn't a link in the item");

  // Remove but cancel.
  let cancelled = BrowserTestUtils.waitForEvent(card, "remove-cancelled");
  removeButton.click();
  await cancelled;

  let removed = BrowserTestUtils.waitForEvent(list, "remove");
  // Tell the mock prompt service that the prompt was accepted.
  promptService._response = 0;
  removeButton.click();
  await removed;

  addon = await AddonManager.getAddonByID(id);
  ok(
    addon && !!(addon.pendingOperations & AddonManager.PENDING_UNINSTALL),
    "The addon is pending uninstall"
  );

  // Ensure that a pending uninstall bar has been created for the
  // pending uninstall extension, and pressing the undo button will
  // refresh the list and render a card to the re-enabled extension.
  assertHasPendingUninstalls(list, 1);
  assertHasPendingUninstallAddon(list, addon);

  // Add a second pending uninstall extension.
  info("Install a second test extension and wait for addon card rendered");
  let added = BrowserTestUtils.waitForEvent(list, "add");
  const extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension 2",
      applications: { gecko: { id: "test-2@mochi.test" } },
      icons: {
        32: "test-icon.png",
      },
    },
    useAddonManager: "temporary",
  });
  await extension2.startup();

  await added;
  ok(
    getCardByAddonId(list, extension2.id),
    "Got a card added for the second extension"
  );

  info("Uninstall the second test extension and wait for addon card removed");
  removed = BrowserTestUtils.waitForEvent(list, "remove");
  const addon2 = await AddonManager.getAddonByID(extension2.id);
  addon2.uninstall(true);
  await removed;

  ok(
    !getCardByAddonId(list, extension2.id),
    "Addon card for the second extension removed"
  );

  assertHasPendingUninstalls(list, 2);
  assertHasPendingUninstallAddon(list, addon2);

  // Addon2 was enabled before entering the pending uninstall state,
  // wait for its startup after pressing undo.
  let addon2Started = AddonTestUtils.promiseWebExtensionStartup(addon2.id);
  await testUndoPendingUninstall(list, addon);
  await testUndoPendingUninstall(list, addon2);
  info("Wait for the second pending uninstal add-ons startup");
  await addon2Started;

  ok(
    getCardByAddonId(disabledSection, addon.id),
    "The card for the first extension is in the disabled section"
  );
  ok(
    getCardByAddonId(enabledSection, addon2.id),
    "The card for the second extension is in the enabled section"
  );

  await extension2.unload();
  await extension.unload();

  // Install a theme and verify that it is not listed in the pending
  // uninstall message bars while the list extensions view is loaded.
  const themeXpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "My theme",
      applications: { gecko: { id: "theme@mochi.test" } },
      theme: {},
    },
  });
  const themeAddon = await AddonManager.installTemporaryAddon(themeXpi);
  // Leave it pending uninstall, the following assertions related to
  // the pending uninstall message bars will fail if the theme is listed.
  await themeAddon.uninstall(true);

  // Install a third addon to verify that is being fully removed once the
  // about:addons page is closed.
  const xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Test extension 3",
      applications: { gecko: { id: "test-3@mochi.test" } },
      icons: {
        32: "test-icon.png",
      },
    },
  });

  added = BrowserTestUtils.waitForEvent(list, "add");
  const addon3 = await AddonManager.installTemporaryAddon(xpi);
  await added;
  ok(
    getCardByAddonId(list, addon3.id),
    "Addon card for the third extension added"
  );

  removed = BrowserTestUtils.waitForEvent(list, "remove");
  addon3.uninstall(true);
  await removed;
  ok(
    !getCardByAddonId(list, addon3.id),
    "Addon card for the third extension removed"
  );

  assertHasPendingUninstalls(list, 1);
  ok(
    addon3 && !!(addon3.pendingOperations & AddonManager.PENDING_UNINSTALL),
    "The third addon is pending uninstall"
  );

  await closeView(win);

  ok(
    !(await AddonManager.getAddonByID(addon3.id)),
    "The third addon has been fully uninstalled"
  );

  ok(
    themeAddon.pendingOperations & AddonManager.PENDING_UNINSTALL,
    "The theme addon is pending after the list extension view is closed"
  );

  await themeAddon.uninstall();

  ok(
    !(await AddonManager.getAddonByID(themeAddon.id)),
    "The theme addon is fully uninstalled"
  );

  assertAboutAddonsTelemetryEvents([
    ["addonsManager", "view", "aboutAddons", "list", { type: "extension" }],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      { type: "extension", addonId: id, view: "list", action: "disable" },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      "cancelled",
      { type: "extension", addonId: id, view: "list", action: "uninstall" },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      "accepted",
      { type: "extension", addonId: id, view: "list", action: "uninstall" },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      { type: "extension", addonId: id, view: "list", action: "undo" },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      {
        type: "extension",
        addonId: "test-2@mochi.test",
        view: "list",
        action: "undo",
      },
    ],
  ]);
});

add_task(async function testMouseSupport() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id: "test@mochi.test" } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  let [card] = getTestCards(doc);
  is(card.addon.id, "test@mochi.test", "The right card is found");

  let panel = card.querySelector("panel-list");

  ok(!panel.open, "The panel is initially closed");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "addon-card[addon-id$='@mochi.test'] button[action='more-options']",
    { type: "mousedown" },
    win.docShell.browsingContext
  );
  ok(panel.open, "The panel is now open");

  await closeView(win);
  await extension.unload();
});

add_task(async function testKeyboardSupport() {
  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Some helpers.
  let tab = event => EventUtils.synthesizeKey("VK_TAB", event);
  let space = () => EventUtils.synthesizeKey(" ", {});
  let isFocused = (el, msg) => is(doc.activeElement, el, msg);

  // Find the addon-list to listen for events.
  let list = doc.querySelector("addon-list");
  let enabledSection = getSection(doc, "extension-enabled-section");
  let disabledSection = getSection(doc, "extension-disabled-section");

  // Find the card.
  let [card] = getTestCards(list);
  is(card.addon.id, "test@mochi.test", "The right card is found");

  // Focus the more options menu button.
  let moreOptionsButton = card.querySelector('[action="more-options"]');
  moreOptionsButton.focus();
  isFocused(moreOptionsButton, "The more options button is focused");

  // Test opening and closing the menu.
  let moreOptionsMenu = card.querySelector("panel-list");
  let expandButton = moreOptionsMenu.querySelector('[action="expand"]');
  let removeButton = card.querySelector('[action="remove"]');
  is(moreOptionsMenu.open, false, "The menu is closed");
  let shown = BrowserTestUtils.waitForEvent(moreOptionsMenu, "shown");
  space();
  await shown;
  is(moreOptionsMenu.open, true, "The menu is open");
  isFocused(removeButton, "The remove button is now focused");
  tab({ shiftKey: true });
  is(moreOptionsMenu.open, true, "The menu stays open");
  isFocused(expandButton, "The focus has looped to the bottom");
  tab();
  is(moreOptionsMenu.open, true, "The menu stays open");
  isFocused(removeButton, "The focus has looped to the top");

  let hidden = BrowserTestUtils.waitForEvent(moreOptionsMenu, "hidden");
  EventUtils.synthesizeKey("Escape", {});
  await hidden;
  isFocused(moreOptionsButton, "Escape closed the menu");

  // Disable the add-on.
  let disableButton = card.querySelector('[action="toggle-disabled"]');
  tab({ shiftKey: true });
  isFocused(disableButton, "The disable toggle is focused");
  is(card.parentNode, enabledSection, "The card is in the enabled section");
  space();
  // Wait for the add-on state to change.
  let [disabledAddon] = await AddonTestUtils.promiseAddonEvent("onDisabled");
  is(disabledAddon.id, id, "The right add-on was disabled");
  is(
    card.parentNode,
    enabledSection,
    "The card is still in the enabled section"
  );
  isFocused(disableButton, "The disable button is still focused");
  let moved = BrowserTestUtils.waitForEvent(list, "move");
  // Click outside the list to clear any focus.
  EventUtils.synthesizeMouseAtCenter(
    doc.querySelector(".header-name"),
    {},
    win
  );
  await moved;
  is(
    card.parentNode,
    disabledSection,
    "The card moved when keyboard focus left the list"
  );

  // Remove the add-on.
  moreOptionsButton.focus();
  shown = BrowserTestUtils.waitForEvent(moreOptionsMenu, "shown");
  space();
  is(moreOptionsMenu.open, true, "The menu is open");
  await shown;
  isFocused(removeButton, "The remove button is focused");
  let removed = BrowserTestUtils.waitForEvent(list, "remove");
  space();
  await removed;
  is(card.parentNode, null, "The card is no longer on the page");

  await extension.unload();
  await closeView(win);
});

add_task(async function testOpenDetailFromNameKeyboard() {
  let id = "details@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Detail extension",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");

  let card = getCardByAddonId(win.document, id);

  info("focus the add-on's name, which should be an <a>");
  card.addonNameEl.focus();

  let detailsLoaded = waitForViewLoad(win);
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await detailsLoaded;

  card = getCardByAddonId(win.document, id);
  is(
    card.addonNameEl.textContent,
    "Detail extension",
    "The right detail view is laoded"
  );

  await extension.unload();
  await closeView(win);
});

add_task(async function testExtensionReordering() {
  let extensions = createExtensions([
    { name: "Extension One" },
    { name: "This is last" },
    { name: "An extension, is first" },
  ]);

  await Promise.all(extensions.map(extension => extension.startup()));

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Get a reference to the addon-list for events.
  let list = doc.querySelector("addon-list");

  // Find the related cards, they should all have @mochi.test ids.
  let enabledSection = getSection(doc, "extension-enabled-section");
  let cards = getTestCards(enabledSection);

  is(cards.length, 3, "Each extension has an addon-card");

  let order = Array.from(cards).map(card => card.addon.name);
  Assert.deepEqual(
    order,
    ["An extension, is first", "Extension One", "This is last"],
    "The add-ons are sorted by name"
  );

  // Disable the second extension.
  let disabledSection = getSection(doc, "extension-disabled-section");
  ok(isEmpty(disabledSection), "The disabled section is initially empty");

  // Disable the add-ons in a different order.
  let reorderedCards = [cards[1], cards[0], cards[2]];
  for (let { addon } of reorderedCards) {
    let moved = BrowserTestUtils.waitForEvent(list, "move");
    await addon.disable();
    await moved;
  }

  order = Array.from(getTestCards(disabledSection)).map(
    card => card.addon.name
  );
  Assert.deepEqual(
    order,
    ["An extension, is first", "Extension One", "This is last"],
    "The add-ons are sorted by name"
  );

  // All of our installed add-ons are disabled, install a new one.
  let [newExtension] = createExtensions([{ name: "Extension New" }]);
  let added = BrowserTestUtils.waitForEvent(list, "add");
  await newExtension.startup();
  await added;

  let [newCard] = getTestCards(enabledSection);
  is(
    newCard.addon.name,
    "Extension New",
    "The new add-on is in the enabled list"
  );

  // Enable everything again.
  for (let { addon } of cards) {
    let moved = BrowserTestUtils.waitForEvent(list, "move");
    await addon.enable();
    await moved;
  }

  order = Array.from(getTestCards(enabledSection)).map(card => card.addon.name);
  Assert.deepEqual(
    order,
    [
      "An extension, is first",
      "Extension New",
      "Extension One",
      "This is last",
    ],
    "The add-ons are sorted by name"
  );

  // Remove the new extension.
  let removed = BrowserTestUtils.waitForEvent(list, "remove");
  await newExtension.unload();
  await removed;
  is(newCard.parentNode, null, "The new card has been removed");

  await Promise.all(extensions.map(extension => extension.unload()));
  await closeView(win);
});

add_task(async function testThemeList() {
  let theme = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: "theme@mochi.test" } },
      name: "My theme",
      theme: {},
    },
    useAddonManager: "temporary",
  });

  let win = await loadInitialView("theme");
  let doc = win.document;

  let list = doc.querySelector("addon-list");

  let cards = getTestCards(list);
  is(cards.length, 0, "There are no test themes to start");

  let added = BrowserTestUtils.waitForEvent(list, "add");
  await theme.startup();
  await added;

  cards = getTestCards(list);
  is(cards.length, 1, "There is now one custom theme");

  let [card] = cards;
  is(card.addon.name, "My theme", "The card is for the test theme");

  let enabledSection = getSection(doc, "theme-enabled-section");
  let disabledSection = getSection(doc, "theme-disabled-section");

  await TestUtils.waitForCondition(
    () => enabledSection.querySelectorAll("addon-card").length == 1
  );

  is(
    card.parentNode,
    enabledSection,
    "The new theme card is in the enabled section"
  );
  is(
    enabledSection.querySelectorAll("addon-card").length,
    1,
    "There is one enabled theme"
  );

  let toggleThemeEnabled = async () => {
    let themesChanged = waitForThemeChange(list);
    card.querySelector('[action="toggle-disabled"]').click();
    await themesChanged;

    await TestUtils.waitForCondition(
      () => enabledSection.querySelectorAll("addon-card").length == 1
    );
  };

  await toggleThemeEnabled();

  is(
    card.parentNode,
    disabledSection,
    "The card is now in the disabled section"
  );
  is(
    enabledSection.querySelectorAll("addon-card").length,
    1,
    "There is one enabled theme"
  );

  // Re-enable the theme.
  await toggleThemeEnabled();
  is(card.parentNode, enabledSection, "Card is back in the Enabled section");

  // Remove theme and verify that the default theme is re-enabled.
  let removed = BrowserTestUtils.waitForEvent(list, "remove");
  // Confirm removal.
  promptService._response = 0;
  card.querySelector('[action="remove"]').click();
  await removed;
  is(card.parentNode, null, "Card has been removed from the view");
  await TestUtils.waitForCondition(
    () => enabledSection.querySelectorAll("addon-card").length == 1
  );

  let defaultTheme = getCardByAddonId(doc, "default-theme@mozilla.org");
  is(defaultTheme.parentNode, enabledSection, "The default theme is reenabled");

  await testUndoPendingUninstall(list, card.addon);
  await TestUtils.waitForCondition(
    () => enabledSection.querySelectorAll("addon-card").length == 1
  );
  is(defaultTheme.parentNode, disabledSection, "The default theme is disabled");
  ok(getCardByAddonId(enabledSection, theme.id), "Theme should be reenabled");

  await theme.unload();
  await closeView(win);
});

add_task(async function testBuiltInThemeButtons() {
  let win = await loadInitialView("theme");
  let doc = win.document;

  // Find the addon-list to listen for events.
  let list = doc.querySelector("addon-list");
  let enabledSection = getSection(doc, "theme-enabled-section");
  let disabledSection = getSection(doc, "theme-disabled-section");

  let defaultTheme = getCardByAddonId(doc, "default-theme@mozilla.org");
  let darkTheme = getCardByAddonId(doc, "firefox-compact-dark@mozilla.org");

  // Check that themes are in the expected spots.
  is(defaultTheme.parentNode, enabledSection, "The default theme is enabled");
  is(darkTheme.parentNode, disabledSection, "The dark theme is disabled");

  // The default theme shouldn't have remove or disable options.
  let defaultButtons = {
    toggleDisabled: defaultTheme.querySelector('[action="toggle-disabled"]'),
    remove: defaultTheme.querySelector('[action="remove"]'),
  };
  is(defaultButtons.toggleDisabled.hidden, true, "Disable is hidden");
  is(defaultButtons.remove.hidden, true, "Remove is hidden");

  // The dark theme should have an enable button, but not remove.
  let darkButtons = {
    toggleDisabled: darkTheme.querySelector('[action="toggle-disabled"]'),
    remove: darkTheme.querySelector('[action="remove"]'),
  };
  is(darkButtons.toggleDisabled.hidden, false, "Enable is visible");
  is(darkButtons.remove.hidden, true, "Remove is hidden");

  // Enable the dark theme and check the buttons again.
  let themesChanged = waitForThemeChange(list);
  darkButtons.toggleDisabled.click();
  await themesChanged;

  await TestUtils.waitForCondition(
    () => enabledSection.querySelectorAll("addon-card").length == 1
  );

  // Check the buttons.
  is(defaultButtons.toggleDisabled.hidden, false, "Enable is visible");
  is(defaultButtons.remove.hidden, true, "Remove is hidden");
  is(darkButtons.toggleDisabled.hidden, false, "Disable is visible");
  is(darkButtons.remove.hidden, true, "Remove is hidden");

  // Disable the dark theme.
  themesChanged = waitForThemeChange(list);
  darkButtons.toggleDisabled.click();
  await themesChanged;

  await TestUtils.waitForCondition(
    () => enabledSection.querySelectorAll("addon-card").length == 1
  );

  // The themes are back to their starting posititons.
  is(defaultTheme.parentNode, enabledSection, "Default is enabled");
  is(darkTheme.parentNode, disabledSection, "Dark is disabled");

  await closeView(win);
});

add_task(async function testSideloadRemoveButton() {
  const id = "sideload@mochi.test";
  mockProvider.createAddons([
    {
      id,
      name: "Sideloaded",
      permissions: 0,
    },
  ]);

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getCardByAddonId(doc, id);

  let moreOptionsPanel = card.querySelector("panel-list");
  let moreOptionsButton = card.querySelector('[action="more-options"]');
  let panelOpened = BrowserTestUtils.waitForEvent(moreOptionsPanel, "shown");
  EventUtils.synthesizeMouseAtCenter(moreOptionsButton, {}, win);
  await panelOpened;

  // Verify the remove button is visible with a SUMO link.
  let removeButton = card.querySelector('[action="remove"]');
  ok(removeButton.disabled, "Remove is disabled");
  ok(!removeButton.hidden, "Remove is visible");

  // Remove but cancel.
  let prevented = BrowserTestUtils.waitForEvent(card, "remove-disabled");
  removeButton.click();
  await prevented;

  // reopen the panel
  panelOpened = BrowserTestUtils.waitForEvent(moreOptionsPanel, "shown");
  EventUtils.synthesizeMouseAtCenter(moreOptionsButton, {}, win);
  await panelOpened;

  let sumoLink = removeButton.querySelector("a");
  ok(sumoLink, "There's a link");
  is(
    doc.l10n.getAttributes(removeButton).id,
    "remove-addon-disabled-button",
    "The can't remove text is shown"
  );
  sumoLink.focus();
  is(doc.activeElement, sumoLink, "The link can be focused");

  let newTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, REMOVE_SUMO_URL);
  sumoLink.click();
  BrowserTestUtils.removeTab(await newTabOpened);

  await closeView(win);
});

add_task(async function testOnlyTypeIsShown() {
  let win = await loadInitialView("theme");
  let doc = win.document;

  // Find the addon-list to listen for events.
  let list = doc.querySelector("addon-list");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id: "test@mochi.test" } },
    },
    useAddonManager: "temporary",
  });

  let skipped = BrowserTestUtils.waitForEvent(
    list,
    "skip-add",
    e => e.detail == "type-mismatch"
  );
  await extension.startup();
  await skipped;

  let cards = getTestCards(list);
  is(cards.length, 0, "There are no test extension cards");

  await extension.unload();
  await closeView(win);
});

add_task(async function testPluginIcons() {
  const pluginIconUrl = "chrome://global/skin/icons/plugin.svg";

  let win = await loadInitialView("plugin");
  let doc = win.document;

  // Check that the icons are set to the plugin icon.
  let icons = doc.querySelectorAll(".card-heading-icon");
  ok(!!icons.length, "There are some plugins listed");

  for (let icon of icons) {
    is(icon.src, pluginIconUrl, "Plugins use the plugin icon");
  }

  await closeView(win);
});

add_task(async function testExtensionGenericIcon() {
  const extensionIconUrl =
    "chrome://mozapps/skin/extensions/extensionGeneric.svg";

  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getCardByAddonId(doc, id);
  let icon = card.querySelector(".addon-icon");
  is(icon.src, extensionIconUrl, "Extensions without icon use the generic one");

  await extension.unload();
  await closeView(win);
});

add_task(async function testSectionHeadingKeys() {
  mockProvider.createAddons([
    {
      id: "test-theme",
      name: "Test Theme",
      type: "theme",
    },
    {
      id: "test-extension-disabled",
      name: "Test Disabled Extension",
      type: "extension",
      userDisabled: true,
    },
    {
      id: "test-plugin-disabled",
      name: "Test Disabled Plugin",
      type: "plugin",
      userDisabled: true,
    },
    {
      id: "test-locale",
      name: "Test Enabled Locale",
      type: "locale",
    },
    {
      id: "test-locale-disabled",
      name: "Test Disabled Locale",
      type: "locale",
      userDisabled: true,
    },
    {
      id: "test-dictionary",
      name: "Test Enabled Dictionary",
      type: "dictionary",
    },
    {
      id: "test-dictionary-disabled",
      name: "Test Disabled Dictionary",
      type: "dictionary",
      userDisabled: true,
    },
    {
      id: "test-sitepermission",
      name: "Test Enabled Site Permission",
      type: "sitepermission",
    },
    {
      id: "test-sitepermission-disabled",
      name: "Test Disabled Site Permission",
      type: "sitepermission",
      userDisabled: true,
    },
  ]);

  for (let type of [
    "extension",
    "theme",
    "plugin",
    "locale",
    "dictionary",
    "sitepermission",
  ]) {
    info(`loading view for addon type ${type}`);
    let win = await loadInitialView(type);
    let doc = win.document;

    for (let status of ["enabled", "disabled"]) {
      let section = getSection(doc, `${type}-${status}-section`);
      let el = section?.querySelector(".list-section-heading");
      isnot(el, null, `Should have ${status} heading for ${type} section`);
      is(
        el && doc.l10n.getAttributes(el).id,
        win.getL10nIdMapping(`${type}-${status}-heading`),
        `Should have correct ${status} heading for ${type} section`
      );
    }

    await closeView(win);
  }
});

add_task(async function testDisabledDimming() {
  const id = "disabled@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Disable me",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let addon = await AddonManager.getAddonByID(id);

  let win = await loadInitialView("extension");
  let doc = win.document;
  let pageHeader = doc.querySelector("addon-page-header");

  // Ensure there's no focus on the list.
  EventUtils.synthesizeMouseAtCenter(pageHeader, {}, win);

  const checkOpacity = (card, expected, msg) => {
    let { opacity } = card.ownerGlobal.getComputedStyle(card.firstElementChild);
    let normalize = val => Math.floor(val * 10);
    is(normalize(opacity), normalize(expected), msg);
  };
  const waitForTransition = card =>
    BrowserTestUtils.waitForEvent(
      card.firstElementChild,
      "transitionend",
      e => e.propertyName === "opacity" && e.target.classList.contains("card")
    );

  let card = getCardByAddonId(doc, id);
  checkOpacity(card, "1", "The opacity is 1 when enabled");

  // Disable the add-on, check again.
  let list = doc.querySelector("addon-list");
  let moved = BrowserTestUtils.waitForEvent(list, "move");
  await addon.disable();
  await moved;

  let disabledSection = getSection(doc, "extension-disabled-section");
  is(card.parentNode, disabledSection, "The card is in the disabled section");
  checkOpacity(card, "0.6", "The opacity is dimmed when disabled");

  // Click on the menu button, this should un-dim the card.
  let transitionEnded = waitForTransition(card);
  let moreOptionsButton = card.querySelector(".more-options-button");
  EventUtils.synthesizeMouseAtCenter(moreOptionsButton, {}, win);
  await transitionEnded;
  checkOpacity(card, "1", "The opacity is 1 when the menu is open");

  // Close the menu, opacity should return.
  transitionEnded = waitForTransition(card);
  EventUtils.synthesizeMouseAtCenter(pageHeader, {}, win);
  await transitionEnded;
  checkOpacity(card, "0.6", "The card is dimmed again");

  await closeView(win);
  await extension.unload();
});

add_task(async function testEmptyMessage() {
  let tests = [
    {
      type: "extension",
      message: "Get extensions and themes on ",
    },
    {
      type: "theme",
      message: "Get extensions and themes on ",
    },
    {
      type: "plugin",
      message: "Get extensions and themes on ",
    },
    {
      type: "locale",
      message: "Get language packs on ",
    },
    {
      type: "dictionary",
      message: "Get dictionaries on ",
    },
  ];

  for (let test of tests) {
    let win = await loadInitialView(test.type);
    let doc = win.document;
    let enabledSection = getSection(doc, `${test.type}-enabled-section`);
    let disabledSection = getSection(doc, `${test.type}-disabled-section`);
    const message = doc.querySelector("#empty-addons-message");

    // Test if the correct locale has been applied.
    ok(
      message.textContent.startsWith(test.message),
      `View ${test.type} has correct empty list message`
    );

    // With at least one enabled/disabled add-on (see testSectionHeadingKeys),
    // the message is hidden.
    is_element_hidden(message, "Empty addons message hidden");

    // The test runner (Mochitest) relies on add-ons that should not be removed.
    // Simulate the scenario of zero add-ons by clearing all rendered sections.
    while (enabledSection.firstChild) {
      enabledSection.firstChild.remove();
    }

    while (disabledSection.firstChild) {
      disabledSection.firstChild.remove();
    }

    if (test.type == "theme") {
      // The colorways section won't exist if there's no active collection.
      // Bug 1774432 is going to make it easier to use a mock collection
      // which would allow for a more predictable setup here.
      getSection(doc, "colorways-section")?.remove();
    }

    // Message should now be displayed
    is_element_visible(message, "Empty addons message visible");

    await closeView(win);
  }
});
