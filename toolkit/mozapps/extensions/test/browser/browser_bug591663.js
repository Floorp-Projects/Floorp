/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that the empty notice in the list view disappears as it should

// Don't use a standard list view (e.g. "extension") to ensure that the list is
// initially empty. Don't need to worry about the list of categories displayed
// since only the list view itself is tested.
var VIEW_ID = "addons://list/mock-addon";

var LIST_ID = "addon-list";
var EMPTY_ID = "addon-list-empty";

var gManagerWindow;
var gProvider;
var gItem;

var gInstallProperties = {
  name: "Bug 591663 Mock Install",
  type: "mock-addon"
};
var gAddonProperties = {
  id: "test1@tests.mozilla.org",
  name: "Bug 591663 Mock Add-on",
  type: "mock-addon"
};
var gExtensionProperties = {
  name: "Bug 591663 Extension Install",
  type: "extension"
};

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider(true, [{
    id: "mock-addon",
    name: "Mock Add-ons",
    uiPriority: 4500,
    flags: AddonManager.TYPE_UI_VIEW_LIST
  }]);

  open_manager(VIEW_ID, function(aWindow) {
    gManagerWindow = aWindow;
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, finish);
}

/**
 * Check that the list view is as expected
 *
 * @param  aItem
 *         The expected item in the list, or null if list should be empty
 */
function check_list(aItem) {
  // Check state of the empty notice
  let emptyNotice = gManagerWindow.document.getElementById(EMPTY_ID);
  ok(emptyNotice != null, "Should have found the empty notice");
  is(!emptyNotice.hidden, (aItem == null), "Empty notice should be showing if list empty");

  // Check the children of the list
  let list = gManagerWindow.document.getElementById(LIST_ID);
  is(list.childNodes.length, aItem ? 1 : 0, "Should get expected number of items in list");
  if (aItem != null) {
    let itemName = list.firstChild.getAttribute("name");
    is(itemName, aItem.name, "List item should have correct name");
  }
}


// Test that the empty notice is showing and no items are showing in list
add_test(function() {
  check_list(null);
  run_next_test();
});

// Test that a new, non-active, install does not affect the list view
add_test(function() {
  gItem = gProvider.createInstalls([gInstallProperties])[0];
  check_list(null);
  run_next_test();
});

// Test that onInstallStarted properly hides empty notice and adds install to list
add_test(function() {
  gItem.addTestListener({
    onDownloadStarted: function() {
      // Install type unknown until download complete
      check_list(null);
    },
    onInstallStarted: function() {
      check_list(gItem);
    },
    onInstallEnded: function() {
      check_list(gItem);
      run_next_test();
    }
  });

  gItem.install();
});

// Test that restarting the manager does not change list
add_test(function() {
  restart_manager(gManagerWindow, VIEW_ID, function(aManagerWindow) {
    gManagerWindow = aManagerWindow;
    check_list(gItem);
    run_next_test();
  });
});

// Test that onInstallCancelled removes install and shows empty notice
add_test(function() {
  gItem.cancel();
  gItem = null;
  check_list(null);
  run_next_test();
});

// Test that add-ons of a different type do not show up in the list view
add_test(function() {
  let extension = gProvider.createInstalls([gExtensionProperties])[0];
  check_list(null);

  extension.addTestListener({
    onDownloadStarted: function() {
      check_list(null);
    },
    onInstallStarted: function() {
      check_list(null);
    },
    onInstallEnded: function() {
      check_list(null);
      extension.cancel();
      run_next_test();
    }
  });

  extension.install();
});

// Test that onExternalInstall properly hides empty notice and adds install to list
add_test(function() {
  gItem = gProvider.createAddons([gAddonProperties])[0];
  check_list(gItem);
  run_next_test();
});

// Test that restarting the manager does not change list
add_test(function() {
  restart_manager(gManagerWindow, VIEW_ID, function(aManagerWindow) {
    gManagerWindow = aManagerWindow;
    check_list(gItem);
    run_next_test();
  });
});

