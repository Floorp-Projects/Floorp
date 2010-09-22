/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the recent updates pane

var gProvider;
var gManagerWindow;
var gCategoryUtilities;

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "updated 6 hours ago",
    version: "1.0",
    updateDate: new Date(Date.now() - (1000 * 60 * 60 * 6)),
    releaseNotesURI: Services.io.newURI(TESTROOT + "releaseNotes.xhtml", null, null)
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
  info("Checking menuitem for Recent Updates opens that pane");
  var recentCat = gManagerWindow.gCategories.get("addons://updates/recent");
  is(gCategoryUtilities.isVisible(recentCat), false, "Recent Updates category should initially be hidden");

  var utilsBtn = gManagerWindow.document.getElementById("header-utils-btn");
  utilsBtn.addEventListener("popupshown", function() {
    wait_for_view_load(gManagerWindow, function() {
      is(gCategoryUtilities.isVisible(recentCat), true, "Recent Updates category should now be visible");
      is(gManagerWindow.document.getElementById("categories").selectedItem.value, "addons://updates/recent", "Recent Updates category should now be selected");
      is(gManagerWindow.gViewController.currentViewId, "addons://updates/recent", "Recent Updates view should be the current view");
      run_next_test();
    }, true);
    var menuitem = gManagerWindow.document.getElementById("utils-viewUpdates");
    EventUtils.synthesizeMouse(menuitem, 2, 2, { }, gManagerWindow);
  }, false);
  EventUtils.synthesizeMouse(utilsBtn, 2, 2, { }, gManagerWindow);
});


add_test(function() {
  var updatesList = gManagerWindow.document.getElementById("updates-list");
  var items = updatesList.getElementsByTagName("richlistitem");
  var possible = ["addon1@tests.mozilla.org", "addon2@tests.mozilla.org", "addon2@tests.mozilla.org"];
  var expected = ["addon2@tests.mozilla.org", "addon1@tests.mozilla.org"];
  for (let i = 0; i < items.length; i++) {
    let item = items[i];
    let itemId = item.mAddon.id;
    if (possible.indexOf(itemId) == -1)
      continue; // skip over any other addons, such as shipped addons that would update on every build
    isnot(expected.length, 0, "Should be expecting more items");
    is(itemId, expected.shift(), "Should get expected item based on recenty of update");
    if (itemId == "addon1@tests.mozilla.org")
      is_element_visible(item._relNotesToggle, "Release notes toggle should be visible for addon with release notes");
    else
      is_element_hidden(item._relNotesToggle, "Release notes toggle should be hidden for addon with no release notes");
  }
  run_next_test();
});
