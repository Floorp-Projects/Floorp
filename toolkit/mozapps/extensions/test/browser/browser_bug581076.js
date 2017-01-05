/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 581076 - No "See all results" link present when searching for add-ons and not all are displayed (extensions.getAddons.maxResults)

const PREF_GETADDONS_GETSEARCHRESULTS = "extensions.getAddons.search.url";
const PREF_GETADDONS_MAXRESULTS = "extensions.getAddons.maxResults";
const SEARCH_URL = TESTROOT + "browser_searching.xml";
const SEARCH_EXPECTED_TOTAL = 100;
const SEARCH_QUERY = "search";

const SEARCHABLE_PAGE = "addons://list/extension";

var gManagerWindow;


function test() {
  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS, SEARCH_URL);
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);

  waitForExplicitFinish();

  open_manager(SEARCHABLE_PAGE, function(aWindow) {
    gManagerWindow = aWindow;
    run_next_test();
  });
}

function end_test() {
  // Test generates a lot of available installs so just cancel them all
  AddonManager.getAllInstalls(function(aInstalls) {
    for (let install of aInstalls)
      install.cancel();

    close_manager(gManagerWindow, finish);
  });
}

function search(aRemoteSearch, aCallback) {
  waitForFocus(function() {
    var searchBox = gManagerWindow.document.getElementById("header-search");
    searchBox.value = SEARCH_QUERY;

    EventUtils.synthesizeMouseAtCenter(searchBox, { }, gManagerWindow);
    EventUtils.synthesizeKey("VK_RETURN", { }, gManagerWindow);

    wait_for_view_load(gManagerWindow, function() {
      let filter;
      if (aRemoteSearch)
        filter = gManagerWindow.document.getElementById("search-filter-remote");
      else
        filter = gManagerWindow.document.getElementById("search-filter-local");
      EventUtils.synthesizeMouseAtCenter(filter, { }, gManagerWindow);

      executeSoon(aCallback);
    });
  }, gManagerWindow);
}

function check_allresultslink(aShouldShow) {
  var list = gManagerWindow.document.getElementById("search-list");
  var link = gManagerWindow.document.getElementById("search-allresults-link");
  is(link.parentNode, list.lastChild, "Footer should be at the end of the richlistbox");
  if (aShouldShow) {
    is_element_visible(link, "All Results link should be visible");
    is(link.value, "See all " + SEARCH_EXPECTED_TOTAL + " results", "All Results link should show the correct message");
    var scope = {};
    Components.utils.import("resource://gre/modules/addons/AddonRepository.jsm", scope);
    is(link.href, scope.AddonRepository.getSearchURL(SEARCH_QUERY), "All Results link should have the correct href");
  } else {
    is_element_hidden(link, "All Results link should be hidden");
  }
}

add_test(function() {
  info("Searching locally");
  search(false, function() {
    check_allresultslink(false);
    restart_manager(gManagerWindow, SEARCHABLE_PAGE, function(aManager) {
      gManagerWindow = aManager;
      run_next_test();
    });
  });
});

add_test(function() {
  info("Searching remotely - more results than cap");
  Services.prefs.setIntPref(PREF_GETADDONS_MAXRESULTS, 3);
  search(true, function() {
    check_allresultslink(true);
    restart_manager(gManagerWindow, SEARCHABLE_PAGE, function(aManager) {
      gManagerWindow = aManager;
      run_next_test();
    });
  });
});

add_test(function() {
  info("Searching remotely - less results than cap");
  Services.prefs.setIntPref(PREF_GETADDONS_MAXRESULTS, 200);
  search(true, function() {
    check_allresultslink(false);
    restart_manager(gManagerWindow, SEARCHABLE_PAGE, function(aManager) {
      gManagerWindow = aManager;
      run_next_test();
    });
  });
});

add_test(function() {
  info("Searching remotely - more results than cap");
  Services.prefs.setIntPref(PREF_GETADDONS_MAXRESULTS, 3);
  search(true, function() {
    check_allresultslink(true);
    run_next_test();
  });
});

add_test(function() {
  info("Switching views");
  gManagerWindow.loadView("addons://list/extension");
  wait_for_view_load(gManagerWindow, function() {
    info("Re-loading previous search");
    search(true, function() {
      check_allresultslink(true);
      run_next_test();
    });
  });
});
