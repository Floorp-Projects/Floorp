"use strict";

const MOBILE_BOOKMARKS_PREF = "browser.bookmarks.showMobileBookmarks";

const expectedRoots = [
  {
    title: "OrganizerQueryHistory",
    uri: `place:sort=${Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING}&type=${Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY}`,
    guid: "history____v",
  },
  {
    title: "OrganizerQueryDownloads",
    uri: `place:transition=${Ci.nsINavHistoryService.TRANSITION_DOWNLOAD}&sort=${Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING}`,
    guid: "downloads__v",
  },
  {
    title: "TagsFolderTitle",
    uri: `place:sort=${Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING}&type=${Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAGS_ROOT}`,
    guid: "tags_______v",
  },
  {
    title: "OrganizerQueryAllBookmarks",
    uri: `place:type=${Ci.nsINavHistoryQueryOptions.RESULTS_AS_ROOTS_QUERY}`,
    guid: "allbms_____v",
  },
];

const placesStrings = Services.strings.createBundle(
  "chrome://places/locale/places.properties"
);

function getLeftPaneQuery() {
  var query = PlacesUtils.history.getNewQuery();

  // Options
  var options = PlacesUtils.history.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_LEFT_PANE_QUERY;

  // Results
  var result = PlacesUtils.history.executeQuery(query, options);
  return result.root;
}

function assertExpectedChildren(root, expectedChildren) {
  Assert.equal(
    root.childCount,
    expectedChildren.length,
    "Should have the expected number of children."
  );

  for (let i = 0; i < root.childCount; i++) {
    Assert.ok(
      PlacesTestUtils.ComparePlacesURIs(
        root.getChild(i).uri,
        expectedChildren[i].uri
      ),
      "Should have the correct uri for root ${i}"
    );
    Assert.equal(
      root.getChild(i).title,
      placesStrings.GetStringFromName(expectedChildren[i].title),
      "Should have the correct title for root ${i}"
    );
    Assert.equal(root.getChild(i).bookmarkGuid, expectedChildren[i].guid);
  }
}

/**
 * This test will test the basic RESULTS_AS_ROOTS_QUERY, that simply returns,
 * the existing bookmark roots.
 */
add_task(async function test_results_as_root() {
  let root = getLeftPaneQuery();
  root.containerOpen = true;

  Assert.equal(
    PlacesUtils.asQuery(root).queryOptions.queryType,
    Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS,
    "Should have a query type of QUERY_TYPE_BOOKMARKS"
  );

  assertExpectedChildren(root, expectedRoots);

  root.containerOpen = false;
});
