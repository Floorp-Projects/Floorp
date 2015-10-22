/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
 * ***** END LICENSE BLOCK ***** */
var beginTime = Date.now();
var testData = [
  {
    isVisit: true,
    title: "page 0",
    uri: "http://mozilla.com/",
    transType: Ci.nsINavHistoryService.TRANSITION_TYPED
  },
  {
    isVisit: true,
    title: "page 1",
    uri: "http://google.com/",
    transType: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD
  },
  {
    isVisit: true,
    title: "page 2",
    uri: "http://microsoft.com/",
    transType: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD
  },
  {
    isVisit: true,
    title: "page 3",
    uri: "http://en.wikipedia.org/",
    transType: Ci.nsINavHistoryService.TRANSITION_BOOKMARK
  },
  {
    isVisit: true,
    title: "page 4",
    uri: "http://fr.wikipedia.org/",
    transType: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD
  },
  {
    isVisit: true,
    title: "page 5",
    uri: "http://apple.com/",
    transType: Ci.nsINavHistoryService.TRANSITION_TYPED
  },
  {
    isVisit: true,
    title: "page 6",
    uri: "http://campus-bike-store.com/",
    transType: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD
  },
  {
    isVisit: true,
    title: "page 7",
    uri: "http://uwaterloo.ca/",
    transType: Ci.nsINavHistoryService.TRANSITION_TYPED
  },
  {
    isVisit: true,
    title: "page 8",
    uri: "http://pugcleaner.com/",
    transType: Ci.nsINavHistoryService.TRANSITION_BOOKMARK
  },
  {
    isVisit: true,
    title: "page 9",
    uri: "http://de.wikipedia.org/",
    transType: Ci.nsINavHistoryService.TRANSITION_TYPED
  },
  {
    isVisit: true,
    title: "arewefastyet",
    uri: "http://arewefastyet.com/",
    transType: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD
  },
  {
    isVisit: true,
    title: "arewefastyet",
    uri: "http://arewefastyet.com/",
    transType: Ci.nsINavHistoryService.TRANSITION_BOOKMARK
  }];
// sets of indices of testData array by transition type
var testDataTyped = [0, 5, 7, 9];
var testDataDownload = [1, 2, 4, 6, 10]; 
var testDataBookmark = [3, 8, 11];

/**
 * run_test is where the magic happens.  This is automatically run by the test
 * harness.  It is where you do the work of creating the query, running it, and
 * playing with the result set.
 */
function run_test()
{
  run_next_test();
}

add_task(function test_transitions()
{
  let timeNow = Date.now();
  for (let item of testData) {
    yield PlacesTestUtils.addVisits({
      uri: uri(item.uri),
      transition: item.transType,
      visitDate: timeNow++ * 1000,
      title: item.title
    });
  }

  //dump_table("moz_places");
  //dump_table("moz_historyvisits");

  var numSortFunc = function (a,b) { return (a - b); };
  var arrs = testDataTyped.concat(testDataDownload).concat(testDataBookmark)
              .sort(numSortFunc);

  // Four tests which compare the result of a query to an expected set.
  var data = arrs.filter(function (index) {
      return (testData[index].uri.match(/arewefastyet\.com/) &&
              testData[index].transType ==
                Ci.nsINavHistoryService.TRANSITION_DOWNLOAD);
    });

  compareQueryToTestData("place:domain=arewefastyet.com&transition=" +
                         Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
                         data.slice());

  compareQueryToTestData("place:transition=" +
                         Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
                         testDataDownload.slice());

  compareQueryToTestData("place:transition=" +
                         Ci.nsINavHistoryService.TRANSITION_TYPED,
                         testDataTyped.slice());

  compareQueryToTestData("place:transition=" +
                         Ci.nsINavHistoryService.TRANSITION_DOWNLOAD +
                         "&transition=" +
                         Ci.nsINavHistoryService.TRANSITION_BOOKMARK,
                         data);

  // Tests the live update property of transitions.
  var query = {};
  var options = {};
  PlacesUtils.history.
    queryStringToQueries("place:transition=" +
                         Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
                         query, {}, options);
  query = (query.value)[0];
  options = PlacesUtils.history.getNewQueryOptions();
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(testDataDownload.length, root.childCount);
  yield PlacesTestUtils.addVisits({
    uri: uri("http://getfirefox.com"),
    transition: TRANSITION_DOWNLOAD
  });
  do_check_eq(testDataDownload.length + 1, root.childCount);
  root.containerOpen = false;
});

/*
 * Takes a query and a set of indices. The indices correspond to elements
 * of testData that are the result of the query.
 */
function compareQueryToTestData(queryStr, data) {
  var query = {};
  var options = {};
  PlacesUtils.history.queryStringToQueries(queryStr, query, {}, options);
  query = query.value[0];
  options = options.value;
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  for (var i = 0; i < data.length; i++) {
    data[i] = testData[data[i]];
    data[i].isInQuery = true;
  }
  compareArrayToResult(data, root);
}
