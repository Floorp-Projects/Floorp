/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that updates that change an add-on's ID show up correctly in the UI

var gProvider;
var gManagerWindow;
var gCategoryUtilities;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "manually updating addon",
    version: "1.0",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE
  }]);

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}

add_test(function() {
  gCategoryUtilities.openType("extension", function() {
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
    is(item._version.value, "1.0", "Should still show the old version in the normal list");
    var name = gManagerWindow.document.getAnonymousElementByAttribute(item, "anonid", "name");
    is(name.value, "manually updating addon", "Should show the old name in the list");
    var update = gManagerWindow.document.getAnonymousElementByAttribute(item, "anonid", "update-btn");
    is_element_visible(update, "Update button should be visible");

    item = get_addon_element(gManagerWindow, "addon2@tests.mozilla.org");
    is(item, null, "Should not show the new version in the list");

    run_next_test();
  });
});

add_test(function() {
  var item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  var update = gManagerWindow.document.getAnonymousElementByAttribute(item, "anonid", "update-btn");
  EventUtils.synthesizeMouseAtCenter(update, { }, gManagerWindow);

  var pending = gManagerWindow.document.getAnonymousElementByAttribute(item, "anonid", "pending");
  is_element_visible(pending, "Pending message should be visible");
  is(pending.textContent,
     get_string("notification.upgrade", "manually updating addon", gApp),
     "Pending message should be correct");

  item = get_addon_element(gManagerWindow, "addon2@tests.mozilla.org");
  is(item, null, "Should not show the new version in the list");

  run_next_test();
});
