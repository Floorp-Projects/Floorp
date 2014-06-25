/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Pre Test Items go here - Note that if you need time constants or relative
 * times, there are several in head_queries.js.
 * Other things to do here might be to create some bookmark folders, for access
 * to the Places API's you can use the global variables defined in head_queries.js.
 * Here is an example of using these to create some bookmark folders:
 */
 // Create Folder1 from root
 PlacesUtils.bookmarks.createFolder(PlacesUtils.placesRootId, "Folder 1",
                                    PlacesUtils.bookmarks.DEFAULT_INDEX);
 var folder1Id = PlacesUtils.bookmarks.getChildFolder(PlacesUtils.placesRootId,
                                                      "Folder 1");

 // Make Folder 1a a child of Folder 1
 PlacesUtils.bookmarks.createFolder(folder1Id, "Folder 1a",
                                    PlacesUtils.bookmarks.DEFAULT_INDEX);
 var folder1aId = PlacesUtils.bookmarks.getChildFolder(folder1Id, "Folder 1a");
 
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

  // This puts a history visit item into the database.  Note that I don't
  // specify the "lastVisit" attribute, because I am ok with "lastVisit"
  // defaulting to "today". isInQuery is true as I expect this to turn up in the
  // query I'll be doing down below.
  {isInQuery: true, isVisit: true, uri: "http://foo.com/"},

  // And so on to get a database that has enough elements to adequately test
  // the edges of your query set.  Be sure to include things that are outside
  // the query set but can be updated so that they are included in the query
  // set. Here's a more complicated example to finish our examples.
  {isInQuery: true, isVisit: true, isDetails: true, title: "taggariffic",
   uri: "http://foo.com/tagging/test.html", lastVisit: beginTime, isTag: true,
   tagArray: ["moz"] }];

  
/**
 * run_test is where the magic happens.  This is automatically run by the test
 * harness.  It is where you do the work of creating the query, running it, and
 * playing with the result set.
 */
function run_test() {

  // This function in head_queries.js creates our database with the above data
  populateDB(testData);

  // Query
  var query = PlacesUtils.history.getNewQuery();
  // Set query attributes here...
  
  // query options
  var options = PlacesUtils.history.getNewQueryOptions();
  // Set queryOptions attributes here

  // Results - this gets the result set and opens it for reading and modification.
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  
  // You can use this to compare the data in the array with the result set,
  // if the array's isInQuery: true items are sorted the same way as the result
  // set.
  compareArrayToResult(testData, root);

  // Make some changes to the result set
  // Let's add something first - you can use populateDB to append/update the
  // database too...
  var addItem = [{isInQuery: true, isVisit: true, isDetails: true, title: "moz",
                 uri: "http://foo.com/i-am-added.html", lastVisit: jan11_800}];
  populateDB(addItem);
  
  // Here's an update
  var change1 = [{isDetails: true, uri: "http://foo.com/",
                  lastVisit: jan12_1730, title: "moz moz mozzie"}];
  populateDB(change1);

  // Here's a batch update
  var updateBatch = {
    runBatched: function (aUserData) {
      var batchChange = [{isDetails: true, uri: "http://foo.com/changeme2",
                          title: "moz", lastVisit: jan7_800},
                         {isPageAnnotation: true, uri: "http://foo.com/begin.html",
                          annoName: badAnnoName, annoVal: val}];
      populateDB(batchChange);
    }
  };

  PlacesUtils.history.runInBatchMode(updateBatch, null);

  // Close the container when finished
  root.containerOpen = false;
}
