/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function init() {
  await cleanUp();
});


// Adds/removes some visits and bookmarks and makes sure the stats are updated.
add_task(async function basic() {
  await checkStats([]);

  let frecenciesByURL = {};
  let urls = [0, 1, 2].map(i => "http://example.com/" + i);

  // Add a URL 0 visit.
  await PlacesTestUtils.addVisits([{ uri: urls[0] }]);
  frecenciesByURL[urls[0]] = frecencyForUrl(urls[0]);
  Assert.ok(frecenciesByURL[urls[0]] > 0, "Sanity check");

  await checkStats(frecenciesByURL);

  // Add a URL 1 visit.
  await PlacesTestUtils.addVisits([{ uri: urls[1] }]);
  frecenciesByURL[urls[1]] = frecencyForUrl(urls[1]);
  Assert.ok(frecenciesByURL[urls[1]] > 0, "Sanity check");

  await checkStats(frecenciesByURL);

  // Add a URL 2 visit.
  await PlacesTestUtils.addVisits([{ uri: urls[2] }]);
  frecenciesByURL[urls[2]] = frecencyForUrl(urls[2]);
  Assert.ok(frecenciesByURL[urls[2]] > 0, "Sanity check");

  await checkStats(frecenciesByURL);

  // Add another URL 2 visit.
  await PlacesTestUtils.addVisits([{ uri: urls[2] }]);
  frecenciesByURL[urls[2]] = frecencyForUrl(urls[2]);
  Assert.ok(frecenciesByURL[urls[2]] > 0, "Sanity check");

  await checkStats(frecenciesByURL);

  // Remove URL 2's visits.
  await PlacesUtils.history.remove([urls[2]]);
  delete frecenciesByURL[urls[2]];

  await checkStats(frecenciesByURL);

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

  await checkStats(frecenciesByURL);

  // Remove URL 1's visit.
  await PlacesUtils.history.remove([urls[1]]);
  frecenciesByURL[urls[1]] = frecencyForUrl(urls[1]);
  Assert.ok(frecenciesByURL[urls[1]] > 0, "Sanity check");

  await checkStats(frecenciesByURL);

  // Remove URL 1's bookmark.  Also need to call history.remove() again to
  // remove the URL from moz_places.  Otherwise it sticks around and keeps
  // contributing to the frecency stats.
  await PlacesUtils.bookmarks.remove(bookmark);
  await PlacesUtils.history.remove(urls[1]);
  delete frecenciesByURL[urls[1]];

  await checkStats(frecenciesByURL);

  // Remove URL 0.
  await PlacesUtils.history.remove([urls[0]]);
  delete frecenciesByURL[urls[0]];

  await checkStats(frecenciesByURL);

  await cleanUp();
});


async function checkStats(frecenciesByURL) {
  let stats = await promiseStats();
  let fs = Object.values(frecenciesByURL);
  Assert.equal(stats.count, fs.length);
  Assert.equal(stats.sum, fs.reduce((memo, f) => memo + f, 0));
  Assert.equal(stats.squares, fs.reduce((memo, f) => memo + (f * f), 0));
}

async function promiseStats() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT
      IFNULL((SELECT value FROM moz_meta WHERE key = "frecency_count"), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = "frecency_sum"), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = "frecency_sum_of_squares"), 0)
  `);
  return {
    count: rows[0].getResultByIndex(0),
    sum: rows[0].getResultByIndex(1),
    squares: rows[0].getResultByIndex(2),
  };
}

async function cleanUp() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
