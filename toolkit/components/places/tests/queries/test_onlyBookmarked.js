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
  { isBookmark: true,
    uri: "http://bookmarked.com/",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    isInQuery: true },

  // Add a bookmark that should not be in the results
  { isBookmark: true,
    uri: "http://bookmarked-elsewhere.com/",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    isInQuery: false },

  // Add an un-bookmarked visit
  { isVisit: true,
    uri: "http://notbookmarked.com/",
    isInQuery: false }
];


/**
 * run_test is where the magic happens.  This is automatically run by the test
 * harness.  It is where you do the work of creating the query, running it, and
 * playing with the result set.
 */
function run_test()
{
  run_next_test();
}

add_task(function test_onlyBookmarked()
{
  // This function in head_queries.js creates our database with the above data
  yield task_populateDB(testData);

  // Query
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);
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
  do_print("begin first test");
  compareArrayToResult(testData, root);
  do_print("end first test");

  /* ******************
  Test live-update
  ********************/
 
  var liveUpdateTestData = [
    //Add a bookmark that should show up
    { isBookmark: true,
      uri: "http://bookmarked2.com/",
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      isInQuery: true },

    //Add a bookmark that should not show up
    { isBookmark: true,
      uri: "http://bookmarked-elsewhere2.com/",
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      isInQuery: false }
  ];
  
  yield task_populateDB(liveUpdateTestData); // add to the db

  // add to the test data
  testData.push(liveUpdateTestData[0]);
  testData.push(liveUpdateTestData[1]);

  // re-query and test
  do_print("begin live-update test");
  compareArrayToResult(testData, root);
  do_print("end live-update test");
/*
  // we are actually not updating during a batch.
  // see bug 432706 for details.

  // Here's a batch update
  var updateBatch = {
    runBatched: function (aUserData) {
      liveUpdateTestData[0].uri = "http://bookmarked3.com";
      liveUpdateTestData[1].uri = "http://bookmarked-elsewhere3.com";
      populateDB(liveUpdateTestData);
      testData.push(liveUpdateTestData[0]);
      testData.push(liveUpdateTestData[1]);
    }
  };

  PlacesUtils.history.runInBatchMode(updateBatch, null);

  // re-query and test
  do_print("begin batched test");
  compareArrayToResult(testData, root);
  do_print("end batched test");
*/
  // Close the container when finished
  root.containerOpen = false;
});
