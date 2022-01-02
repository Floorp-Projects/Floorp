/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The next thing we do is create a test database for us.  Each test runs with
 * its own database (tail_queries.js will clear it after the run).  Take a look
 * at the queryData object in head_queries.js, and you'll see how this object
 * works.  You can call it anything you like, but I usually use "testData".
 * I'll include a couple of example entries in the database.
 *
 * Note that to use the compareArrayToResult API, you need to put all the
 * results that are in the query set at the top of the testData list, and those
 * results MUST be in the same sort order as the items in the resulting query.
 */

var testData = [
  // Add a bookmark that should be in the results
  {
    isBookmark: true,
    uri: "http://bookmarked.com/",
    title: "",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    isInQuery: true,
  },

  // Add a bookmark that should not be in the results
  {
    isBookmark: true,
    uri: "http://bookmarked-elsewhere.com/",
    title: "",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    isInQuery: false,
  },

  // Add an un-bookmarked visit
  {
    isVisit: true,
    uri: "http://notbookmarked.com/",
    title: "",
    isInQuery: false,
  },
];

add_task(async function test_onlyBookmarked() {
  // This function in head_queries.js creates our database with the above data
  await task_populateDB(testData);

  // Query
  var query = PlacesUtils.history.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.toolbarGuid]);
  query.onlyBookmarked = true;

  // query options
  var options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_HISTORY;

  // Results - this gets the result set and opens it for reading and modification.
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  // You can use this to compare the data in the array with the result set,
  // if the array's isInQuery: true items are sorted the same way as the result
  // set.
  info("begin first test");
  compareArrayToResult(testData, root);
  info("end first test");

  // Test live-update
  var liveUpdateTestData = [
    // Add a bookmark that should show up
    {
      isBookmark: true,
      uri: "http://bookmarked2.com/",
      title: "",
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      isInQuery: true,
    },

    // Add a bookmark that should not show up
    {
      isBookmark: true,
      uri: "http://bookmarked-elsewhere2.com/",
      title: "",
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      isInQuery: false,
    },
  ];

  await task_populateDB(liveUpdateTestData); // add to the db

  // add to the test data
  testData.push(liveUpdateTestData[0]);
  testData.push(liveUpdateTestData[1]);

  // re-query and test
  info("begin live-update test");
  compareArrayToResult(testData, root);
  info("end live-update test");

  // Close the container when finished
  root.containerOpen = false;
});
