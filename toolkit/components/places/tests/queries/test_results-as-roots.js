"use strict";

const MOBILE_BOOKMARKS_PREF = "browser.bookmarks.showMobileBookmarks";

const expectedRoots = [{
  title: "BookmarksToolbarFolderTitle",
  uri: `place:parent=${PlacesUtils.bookmarks.toolbarGuid}`,
  guid: PlacesUtils.bookmarks.virtualToolbarGuid,
}, {
  title: "BookmarksMenuFolderTitle",
  uri: `place:parent=${PlacesUtils.bookmarks.menuGuid}`,
  guid: PlacesUtils.bookmarks.virtualMenuGuid,
}, {
  title: "OtherBookmarksFolderTitle",
  uri: `place:parent=${PlacesUtils.bookmarks.unfiledGuid}`,
  guid: PlacesUtils.bookmarks.virtualUnfiledGuid,
}];

const expectedRootsWithMobile = [...expectedRoots, {
  title: "MobileBookmarksFolderTitle",
  uri: `place:parent=${PlacesUtils.bookmarks.mobileGuid}`,
  guid: PlacesUtils.bookmarks.virtualMobileGuid,
}];

const placesStrings = Services.strings.createBundle("chrome://places/locale/places.properties");

function getAllBookmarksQuery() {
  var query = PlacesUtils.history.getNewQuery();

  // Options
  var options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_VISITCOUNT_ASCENDING;
  options.resultType = options.RESULTS_AS_ROOTS_QUERY;

  // Results
  var result = PlacesUtils.history.executeQuery(query, options);
  return result.root;
}

function assertExpectedChildren(root, expectedChildren) {
  Assert.equal(root.childCount, expectedChildren.length, "Should have the expected number of children.");

  for (let i = 0; i < root.childCount; i++) {
    Assert.equal(root.getChild(i).uri, expectedChildren[i].uri,
                 "Should have the correct uri for root ${i}");
    Assert.equal(root.getChild(i).title, placesStrings.GetStringFromName(expectedChildren[i].title),
                 "Should have the correct title for root ${i}");
    Assert.equal(root.getChild(i).bookmarkGuid, expectedChildren[i].guid);
  }
}

/**
 * This test will test the basic RESULTS_AS_ROOTS_QUERY, that simply returns,
 * the existing bookmark roots.
 */
add_task(async function test_results_as_root() {
  let root = getAllBookmarksQuery();
  root.containerOpen = true;

  Assert.equal(PlacesUtils.asQuery(root).queryOptions.queryType,
    Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS,
    "Should have a query type of QUERY_TYPE_BOOKMARKS");

  assertExpectedChildren(root, expectedRoots);

  root.containerOpen = false;
});

add_task(async function test_results_as_root_with_mobile() {
  Services.prefs.setBoolPref(MOBILE_BOOKMARKS_PREF, true);

  let root = getAllBookmarksQuery();
  root.containerOpen = true;

  assertExpectedChildren(root, expectedRootsWithMobile);

  root.containerOpen = false;
  Services.prefs.clearUserPref(MOBILE_BOOKMARKS_PREF);
});

add_task(async function test_results_as_root_remove_mobile_dynamic() {
  Services.prefs.setBoolPref(MOBILE_BOOKMARKS_PREF, true);

  let root = getAllBookmarksQuery();
  root.containerOpen = true;

  // Now un-set the pref, and poke the database to update the query.
  Services.prefs.clearUserPref(MOBILE_BOOKMARKS_PREF);

  assertExpectedChildren(root, expectedRoots);

  root.containerOpen = false;
});
