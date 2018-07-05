/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function promiseAllURLFrecencies() {
  let frecencies = new Map();
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT url, frecency
    FROM moz_places
    WHERE url_hash BETWEEN hash('http', 'prefix_lo') AND
                           hash('http', 'prefix_hi')`);
  for (let row of rows) {
    let url = row.getResultByName("url");
    let frecency = row.getResultByName("frecency");
    frecencies.set(url, frecency);
  }
  return frecencies;
}

function mapFilterIterator(iter, fn) {
  let results = [];
  for (let value of iter) {
    let newValue = fn(value);
    if (newValue) {
      results.push(newValue);
    }
  }
  return results;
}

add_task(async function test_update_frecencies() {
  let buf = await openMirror("update_frecencies");

  info("Make local changes");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      // Not modified in mirror; shouldn't recalculate frecency.
      guid: "bookmarkAAAA",
      title: "A",
      url: "http://example.com/a",
    }, {
      // URL changed to B1 in mirror; should recalculate frecency for B
      // and B1, using existing frecency to determine order.
      guid: "bookmarkBBBB",
      title: "B",
      url: "http://example.com/b",
    }, {
      // URL changed to new URL in mirror, should recalculate frecency
      // for new URL first, before B1.
      guid: "bookmarkBBB1",
      title: "B1",
      url: "http://example.com/b1",
    }, {
      // Query; shouldn't recalculate frecency.
      guid: "queryCCCCCCC",
      title: "C",
      url: "place:type=6&sort=14&maxResults=10",
    }],
  });

  info("Calculate frecencies for all local URLs");
  await PlacesUtils.withConnectionWrapper("Update all frecencies",
    async function(db) {
      await db.execute(`UPDATE moz_places SET
        frecency = CALCULATE_FRECENCY(id)`);
    }
  );

  info("Make remote changes");
  await storeRecords(buf, [{
    // Existing bookmark changed to existing URL.
    id: "bookmarkBBBB",
    type: "bookmark",
    title: "B",
    bmkUri: "http://example.com/b1",
  }, {
    // Existing bookmark with new URL; should recalculate frecency first.
    id: "bookmarkBBB1",
    type: "bookmark",
    title: "B1",
    bmkUri: "http://example.com/b11",
  }, {
    id: "bookmarkBBB2",
    type: "bookmark",
    title: "B2",
    bmkUri: "http://example.com/b",
  }, {
    // New bookmark with new URL; should recalculate frecency first.
    id: "bookmarkDDDD",
    type: "bookmark",
    title: "D",
    bmkUri: "http://example.com/d",
  }, {
    // New bookmark with new URL.
    id: "bookmarkEEEE",
    type: "bookmark",
    title: "E",
    bmkUri: "http://example.com/e",
  }, {
    // New query; shouldn't count against limit.
    id: "queryFFFFFFF",
    type: "query",
    title: "F",
    bmkUri: `place:parent=${PlacesUtils.bookmarks.menuGuid}`,
  }]);

  info("Apply new items and recalculate 3 frecencies");
  await buf.apply({
    maxFrecenciesToRecalculate: 3,
  });

  {
    let frecencies = await promiseAllURLFrecencies();
    let urlsWithFrecency = mapFilterIterator(frecencies.entries(),
      ([href, frecency]) => frecency > 0 ? href : null);

    // A is unchanged, and we should recalculate frecency for three more
    // random URLs.
    equal(urlsWithFrecency.length, 4,
      "Should keep unchanged frecency and recalculate 3");
    let unexpectedURLs = CommonUtils.difference(urlsWithFrecency, new Set([
      // A is unchanged.
      "http://example.com/a",

      // B11, D, and E are new URLs.
      "http://example.com/b11",
      "http://example.com/d",
      "http://example.com/e",

      // B and B1 are existing, changed URLs.
      "http://example.com/b",
      "http://example.com/b1",
    ]));
    ok(!unexpectedURLs.size,
      "Should recalculate frecency for new and changed URLs only");
  }

  info("Change non-URL property of D");
  await storeRecords(buf, [{
    id: "bookmarkDDDD",
    type: "bookmark",
    title: "D (remote)",
    bmkUri: "http://example.com/d",
  }]);

  info("Apply new item and recalculate remaining frecencies");
  await buf.apply();

  {
    let frecencies = await promiseAllURLFrecencies();
    let urlsWithoutFrecency = mapFilterIterator(frecencies.entries(),
      ([href, frecency]) => frecency <= 0 ? href : null);
    deepEqual(urlsWithoutFrecency, [],
      "Should finish calculating remaining frecencies");
  }
});
