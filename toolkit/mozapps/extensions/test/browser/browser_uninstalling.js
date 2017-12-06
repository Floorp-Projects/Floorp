/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gManagerWindow;
var gDocument;
var gCategoryUtilities;
var gProvider;

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "Uninstall needs restart",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_UNINSTALL
  }, {
    id: "addon2@tests.mozilla.org",
    name: "Uninstall doesn't need restart 1",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon3@tests.mozilla.org",
    name: "Uninstall doesn't need restart 2",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon4@tests.mozilla.org",
    name: "Uninstall doesn't need restart 3",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon5@tests.mozilla.org",
    name: "Uninstall doesn't need restart 4",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon6@tests.mozilla.org",
    name: "Uninstall doesn't need restart 5",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon7@tests.mozilla.org",
    name: "Uninstall doesn't need restart 6",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon8@tests.mozilla.org",
    name: "Uninstall doesn't need restart 7",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon9@tests.mozilla.org",
    name: "Uninstall doesn't need restart 8",
    type: "extension",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }]);

  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gDocument = gManagerWindow.document;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}

function get_item_in_list(aId, aList) {
  var item = aList.firstChild;
  while (item) {
    if ("mAddon" in item && item.mAddon.id == aId) {
      aList.ensureElementIsVisible(item);
      return item;
    }
    item = item.nextSibling;
  }
  return null;
}

// Tests that uninstalling a normal add-on from the list view can be undone
add_test(function() {
  var ID = "addon1@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    AddonManager.getAddonByID(ID, function(aAddon) {
      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");
      ok(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL, "Add-on should require a restart to uninstall");

      var item = get_item_in_list(ID, list);
      isnot(item, null, "Should have found the add-on in the list");

      var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      // Force XBL to apply
      item.clientTop;

      is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

      ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");

      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
      isnot(button, null, "Should have a restart button");
      ok(!button.hidden, "Restart button should not be hidden");
      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
      isnot(button, null, "Should have an undo button");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      // Force XBL to apply
      item.clientTop;

      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");
      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      run_next_test();
    });
  });
});

// Tests that uninstalling a restartless add-on from the list view can be undone
add_test(function() {
  var ID = "addon2@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    AddonManager.getAddonByID(ID, function(aAddon) {
      ok(aAddon.isActive, "Add-on should be active");
      ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

      var item = get_item_in_list(ID, list);
      isnot(item, null, "Should have found the add-on in the list");

      var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      // Force XBL to apply
      item.clientTop;

      is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

      ok(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL, "Add-on should be pending uninstall");
      ok(!aAddon.isActive, "Add-on should be inactive");

      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
      isnot(button, null, "Should have a restart button");
      ok(button.hidden, "Restart button should be hidden");
      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
      isnot(button, null, "Should have an undo button");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      // Force XBL to apply
      item.clientTop;

      ok(aAddon.isActive, "Add-on should be active");
      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      run_next_test();
    });
  });
});

// Tests that uninstalling a disabled restartless add-on from the list view can
// be undone and doesn't re-enable
add_test(function() {
  var ID = "addon2@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    AddonManager.getAddonByID(ID, function(aAddon) {
      aAddon.userDisabled = true;

      ok(!aAddon.isActive, "Add-on should be inactive");
      ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

      var item = get_item_in_list(ID, list);
      isnot(item, null, "Should have found the add-on in the list");

      var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      // Force XBL to apply
      item.clientTop;

      is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

      ok(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL, "Add-on should be pending uninstall");
      ok(!aAddon.isActive, "Add-on should be inactive");

      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
      isnot(button, null, "Should have a restart button");
      ok(button.hidden, "Restart button should be hidden");
      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
      isnot(button, null, "Should have an undo button");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      // Force XBL to apply
      item.clientTop;

      ok(!aAddon.isActive, "Add-on should be inactive");
      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      aAddon.userDisabled = false;
      ok(aAddon.isActive, "Add-on should be active");

      run_next_test();
    });
  });
});

// Tests that uninstalling a normal add-on from the details view switches back
// to the list view and can be undone
add_test(function() {
  var ID = "addon1@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    AddonManager.getAddonByID(ID, function(aAddon) {
      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");
      ok(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL, "Add-on should require a restart to uninstall");

      var item = get_item_in_list(ID, list);
      isnot(item, null, "Should have found the add-on in the list");

      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);
      wait_for_view_load(gManagerWindow, function() {
        is(get_current_view(gManagerWindow).id, "detail-view", "Should be in the detail view");

        var button = gDocument.getElementById("detail-uninstall-btn");
        isnot(button, null, "Should have a remove button");
        ok(!button.disabled, "Button should not be disabled");

        EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

        wait_for_view_load(gManagerWindow, function() {
          is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

          var item = get_item_in_list(ID, list);
          isnot(item, null, "Should have found the add-on in the list");
          is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

          ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");

          // Force XBL to apply
          item.clientTop;

          var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
          isnot(button, null, "Should have a restart button");
          ok(!button.hidden, "Restart button should not be hidden");
          button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
          isnot(button, null, "Should have an undo button");

          EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

          // Force XBL to apply
          item.clientTop;

          ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");
          button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
          isnot(button, null, "Should have a remove button");
          ok(!button.disabled, "Button should not be disabled");

          run_next_test();
        });
      });
    });
  });
});

// Tests that uninstalling a restartless add-on from the details view switches
// back to the list view and can be undone
add_test(function() {
  var ID = "addon2@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    AddonManager.getAddonByID(ID, function(aAddon) {
      ok(aAddon.isActive, "Add-on should be active");
      ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

      var item = get_item_in_list(ID, list);
      isnot(item, null, "Should have found the add-on in the list");

      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);
      wait_for_view_load(gManagerWindow, function() {
        is(get_current_view(gManagerWindow).id, "detail-view", "Should be in the detail view");

        var button = gDocument.getElementById("detail-uninstall-btn");
        isnot(button, null, "Should have a remove button");
        ok(!button.disabled, "Button should not be disabled");

        EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

        wait_for_view_load(gManagerWindow, function() {
          is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

          var item = get_item_in_list(ID, list);
          isnot(item, null, "Should have found the add-on in the list");
          is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

          ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");
          ok(!aAddon.isActive, "Add-on should be inactive");

          // Force XBL to apply
          item.clientTop;

          var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
          isnot(button, null, "Should have a restart button");
          ok(button.hidden, "Restart button should be hidden");
          button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
          isnot(button, null, "Should have an undo button");

          EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

          // Force XBL to apply
          item.clientTop;

          ok(aAddon.isActive, "Add-on should be active");
          button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
          isnot(button, null, "Should have a remove button");
          ok(!button.disabled, "Button should not be disabled");

          run_next_test();
        });
      });
    });
  });
});

// Tests that uninstalling a restartless add-on from the details view switches
// back to the list view and can be undone and doesn't re-enable
add_test(function() {
  var ID = "addon2@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    AddonManager.getAddonByID(ID, function(aAddon) {
      aAddon.userDisabled = true;

      ok(!aAddon.isActive, "Add-on should be inactive");
      ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

      var item = get_item_in_list(ID, list);
      isnot(item, null, "Should have found the add-on in the list");

      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);
      wait_for_view_load(gManagerWindow, function() {
        is(get_current_view(gManagerWindow).id, "detail-view", "Should be in the detail view");

        var button = gDocument.getElementById("detail-uninstall-btn");
        isnot(button, null, "Should have a remove button");
        ok(!button.disabled, "Button should not be disabled");

        EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

        wait_for_view_load(gManagerWindow, function() {
          is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

          var item = get_item_in_list(ID, list);
          isnot(item, null, "Should have found the add-on in the list");
          is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

          ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");
          ok(!aAddon.isActive, "Add-on should be inactive");

          // Force XBL to apply
          item.clientTop;

          var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
          isnot(button, null, "Should have a restart button");
          ok(button.hidden, "Restart button should be hidden");
          button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
          isnot(button, null, "Should have an undo button");

          EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

          // Force XBL to apply
          item.clientTop;

          ok(!aAddon.isActive, "Add-on should be inactive");
          button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
          isnot(button, null, "Should have a remove button");
          ok(!button.disabled, "Button should not be disabled");

          aAddon.userDisabled = false;
          ok(aAddon.isActive, "Add-on should be active");

          run_next_test();
        });
      });
    });
  });
});

// Tests that a normal add-on pending uninstall shows up in the list view
add_test(function() {
  var ID = "addon1@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    AddonManager.getAddonByID(ID, function(aAddon) {
      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");
      ok(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL, "Add-on should require a restart to uninstall");

      var item = get_item_in_list(ID, list);
      isnot(item, null, "Should have found the add-on in the list");

      var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      // Force XBL to apply
      item.clientTop;

      is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

      ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");

      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
      isnot(button, null, "Should have a restart button");
      ok(!button.hidden, "Restart button should not be hidden");
      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
      isnot(button, null, "Should have an undo button");

      gCategoryUtilities.openType("plugin", function() {
        is(gCategoryUtilities.selectedCategory, "plugin", "View should have changed to plugin");
        gCategoryUtilities.openType("extension", function() {
          is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

          var item = get_item_in_list(ID, list);
          isnot(item, null, "Should have found the add-on in the list");
          is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

          ok(!!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should be pending uninstall");

          var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
          isnot(button, null, "Should have a restart button");
          ok(!button.hidden, "Restart button should not be hidden");
          button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
          isnot(button, null, "Should have an undo button");

          EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

          // Force XBL to apply
          item.clientTop;
          ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");
          button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
          isnot(button, null, "Should have a remove button");
          ok(!button.disabled, "Button should not be disabled");

          run_next_test();
        });
      });
    });
  });
});

// Tests that switching away from the list view finalises the uninstall of
// multiple restartless add-ons
add_test(function() {
  var ID = "addon2@tests.mozilla.org";
  var ID2 = "addon6@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    AddonManager.getAddonByID(ID, function(aAddon) {
      ok(aAddon.isActive, "Add-on should be active");
      ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

      var item = get_item_in_list(ID, list);
      isnot(item, null, "Should have found the add-on in the list");

      var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      // Force XBL to apply
      item.clientTop;

      is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");
      ok(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL, "Add-on should be pending uninstall");
      ok(!aAddon.isActive, "Add-on should be inactive");

      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
      isnot(button, null, "Should have a restart button");
      ok(button.hidden, "Restart button should be hidden");
      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
      isnot(button, null, "Should have an undo button");

      item = get_item_in_list(ID2, list);
      isnot(item, null, "Should have found the add-on in the list");

      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      gCategoryUtilities.openType("plugin", function() {
        is(gCategoryUtilities.selectedCategory, "plugin", "View should have changed to extension");

        AddonManager.getAddonsByIDs([ID, ID2], function([aAddon, aAddon2]) {
          is(aAddon, null, "Add-on should no longer be installed");
          is(aAddon2, null, "Second add-on should no longer be installed");

          gCategoryUtilities.openType("extension", function() {
            is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

            var item = get_item_in_list(ID, list);
            is(item, null, "Should not have found the add-on in the list");
            item = get_item_in_list(ID2, list);
            is(item, null, "Should not have found the second add-on in the list");

            run_next_test();
          });
        });
      });
    });
  });
});

// Tests that closing the manager from the list view finalises the uninstall of
// multiple restartless add-ons
add_test(function() {
  var ID = "addon4@tests.mozilla.org";
  var ID2 = "addon8@tests.mozilla.org";
  var list = gDocument.getElementById("addon-list");

  // Select the extensions category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    AddonManager.getAddonByID(ID, function(aAddon) {
      ok(aAddon.isActive, "Add-on should be active");
      ok(!(aAddon.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL), "Add-on should not require a restart to uninstall");
      ok(!(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL), "Add-on should not be pending uninstall");

      var item = get_item_in_list(ID, list);
      isnot(item, null, "Should have found the add-on in the list");

      var button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      // Force XBL to apply
      item.clientTop;

      is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

      ok(aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL, "Add-on should be pending uninstall");
      ok(!aAddon.isActive, "Add-on should be inactive");

      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "restart-btn");
      isnot(button, null, "Should have a restart button");
      ok(button.hidden, "Restart button should be hidden");
      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "undo-btn");
      isnot(button, null, "Should have an undo button");

      item = get_item_in_list(ID2, list);
      isnot(item, null, "Should have found the add-on in the list");

      button = gDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
      isnot(button, null, "Should have a remove button");
      ok(!button.disabled, "Button should not be disabled");

      EventUtils.synthesizeMouseAtCenter(button, { }, gManagerWindow);

      close_manager(gManagerWindow, function() {
        AddonManager.getAddonsByIDs([ID, ID2], function([aAddon, aAddon2]) {
          is(aAddon, null, "Add-on should no longer be installed");
          is(aAddon2, null, "Second add-on should no longer be installed");

          open_manager(null, function(aWindow) {
            gManagerWindow = aWindow;
            gDocument = gManagerWindow.document;
            gCategoryUtilities = new CategoryUtilities(gManagerWindow);
            var list = gDocument.getElementById("addon-list");

            is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

            var item = get_item_in_list(ID, list);
            is(item, null, "Should not have found the add-on in the list");
            item = get_item_in_list(ID2, list);
            is(item, null, "Should not have found the second add-on in the list");

            run_next_test();
          });
        });
      });
    });
  });
});
