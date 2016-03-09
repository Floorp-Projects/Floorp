add_task(function* invalid_input_throws() {
  Assert.throws(() => PlacesUtils.bookmarks.getRecent(),
                /numberOfItems argument is required/);
  Assert.throws(() => PlacesUtils.bookmarks.getRecent("abc"),
                /numberOfItems argument must be an integer/);
  Assert.throws(() => PlacesUtils.bookmarks.getRecent(1.2),
                /numberOfItems argument must be an integer/);
  Assert.throws(() => PlacesUtils.bookmarks.getRecent(0),
                /numberOfItems argument must be greater than zero/);
  Assert.throws(() => PlacesUtils.bookmarks.getRecent(-1),
                /numberOfItems argument must be greater than zero/);
});

add_task(function* getRecent_returns_recent_bookmarks() {
  yield PlacesUtils.bookmarks.eraseEverything();

  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  let bm2 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.org/path",
                                                 title: "another bookmark" });
  let bm3 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.net/",
                                                 title: "another bookmark" });
  let bm4 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.net/path",
                                                 title: "yet another bookmark" });
  checkBookmarkObject(bm1);
  checkBookmarkObject(bm2);
  checkBookmarkObject(bm3);
  checkBookmarkObject(bm4);

  let results = yield PlacesUtils.bookmarks.getRecent(3);
  Assert.equal(results.length, 3);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm4, results[0]);
  checkBookmarkObject(results[1]);
  Assert.deepEqual(bm3, results[1]);
  checkBookmarkObject(results[2]);
  Assert.deepEqual(bm2, results[2]);

  yield PlacesUtils.bookmarks.eraseEverything();
});
