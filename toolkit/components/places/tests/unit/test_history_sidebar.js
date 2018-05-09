/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get history service
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);

/**
 * Adds a test URI visit to the database.
 *
 * @param aURI
 *        The URI to add a visit for.
 * @param aTime
 *        Reference "now" time.
 * @param aDayOffset
 *        number of days to add, pass a negative value to subtract them.
 */
async function task_add_normalized_visit(aURI, aTime, aDayOffset) {
  var dateObj = new Date(aTime);
  // Normalize to midnight
  dateObj.setHours(0);
  dateObj.setMinutes(0);
  dateObj.setSeconds(0);
  dateObj.setMilliseconds(0);
  // Days where DST changes should be taken in count.
  var previousDateObj = new Date(dateObj.getTime() + aDayOffset * 86400000);
  var DSTCorrection = (dateObj.getTimezoneOffset() -
                       previousDateObj.getTimezoneOffset()) * 60 * 1000;
  // Substract aDayOffset
  var PRTimeWithOffset = (previousDateObj.getTime() - DSTCorrection) * 1000;
  var timeInMs = new Date(PRTimeWithOffset / 1000);
  print("Adding visit to " + aURI.spec + " at " + timeInMs);
  await PlacesTestUtils.addVisits({
    uri: aURI,
    visitDate: PRTimeWithOffset
  });
}

function days_for_x_months_ago(aNowObj, aMonths) {
  var oldTime = new Date();
  // Set day before month, otherwise we could try to calculate 30 February, or
  // other nonexistent days.
  oldTime.setDate(1);
  oldTime.setMonth(aNowObj.getMonth() - aMonths);
  oldTime.setHours(0);
  oldTime.setMinutes(0);
  oldTime.setSeconds(0);
  // Stay larger for eventual timezone issues, add 2 days.
  return parseInt((aNowObj - oldTime) / (1000 * 60 * 60 * 24)) + 2;
}

var nowObj = new Date();
// This test relies on en-US locale
// Offset is number of days
/* eslint-disable comma-spacing */
var containers = [
  { label: "Today"               , offset: 0                                 , visible: true },
  { label: "Yesterday"           , offset: -1                                , visible: true },
  { label: "Last 7 days"         , offset: -3                                , visible: true },
  { label: "This month"          , offset: -8                                , visible: nowObj.getDate() > 8 },
  { label: ""                    , offset: -days_for_x_months_ago(nowObj, 0) , visible: true },
  { label: ""                    , offset: -days_for_x_months_ago(nowObj, 1) , visible: true },
  { label: ""                    , offset: -days_for_x_months_ago(nowObj, 2) , visible: true },
  { label: ""                    , offset: -days_for_x_months_ago(nowObj, 3) , visible: true },
  { label: ""                    , offset: -days_for_x_months_ago(nowObj, 4) , visible: true },
  { label: "Older than 6 months" , offset: -days_for_x_months_ago(nowObj, 5) , visible: true },
];
/* eslint-enable comma-spacing */

var visibleContainers = containers.filter(
  function(aContainer) { return aContainer.visible; });

/**
 * Asynchronous task that fills history and checks containers' labels.
 */
add_task(async function task_fill_history() {
  print("\n\n*** TEST Fill History\n");
  // We can't use "now" because our hardcoded offsets would be invalid for some
  // date.  So we hardcode a date.
  for (let i = 0; i < containers.length; i++) {
    let container = containers[i];
    var testURI = uri("http://mirror" + i + ".mozilla.com/b");
    await task_add_normalized_visit(testURI, nowObj.getTime(), container.offset);
    testURI = uri("http://mirror" + i + ".mozilla.com/a");
    await task_add_normalized_visit(testURI, nowObj.getTime(), container.offset);
    testURI = uri("http://mirror" + i + ".google.com/b");
    await task_add_normalized_visit(testURI, nowObj.getTime(), container.offset);
    testURI = uri("http://mirror" + i + ".google.com/a");
    await task_add_normalized_visit(testURI, nowObj.getTime(), container.offset);
    // Bug 485703 - Hide date containers not containing additional entries
    //              compared to previous ones.
    // Check after every new container is added.
    check_visit(container.offset);
  }

  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_SITE_QUERY;
  var query = hs.getNewQuery();

  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  var cc = root.childCount;
  print("Found containers:");
  var previousLabels = [];
  for (let i = 0; i < cc; i++) {
    let container = visibleContainers[i];
    var node = root.getChild(i);
    print(node.title);
    if (container.label)
      Assert.equal(node.title, container.label);
    // Check labels are not repeated.
    Assert.equal(previousLabels.indexOf(node.title), -1);
    previousLabels.push(node.title);
  }
  Assert.equal(cc, visibleContainers.length);
  root.containerOpen = false;
});

/**
 * Bug 485703 - Hide date containers not containing additional entries compared
 *              to previous ones.
 */
function check_visit(aOffset) {
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_SITE_QUERY;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;

  var unexpected = [];
  switch (aOffset) {
    case 0:
      unexpected = ["Yesterday", "Last 7 days", "This month"];
      break;
    case -1:
      unexpected = ["Last 7 days", "This month"];
      break;
    case -3:
      unexpected = ["This month"];
      break;
    default:
      // Other containers are tested later.
  }

  print("Found containers:");
  for (var i = 0; i < cc; i++) {
    var node = root.getChild(i);
    print(node.title);
    Assert.equal(unexpected.indexOf(node.title), -1);
  }

  root.containerOpen = false;
}

/**
 * Queries history grouped by date and site, checking containers' labels and
 * children.
 */
add_task(async function test_RESULTS_AS_DATE_SITE_QUERY() {
  print("\n\n*** TEST RESULTS_AS_DATE_SITE_QUERY\n");
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_SITE_QUERY;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  // Check one of the days
  var dayNode = root.getChild(0)
                    .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayNode.containerOpen = true;
  Assert.equal(dayNode.childCount, 2);

  // Items should be sorted by host
  var site1 = dayNode.getChild(0)
                     .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  Assert.equal(site1.title, "mirror0.google.com");

  var site2 = dayNode.getChild(1)
                     .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  Assert.equal(site2.title, "mirror0.mozilla.com");

  site1.containerOpen = true;
  Assert.equal(site1.childCount, 2);

  // Inside of host sites are sorted by title
  var site1visit = site1.getChild(0);
  Assert.equal(site1visit.uri, "http://mirror0.google.com/a");

  // Bug 473157: changing sorting mode should not affect the containers
  result.sortingMode = options.SORT_BY_TITLE_DESCENDING;

  // Check one of the days
  dayNode = root.getChild(0)
                    .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayNode.containerOpen = true;
  Assert.equal(dayNode.childCount, 2);

  // Hosts are still sorted by title
  site1 = dayNode.getChild(0)
                     .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  Assert.equal(site1.title, "mirror0.google.com");

  site2 = dayNode.getChild(1)
                     .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  Assert.equal(site2.title, "mirror0.mozilla.com");

  site1.containerOpen = true;
  Assert.equal(site1.childCount, 2);

  // But URLs are now sorted by title descending
  site1visit = site1.getChild(0);
  Assert.equal(site1visit.uri, "http://mirror0.google.com/b");

  site1.containerOpen = false;
  dayNode.containerOpen = false;
  root.containerOpen = false;
});

/**
 * Queries history grouped by date, checking containers' labels and children.
 */
add_task(async function test_RESULTS_AS_DATE_QUERY() {
  print("\n\n*** TEST RESULTS_AS_DATE_QUERY\n");
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_QUERY;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  var cc = root.childCount;
  Assert.equal(cc, visibleContainers.length);
  print("Found containers:");
  for (var i = 0; i < cc; i++) {
    var container = visibleContainers[i];
    var node = root.getChild(i);
    print(node.title);
    if (container.label)
      Assert.equal(node.title, container.label);
  }

  // Check one of the days
  var dayNode = root.getChild(0)
                    .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayNode.containerOpen = true;
  Assert.equal(dayNode.childCount, 4);

  // Items should be sorted by title
  var visit1 = dayNode.getChild(0);
  Assert.equal(visit1.uri, "http://mirror0.google.com/a");

  var visit2 = dayNode.getChild(3);
  Assert.equal(visit2.uri, "http://mirror0.mozilla.com/b");

  // Bug 473157: changing sorting mode should not affect the containers
  result.sortingMode = options.SORT_BY_TITLE_DESCENDING;

  // Check one of the days
  dayNode = root.getChild(0)
                    .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayNode.containerOpen = true;
  Assert.equal(dayNode.childCount, 4);

  // But URLs are now sorted by title descending
  visit1 = dayNode.getChild(0);
  Assert.equal(visit1.uri, "http://mirror0.mozilla.com/b");

  visit2 = dayNode.getChild(3);
  Assert.equal(visit2.uri, "http://mirror0.google.com/a");

  dayNode.containerOpen = false;
  root.containerOpen = false;
});

/**
 * Queries history grouped by site, checking containers' labels and children.
 */
add_task(async function test_RESULTS_AS_SITE_QUERY() {
  print("\n\n*** TEST RESULTS_AS_SITE_QUERY\n");
  // add a bookmark with a domain not in the set of visits in the db
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://foobar",
    title: ""
  });

  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_SITE_QUERY;
  options.sortingMode = options.SORT_BY_TITLE_ASCENDING;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, containers.length * 2);

/* Expected results:
    "mirror0.google.com",
    "mirror0.mozilla.com",
    "mirror1.google.com",
    "mirror1.mozilla.com",
    "mirror2.google.com",
    "mirror2.mozilla.com",
    "mirror3.google.com",  <== We check for this site (index 6)
    "mirror3.mozilla.com",
    "mirror4.google.com",
    "mirror4.mozilla.com",
    "mirror5.google.com",
    "mirror5.mozilla.com",
    ...
*/

  // Items should be sorted by host
  var siteNode = root.getChild(6)
                     .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  Assert.equal(siteNode.title, "mirror3.google.com");

  siteNode.containerOpen = true;
  Assert.equal(siteNode.childCount, 2);

  // Inside of host sites are sorted by title
  var visitNode = siteNode.getChild(0);
  Assert.equal(visitNode.uri, "http://mirror3.google.com/a");

  // Bug 473157: changing sorting mode should not affect the containers
  result.sortingMode = options.SORT_BY_TITLE_DESCENDING;
  siteNode = root.getChild(6)
                     .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  Assert.equal(siteNode.title, "mirror3.google.com");

  siteNode.containerOpen = true;
  Assert.equal(siteNode.childCount, 2);

  // But URLs are now sorted by title descending
  var visit = siteNode.getChild(0);
  Assert.equal(visit.uri, "http://mirror3.google.com/b");

  siteNode.containerOpen = false;
  root.containerOpen = false;

  // Cleanup.
  await PlacesUtils.bookmarks.remove(bookmark.guid);
});

/**
 * Checks that queries grouped by date do liveupdate correctly.
 */
async function task_test_date_liveupdate(aResultType) {
  var midnight = nowObj;
  midnight.setHours(0);
  midnight.setMinutes(0);
  midnight.setSeconds(0);
  midnight.setMilliseconds(0);

  // TEST 1. Test that the query correctly updates when it is root.
  var options = hs.getNewQueryOptions();
  options.resultType = aResultType;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  Assert.equal(root.childCount, visibleContainers.length);
  // Remove "Today".
  await PlacesUtils.history.removeByFilter({
    beginDate: new Date(midnight.getTime()),
    endDate: new Date(Date.now())
  });
  Assert.equal(root.childCount, visibleContainers.length - 1);

  // Open "Last 7 days" container, this way we will have a container accepting
  // the new visit, but we should still add back "Today" container.
  var last7Days = root.getChild(1)
                      .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  last7Days.containerOpen = true;

  // Add a visit for "Today".  This should add back the missing "Today"
  // container.
  await task_add_normalized_visit(uri("http://www.mozilla.org/"), nowObj.getTime(), 0);
  Assert.equal(root.childCount, visibleContainers.length);

  last7Days.containerOpen = false;
  root.containerOpen = false;

  // TEST 2. Test that the query correctly updates even if it is not root.
  var bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "place:type=" + aResultType,
    title: "",
  });

  // Query toolbar and open our query container, then check again liveupdate.
  options = hs.getNewQueryOptions();
  query = hs.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);
  result = hs.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);
  var dateContainer = root.getChild(0).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dateContainer.containerOpen = true;

  Assert.equal(dateContainer.childCount, visibleContainers.length);
  // Remove "Today".
  await PlacesUtils.history.removeByFilter({
    beginDate: new Date(midnight.getTime()),
    endDate: new Date(Date.now())
  });
  Assert.equal(dateContainer.childCount, visibleContainers.length - 1);
  // Add a visit for "Today".
  await task_add_normalized_visit(uri("http://www.mozilla.org/"), nowObj.getTime(), 0);
  Assert.equal(dateContainer.childCount, visibleContainers.length);

  dateContainer.containerOpen = false;
  root.containerOpen = false;

  // Cleanup.
  await PlacesUtils.bookmarks.remove(bookmark.guid);
}

function run_test() {
  // If we're dangerously close to a date change, just bail out.
  if (nowObj.getHours() == 23 && nowObj.getMinutes() >= 50) {
    return;
  }

  run_next_test();
}

add_task(async function test_history_sidebar() {
  await task_test_date_liveupdate(Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY);
  await task_test_date_liveupdate(Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY);

  // The remaining views are
  //   RESULTS_AS_URI + SORT_BY_VISITCOUNT_DESCENDING
  //   ->  test_399266.js
  //   RESULTS_AS_URI + SORT_BY_DATE_DESCENDING
  //   ->  test_385397.js
});
