/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests manual updates, including the Available Updates pane

var gProvider;
var gManagerWindow;
var gCategoryUtilities;
var gAvailableCategory;

async function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "auto updating addon",
    version: "1.0",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_ENABLE
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
  gAvailableCategory = gManagerWindow.gCategories.get("addons://updates/available");
  is(gCategoryUtilities.isVisible(gAvailableCategory), false, "Available Updates category should initially be hidden");

  gProvider.createAddons([{
    id: "addon2@tests.mozilla.org",
    name: "manually updating addon",
    version: "1.0",
    isCompatible: false,
    operationsRequiringRestart: 0,
    blocklistState: Ci.nsIBlocklistService.STATE_BLOCKED,
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE
  }]);

  is(gCategoryUtilities.isVisible(gAvailableCategory), false, "Available Updates category should still be hidden");

  run_next_test();
});


add_test(async function() {
  let finished = 0;
  function maybeRunNext() {
    if (++finished == 2)
      run_next_test();
  }

  gAvailableCategory.addEventListener("CategoryBadgeUpdated", function() {
    is(gCategoryUtilities.isVisible(gAvailableCategory), true, "Available Updates category should now be visible");
    is(gAvailableCategory.badgeCount, 1, "Badge for Available Updates should now be 1");
    maybeRunNext();
  }, {once: true});

  await gCategoryUtilities.openType("extension");
  gProvider.createInstalls([{
    name: "manually updating addon (new and improved!)",
    existingAddon: gProvider.addons[1],
    version: "1.1",
    releaseNotesURI: Services.io.newURI(TESTROOT + "thereIsNoFileHere.xhtml")
  }]);

  var item = get_addon_element(gManagerWindow, "addon2@tests.mozilla.org");
  get_tooltip_info(item).then(({ version }) => {
    is(version, "1.0", "Should still show the old version in the tooltip");
    maybeRunNext();
  });
});


add_test(async function() {
  Promise.resolve().then(() => {
    EventUtils.synthesizeMouseAtCenter(gAvailableCategory, { }, gManagerWindow);
  });
  await wait_for_view_load(gManagerWindow, null, true);
  is(gManagerWindow.document.getElementById("categories").selectedItem.value, "addons://updates/available", "Available Updates category should now be selected");
  is(gManagerWindow.gViewController.currentViewId, "addons://updates/available", "Available Updates view should be the current view");
  run_next_test();
});


add_test(async function() {
  var list = gManagerWindow.document.getElementById("updates-list");
  is(list.itemCount, 1, "Should be 1 available update listed");
  var item = list.firstChild;
  is(item.mAddon.id, "addon2@tests.mozilla.org", "Update item should be for the manually updating addon");

  // The item in the list will be checking for update information asynchronously
  // so we have to wait for it to complete. Doing the same async request should
  // make our callback be called later.
  await AddonManager.getAllInstalls();
  run_next_test();
});

add_test(function() {
  var list = gManagerWindow.document.getElementById("updates-list");
  var item = list.firstChild;
  get_tooltip_info(item).then(({ version }) => {
    is(version, "1.1", "Update item should have version number of the update");
    var postfix = gManagerWindow.document.getAnonymousElementByAttribute(item, "class", "update-postfix");
    is_element_visible(postfix, "'Update' postfix should be visible");
    is_element_visible(item._updateAvailable, "");
    is_element_visible(item._relNotesToggle, "Release notes toggle should be visible");
    is_element_hidden(item._warning, "Incompatible warning should be hidden");
    is_element_hidden(item._error, "Blocklist error should be hidden");

    info("Opening release notes");
    item.addEventListener("RelNotesToggle", function() {
      info("Release notes now open");

      is_element_hidden(item._relNotesLoading, "Release notes loading message should be hidden");
      is_element_visible(item._relNotesError, "Release notes error message should be visible");
      is(item._relNotes.childElementCount, 0, "Release notes should be empty");

      info("Closing release notes");
      item.addEventListener("RelNotesToggle", function() {
        info("Release notes now closed");
        info("Setting Release notes URI to something that should load");
        gProvider.installs[0].releaseNotesURI = Services.io.newURI(TESTROOT + "releaseNotes.xhtml");

        info("Re-opening release notes");
        item.addEventListener("RelNotesToggle", function() {
          info("Release notes now open");

          is_element_hidden(item._relNotesLoading, "Release notes loading message should be hidden");
          is_element_hidden(item._relNotesError, "Release notes error message should be hidden");
          isnot(item._relNotes.childElementCount, 0, "Release notes should have been inserted into container");
          run_next_test();

        }, {once: true});
        EventUtils.synthesizeMouseAtCenter(item._relNotesToggle, { }, gManagerWindow);
        is_element_visible(item._relNotesLoading, "Release notes loading message should be visible");

      }, {once: true});
      EventUtils.synthesizeMouseAtCenter(item._relNotesToggle, { }, gManagerWindow);

    }, {once: true});
    EventUtils.synthesizeMouseAtCenter(item._relNotesToggle, { }, gManagerWindow);
    is_element_visible(item._relNotesLoading, "Release notes loading message should be visible");
  });
});


add_test(function() {
  var badgeUpdated = false;
  var installCompleted = false;

  gAvailableCategory.addEventListener("CategoryBadgeUpdated", function() {
    if (installCompleted)
      run_next_test();
    else
      badgeUpdated = true;
  }, {once: true});

  var list = gManagerWindow.document.getElementById("updates-list");
  var item = list.firstChild;
  var updateBtn = item._updateBtn;
  is_element_visible(updateBtn, "Update button should be visible");

  var install = gProvider.installs[0];
  var listener = {
    onInstallStarted() {
      info("Install started");
      is_element_visible(item._installStatus, "Install progress widget should be visible");
    },
    onInstallEnded(...args) {
      install.removeTestListener(this);
      info("Install ended");
      is_element_hidden(item._installStatus, "Install progress widget should be hidden");

      if (badgeUpdated)
        run_next_test();
      else
        installCompleted = true;
    }
  };
  install.addTestListener(listener);
  EventUtils.synthesizeMouseAtCenter(updateBtn, { }, gManagerWindow);
});


add_test(async function() {
  is(gCategoryUtilities.isVisible(gAvailableCategory), true, "Available Updates category should still be visible");
  is(gAvailableCategory.badgeCount, 0, "Badge for Available Updates should now be 0");

  await gCategoryUtilities.openType("extension");
  is(gCategoryUtilities.isVisible(gAvailableCategory), false, "Available Updates category should be hidden");

  await close_manager(gManagerWindow);
  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  gAvailableCategory = gManagerWindow.gCategories.get("addons://updates/available");

  is(gCategoryUtilities.isVisible(gAvailableCategory), false, "Available Updates category should be hidden");

  run_next_test();
});

add_test(function() {
  gAvailableCategory.addEventListener("CategoryBadgeUpdated", async function() {
    is(gCategoryUtilities.isVisible(gAvailableCategory), true, "Available Updates category should now be visible");
    is(gAvailableCategory.badgeCount, 1, "Badge for Available Updates should now be 1");

    gAvailableCategory.addEventListener("CategoryBadgeUpdated", function() {
      is(gCategoryUtilities.isVisible(gAvailableCategory), false, "Available Updates category should now be hidden");

      run_next_test();
    }, {once: true});

    let aAddon = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
    aAddon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
  }, {once: true});

  gProvider.createInstalls([{
    name: "manually updating addon (new and even more improved!)",
    existingAddon: gProvider.addons[1],
    version: "1.2",
    releaseNotesURI: Services.io.newURI(TESTROOT + "thereIsNoFileHere.xhtml")
  }]);
});
