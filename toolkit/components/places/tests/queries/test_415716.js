/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function modHistoryTypes(val) {
  switch (val % 8) {
    case 0:
    case 1:
      return TRANSITION_LINK;
    case 2:
      return TRANSITION_TYPED;
    case 3:
      return TRANSITION_BOOKMARK;
    case 4:
      return TRANSITION_EMBED;
    case 5:
      return TRANSITION_REDIRECT_PERMANENT;
    case 6:
      return TRANSITION_REDIRECT_TEMPORARY;
    case 7:
      return TRANSITION_DOWNLOAD;
    case 8:
      return TRANSITION_FRAMED_LINK;
  }
  return TRANSITION_TYPED;
}

/**
 * Builds a test database by hand using various times, annotations and
 * visit numbers for this test
 */
add_task(async function test_buildTestDatabase() {
  // This is the set of visits that we will match - our min visit is 2 so that's
  // why we add more visits to the same URIs.
  let testURI = "http://www.foo.com";
  let places = [];

  for (let i = 0; i < 12; ++i) {
    places.push({
      uri: testURI,
      transition: modHistoryTypes(i),
      visitDate: today,
    });
  }

  testURI = "http://foo.com/youdontseeme.html";
  let testAnnoName = "moz-test-places/testing123";
  let testAnnoVal = "test";
  for (let i = 0; i < 12; ++i) {
    places.push({
      uri: testURI,
      transition: modHistoryTypes(i),
      visitDate: today,
    });
  }

  await PlacesTestUtils.addVisits(places);

  await PlacesUtils.history.update({
    url: testURI,
    annotations: new Map([[testAnnoName, testAnnoVal]]),
  });
});

/**
 * This test will test Queries that use relative Time Range, minVists, maxVisits,
 * annotation.
 * The Query:
 * Annotation == "moz-test-places/testing123" &&
 * TimeRange == "now() - 2d" &&
 * minVisits == 2 &&
 * maxVisits == 10
 */
add_task(function test_execute() {
  let query = PlacesUtils.history.getNewQuery();
  query.annotation = "moz-test-places/testing123";
  query.beginTime = daybefore * 1000;
  query.beginTimeReference = PlacesUtils.history.TIME_RELATIVE_NOW;
  query.endTime = today * 1000;
  query.endTimeReference = PlacesUtils.history.TIME_RELATIVE_NOW;
  query.minVisits = 2;
  query.maxVisits = 10;

  // Options
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;

  // Results
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  dump("----> cc is: " + cc + "\n");
  for (let i = 0; i < root.childCount; ++i) {
    let resultNode = root.getChild(i);
    let accesstime = Date(resultNode.time / 1000);
    dump(
      "----> result: " +
        resultNode.uri +
        "   Date: " +
        accesstime.toLocaleString() +
        "\n"
    );
  }
  Assert.equal(cc, 0);
  root.containerOpen = false;
});
