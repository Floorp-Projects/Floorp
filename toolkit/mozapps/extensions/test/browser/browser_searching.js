/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that searching for add-ons works correctly

const PREF_GETADDONS_GETSEARCHRESULTS = "extensions.getAddons.search.url";
const SEARCH_URL = TESTROOT + "browser_searching.xml";
const NO_MATCH_URL = TESTROOT + "browser_searching_empty.xml";

const QUERY = "SEARCH";
const NO_MATCH_QUERY = "NOMATCHQUERY";
const REMOTE_TO_INSTALL = "remote1";
const REMOTE_INSTALL_URL = TESTROOT + "addons/browser_searching.xpi";

var gManagerWindow;
var gCategoryUtilities;
var gProvider;
var gServer;
var gAddonInstalled = false;

function test() {
  requestLongerTimeout(2);
  // Turn on searching for this test
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);
  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);

  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "PASS - f",
    description: "Test description - SEARCH",
    size: 3,
    version: "1.0",
    updateDate: new Date(2010, 4, 2, 0, 0, 1)
  }, {
    id: "fail-addon1@tests.mozilla.org",
    name: "FAIL",
    description: "Does not match query"
  }, {
    id: "addon2@tests.mozilla.org",
    name: "PASS - c",
    description: "Test description - reSEARCHing SEARCH SEARCH",
    size: 6,
    version: "2.0",
    updateDate: new Date(2010, 4, 2, 0, 0, 0)
  }]);

  var installs = gProvider.createInstalls([{
    name: "PASS - a - SEARCHing",
    sourceURI: "http://example.com/install1.xpi"
  }, {
    name: "PASS - g - reSEARCHing SEARCH",
    sourceURI: "http://example.com/install2.xpi"
  }, {
    // Does not match query
    name: "FAIL",
    sourceURI: "http://example.com/fail-install1.xpi"
  }]);

  for (let install of installs )
    install.install();

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    var installedAddon = get_addon_item(REMOTE_TO_INSTALL).mAddon;
    installedAddon.uninstall();

    AddonManager.getAllInstalls(function(aInstallsList) {
      for (var install of aInstallsList) {
        var sourceURI = install.sourceURI.spec;
        if (sourceURI == REMOTE_INSTALL_URL ||
            sourceURI.match(/^http:\/\/example\.com\/(.+)\.xpi$/) != null)
          install.cancel();
      }

      finish();
    });
  });
}

function getAnonymousElementByAttribute(aElement, aName, aValue) {
  return gManagerWindow.document.getAnonymousElementByAttribute(aElement,
                                                                aName,
                                                                aValue);
}

/*
 * Checks whether or not the Add-ons Manager is currently searching
 *
 * @param  aExpectedSearching
 *         The expected isSearching state
 */
function check_is_searching(aExpectedSearching) {
  var loading = gManagerWindow.document.getElementById("search-loading");
  is(!is_hidden(loading), aExpectedSearching,
     "Search throbber should be showing iff currently searching");
}

/*
 * Completes a search
 *
 * @param  aQuery
 *         The query to search for
 * @param  aFinishImmediately
 *         Boolean representing whether or not the search is expected to
 *         finish immediately
 * @param  aCallback
 *         The callback to call when the search is done
 * @param  aCategoryType
 *         The expected selected category after the search is done.
 *         Optional and defaults to "search"
 */
function search(aQuery, aFinishImmediately, aCallback, aCategoryType) {
  // Point search to the correct xml test file
  var url = (aQuery == NO_MATCH_QUERY) ? NO_MATCH_URL : SEARCH_URL;
  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS, url);

  aCategoryType = aCategoryType ? aCategoryType : "search";

  var searchBox = gManagerWindow.document.getElementById("header-search");
  searchBox.value = aQuery;

  EventUtils.synthesizeMouseAtCenter(searchBox, { }, gManagerWindow);
  EventUtils.synthesizeKey("VK_RETURN", { }, gManagerWindow);

  var finishImmediately = true;
  wait_for_view_load(gManagerWindow, function() {
    is(gCategoryUtilities.selectedCategory, aCategoryType, "Expected category view should be selected");
    is(gCategoryUtilities.isTypeVisible("search"), aCategoryType == "search",
       "Search category should only be visible if it is the current view");
    check_is_searching(false);
    is(finishImmediately, aFinishImmediately, "Search should finish immediately only if expected");

    aCallback();
  });

  finishImmediately = false
  if (!aFinishImmediately)
    check_is_searching(true);
}

/*
 * Return results of a search
 *
 * @return Array of objects, each containing the name and item of a specific
 *         result
 */
function get_actual_results() {
  var list = gManagerWindow.document.getElementById("search-list");
  var rows = list.getElementsByTagName("richlistitem");

  var results = [];
  for (var item of rows) {

    // Only consider items that are currently showing
    var style = gManagerWindow.document.defaultView.getComputedStyle(item, "");
    if (style.display == "none" || style.visibility != "visible")
      continue;

    if (item.mInstall || item.isPending("install")) {
      var sourceURI = item.mInstall.sourceURI.spec;
      if (sourceURI == REMOTE_INSTALL_URL) {
        results.push({name: REMOTE_TO_INSTALL, item: item});
        continue;
      }

      var result = sourceURI.match(/^http:\/\/example\.com\/(.+)\.xpi$/);
      if (result != null) {
        is(item.mInstall.name.indexOf("PASS"), 0, "Install name should start with PASS");
        results.push({name: result[1], item: item});
        continue;
      }
    }
    else if (item.mAddon) {
      var result = item.mAddon.id.match(/^(.+)@tests\.mozilla\.org$/);
      if (result != null) {
        is(item.mAddon.name.indexOf("PASS"), 0, "Addon name should start with PASS");
        results.push({name: result[1], item: item});
        continue;
      }
    }
    else {
      ok(false, "Found an item in the list that was neither installing or installed");
    }
  }

  return results;
}

/*
 * Returns expected results when searching for QUERY with default ordering
 *
 * @param  aSortBy
 *         How the results are sorted (e.g. "name")
 * @param  aLocalExpected
 *         Boolean representing if local results are expected
 * @return A pair: [array of results with an expected order,
 *                  array of results with unknown order]
 */
function get_expected_results(aSortBy, aLocalExpected) {
  var expectedOrder = null, unknownOrder = null;
  switch (aSortBy) {
    case "relevancescore":
      expectedOrder = [ "addon2"  , "remote1", "install2", "addon1",
                        "install1", "remote2", "remote3" , "remote4" ];
      unknownOrder = [];
      break;
    case "name":
      // Defaults to ascending order
      expectedOrder = [ "install1", "remote1",  "addon2" , "remote2",
                        "remote3" , "addon1" , "install2", "remote4" ];
      unknownOrder = [];
      break;
    case "dateUpdated":
      expectedOrder = [ "addon1", "addon2" ];
      // Updated date not available for installs and remote add-ons
      unknownOrder = [ "install1", "install2", "remote1",
                       "remote2" , "remote3" , "remote4" ];
      break;
    default:
      ok(false, "Should recognize sortBy when checking the order of items");
  }

  // Only keep expected results
  function filterResults(aId) {
    // Include REMOTE_TO_INSTALL as a local add-on if it has been installed
    if (gAddonInstalled && aId == REMOTE_TO_INSTALL)
      return aLocalExpected;

    if (aId.indexOf("addon") == 0 || aId.indexOf("install") == 0)
      return aLocalExpected;
    if (aId.indexOf("remote") == 0)
      return !aLocalExpected;

    return false;
  }


  return [expectedOrder.filter(filterResults),
          unknownOrder.filter(filterResults)]
}

/*
 * Check that the actual and expected results are the same
 *
 * @param  aQuery
 *         The search query used
 * @param  aSortBy
 *         How the results are sorted (e.g. "name")
 * @param  aReverseOrder
 *         Boolean representing if the results are in reverse default order
 * @param  aShowLocal
 *         Boolean representing if local results are being shown
 */
function check_results(aQuery, aSortBy, aReverseOrder, aShowLocal) {
  var localFilterSelected = gManagerWindow.document.getElementById("search-filter-local").selected;
  var remoteFilterSelected = gManagerWindow.document.getElementById("search-filter-remote").selected;
  is(localFilterSelected, aShowLocal, "Local filter should be selected if showing local items");
  is(remoteFilterSelected, !aShowLocal, "Remote filter should be selected if showing remote items");

  // Get expected order assuming default order
  var expectedOrder = [], unknownOrder = [];
  if (aQuery == QUERY)
    [expectedOrder, unknownOrder] = get_expected_results(aSortBy, aShowLocal);

  // Get actual order of results
  var actualResults = get_actual_results();
  var actualOrder = [result.name for each(result in actualResults)];

  // Reverse array of actual results if supposed to be in reverse order.
  // Reverse actualOrder instead of expectedOrder so can always check
  // expectedOrder before unknownOrder
  if (aReverseOrder)
    actualOrder.reverse();

  // Check actual vs. expected list of results
  var totalExpectedResults = expectedOrder.length + unknownOrder.length;
  is(actualOrder.length, totalExpectedResults, "Should get correct number of results");

  // Check the "first" and "last" attributes are set correctly
  for (let i = 0; i < actualResults.length; i++) {
    if (i == 0) {
      is(actualResults[0].item.hasAttribute("first"), true,
         "First item should have 'first' attribute set");
      is(actualResults[0].item.hasAttribute("last"), false,
         "First item should not have 'last' attribute set");
    } else if (i == (actualResults.length - 1)) {
      is(actualResults[actualResults.length - 1].item.hasAttribute("first"), false,
         "Last item should not have 'first' attribute set");
      is(actualResults[actualResults.length - 1].item.hasAttribute("last"), true,
         "Last item should have 'last' attribute set");
    } else {
      is(actualResults[i].item.hasAttribute("first"), false,
         "Item " + i + " should not have 'first' attribute set");
      is(actualResults[i].item.hasAttribute("last"), false,
         "Item " + i + " should not have 'last' attribute set");
    }
  }

  var i = 0;
  for (; i < expectedOrder.length; i++)
    is(actualOrder[i], expectedOrder[i], "Should have seen expected item");

  // Items with data that is unknown can appear in any order among themselves,
  // so just check that these items exist
  for (; i < actualOrder.length; i++) {
    var unknownOrderIndex = unknownOrder.indexOf(actualOrder[i]);
    ok(unknownOrderIndex >= 0, "Should expect to see item with data that is unknown");
    unknownOrder[unknownOrderIndex] = null;
  }

  // Check status of empty notice
  var emptyNotice = gManagerWindow.document.getElementById("search-list-empty");
  is(emptyNotice.hidden, totalExpectedResults > 0,
     "Empty notice should be hidden only if expecting shown items");
}

/*
 * Check results of a search with different filterings
 *
 * @param  aQuery
 *         The search query used
 * @param  aSortBy
 *         How the results are sorted (e.g. "name")
 * @param  aReverseOrder
 *         Boolean representing if the results are in reverse default order
 */
function check_filtered_results(aQuery, aSortBy, aReverseOrder) {
  var localFilter = gManagerWindow.document.getElementById("search-filter-local");
  var remoteFilter = gManagerWindow.document.getElementById("search-filter-remote");

  var list = gManagerWindow.document.getElementById("search-list");
  list.ensureElementIsVisible(localFilter);

  // Check with showing local add-ons
  EventUtils.synthesizeMouseAtCenter(localFilter, { }, gManagerWindow);
  check_results(aQuery, aSortBy, aReverseOrder, true);

  // Check with showing remote add-ons
  EventUtils.synthesizeMouseAtCenter(remoteFilter, { }, gManagerWindow);
  check_results(aQuery, aSortBy, aReverseOrder, false);
}

/*
 * Get item for a specific add-on by name
 *
 * @param  aName
 *         The name of the add-on to search for
 * @return Row of add-on if found, null otherwise
 */
function get_addon_item(aName) {
  var id = aName + "@tests.mozilla.org";
  var list = gManagerWindow.document.getElementById("search-list");
  var rows = list.getElementsByTagName("richlistitem");
  for (var row of rows) {
    if (row.mAddon && row.mAddon.id == id)
      return row;
  }

  return null;
}

/*
 * Get item for a specific install by name
 *
 * @param  aName
 *         The name of the install to search for
 * @return Row of install if found, null otherwise
 */
function get_install_item(aName) {
  var sourceURI = "http://example.com/" + aName + ".xpi";
  var list = gManagerWindow.document.getElementById("search-list");
  var rows = list.getElementsByTagName("richlistitem");
  for (var row of rows) {
    if (row.mInstall && row.mInstall.sourceURI.spec == sourceURI)
      return row;
  }

  return null;
}

/*
 * Gets the install button for a specific item
 *
 * @param  aItem
 *         The item to get the install button for
 * @return The install button for aItem
 */
function get_install_button(aItem) {
  isnot(aItem, null, "Item should not be null when checking state of install button");
  var installStatus = getAnonymousElementByAttribute(aItem, "anonid", "install-status");
  return getAnonymousElementByAttribute(installStatus, "anonid", "install-remote-btn");
}


// Tests that searching for the empty string does nothing when not in the search view
add_test(function() {
  is(gCategoryUtilities.isTypeVisible("search"), false, "Search category should initially be hidden");

  var selectedCategory = gCategoryUtilities.selectedCategory;
  isnot(selectedCategory, "search", "Selected type should not initially be the search view");
  search("", true, run_next_test, selectedCategory);
});

// Tests that the results from a query are sorted by relevancescore in descending order.
// Also test that double clicking non-install items goes to the detail view, and that
// only remote items have install buttons showing
add_test(function() {
  search(QUERY, false, function() {
    check_filtered_results(QUERY, "relevancescore", false);

    var list = gManagerWindow.document.getElementById("search-list");
    var results = get_actual_results();
    for (var result of results) {
      var installBtn = get_install_button(result.item);
      is(installBtn.hidden, result.name.indexOf("remote") != 0,
         "Install button should only be showing for remote items");
    }

    var currentIndex = -1;
    function run_next_double_click_test() {
      currentIndex++;
      if (currentIndex >= results.length) {
        run_next_test();
        return;
      }

      var result = results[currentIndex];
      if (result.name.indexOf("install") == 0) {
        run_next_double_click_test();
        return;
      }

      var item = result.item;
      list.ensureElementIsVisible(item);
      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);
      wait_for_view_load(gManagerWindow, function() {
        var name = gManagerWindow.document.getElementById("detail-name").textContent;
        is(name, item.mAddon.name, "Name in detail view should be correct");
        var version = gManagerWindow.document.getElementById("detail-version").value;
        is(version, item.mAddon.version, "Version in detail view should be correct");

        EventUtils.synthesizeMouseAtCenter(gManagerWindow.document.getElementById("category-search"),
                                           { }, gManagerWindow);
        wait_for_view_load(gManagerWindow, run_next_double_click_test);
      });
    }

    run_next_double_click_test();
  });
});

// Tests that the sorters and filters correctly manipulate the results
add_test(function() {
  var sorters = gManagerWindow.document.getElementById("search-sorters");
  var originalHandler = sorters.handler;

  var sorterNames = ["name", "dateUpdated"];
  var buttonIds = ["name-btn", "date-btn"];
  var currentIndex = 0;
  var currentReversed = false;

  function run_sort_test() {
    if (currentIndex >= sorterNames.length) {
      sorters.handler = originalHandler;
      run_next_test();
      return;
    }

    // Simulate clicking on a specific sorter
    var buttonId = buttonIds[currentIndex];
    var sorter = getAnonymousElementByAttribute(sorters, "anonid", buttonId);
    is_element_visible(sorter);
    EventUtils.synthesizeMouseAtCenter(sorter, { }, gManagerWindow);
  }

  sorters.handler = {
    onSortChanged: function(aSortBy, aAscending) {
      if (originalHandler && "onSortChanged" in originalHandler)
        originalHandler.onSortChanged(aSortBy, aAscending);

      check_filtered_results(QUERY, sorterNames[currentIndex], currentReversed);

      if (currentReversed)
        currentIndex++;
      currentReversed = !currentReversed;

      run_sort_test();
    }
  };

  check_filtered_results(QUERY, "relevancescore", false);
  run_sort_test();
});

// Tests that searching for the empty string does nothing when in search view
add_test(function() {
  search("", true, function() {
    check_filtered_results(QUERY, "dateUpdated", true);
    run_next_test();
  });
});

// Tests that clicking a different category hides the search query
add_test(function() {
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.isTypeVisible("search"), false, "Search category should be hidden");
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");
    run_next_test();
  });
});

// Tests that re-searching for query doesn't actually complete a new search,
// and the last sort is still used
add_test(function() {
  search(QUERY, true, function() {
    check_filtered_results(QUERY, "dateUpdated", true);
    run_next_test();
  });
});

// Tests that getting zero results works correctly
add_test(function() {
  search(NO_MATCH_QUERY, false, function() {
    check_filtered_results(NO_MATCH_QUERY, "relevancescore", false);
    run_next_test();
  });
});

// Tests that installing a remote add-on works
add_test(function() {
  var installBtn = null;

  var listener = {
    onInstallEnded: function(aInstall, aAddon) {
      // Don't immediately consider the installed add-on as local because
      // if the user was filtering out local add-ons, the installed add-on
      // would vanish. Only consider add-on as local on new searches.

      aInstall.removeListener(this);

      is(installBtn.hidden, true, "Install button should be hidden after install ended");
      check_filtered_results(QUERY, "relevancescore", false);
      run_next_test();
    }
  }

  search(QUERY, false, function() {
    var list = gManagerWindow.document.getElementById("search-list");
    var remoteItem = get_addon_item(REMOTE_TO_INSTALL);
    list.ensureElementIsVisible(remoteItem);

    installBtn = get_install_button(remoteItem);
    is(installBtn.hidden, false, "Install button should be showing before install");
    remoteItem.mAddon.install.addListener(listener);
    EventUtils.synthesizeMouseAtCenter(installBtn, { }, gManagerWindow);
  });
});

// Tests that re-searching for query results in correct results
add_test(function() {
  // Select a different category
  gCategoryUtilities.openType("extension", function() {
    is(gCategoryUtilities.isTypeVisible("search"), false, "Search category should be hidden");
    is(gCategoryUtilities.selectedCategory, "extension", "View should have changed to extension");

    var installBtn = get_install_button(get_addon_item(REMOTE_TO_INSTALL));
    is(installBtn.hidden, true, "Install button should be hidden for installed item");

    search(QUERY, true, function() {
      check_filtered_results(QUERY, "relevancescore", false);
      run_next_test();
    });
  });
});

// Tests that incompatible add-ons are shown with a warning if compatibility checking is disabled
add_test(function() {
  AddonManager.checkCompatibility = false;
  search("incompatible", false, function() {
    var item = get_addon_item("remote5");
    is_element_visible(item, "Incompatible addon should be visible");
    is(item.getAttribute("notification"), "warning", "Compatibility warning should be shown");

    item = get_addon_item("remote6");
    is(item, null, "Addon incompatible with the product should not be visible");

    AddonManager.checkCompatibility = true;
    run_next_test();
  });
});

// Tests that compatible-by-default addons are shown if strict compatibility checking is disabled
add_test(function() {
  restart_manager(gManagerWindow, null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    Services.prefs.setBoolPref(PREF_STRICT_COMPAT, false);
    search("incompatible", false, function() {
      var item = get_addon_item("remote5");
      is_element_visible(item, "Incompatible addon should be visible");
      isnot(item.getAttribute("notification"), "warning", "Compatibility warning should not be shown");
  
      var item = get_addon_item("remote6");
      is(item, null, "Addon incompatible with the product should not be visible");
  
      Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);
      run_next_test();
    });
  });
});


// Tests that restarting the manager doesn't change search results
add_test(function() {
  restart_manager(gManagerWindow, null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    // We never restore to the search pane
    is(gCategoryUtilities.selectedCategory, "discover", "View should have changed to discover");

    // Installed add-on is considered local on new search
    gAddonInstalled = true;

    search(QUERY, false, function() {
      check_filtered_results(QUERY, "relevancescore", false);

      var installBtn = get_install_button(get_addon_item(REMOTE_TO_INSTALL));
      is(installBtn.hidden, true, "Install button should be hidden for installed item");

      run_next_test();
    });
  });
});

