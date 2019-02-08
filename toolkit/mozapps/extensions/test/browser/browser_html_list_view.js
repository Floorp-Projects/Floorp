let gManagerWindow;
let gCategoryUtilities;

async function loadInitialView(type) {
  gManagerWindow = await open_manager(null);
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  await gCategoryUtilities.openType(type);

  let browser = gManagerWindow.document.getElementById("html-view-browser");
  return browser.contentWindow;
}

function closeView() {
  return close_manager(gManagerWindow);
}

function getSection(doc, type) {
  return doc.querySelector(`.list-section[type="${type}"]`);
}

add_task(async function enableHtmlViews() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
});

add_task(async function testExtensionList() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: {gecko: {id: "test@mochi.test"}},
      icons: {
        32: "test-icon.png",
      },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let addon = await AddonManager.getAddonByID("test@mochi.test");
  ok(addon, "The add-on can be found");

  let win = await loadInitialView("extension");
  let doc = win.document;

  // There shouldn't be any disabled extensions.
  is(getSection(doc, "disabled"), null, "The disabled section is hidden");

  // The loaded extension should be in the enabled list.
  let enabledSection = getSection(doc, "enabled");
  let card = enabledSection.querySelector('[addon-id="test@mochi.test"]');
  ok(card, "The card is in the enabled section");

  // Check the properties of the card.
  is(card.querySelector(".addon-name").textContent, "Test extension",
     "The name is set");
  let icon = card.querySelector(".addon-icon");
  ok(icon.src.endsWith("/test-icon.png"), "The icon is set");

  // Disable the extension.
  let disableButton = card.querySelector('[action="toggle-disabled"]');
  is(doc.l10n.getAttributes(disableButton).id, "disable-addon-button",
     "The button has the disable label");

  let disabled = TestUtils.waitForCondition(() => getSection(doc, "disabled"));
  disableButton.click();
  await disabled;

  // The disable button is now enable.
  let disabledSection = getSection(doc, "disabled");
  card = disabledSection.querySelector('[addon-id="test@mochi.test"]');
  let enableButton = card.querySelector('[action="toggle-disabled"]');
  is(doc.l10n.getAttributes(enableButton).id, "enable-addon-button",
     "The button has the enable label");

  // Remove the add-on.
  let removeButton = card.querySelector('[action="remove"]');
  is(doc.l10n.getAttributes(removeButton).id, "remove-addon-button",
     "The button has the remove label");

  let removed = TestUtils.waitForCondition(() => !getSection(doc, "disabled"));
  removeButton.click();
  await removed;

  addon = await AddonManager.getAddonByID("test@mochi.test");
  ok(!addon, "The addon is not longer found");

  await extension.unload();
  await closeView(win);
});

add_task(async function testPluginIcons() {
  const pluginIconUrl = "chrome://global/skin/plugins/pluginGeneric.svg";

  let win = await loadInitialView("plugin");
  let doc = win.document;

  // Check that the icons are set to the plugin icon.
  let icons = doc.querySelectorAll(".card-heading-icon");
  ok(icons.length > 0, "There are some plugins listed");

  for (let icon of icons) {
    is(icon.src, pluginIconUrl, "Plugins use the plugin icon");
  }

  await closeView(win);
});
