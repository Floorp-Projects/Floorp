/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the recent updates pane

var gProvider;
var gManagerWindow;
var gCategoryUtilities;

async function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "updated 6 hours ago",
    version: "1.0",
    updateDate: new Date(Date.now() - (1000 * 60 * 60 * 6)),
    releaseNotesURI: Services.io.newURI(TESTROOT + "releaseNotes.xhtml")
  }, {
    id: "addon2@tests.mozilla.org",
    name: "updated 5 seconds ago",
    version: "1.0",
    updateDate: new Date(Date.now() - (1000 * 5))
  }, {
    id: "addon3@tests.mozilla.org",
    name: "updated 1 month ago",
    version: "1.0",
    updateDate: new Date(Date.now() - (1000 * 60 * 60 * 25 * 30))
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


add_test(function() {
  info("Checking menuitem for Recent Updates opens that pane");
  var recentCat = gManagerWindow.gCategories.get("addons://updates/recent");
  is(gCategoryUtilities.isVisible(recentCat), false, "Recent Updates category should initially be hidden");

  var utilsBtn = gManagerWindow.document.getElementById("header-utils-btn");
  utilsBtn.addEventListener("popupshown", async function() {
    Promise.resolve().then(() => {
      var menuitem = gManagerWindow.document.getElementById("utils-viewUpdates");
      EventUtils.synthesizeMouse(menuitem, 2, 2, { }, gManagerWindow);
    });
    await wait_for_view_load(gManagerWindow, null, true);
    is(gCategoryUtilities.isVisible(recentCat), true, "Recent Updates category should now be visible");
    is(gManagerWindow.document.getElementById("categories").selectedItem.value, "addons://updates/recent", "Recent Updates category should now be selected");
    is(gManagerWindow.gViewController.currentViewId, "addons://updates/recent", "Recent Updates view should be the current view");
    run_next_test();
  }, {once: true});
  EventUtils.synthesizeMouse(utilsBtn, 2, 2, { }, gManagerWindow);
});


add_test(async function() {
  await close_manager(gManagerWindow);
  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  var recentCat = gManagerWindow.gCategories.get("addons://updates/recent");
  is(gCategoryUtilities.isVisible(recentCat), true, "Recent Updates category should still be visible");

  run_next_test();
});
