/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var bm;
var fakeQuery;
var folderShortcut;

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();

  bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/",
    title: "a bookmark",
  });
  fakeQuery = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "place:terms=foo",
    title: "a bookmark",
  });
  folderShortcut = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: `place:parent=${PlacesUtils.bookmarks.toolbarGuid}`,
    title: "a bookmark",
  });

  checkBookmarkObject(bm);
  checkBookmarkObject(fakeQuery);
  checkBookmarkObject(folderShortcut);
});

add_task(async function test_bookmarks_url_query_implicit_exclusions() {
  // When we run bookmarks url queries, we implicity filter out queries and
  // folder shortcuts regardless of excludeQueries. They don't make sense to
  // include in the results.
  let expectedGuids = [bm.guid];
  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  options.excludeQueries = true;
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;

  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  Assert.equal(
    root.childCount,
    expectedGuids.length,
    "Checking root child count"
  );
  for (let i = 0; i < expectedGuids.length; i++) {
    Assert.equal(
      root.getChild(i).bookmarkGuid,
      expectedGuids[i],
      "should have got the expected item"
    );
  }

  root.containerOpen = false;
});

add_task(async function test_bookmarks_excludeQueries() {
  // When excluding queries, we exclude actual queries, but not folder shortcuts.
  let expectedGuids = [bm.guid, folderShortcut.guid];
  let query = {},
    options = {};
  let queryString = `place:parent=${PlacesUtils.bookmarks.unfiledGuid}&excludeQueries=1`;
  PlacesUtils.history.queryStringToQuery(queryString, query, options);

  let root = PlacesUtils.history.executeQuery(query.value, options.value).root;
  root.containerOpen = true;

  Assert.equal(
    root.childCount,
    expectedGuids.length,
    "Checking root child count"
  );
  for (let i = 0; i < expectedGuids.length; i++) {
    Assert.equal(
      root.getChild(i).bookmarkGuid,
      expectedGuids[i],
      "should have got the expected item"
    );
  }

  root.containerOpen = false;
});

add_task(async function test_search_excludesQueries() {
  // Searching implicity removes queries and folder shortcuts even if excludeQueries
  // is not specified.
  let expectedGuids = [bm.guid];

  let query = PlacesUtils.history.getNewQuery();
  query.searchTerms = "bookmark";

  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;

  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  Assert.equal(
    root.childCount,
    expectedGuids.length,
    "Checking root child count"
  );
  for (let i = 0; i < expectedGuids.length; i++) {
    Assert.equal(
      root.getChild(i).bookmarkGuid,
      expectedGuids[i],
      "should have got the expected item"
    );
  }

  root.containerOpen = false;
});
