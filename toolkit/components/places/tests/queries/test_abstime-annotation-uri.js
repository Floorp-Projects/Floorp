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

// Some range dates inside our query
var now = PlacesUtils.toPRTime(Date.now());
var jan7_8_00 = PlacesUtils.toPRTime(beginTime + DAY_MSEC);
var jan6_8_15 = PlacesUtils.toPRTime(beginTime + MIN_MSEC * 15);
var jan14_21_30 = PlacesUtils.toPRTime(endTime - DAY_MSEC);
var jan15_20_45 = PlacesUtils.toPRTime(endTime - MIN_MSEC * 45);
var jan12_17_30 = PlacesUtils.toPRTime(endTime - DAY_MSEC * 3 - HOUR_MSEC * 4);

// Dates outside our query
var jan6_7_00 = PlacesUtils.toPRTime(beginTime - HOUR_MSEC);
var dec27_8_00 = PlacesUtils.toPRTime(beginTime - DAY_MSEC * 10);

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
  // Test flat domain with annotation
  {
    isInQuery: true,
    isVisit: true,
    isDetails: true,
    isPageAnnotation: true,
    uri: "http://foo.com/",
    annoName: goodAnnoName,
    annoVal: val,
    lastVisit: jan14_21_30,
    title: "moz",
  },

  // Begin the invalid queries:
  // Test www. style URI is not included, with an annotation
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    isPageAnnotation: true,
    uri: "http://www.foo.com/yiihah",
    annoName: goodAnnoName,
    annoVal: val,
    lastVisit: jan7_8_00,
    title: "moz",
  },

  // Test subdomain not inclued at the leading time edge
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    uri: "http://mail.foo.com/yiihah",
    title: "moz",
    lastVisit: jan6_8_15,
  },

  // Test https protocol
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "moz",
    uri: "https://foo.com/",
    lastVisit: jan15_20_45,
  },

  // Test ftp protocol
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    uri: "ftp://foo.com/ftp",
    lastVisit: jan12_17_30,
    title: "hugelongconfmozlagurationofwordswithasearchtermsinit whoo-hoo",
  },

  // Test too early
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "moz",
    uri: "http://foo.com/tooearly.php",
    lastVisit: jan6_7_00,
  },

  // Test Bad Annotation
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    isPageAnnotation: true,
    title: "moz",
    uri: "http://foo.com/badanno.htm",
    lastVisit: jan12_17_30,
    annoName: badAnnoName,
    annoVal: val,
  },

  // Test afterward, one to update
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "changeme",
    uri: "http://foo.com/changeme1.htm",
    lastVisit: jan12_17_30,
  },

  // Test invalid title
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "changeme2",
    uri: "http://foo.com/changeme2.htm",
    lastVisit: jan7_8_00,
  },

  // Test changing the lastVisit
  {
    isInQuery: false,
    isVisit: true,
    isDetails: true,
    title: "moz",
    uri: "http://foo.com/changeme3.htm",
    lastVisit: dec27_8_00,
  },
];

/**
 * This test will test a Query using several terms and do a bit of negative
 * testing for items that should be ignored while querying over history.
 * The Query:WHERE absoluteTime(matches) AND searchTerms AND URI
 *                 AND annotationIsNot(match) GROUP BY Domain, Day SORT BY uri,ascending
 *                 excludeITems(should be ignored)
 */
add_task(async function test_abstime_annotation_uri() {
  // Initialize database
  await task_populateDB(testData);

  // Query
  var query = PlacesUtils.history.getNewQuery();
  // So that we can easily use these too, convert them to PRTIME
  query.beginTime = PlacesUtils.toPRTime(beginTime);
  query.endTime = PlacesUtils.toPRTime(endTime);
  query.beginTimeReference = PlacesUtils.history.TIME_RELATIVE_EPOCH;
  query.endTimeReference = PlacesUtils.history.TIME_RELATIVE_EPOCH;
  query.searchTerms = "moz";
  query.uri = uri("http://foo.com");
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

  // live update.
  info("change title");
  var change1 = [
    { isDetails: true, uri: "http://foo.com/", title: "mo", lastVisit: now },
  ];
  await task_populateDB(change1);
  Assert.ok(!nodeInResult({ uri: "http://foo.com/" }, root));

  var change2 = [
    {
      isDetails: true,
      uri: "http://foo.com/",
      title: "moz",
      lastVisit: PlacesUtils.toPRTime(endTime + 1),
    },
  ];
  await task_populateDB(change2);

  let node = nodeInResult({ uri: "http://foo.com/" }, root);
  Assert.ok(node);
  Assert.equal(
    node.time,
    now,
    "The node time should be the absolute last visit date"
  );

  // Let's delete something from the result set - using annotation
  var change3 = [
    {
      isPageAnnotation: true,
      uri: "http://foo.com/",
      annoName: badAnnoName,
      annoVal: "test",
    },
  ];
  await task_populateDB(change3);
  info("LiveUpdate by removing annotation");
  node = nodeInResult({ uri: "http://foo.com/" }, root);
  Assert.ok(node);
  Assert.equal(
    node.time,
    now,
    "The node time should be the last visit date, future dates are capped to now"
  );

  root.containerOpen = false;
});
