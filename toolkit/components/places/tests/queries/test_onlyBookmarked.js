/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Dietrich Ayala <dietrich@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    parentFolder: bmsvc.toolbarFolder,
    index: bmsvc.DEFAULT_INDEX,
    title: "",
    isInQuery: true },

  // Add a bookmark that should not be in the results
  { isBookmark: true,
    uri: "http://bookmarked-elsewhere.com/",
    parentFolder: bmsvc.bookmarksMenuFolder,
    index: bmsvc.DEFAULT_INDEX,
    title: "",
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
function run_test() {

  // This function in head_queries.js creates our database with the above data
  populateDB(testData);

  // Query
  var query = histsvc.getNewQuery();
  query.setFolders([bmsvc.toolbarFolder], 1);
  query.onlyBookmarked = true;
  
  // query options
  var options = histsvc.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_HISTORY;

  // Results - this gets the result set and opens it for reading and modification.
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  
  // You can use this to compare the data in the array with the result set,
  // if the array's isInQuery: true items are sorted the same way as the result
  // set.
  LOG("begin first test");
  compareArrayToResult(testData, root);
  LOG("end first test");

  /* ******************
  Test live-update
  ********************/
 
  var liveUpdateTestData = [
    //Add a bookmark that should show up
    { isBookmark: true,
      uri: "http://bookmarked2.com/",
      parentFolder: bmsvc.toolbarFolder,
      index: bmsvc.DEFAULT_INDEX,
      title: "",
      isInQuery: true },

    //Add a bookmark that should not show up
    { isBookmark: true,
      uri: "http://bookmarked-elsewhere2.com/",
      parentFolder: bmsvc.bookmarksMenuFolder,
      index: bmsvc.DEFAULT_INDEX,
      title: "",
      isInQuery: false }
  ];
  
  populateDB(liveUpdateTestData); // add to the db

  // add to the test data
  testData.push(liveUpdateTestData[0]);
  testData.push(liveUpdateTestData[1]);

  // re-query and test
  LOG("begin live-update test");
  compareArrayToResult(testData, root);
  LOG("end live-update test");
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

  histsvc.runInBatchMode(updateBatch, null);

  // re-query and test
  LOG("begin batched test");
  compareArrayToResult(testData, root);
  LOG("end batched test");
*/
  // Close the container when finished
  root.containerOpen = false;
}
