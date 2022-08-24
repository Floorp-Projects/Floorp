/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_fetchMany() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  let pages = [
    {
      url: "https://mozilla.org/test1/",
      title: "test 1",
    },
    {
      url: "https://mozilla.org/test2/",
      title: "test 2",
    },
    {
      url: "https://mozilla.org/test3/",
      title: "test 3",
    },
  ];
  await PlacesTestUtils.addVisits(pages);

  // Add missing page info from the database.
  for (let page of pages) {
    page.guid = await PlacesTestUtils.fieldInDB(page.url, "guid");
    page.frecency = await PlacesTestUtils.fieldInDB(page.url, "frecency");
  }

  info("Fetch by url");
  let fetched = await PlacesUtils.history.fetchMany(pages.map(p => p.url));
  Assert.equal(fetched.size, 3, "Map should contain same number of entries");
  for (let page of pages) {
    Assert.deepEqual(page, fetched.get(page.url));
  }
  info("Fetch by GUID");
  fetched = await PlacesUtils.history.fetchMany(pages.map(p => p.guid));
  Assert.equal(fetched.size, 3, "Map should contain same number of entries");
  for (let page of pages) {
    Assert.deepEqual(page, fetched.get(page.guid));
  }
  info("Fetch mixed");
  let keys = pages.map((p, i) => (i % 2 == 0 ? p.guid : p.url));
  fetched = await PlacesUtils.history.fetchMany(keys);
  Assert.equal(fetched.size, 3, "Map should contain same number of entries");
  for (let key of keys) {
    let page = pages.find(p => p.guid == key || p.url == key);
    Assert.deepEqual(page, fetched.get(key));
    Assert.ok(URL.isInstance(fetched.get(key).url));
  }
});

add_task(async function test_fetch_empty() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  let fetched = await PlacesUtils.history.fetchMany([]);
  Assert.equal(fetched.size, 0, "Map should contain no entries");
});

add_task(async function test_fetch_nonexistent() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  let uri = NetUtil.newURI("http://doesntexist.in.db");
  let fetched = await PlacesUtils.history.fetchMany([uri]);
  Assert.equal(fetched.size, 0, "Map should contain no entries");
});

add_task(async function test_error_cases() {
  Assert.throws(
    () => PlacesUtils.history.fetchMany("3"),
    /TypeError: Input is not an array/
  );
  Assert.throws(
    () => PlacesUtils.history.fetchMany([{ not: "a valid string or guid" }]),
    /TypeError: Invalid url or guid/
  );
  Assert.throws(
    () =>
      PlacesUtils.history.fetchMany(["http://valid.uri.com", "not an object"]),
    /TypeError: URL constructor/
  );
  Assert.throws(
    () => PlacesUtils.history.fetchMany(["http://valid.uri.com", null]),
    /TypeError: Invalid url or guid/
  );
});
