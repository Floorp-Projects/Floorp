/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the eula is shown correctly for search results

var gManagerWindow;
var gCategoryUtilities;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gSearchCount = 0;

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  // Turn on searching for this test
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);
  Services.prefs.setCharPref("extensions.getAddons.search.url", TESTROOT + "browser_eula.xml");

  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    Services.prefs.clearUserPref("extensions.getAddons.search.url");

    finish();
  });
}

function get_node(parent, anonid) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "anonid", anonid);
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

// Install an add-on through the search page, accept the EULA and then undo it
add_test(function() {
  // Accept the EULA when it appears
  let sawEULA = false;
  wait_for_window_open(function(aWindow) {
    sawEULA = true;
    is(aWindow.location.href, "chrome://mozapps/content/extensions/eula.xul", "Window opened should be correct");
    is(aWindow.document.getElementById("eula").value, "This is the EULA for this add-on", "EULA should be correct");

    aWindow.document.documentElement.acceptDialog();
  });

  installSearchResult(function() {
    ok(sawEULA, "Should have seen the EULA");

    AddonManager.getAllInstalls(function(aInstalls) {
      is(aInstalls.length, 1, "Should be one pending install");
      aInstalls[0].cancel();

      run_next_test();
    });
  });
});
