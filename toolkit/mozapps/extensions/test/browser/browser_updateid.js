/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that updates that change an add-on's ID show up correctly in the UI

var gProvider;
var gManagerWindow;
var gCategoryUtilities;

function getName(item) {
  return item.addonNameEl.textContent;
}

async function getUpdateButton(item) {
  let button = item.querySelector('[action="install-update"]');
  let panel = button.closest("panel-list");
  let shown = BrowserTestUtils.waitForEvent(panel, "shown");
  let moreOptionsButton = item.querySelector('[action="more-options"]');
  EventUtils.synthesizeMouseAtCenter(moreOptionsButton, {}, item.ownerGlobal);
  await shown;
  return button;
}

add_task(async function test_updateid() {
  // Close the existing about:addons tab and unrestier the existing MockProvider
  // instance if a previous failed test has not been able to clear them.
  if (gManagerWindow) {
    await close_manager(gManagerWindow);
  }
  if (gProvider) {
    gProvider.unregister();
  }

  gProvider = new MockProvider();

  gProvider.createAddons([
    {
      id: "addon1@tests.mozilla.org",
      name: "manually updating addon",
      version: "1.0",
      applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE,
    },
  ]);

  gManagerWindow = await open_manager("addons://list/extension");
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  await gCategoryUtilities.openType("extension");

  gProvider.createInstalls([
    {
      name: "updated add-on",
      existingAddon: gProvider.addons[0],
      version: "2.0",
    },
  ]);
  var newAddon = new MockAddon("addon2@tests.mozilla.org");
  newAddon.name = "updated add-on";
  newAddon.version = "2.0";
  newAddon.pendingOperations = AddonManager.PENDING_INSTALL;
  gProvider.installs[0]._addonToInstall = newAddon;

  var item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  is(
    getName(item),
    "manually updating addon",
    "Should show the old name in the list"
  );
  const { name, version } = await get_tooltip_info(item, gManagerWindow);
  is(
    name,
    "manually updating addon",
    "Should show the old name in the tooltip"
  );
  is(version, "1.0", "Should still show the old version in the tooltip");

  var update = await getUpdateButton(item);
  is_element_visible(update, "Update button should be visible");

  item = get_addon_element(gManagerWindow, "addon2@tests.mozilla.org");
  is(item, null, "Should not show the new version in the list");

  await close_manager(gManagerWindow);
  gManagerWindow = null;
  gProvider.unregister();
  gProvider = null;
});
