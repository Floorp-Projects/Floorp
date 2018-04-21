/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that updates that change an add-on's ID show up correctly in the UI

var gProvider;
var gManagerWindow;
var gCategoryUtilities;

async function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "manually updating addon",
    version: "1.0",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE
  }]);

  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
  finish();
}

add_test(async function() {
  await gCategoryUtilities.openType("extension");
  gProvider.createInstalls([{
    name: "updated add-on",
    existingAddon: gProvider.addons[0],
    version: "2.0"
  }]);
  var newAddon = new MockAddon("addon2@tests.mozilla.org");
  newAddon.name = "updated add-on";
  newAddon.version = "2.0";
  newAddon.pendingOperations = AddonManager.PENDING_INSTALL;
  gProvider.installs[0]._addonToInstall = newAddon;

  var item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  var name = gManagerWindow.document.getAnonymousElementByAttribute(item, "anonid", "name");
  is(name.value, "manually updating addon", "Should show the old name in the list");
  get_tooltip_info(item).then(({ name, version }) => {
    is(name, "manually updating addon", "Should show the old name in the tooltip");
    is(version, "1.0", "Should still show the old version in the tooltip");

    var update = gManagerWindow.document.getAnonymousElementByAttribute(item, "anonid", "update-btn");
    is_element_visible(update, "Update button should be visible");

    item = get_addon_element(gManagerWindow, "addon2@tests.mozilla.org");
    is(item, null, "Should not show the new version in the list");

    run_next_test();
  });
});
