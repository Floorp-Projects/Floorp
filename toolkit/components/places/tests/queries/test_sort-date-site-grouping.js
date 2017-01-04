/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
 * ***** END LICENSE BLOCK ***** */

// This test ensures that the date and site type of |place:| query maintains
// its quantifications correctly. Namely, it ensures that the date part of the
// query is not lost when the domain queries are made.

// We specifically craft these entries so that if a by Date and Site sorting is
// applied, we find one domain in the today range, and two domains in the older
// than six months range.
// The correspondence between item in |testData| and date range is stored in
// leveledTestData.
var testData = [
  {
    isVisit: true,
    uri: "file:///directory/1",
    lastVisit: today,
    title: "test visit",
    isInQuery: true
  },
  {
    isVisit: true,
    uri: "http://example.com/1",
    lastVisit: today,
    title: "test visit",
    isInQuery: true
  },
  {
    isVisit: true,
    uri: "http://example.com/2",
    lastVisit: today,
    title: "test visit",
    isInQuery: true
  },
  {
    isVisit: true,
    uri: "file:///directory/2",
    lastVisit: olderthansixmonths,
    title: "test visit",
    isInQuery: true
  },
  {
    isVisit: true,
    uri: "http://example.com/3",
    lastVisit: olderthansixmonths,
    title: "test visit",
    isInQuery: true
  },
  {
    isVisit: true,
    uri: "http://example.com/4",
    lastVisit: olderthansixmonths,
    title: "test visit",
    isInQuery: true
  },
  {
    isVisit: true,
    uri: "http://example.net/1",
    lastVisit: olderthansixmonths + 1000,
    title: "test visit",
    isInQuery: true
  }
];
var domainsInRange = [2, 3];
var leveledTestData = [// Today
                       [[0],    // Today, local files
                        [1, 2]], // Today, example.com
                       // Older than six months
                       [[3],    // Older than six months, local files
                        [4, 5],  // Older than six months, example.com
                        [6]     // Older than six months, example.net
                        ]];

// This test data is meant for live updating. The |levels| property indicates
// date range index and then domain index.
var testDataAddedLater = [
  {
    isVisit: true,
    uri: "http://example.com/5",
    lastVisit: olderthansixmonths,
    title: "test visit",
    isInQuery: true,
    levels: [1, 1]
  },
  {
    isVisit: true,
    uri: "http://example.com/6",
    lastVisit: olderthansixmonths,
    title: "test visit",
    isInQuery: true,
    levels: [1, 1]
  },
  {
    isVisit: true,
    uri: "http://example.com/7",
    lastVisit: today,
    title: "test visit",
    isInQuery: true,
    levels: [0, 1]
  },
  {
    isVisit: true,
    uri: "file:///directory/3",
    lastVisit: today,
    title: "test visit",
    isInQuery: true,
    levels: [0, 0]
  }
];

function run_test() {
  run_next_test();
}

add_task(function* test_sort_date_site_grouping() {
  yield task_populateDB(testData);

  // On Linux, the (local files) folder is shown after sites unlike Mac/Windows.
  // Thus, we avoid running this test on Linux but this should be re-enabled
  // after bug 624024 is resolved.
  let isLinux = ("@mozilla.org/gnome-gconf-service;1" in Components.classes);
  if (isLinux)
    return;

  // In this test, there are three levels of results:
  // 1st: Date queries. e.g., today, last week, or older than 6 months.
  // 2nd: Domain queries restricted to a date. e.g. mozilla.com today.
  // 3rd: Actual visits. e.g. mozilla.com/index.html today.
  //
  // We store all the third level result roots so that we can easily close all
  // containers and test live updating into specific results.
  let roots = [];

  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY;

  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  // This corresponds to the number of date ranges.
  do_check_eq(root.childCount, leveledTestData.length);

  // We pass off to |checkFirstLevel| to check the first level of results.
  for (let index = 0; index < leveledTestData.length; index++) {
    let node = root.getChild(index);
    checkFirstLevel(index, node, roots);
  }

  // Test live updating.
  for (let visit of testDataAddedLater) {
    yield task_populateDB([visit]);
    let oldLength = testData.length;
    let i = visit.levels[0];
    let j = visit.levels[1];
    testData.push(visit);
    leveledTestData[i][j].push(oldLength);
    compareArrayToResult(leveledTestData[i][j].
                         map(x => testData[x]), roots[i][j]);
  }

  for (let i = 0; i < roots.length; i++) {
    for (let j = 0; j < roots[i].length; j++)
      roots[i][j].containerOpen = false;
  }

  root.containerOpen = false;
});

function checkFirstLevel(index, node, roots) {
    PlacesUtils.asContainer(node).containerOpen = true;

    do_check_true(PlacesUtils.nodeIsDay(node));
    PlacesUtils.asQuery(node);
    let queries = node.getQueries();
    let options = node.queryOptions;

    do_check_eq(queries.length, 1);
    let query = queries[0];

    do_check_true(query.hasBeginTime && query.hasEndTime);

    // Here we check the second level of results.
    let root = PlacesUtils.history.executeQuery(query, options).root;
    roots.push([]);
    root.containerOpen = true;

    do_check_eq(root.childCount, leveledTestData[index].length);
    for (var secondIndex = 0; secondIndex < root.childCount; secondIndex++) {
      let child = PlacesUtils.asQuery(root.getChild(secondIndex));
      checkSecondLevel(index, secondIndex, child, roots);
    }
    root.containerOpen = false;
    node.containerOpen = false;
}

function checkSecondLevel(index, secondIndex, child, roots) {
    let queries = child.getQueries();
    let options = child.queryOptions;

    do_check_eq(queries.length, 1);
    let query = queries[0];

    do_check_true(query.hasDomain);
    do_check_true(query.hasBeginTime && query.hasEndTime);

    let root = PlacesUtils.history.executeQuery(query, options).root;
    // We should now have that roots[index][secondIndex] is set to the second
    // level's results root.
    roots[index].push(root);

    // We pass off to compareArrayToResult to check the third level of
    // results.
    root.containerOpen = true;
    compareArrayToResult(leveledTestData[index][secondIndex].
                         map(x => testData[x]), root);
    // We close |root|'s container later so that we can test live
    // updates into it.
}
