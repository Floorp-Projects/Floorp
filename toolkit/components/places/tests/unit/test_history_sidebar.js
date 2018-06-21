/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let nowObj = new Date();

/**
 * Normalizes a Date to midnight.
 *
 * @param {Date} inputDate
 * @return normalized Date
 */
function toMidnight(inputDate) {
  let date = new Date(inputDate);
  date.setHours(0);
  date.setMinutes(0);
  date.setSeconds(0);
  date.setMilliseconds(0);
  return date;
}

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
async function addNormalizedVisit(aURI, aTime, aDayOffset) {
  let dateObj = toMidnight(aTime);
  // Days where DST changes should be taken into account.
  let previousDateObj = new Date(dateObj.getTime() + aDayOffset * 86400000);
  let DSTCorrection = (dateObj.getTimezoneOffset() -
                       previousDateObj.getTimezoneOffset()) * 60 * 1000;
  // Substract aDayOffset
  let PRTimeWithOffset = (previousDateObj.getTime() - DSTCorrection) * 1000;
  info("Adding visit to " + aURI.spec + " at " + PlacesUtils.toDate(PRTimeWithOffset));
  await PlacesTestUtils.addVisits({
    uri: aURI,
    visitDate: PRTimeWithOffset
  });
}

function openRootForResultType(resultType) {
  let options = PlacesUtils.history.getNewQueryOptions();
  options.resultType = resultType;
  let query = PlacesUtils.history.getNewQuery();
  let result = PlacesUtils.history.executeQuery(query, options);
  let root = result.root;
  root.containerOpen = true;
  return result;
}

function daysForMonthsAgo(months) {
  let oldTime = toMidnight(new Date());
  // Set day before month, otherwise we could try to calculate 30 February, or
  // other nonexistent days.
  oldTime.setDate(1);
  oldTime.setMonth(nowObj.getMonth() - months);
  // Stay larger for eventual timezone issues, add 2 days.
  return parseInt((nowObj - oldTime) / (1000 * 60 * 60 * 24)) + 2;
}

// This test relies on en-US locale
// Offset is number of days
let containers = [
  { label: "Today",               offset: 0,                    visible: true },
  { label: "Yesterday",           offset: -1,                   visible: true },
  { label: "Last 7 days",         offset: -2,                   visible: true },
  { label: "This month",          offset: -8,                   visible: nowObj.getDate() > 8 },
  { label: "",                    offset: -daysForMonthsAgo(0), visible: true },
  { label: "",                    offset: -daysForMonthsAgo(1), visible: true },
  { label: "",                    offset: -daysForMonthsAgo(2), visible: true },
  { label: "",                    offset: -daysForMonthsAgo(3), visible: true },
  { label: "",                    offset: -daysForMonthsAgo(4), visible: true },
  { label: "Older than 6 months", offset: -daysForMonthsAgo(5), visible: true },
];

let visibleContainers = containers.filter(container => container.visible);

/**
 * Asynchronous task that fills history and checks containers' labels.
 */
add_task(async function task_fill_history() {
  info("*** TEST Fill History");
  // We can't use "now" because our hardcoded offsets would be invalid for some
  // date.  So we hardcode a date.
  for (let i = 0; i < containers.length; i++) {
    let container = containers[i];
    let testURI = uri("http://mirror" + i + ".mozilla.com/b");
    await addNormalizedVisit(testURI, nowObj, container.offset);
    testURI = uri("http://mirror" + i + ".mozilla.com/a");
    await addNormalizedVisit(testURI, nowObj, container.offset);
    testURI = uri("http://mirror" + i + ".google.com/b");
    await addNormalizedVisit(testURI, nowObj, container.offset);
    testURI = uri("http://mirror" + i + ".google.com/a");
    await addNormalizedVisit(testURI, nowObj, container.offset);
    // Bug 485703 - Hide date containers not containing additional entries
    //              compared to previous ones.
    // Check after every new container is added.
    check_visit(container.offset);
  }

  let root = openRootForResultType(Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY).root;
  let cc = root.childCount;
  info("Found containers:");
  let previousLabels = [];
  for (let i = 0; i < cc; i++) {
    let container = visibleContainers[i];
    let node = root.getChild(i);
    info(node.title);
    if (container.label)
      Assert.equal(node.title, container.label);
    // Check labels are not repeated.
    Assert.ok(!previousLabels.includes(node.title));
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
  let root = openRootForResultType(Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY).root;

  let unexpected = [];
  switch (aOffset) {
    case 0:
      unexpected = ["Yesterday", "Last 7 days", "This month"];
      break;
    case -1:
      unexpected = ["Last 7 days", "This month"];
      break;
    case -2:
      unexpected = ["This month"];
      break;
    default:
      // Other containers are tested later.
  }

  info("Found containers:");
  let cc = root.childCount;
  for (let i = 0; i < cc; i++) {
    let node = root.getChild(i);
    info(node.title);
    Assert.ok(!unexpected.includes(node.title));
  }
  root.containerOpen = false;
}

/**
 * Queries history grouped by date and site, checking containers' labels and
 * children.
 */
add_task(async function test_RESULTS_AS_DATE_SITE_QUERY() {
  info("*** TEST RESULTS_AS_DATE_SITE_QUERY");
  let result = openRootForResultType(Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY);
  let root = result.root;

  // Check one of the days
  let dayNode = PlacesUtils.asContainer(root.getChild(0));
  dayNode.containerOpen = true;
  Assert.equal(dayNode.childCount, 2);

  // Items should be sorted by host
  let site1 = PlacesUtils.asContainer(dayNode.getChild(0));
  Assert.equal(site1.title, "mirror0.google.com");

  let site2 = PlacesUtils.asContainer(dayNode.getChild(1));
  Assert.equal(site2.title, "mirror0.mozilla.com");

  site1.containerOpen = true;
  Assert.equal(site1.childCount, 2);

  // Inside of host sites are sorted by title
  let site1visit = site1.getChild(0);
  Assert.equal(site1visit.uri, "http://mirror0.google.com/a");

  // Bug 473157: changing sorting mode should not affect the containers
  result.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_DESCENDING;

  // Check one of the days
  dayNode = PlacesUtils.asContainer(root.getChild(0));
  dayNode.containerOpen = true;
  Assert.equal(dayNode.childCount, 2);

  // Hosts are still sorted by title
  site1 = PlacesUtils.asContainer(dayNode.getChild(0));
  Assert.equal(site1.title, "mirror0.google.com");

  site2 = PlacesUtils.asContainer(dayNode.getChild(1));
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
  info("*** TEST RESULTS_AS_DATE_QUERY");
  let result = openRootForResultType(Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY);
  let root = result.root;
  let cc = root.childCount;
  Assert.equal(cc, visibleContainers.length);
  info("Found containers:");
  for (let i = 0; i < cc; i++) {
    let container = visibleContainers[i];
    let node = root.getChild(i);
    info(node.title);
    if (container.label)
      Assert.equal(node.title, container.label);
  }

  // Check one of the days
  let dayNode = PlacesUtils.asContainer(root.getChild(0));
  dayNode.containerOpen = true;
  Assert.equal(dayNode.childCount, 4);

  // Items should be sorted by title
  let visit1 = dayNode.getChild(0);
  Assert.equal(visit1.uri, "http://mirror0.google.com/a");

  let visit2 = dayNode.getChild(3);
  Assert.equal(visit2.uri, "http://mirror0.mozilla.com/b");

  // Bug 473157: changing sorting mode should not affect the containers
  result.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_DESCENDING;

  // Check one of the days
  dayNode = PlacesUtils.asContainer(root.getChild(0));
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
  info("*** TEST RESULTS_AS_SITE_QUERY");
  // add a bookmark with a domain not in the set of visits in the db
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://foobar",
    title: ""
  });

  let options = PlacesUtils.history.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_SITE_QUERY;
  options.sortingMode = options.SORT_BY_TITLE_ASCENDING;
  let query = PlacesUtils.history.getNewQuery();
  let result = PlacesUtils.history.executeQuery(query, options);
  let root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, containers.length * 2);

  // Expected results:
  //   "mirror0.google.com",
  //   "mirror0.mozilla.com",
  //   "mirror1.google.com",
  //   "mirror1.mozilla.com",
  //   "mirror2.google.com",
  //   "mirror2.mozilla.com",
  //   "mirror3.google.com",  <== We check for this site (index 6)
  //   "mirror3.mozilla.com",
  //   "mirror4.google.com",
  //   "mirror4.mozilla.com",
  //   "mirror5.google.com",
  //   "mirror5.mozilla.com",
  //   ...

  // Items should be sorted by host
  let siteNode = PlacesUtils.asContainer(root.getChild(6));
  Assert.equal(siteNode.title, "mirror3.google.com");

  siteNode.containerOpen = true;
  Assert.equal(siteNode.childCount, 2);

  // Inside of host sites are sorted by title
  let visitNode = siteNode.getChild(0);
  Assert.equal(visitNode.uri, "http://mirror3.google.com/a");

  // Bug 473157: changing sorting mode should not affect the containers
  result.sortingMode = options.SORT_BY_TITLE_DESCENDING;
  siteNode = PlacesUtils.asContainer(root.getChild(6));
  Assert.equal(siteNode.title, "mirror3.google.com");

  siteNode.containerOpen = true;
  Assert.equal(siteNode.childCount, 2);

  // But URLs are now sorted by title descending
  let visit = siteNode.getChild(0);
  Assert.equal(visit.uri, "http://mirror3.google.com/b");

  siteNode.containerOpen = false;
  root.containerOpen = false;

  // Cleanup.
  await PlacesUtils.bookmarks.remove(bookmark.guid);
});

/**
 * Checks that queries grouped by date do liveupdate correctly.
 */
async function test_date_liveupdate(aResultType) {
  let midnight = toMidnight(nowObj);

  // TEST 1. Test that the query correctly updates when it is root.
  let root = openRootForResultType(aResultType).root;
  Assert.equal(root.childCount, visibleContainers.length);

  // Remove "Today".
  await PlacesUtils.history.removeByFilter({
    beginDate: new Date(midnight.getTime()),
    endDate: new Date(Date.now())
  });
  Assert.equal(root.childCount, visibleContainers.length - 1);

  // Open "Last 7 days" container, this way we will have a container accepting
  // the new visit, but we should still add back "Today" container.
  let last7Days = PlacesUtils.asContainer(root.getChild(1));
  last7Days.containerOpen = true;

  // Add a visit for "Today".  This should add back the missing "Today"
  // container.
  await addNormalizedVisit(uri("http://www.mozilla.org/"), nowObj, 0);
  Assert.equal(root.childCount, visibleContainers.length);

  last7Days.containerOpen = false;
  root.containerOpen = false;

  // TEST 2. Test that the query correctly updates even if it is not root.
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "place:type=" + aResultType,
    title: "",
  });

  // Query toolbar and open our query container, then check again liveupdate.
  root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.toolbarGuid).root;
  Assert.equal(root.childCount, 1);
  let dateContainer = PlacesUtils.asContainer(root.getChild(0));
  dateContainer.containerOpen = true;

  Assert.equal(dateContainer.childCount, visibleContainers.length);
  // Remove "Today".
  await PlacesUtils.history.removeByFilter({
    beginDate: new Date(midnight.getTime()),
    endDate: new Date(Date.now())
  });
  Assert.equal(dateContainer.childCount, visibleContainers.length - 1);
  // Add a visit for "Today".
  await addNormalizedVisit(uri("http://www.mozilla.org/"), nowObj, 0);
  Assert.equal(dateContainer.childCount, visibleContainers.length);

  dateContainer.containerOpen = false;
  root.containerOpen = false;

  // Cleanup.
  await PlacesUtils.bookmarks.remove(bookmark.guid);
}

add_task(async function test_history_sidebar() {
  await test_date_liveupdate(Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY);
  await test_date_liveupdate(Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY);

  // The remaining views are
  //   RESULTS_AS_URI + SORT_BY_VISITCOUNT_DESCENDING
  //   ->  test_399266.js
  //   RESULTS_AS_URI + SORT_BY_DATE_DESCENDING
  //   ->  test_385397.js
});
