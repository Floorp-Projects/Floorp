/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests manual updates, including the Available Updates pane

var gProvider;
var gManagerWindow;
var gCategoryUtilities;
var gAvailableCategory;

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "auto updating addon",
    version: "1.0",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_ENABLE
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
  gAvailableCategory = gManagerWindow.gCategories.get("addons://updates/available");
  is(gCategoryUtilities.isVisible(gAvailableCategory), false, "Available Updates category should initially be hidden");
  
  gProvider.createAddons([{
    id: "addon2@tests.mozilla.org",
    name: "manually updating addon",
    version: "1.0",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE
  }]);
  
  is(gCategoryUtilities.isVisible(gAvailableCategory), true, "Available Updates category should now be visible");
  
  gAvailableCategory.addEventListener("CategoryVisible", function() {
    gAvailableCategory.removeEventListener("CategoryVisible", arguments.callee, false);
    is(gCategoryUtilities.isVisible(gAvailableCategory), false, "Available Updates category should not be visible");
    gAvailableCategory.addEventListener("CategoryVisible", function() {
      gAvailableCategory.removeEventListener("CategoryVisible", arguments.callee, false);
      is(gCategoryUtilities.isVisible(gAvailableCategory), true, "Available Updates category should be visible");
      run_next_test();
    }, false);
    gProvider.addons[1].applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;
  }, false);
  gProvider.addons[1].applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
});


add_test(function() {
  gAvailableCategory.addEventListener("CategoryBadgeUpdated", function() {
    gAvailableCategory.removeEventListener("CategoryBadgeUpdated", arguments.callee, false);
    is(gAvailableCategory.badgeCount, 1, "Badge for Available Updates should now be 1");
    run_next_test();
  }, false);
  
  gProvider.createInstalls([{
    name: "manually updating addon (new and improved!)",
    existingAddon: gProvider.addons[1],
    version: "1.1",
    releaseNotesURI: Services.io.newURI(TESTROOT + "thereIsNoFileHere.xhtml", null, null)
  }]);
});


add_test(function() {
  wait_for_view_load(gManagerWindow, function() {
    is(gManagerWindow.document.getElementById("categories").selectedItem.value, "addons://updates/available", "Available Updates category should now be selected");
    is(gManagerWindow.gViewController.currentViewId, "addons://updates/available", "Available Updates view should be the current view");
    run_next_test();
  }, true);
  EventUtils.synthesizeMouse(gAvailableCategory, 2, 2, { }, gManagerWindow);
});


add_test(function() {
  var list = gManagerWindow.document.getElementById("updates-list");
  is(list.itemCount, 1, "Should be 1 available update listed");
  var item = list.firstChild;
  is(item.mAddon.id, "addon2@tests.mozilla.org", "Update item should be for the manually updating addon");
  
  // for manual update items, update-related properties are updated asynchronously,
  // so we poll for one of the expected changes to know when its done
  function waitForAsyncInit() {
    if (item._version.value == "1.1") {
      run_next_test();
      return;
    }
    info("Update item not initialized yet, checking again in 100ms");
    setTimeout(waitForAsyncInit, 100);
  }
  waitForAsyncInit();
});

add_test(function() {
  var list = gManagerWindow.document.getElementById("updates-list");
  var item = list.firstChild;
  is(item._version.value, "1.1", "Update item should have version number of the update");
  var postfix = gManagerWindow.document.getAnonymousElementByAttribute(item, "class", "update-postfix");
  is_element_visible(postfix, "'Update' postfix should be visible");
  is_element_visible(item._updateAvailable, "");
  is_element_visible(item._relNotesToggle, "Release notes toggle should be visible");

  info("Opening release notes");
  item.addEventListener("RelNotesToggle", function() {
    item.removeEventListener("RelNotesToggle", arguments.callee, false);
    info("Release notes now open");

    is_element_hidden(item._relNotesLoading, "Release notes loading message should be hidden");
    is_element_visible(item._relNotesError, "Release notes error message should be visible");
    is(item._relNotes.childElementCount, 0, "Release notes should be empty");

    info("Closing release notes");
    item.addEventListener("RelNotesToggle", function() {
      item.removeEventListener("RelNotesToggle", arguments.callee, false);
      info("Release notes now closed");
      info("Setting Release notes URI to something that should load");
      gProvider.installs[0].releaseNotesURI = Services.io.newURI(TESTROOT + "releaseNotes.xhtml", null, null)

      info("Re-opening release notes");
      item.addEventListener("RelNotesToggle", function() {
        item.removeEventListener("RelNotesToggle", arguments.callee, false);
        info("Release notes now open");

        is_element_hidden(item._relNotesLoading, "Release notes loading message should be hidden");
        is_element_hidden(item._relNotesError, "Release notes error message should be hidden");
        isnot(item._relNotes.childElementCount, 0, "Release notes should have been inserted into container");
        run_next_test();

      }, false);
      EventUtils.synthesizeMouse(item._relNotesToggle, 2, 2, { }, gManagerWindow);
      is_element_visible(item._relNotesLoading, "Release notes loading message should be visible");

    }, false);
    EventUtils.synthesizeMouse(item._relNotesToggle, 2, 2, { }, gManagerWindow);

  }, false);
  EventUtils.synthesizeMouse(item._relNotesToggle, 2, 2, { }, gManagerWindow);
  is_element_visible(item._relNotesLoading, "Release notes loading message should be visible");
});


add_test(function() {
  var list = gManagerWindow.document.getElementById("updates-list");
  var item = list.firstChild;
  var updateBtn = item._updateBtn;
  is_element_visible(updateBtn, "Update button should be visible");

  var install = gProvider.installs[0];
  var listener = {
    onInstallStarted: function() {
      info("Install started");
      is_element_visible(item._installStatus, "Install progress widget should be visible");
    },
    onInstallEnded: function() {
      install.removeTestListener(this);
      info("install ended");
      is_element_hidden(item._installStatus, "Install progress widget should be hidden");
      run_next_test();
    }
  };
  install.addTestListener(listener);
  EventUtils.synthesizeMouse(updateBtn, 2, 2, { }, gManagerWindow);
});
