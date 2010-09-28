/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests tha installs and undoing installs show up correctly

var gManagerWindow;
var gCategoryUtilities;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gSearchCount = 0;

function test() {
  waitForExplicitFinish();

  // Turn on searching for this test
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);
  Services.prefs.setCharPref("extensions.getAddons.search.url", TESTROOT + "browser_install.xml");

  open_manager(null, function(aWindow) {
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

function get_node(parent, anonid) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "anonid", anonid);
}

function installAddon(aCallback) {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_install.xpi",
                                function(aInstall) {
    aInstall.addListener({
      onInstallEnded: function() {
        executeSoon(aCallback);
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

  EventUtils.synthesizeMouse(searchBox, 2, 2, { }, gManagerWindow);
  EventUtils.synthesizeKey("VK_RETURN", { }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    let remote = gManagerWindow.document.getElementById("search-filter-remote")
    EventUtils.synthesizeMouse(remote, 2, 2, { }, gManagerWindow);

    let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
    ok(!!item, "Should see the search result in the list");

    let status = get_node(item, "install-status");
    EventUtils.synthesizeMouse(get_node(status, "install-remote"), 2, 2, {}, gManagerWindow);

    item.mInstall.addListener({
      onInstallEnded: function() {
        executeSoon(aCallback);
      }
    });
  });
}

function check_undo_install() {
  let item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  ok(!!item, "Should see the pending install in the list");
  // Force XBL to apply
  item.clientTop;
  is_element_visible(get_node(item, "pending"), "Pending message should be visible");
  is(get_node(item, "pending").textContent, "Install Tests will be installed after you restart " + gApp + ".", "Pending message should be correct");

  EventUtils.synthesizeMouse(get_node(item, "undo"), 2, 2, {}, gManagerWindow);
  item = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  ok(!item, "Should no longer see the pending install");
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
