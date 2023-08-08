/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test ensures that visitis are correctly live-updated in a history
// query filtered on searchterms and time.

const USEC_PER_DAY = 86400000000;
const now = PlacesUtils.toPRTime(new Date());

add_task(async function pages_query() {
  let query = PlacesUtils.history.getNewQuery();
  query.beginTime = now - 15 * USEC_PER_DAY;
  query.endTime = now - 5 * USEC_PER_DAY;
  query.beginTimeReference = PlacesUtils.history.TIME_RELATIVE_EPOCH;
  query.endTimeReference = PlacesUtils.history.TIME_RELATIVE_EPOCH;
  query.searchTerms = "mo";

  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_URI_ASCENDING;
  options.resultType = options.RESULTS_AS_URI;
  await testQuery(query, options);

  options.sortingMode = options.SORT_BY_DATE_ASCENDING;
  options.resultType = options.RESULTS_AS_VISITS;
  await testQuery(query, options);
});

async function testQuery(query, options) {
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  Assert.equal(root.childCount, 0, "There should be zero results initially");

  await PlacesTestUtils.addVisits([
    // SearchTerms matching URL but out of the timeframe.
    {
      url: "https://test.moz.org",
      title: "abc",
      visitDate: now - 2 * USEC_PER_DAY,
    },
    // In the timeframe but no searchTerms match.
    {
      url: "https://test.def.org",
      title: "def",
      visitDate: now - 10 * USEC_PER_DAY,
    },
    // In the timeframe, matching title.
    {
      url: "https://test.ghi.org",
      title: "amo",
      visitDate: now - 10 * USEC_PER_DAY,
    },
  ]);

  Assert.equal(root.childCount, 1, "Check matching results");
  let node = root.getChild(0);
  Assert.equal(node.title, "amo");

  // Change title so it's no longer matching.
  await PlacesTestUtils.addVisits({
    url: "https://test.ghi.org",
    title: "ghi",
    visitDate: now - 10 * USEC_PER_DAY,
  });

  Assert.equal(root.childCount, 0, "Check matching results");

  // Add visit in the timeframe.
  await PlacesTestUtils.addVisits({
    url: "https://test.moz.org",
    title: "abc",
    visitDate: now - 10 * USEC_PER_DAY,
  });

  Assert.equal(root.childCount, 1, "Check matching results");
  node = root.getChild(0);
  Assert.equal(node.title, "abc");

  // Remove visit in the timeframe.
  await PlacesUtils.history.removeVisitsByFilter({
    beginDate: PlacesUtils.toDate(now - 15 * USEC_PER_DAY),
    endDate: PlacesUtils.toDate(now - 5 * USEC_PER_DAY),
  });
  await PlacesTestUtils.dumpTable({
    table: "moz_places",
    columns: ["id", "url"],
  });
  await PlacesTestUtils.dumpTable({
    table: "moz_historyvisits",
    columns: ["place_id", "visit_date"],
  });

  Assert.equal(root.childCount, 0, "Check matching results");

  // Add matching visit out of the timeframe.
  await PlacesTestUtils.addVisits(
    // SearchTerms matching URL but out of the timeframe.
    {
      url: "https://test.mozilla.org",
      title: "mozilla",
      visitDate: now - 2 * USEC_PER_DAY,
    }
  );

  Assert.equal(root.childCount, 0, "Check matching results");

  root.containerOpen = false;
  await PlacesUtils.history.clear();
}
