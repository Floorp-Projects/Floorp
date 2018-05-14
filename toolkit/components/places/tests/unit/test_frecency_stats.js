/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function init() {
  await cleanUp();
});


// Adds/removes some visits and bookmarks and makes sure the stats are updated.
add_task(async function basic() {
  Assert.equal(PlacesUtils.history.frecencyMean, 0);
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation, 0);

  let frecenciesByURL = {};
  let urls = [0, 1, 2].map(i => "http://example.com/" + i);

  // Add a URL 0 visit.
  await PlacesTestUtils.addVisits([{ uri: urls[0] }]);
  frecenciesByURL[urls[0]] = frecencyForUrl(urls[0]);
  Assert.ok(frecenciesByURL[urls[0]] > 0, "Sanity check");
  Assert.equal(PlacesUtils.history.frecencyMean,
               mean(Object.values(frecenciesByURL)));
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation,
               stddev(Object.values(frecenciesByURL)));

  // Add a URL 1 visit.
  await PlacesTestUtils.addVisits([{ uri: urls[1] }]);
  frecenciesByURL[urls[1]] = frecencyForUrl(urls[1]);
  Assert.ok(frecenciesByURL[urls[1]] > 0, "Sanity check");
  Assert.equal(PlacesUtils.history.frecencyMean,
               mean(Object.values(frecenciesByURL)));
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation,
               stddev(Object.values(frecenciesByURL)));

  // Add a URL 2 visit.
  await PlacesTestUtils.addVisits([{ uri: urls[2] }]);
  frecenciesByURL[urls[2]] = frecencyForUrl(urls[2]);
  Assert.ok(frecenciesByURL[urls[2]] > 0, "Sanity check");
  Assert.equal(PlacesUtils.history.frecencyMean,
               mean(Object.values(frecenciesByURL)));
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation,
               stddev(Object.values(frecenciesByURL)));

  // Add another URL 2 visit.
  await PlacesTestUtils.addVisits([{ uri: urls[2] }]);
  frecenciesByURL[urls[2]] = frecencyForUrl(urls[2]);
  Assert.ok(frecenciesByURL[urls[2]] > 0, "Sanity check");
  Assert.equal(PlacesUtils.history.frecencyMean,
               mean(Object.values(frecenciesByURL)));
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation,
               stddev(Object.values(frecenciesByURL)));

  // Remove URL 2's visits.
  await PlacesUtils.history.remove([urls[2]]);
  delete frecenciesByURL[urls[2]];
  Assert.equal(PlacesUtils.history.frecencyMean,
               mean(Object.values(frecenciesByURL)));
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation,
               stddev(Object.values(frecenciesByURL)));

  // Bookmark URL 1.
  let parentGuid =
    await PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid,
    title: "A bookmark",
    url: NetUtil.newURI(urls[1]),
  });
  await PlacesUtils.promiseItemId(bookmark.guid);

  frecenciesByURL[urls[1]] = frecencyForUrl(urls[1]);
  Assert.ok(frecenciesByURL[urls[1]] > 0, "Sanity check");
  Assert.equal(PlacesUtils.history.frecencyMean,
               mean(Object.values(frecenciesByURL)));
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation,
               stddev(Object.values(frecenciesByURL)));

  // Remove URL 1's visit.
  await PlacesUtils.history.remove([urls[1]]);
  frecenciesByURL[urls[1]] = frecencyForUrl(urls[1]);
  Assert.ok(frecenciesByURL[urls[1]] > 0, "Sanity check");
  Assert.equal(PlacesUtils.history.frecencyMean,
               mean(Object.values(frecenciesByURL)));
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation,
               stddev(Object.values(frecenciesByURL)));

  // Remove URL 1's bookmark.  Also need to call history.remove() again to
  // remove the URL from moz_places.  Otherwise it sticks around and keeps
  // contributing to the frecency stats.
  await PlacesUtils.bookmarks.remove(bookmark);
  await PlacesUtils.history.remove(urls[1]);
  delete frecenciesByURL[urls[1]];
  Assert.equal(PlacesUtils.history.frecencyMean,
               mean(Object.values(frecenciesByURL)));
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation,
               stddev(Object.values(frecenciesByURL)));

  // Remove URL 0.
  await PlacesUtils.history.remove([urls[0]]);
  delete frecenciesByURL[urls[0]];
  Assert.equal(PlacesUtils.history.frecencyMean, 0);
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation, 0);

  await cleanUp();
});


// Makes sure the prefs that store the stats are updated.
add_task(async function preferences() {
  Assert.equal(PlacesUtils.history.frecencyMean, 0);
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation, 0);

  let url = "http://example.com/";
  await PlacesTestUtils.addVisits([{ uri: url }]);
  let frecency = frecencyForUrl(url);
  Assert.ok(frecency > 0, "Sanity check");
  Assert.equal(PlacesUtils.history.frecencyMean, frecency);
  Assert.equal(PlacesUtils.history.frecencyStandardDeviation, 0);

  let expectedValuesByName = {
    "places.frecency.stats.count": "1",
    "places.frecency.stats.sum": String(frecency),
    "places.frecency.stats.sumOfSquares": String(frecency * frecency),
  };

  info("Waiting for preferences to be updated...");
  await TestUtils.topicObserved("places-frecency-stats-prefs-updated", () => {
    return Object.entries(expectedValuesByName).every(([name, expected]) => {
      let actual = Services.prefs.getCharPref(name, "");
      info(`${name} => ${actual} (expected=${expected})`);
      return actual == expected;
    });
  });
  Assert.ok(true, "Preferences updated as expected");

  await cleanUp();
});


function mean(values) {
  if (values.length == 0) {
    return 0;
  }
  return values.reduce((sum, value) => {
    sum += value;
    return sum;
  }, 0) / values.length;
}

function stddev(values) {
  if (values.length <= 1) {
    return 0;
  }
  let sum = values.reduce((memo, value) => {
    memo += value;
    return memo;
  }, 0);
  let sumOfSquares = values.reduce((memo, value) => {
    memo += value * value;
    return memo;
  }, 0);
  return Math.sqrt(
    (sumOfSquares - ((sum * sum) / values.length)) / values.length
  );
}

async function cleanUp() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
