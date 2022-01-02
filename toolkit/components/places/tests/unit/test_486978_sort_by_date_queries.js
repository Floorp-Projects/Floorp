/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * TEST DESCRIPTION:
 *
 * This test checks that setting a sort on a RESULTS_AS_DATE_QUERY query,
 * children of inside containers are sorted accordingly.
 */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
  Ci.nsINavHistoryService
);

// Will be inserted in this order, so last one will be the newest visit.
var pages = [
  "http://a.mozilla.org/1/",
  "http://a.mozilla.org/2/",
  "http://a.mozilla.org/3/",
  "http://a.mozilla.org/4/",
  "http://b.mozilla.org/5/",
  "http://b.mozilla.org/6/",
  "http://b.mozilla.org/7/",
  "http://b.mozilla.org/8/",
];

add_task(async function test_initialize() {
  // Add visits.
  let now = new Date();
  for (let pageIndex = 0; pageIndex < pages.length; ++pageIndex) {
    let page = pages[pageIndex];
    await PlacesTestUtils.addVisits({
      uri: uri(page),
      visitDate: new Date(now - (pages.length - pageIndex)),
    });
  }
});

/**
 * Tests that sorting date query by none will sort by title asc.
 */
add_task(function() {
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_QUERY;
  // This should sort by title asc.
  options.sortingMode = options.SORT_BY_NONE;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var dayContainer = root
    .getChild(0)
    .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayContainer.containerOpen = true;

  var cc = dayContainer.childCount;
  Assert.equal(cc, pages.length);
  for (var i = 0; i < cc; i++) {
    var node = dayContainer.getChild(i);
    Assert.equal(pages[i], node.uri);
  }

  dayContainer.containerOpen = false;
  root.containerOpen = false;
});

/**
 * Tests that sorting date query by date will sort accordingly.
 */
add_task(function() {
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_QUERY;
  // This should sort by title asc.
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var dayContainer = root
    .getChild(0)
    .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayContainer.containerOpen = true;

  var cc = dayContainer.childCount;
  Assert.equal(cc, pages.length);
  for (var i = 0; i < cc; i++) {
    var node = dayContainer.getChild(i);
    Assert.equal(pages[pages.length - i - 1], node.uri);
  }

  dayContainer.containerOpen = false;
  root.containerOpen = false;
});

/**
 * Tests that sorting date site query by date will still sort by title asc.
 */
add_task(function() {
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_SITE_QUERY;
  // This should sort by title asc.
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var dayContainer = root
    .getChild(0)
    .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayContainer.containerOpen = true;
  var siteContainer = dayContainer
    .getChild(0)
    .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  Assert.equal(siteContainer.title, "a.mozilla.org");
  siteContainer.containerOpen = true;

  var cc = siteContainer.childCount;
  Assert.equal(cc, pages.length / 2);
  for (var i = 0; i < cc / 2; i++) {
    var node = siteContainer.getChild(i);
    Assert.equal(pages[i], node.uri);
  }

  siteContainer.containerOpen = false;
  dayContainer.containerOpen = false;
  root.containerOpen = false;
});
