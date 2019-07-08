/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  // Check particular unicode urls with insertion and selection APIs to ensure
  // url hashes match properly.
  const URLS = [
    "http://президент.президент/президент/",
    "https://www.аррӏе.com/аррӏе/",
    "http://名がドメイン/",
  ];

  for (let url of URLS) {
    await PlacesTestUtils.addVisits(url);
    Assert.ok(await PlacesUtils.history.fetch(url), "Found the added visit");
    await PlacesUtils.bookmarks.insert({
      url,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    });
    Assert.ok(
      await PlacesUtils.bookmarks.fetch({ url }),
      "Found the added bookmark"
    );
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute(
      "SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url",
      { url: new URL(url).href }
    );
    Assert.equal(rows.length, 1, "Matched the place from the database");
    let id = rows[0].getResultByName("id");

    // Now, suppose the urls has been inserted without proper parsing and retry.
    // This should normally not happen through the API, but we have evidence
    // it somehow happened.
    await PlacesUtils.withConnectionWrapper("test_hash.js", async wdb => {
      await wdb.execute(
        `
        UPDATE moz_places SET url_hash = hash(:url), url = :url
        WHERE id = :id
      `,
        { url, id }
      );
      rows = await wdb.execute(
        "SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url",
        { url }
      );
      Assert.equal(rows.length, 1, "Matched the place from the database");
    });
  }
});
