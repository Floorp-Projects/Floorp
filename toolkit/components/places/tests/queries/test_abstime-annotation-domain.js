/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const DAY_MSEC = 86400000;
const MIN_MSEC = 60000;
const HOUR_MSEC = 3600000;
// Jan 6 2008 at 8am is our begin edge of the query
var beginTimeDate = new Date(2008, 0, 6, 8, 0, 0, 0);
// Jan 15 2008 at 9:30pm is our ending edge of the query
var endTimeDate = new Date(2008, 0, 15, 21, 30, 0, 0);

// These as millisecond values
var beginTime = beginTimeDate.getTime();
var endTime = endTimeDate.getTime();

// Some range dates inside our query - mult by 1000 to convert to PRTIME
var jan7_800 = (beginTime + DAY_MSEC) * 1000;
var jan6_815 = (beginTime + MIN_MSEC * 15) * 1000;
var jan11_800 = (beginTime + DAY_MSEC * 5) * 1000;
var jan14_2130 = (endTime - DAY_MSEC) * 1000;
var jan15_2045 = (endTime - MIN_MSEC * 45) * 1000;
var jan12_1730 = (endTime - DAY_MSEC * 3 - HOUR_MSEC * 4) * 1000;

// Dates outside our query - mult by 1000 to convert to PRTIME
var jan6_700 = (beginTime - HOUR_MSEC) * 1000;
var dec27_800 = (beginTime - DAY_MSEC * 10) * 1000;

// So that we can easily use these too, convert them to PRTIME
beginTime *= 1000;
endTime *= 1000;

/**
 * Array of objects to build our test database
 */
var goodAnnoName = "moz-test-places/testing123";
var val = "test";
var badAnnoName = "text/foo";

// The test data for our database, note that the ordering of the results that
// will be returned by the query (the isInQuery: true objects) is IMPORTANT.
// see compareArrayToResult in head_queries.js for more info.
var testData = [
  // Test ftp protocol - vary the title length
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    uri: "ftp://foo.com/ftp",
    lastVisit: jan12_1730,
    title: "hugelongconfmozlagurationofwordswithasearchtermsinit whoo-hoo",
  },

  // Test flat domain with annotation
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    isPageAnnotation: true,
    uri: "http://foo.com/",
    annoName: goodAnnoName,
    annoVal: val,
    lastVisit: jan14_2130,
    title: "moz",
  },

  // Test subdomain included with isRedirect=true, different transtype
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    title: "moz",
    isRedirect: true,
    uri: "http://mail.foo.com/redirect",
    lastVisit: jan11_800,
    transType: PlacesUtils.history.TRANSITION_LINK,
  },

  // Test subdomain inclued at the leading time edge
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    uri: "http://mail.foo.com/yiihah",
    title: "moz",
    lastVisit: jan6_815,
  },

  // Test www. style URI is included, with an annotation
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    isPageAnnotation: true,
    uri: "http://www.foo.com/yiihah",
    annoName: goodAnnoName,
    annoVal: val,
    lastVisit: jan7_800,
    title: "moz",
  },

  // Test https protocol
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    title: "moz",
    uri: "https://foo.com/",
    lastVisit: jan15_2045,
  },

  // Test begin edge of time
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    title: "moz mozilla",
    uri: "https://foo.com/begin.html",
    lastVisit: beginTime,
  },

  // Test end edge of time
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    title: "moz mozilla",
    uri: "https://foo.com/end.html",
    lastVisit: endTime,
  },

  // Test an image link, with annotations
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    isPageAnnotation: true,
    title: "mozzie the dino",
    uri: "https://foo.com/mozzie.png",
    annoName: goodAnnoName,
    annoVal: val,
    lastVisit: jan14_2130,
  },

  // Begin the invalid queries: Test too early
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "moz",
    uri: "http://foo.com/tooearly.php",
    lastVisit: jan6_700,
  },

  // Test Bad Annotation
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    isPageAnnotation: true,
    title: "moz",
    uri: "http://foo.com/badanno.htm",
    lastVisit: jan12_1730,
    annoName: badAnnoName,
    annoVal: val,
  },

  // Test bad URI
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "moz",
    uri: "http://somefoo.com/justwrong.htm",
    lastVisit: jan11_800,
  },

  // Test afterward, one to update
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "changeme",
    uri: "http://foo.com/changeme1.htm",
    lastVisit: jan12_1730,
  },

  // Test invalid title
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "changeme2",
    uri: "http://foo.com/changeme2.htm",
    lastVisit: jan7_800,
  },

  // Test changing the lastVisit
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "moz",
    uri: "http://foo.com/changeme3.htm",
    lastVisit: dec27_800,
  },
];

/**
 * This test will test a Query using several terms and do a bit of negative
 * testing for items that should be ignored while querying over history.
 * The Query:WHERE absoluteTime(matches) AND searchTerms AND URI
 *                 AND annotationIsNot(match) GROUP BY Domain, Day SORT BY uri,ascending
 *                 excludeITems(should be ignored)
 */
add_task(async function test_abstime_annotation_domain() {
  // Initialize database
  await task_populateDB(testData);

  // Query
  var query = PlacesUtils.history.getNewQuery();
  query.beginTime = beginTime;
  query.endTime = endTime;
  query.beginTimeReference = PlacesUtils.history.TIME_RELATIVE_EPOCH;
  query.endTimeReference = PlacesUtils.history.TIME_RELATIVE_EPOCH;
  query.searchTerms = "moz";
  query.domain = "foo.com";
  query.domainIsHost = false;
  query.annotation = "text/foo";
  query.annotationIsNot = true;

  // Options
  var options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_URI_ASCENDING;
  options.resultType = options.RESULTS_AS_URI;
  // The next two options should be ignored
  // can't use this one, breaks test - bug 419779
  // options.excludeItems = true;

  // Results
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  // Ensure the result set is correct
  compareArrayToResult(testData, root);

  // Make some changes to the result set
  // Let's add something first
  var addItem = [
    {
      isInQuery: true,
      isVisit: true,
      isDetails: true,
      title: "moz",
      uri: "http://www.foo.com/i-am-added.html",
      lastVisit: jan11_800,
    },
  ];
  await task_populateDB(addItem);
  info("Adding item foo.com/i-am-added.html");
  Assert.equal(isInResult(addItem, root), true);

  // Let's update something by title
  var change1 = [
    {
      isDetails: true,
      uri: "http://foo.com/changeme1",
      lastVisit: jan12_1730,
      title: "moz moz mozzie",
    },
  ];
  await task_populateDB(change1);
  info("LiveUpdate by changing title");
  Assert.equal(isInResult(change1, root), true);

  // Let's update something by annotation
  // Updating a page by removing an annotation does not cause it to join this
  // query set.  I tend to think that it should cause that page to join this
  // query set, because this visit fits all theother specified criteria once the
  // annotation is removed. Uncommenting this will fail the test.
  // Bug 424050
  /* var change2 = [{isPageAnnotation: true, uri: "http://foo.com/badannotaion.html",
                  annoName: "text/mozilla", annoVal: "test"}];
  yield task_populateDB(change2);
  do_print("LiveUpdate by removing annotation");
  do_check_eq(isInResult(change2, root), true);*/

  // Let's update by adding a visit in the time range for an existing URI
  var change3 = [
    {
      isDetails: true,
      uri: "http://foo.com/changeme3.htm",
      title: "moz",
      lastVisit: jan15_2045,
    },
  ];
  await task_populateDB(change3);
  info("LiveUpdate by adding visit within timerange");
  Assert.equal(isInResult(change3, root), true);

  // And delete something from the result set - using annotation
  // Once again, bug 424050 prevents this from passing
  /* var change4 = [{isPageAnnotation: true, uri: "ftp://foo.com/ftp",
                  annoVal: "test", annoName: badAnnoName}];
  yield task_populateDB(change4);
  do_print("LiveUpdate by deleting item from set by adding annotation");
  do_check_eq(isInResult(change4, root), false);*/

  // Delete something by changing the title
  var change5 = [
    { isDetails: true, uri: "http://foo.com/end.html", title: "deleted" },
  ];
  await task_populateDB(change5);
  info("LiveUpdate by deleting item by changing title");
  Assert.equal(isInResult(change5, root), false);

  root.containerOpen = false;
});
