add_task(async function invalid_input_throws() {
  Assert.throws(
    () => PlacesUtils.bookmarks.getRecent(),
    /numberOfItems argument is required/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.getRecent("abc"),
    /numberOfItems argument must be an integer/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.getRecent(1.2),
    /numberOfItems argument must be an integer/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.getRecent(0),
    /numberOfItems argument must be greater than zero/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.getRecent(-1),
    /numberOfItems argument must be greater than zero/
  );
});

add_task(async function getRecent_returns_recent_bookmarks() {
  await PlacesUtils.bookmarks.eraseEverything();

  let bm1 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/",
    title: "a bookmark",
  });
  let bm2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.org/path",
    title: "another bookmark",
  });
  let bm3 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.net/",
    title: "another bookmark",
  });
  let bm4 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.net/path",
    title: "yet another bookmark",
  });
  checkBookmarkObject(bm1);
  checkBookmarkObject(bm2);
  checkBookmarkObject(bm3);
  checkBookmarkObject(bm4);

  // Add a tag to the most recent url to prove it doesn't get returned.
  PlacesUtils.tagging.tagURI(uri(bm4.url), ["Test Tag"]);

  // Add a separator.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
  });

  // Add a query bookmark.
  let queryURL = `place:parent=${PlacesUtils.bookmarks.menuGuid}&queryType=1`;
  let bm5 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: queryURL,
    title: "a test query",
  });
  checkBookmarkObject(bm5);

  // Verify that getRecent only returns actual bookmarks.
  let results = await PlacesUtils.bookmarks.getRecent(100);
  Assert.equal(
    results.length,
    4,
    "The expected number of bookmarks was returned."
  );
  checkBookmarkObject(results[0]);
  Assert.deepEqual(
    bm4,
    results[0],
    "The first result is the expected bookmark."
  );
  checkBookmarkObject(results[1]);
  Assert.deepEqual(
    bm3,
    results[1],
    "The second result is the expected bookmark."
  );
  checkBookmarkObject(results[2]);
  Assert.deepEqual(
    bm2,
    results[2],
    "The third result is the expected bookmark."
  );
  checkBookmarkObject(results[3]);
  Assert.deepEqual(
    bm1,
    results[3],
    "The fourth result is the expected bookmark."
  );

  // Verify that getRecent utilizes the numberOfItems argument.
  results = await PlacesUtils.bookmarks.getRecent(1);
  Assert.equal(
    results.length,
    1,
    "The expected number of bookmarks was returned."
  );
  checkBookmarkObject(results[0]);
  Assert.deepEqual(
    bm4,
    results[0],
    "The first result is the expected bookmark."
  );

  await PlacesUtils.bookmarks.eraseEverything();
});
