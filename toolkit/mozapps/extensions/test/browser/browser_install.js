/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests tha installs and undoing installs show up correctly

var gManagerWindow;
var gCategoryUtilities;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gSearchCount = 0;

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  // Turn on searching for this test
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);
  Services.prefs.setCharPref("extensions.getAddons.search.url", TESTROOT + "browser_install.xml");
  // Allow http update checks
  Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    Services.prefs.clearUserPref("extensions.getAddons.search.url");
    Services.prefs.clearUserPref("extensions.checkUpdateSecurity");

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
      aAddon.uninstall();
      finish();
    });
  });
}

function get_node(parent, anonid) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "anonid", anonid);
}

function installAddon(aCallback) {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_install1_2.xpi",
                                function(aInstall) {
    aInstall.addListener({
      onInstallEnded: function() {
        executeSoon(aCallback);
      }
    });
    aInstall.install();
  }, "application/x-xpinstall");
}

function installUpgrade(aCallback) {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
    aAddon.findUpdates({
      onUpdateAvailable: function(aAddon, aInstall) {
        is(get_list_item_count(), 1, "Should be only one item in the list");

        aInstall.addListener({
          onInstallEnded: function() {
            executeSoon(aCallback);
          }
        });
        aInstall.install();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

function cancelInstall(aCallback) {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_install1_2.xpi",
                                function(aInstall) {
    aInstall.addListener({
      onDownloadEnded: function(aInstall) {
        executeSoon(function() {
          aInstall.cancel();
          aCallback();
        });
        return false;
      }
    });
    aInstall.install();
  }, "application/x-xpinstall");
}

function installSearchResult(aCallback) {
  var searchBox = gManagerWindow.document.getElementById("header-search");
  // Search for something different each time
  searchBox.value = "foo" + gSearchCount;
  gSearchCount++;

  EventUtils.synthesizeMouseAtCenter(searchBox, { }, gManagerWindow);
  EventUtils.synthesizeKey("VK_RETURN", { }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    let remote = gManagerWindow.document.getElementById("search-filter-remote")
    EventUtils.synthesizeMouseAtCenter(remote, { }, gManagerWindow);

    let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
    ok(!!item, "Should see the search result in the list");

    let status = get_node(item, "install-status");
    EventUtils.synthesizeMouseAtCenter(get_node(status, "install-remote-btn"), {}, gManagerWindow);

    item.mInstall.addListener({
      onInstallEnded: function() {
        executeSoon(aCallback);
      }
    });
  });
}

function get_list_item_count() {
  return get_test_items_in_list(gManagerWindow).length;
}

function check_undo_install() {
  is(get_list_item_count(), 1, "Should be only one item in the list");

  let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  ok(!!item, "Should see the pending install in the list");
  // Force XBL to apply
  item.clientTop;
  is_element_visible(get_node(item, "pending"), "Pending message should be visible");
  is(get_node(item, "pending").textContent, "Install Tests will be installed after you restart " + gApp + ".", "Pending message should be correct");

  EventUtils.synthesizeMouseAtCenter(get_node(item, "undo-btn"), {}, gManagerWindow);

  is(get_list_item_count(), 0, "Should be no items in the list");

  item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  ok(!item, "Should no longer see the pending install");
}

function check_undo_upgrade() {
  is(get_list_item_count(), 1, "Should be only one item in the list");

  let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  ok(!!item, "Should see the pending upgrade in the list");
  // Force XBL to apply
  item.clientTop;
  is_element_visible(get_node(item, "pending"), "Pending message should be visible");
  is(get_node(item, "pending").textContent, "Install Tests will be updated after you restart " + gApp + ".", "Pending message should be correct");

  EventUtils.synthesizeMouseAtCenter(get_node(item, "undo-btn"), {}, gManagerWindow);

  is(get_list_item_count(), 1, "Should be only one item in the list");

  item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  ok(!!item, "Should still see installed item in the list");
  is_element_hidden(get_node(item, "pending"), "Pending message should be hidden");
}

// Install an add-on through the API with the manager open
add_test(function() {
  gCategoryUtilities.openType("extension", function() {
    installAddon(function() {
      check_undo_install();
      run_next_test();
    });
  });
});

// Install an add-on with the manager closed then open it
add_test(function() {
  close_manager(gManagerWindow, function() {
    installAddon(function() {
      open_manager(null, function(aWindow) {
        gManagerWindow = aWindow;
        gCategoryUtilities = new CategoryUtilities(gManagerWindow);
        check_undo_install();
        run_next_test();
      });
    });
  });
});

// Install an add-on through the search page and then undo it
add_test(function() {
  installSearchResult(function() {
    check_undo_install();
    run_next_test();
  });
});

// Install an add-on through the search page then switch to the extensions page
// and then undo it
add_test(function() {
  installSearchResult(function() {
    gCategoryUtilities.openType("extension", function() {
      check_undo_install();
      run_next_test();
    });
  });
});

// Install an add-on through the search page then re-open the manager and then
// undo it
add_test(function() {
  installSearchResult(function() {
    close_manager(gManagerWindow, function() {
      open_manager(null, function(aWindow) {
        gManagerWindow = aWindow;
        gCategoryUtilities = new CategoryUtilities(gManagerWindow);
        check_undo_install();
        run_next_test();
      });
    });
  });
});

// Cancel an install after download with the manager open
add_test(function() {
  cancelInstall(function() {
    is(get_list_item_count(), 0, "Should be no items in the list");

    run_next_test();
  });
});

// Cancel an install after download with the manager closed
add_test(function() {
  close_manager(gManagerWindow, function() {
    cancelInstall(function() {
      open_manager(null, function(aWindow) {
        gManagerWindow = aWindow;
        gCategoryUtilities = new CategoryUtilities(gManagerWindow);
        is(get_list_item_count(), 0, "Should be no items in the list");

        run_next_test();
      });
    });
  });
});

// Install an existing add-on for the subsequent tests
add_test(function() {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_install1_1.xpi",
                                function(aInstall) {
    aInstall.addListener({
      onInstallEnded: function() {
        executeSoon(run_next_test);
      }
    });
    aInstall.install();
  }, "application/x-xpinstall");
});

// Install an upgrade through the API with the manager open
add_test(function() {
  installAddon(function() {
    check_undo_upgrade();
    run_next_test();
  });
});

// Install an upgrade through the API with the manager open
add_test(function() {
  installUpgrade(function() {
    check_undo_upgrade();
    run_next_test();
  });
});

// Install an upgrade through the API with the manager closed
add_test(function() {
  close_manager(gManagerWindow, function() {
    installAddon(function() {
      open_manager(null, function(aWindow) {
        gManagerWindow = aWindow;
        gCategoryUtilities = new CategoryUtilities(gManagerWindow);
        check_undo_upgrade();
        run_next_test();
      });
    });
  });
});

// Cancel an upgrade after download with the manager open
add_test(function() {
  cancelInstall(function() {
    is(get_list_item_count(), 1, "Should be no items in the list");
    let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
    ok(!!item, "Should still see installed item in the list");
    is_element_hidden(get_node(item, "pending"), "Pending message should be hidden");

    run_next_test();
  });
});

// Cancel an upgrade after download with the manager closed
add_test(function() {
  close_manager(gManagerWindow, function() {
    cancelInstall(function() {
      open_manager(null, function(aWindow) {
        gManagerWindow = aWindow;
        gCategoryUtilities = new CategoryUtilities(gManagerWindow);
        is(get_list_item_count(), 1, "Should be no items in the list");
        let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
        ok(!!item, "Should still see installed item in the list");
        is_element_hidden(get_node(item, "pending"), "Pending message should be hidden");

        run_next_test();
      });
    });
  });
});
