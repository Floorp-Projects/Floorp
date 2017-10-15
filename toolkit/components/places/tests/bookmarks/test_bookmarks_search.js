add_task(async function invalid_input_throws() {
  Assert.throws(() => PlacesUtils.bookmarks.search(),
                /Query object is required/);
  Assert.throws(() => PlacesUtils.bookmarks.search(null),
                /Query object is required/);
  Assert.throws(() => PlacesUtils.bookmarks.search({title: 50}),
                /Title option must be a string/);
  Assert.throws(() => PlacesUtils.bookmarks.search({url: {url: "wombat"}}),
                /Url option must be a string or a URL object/);
  Assert.throws(() => PlacesUtils.bookmarks.search(50),
                /Query must be an object or a string/);
  Assert.throws(() => PlacesUtils.bookmarks.search(true),
                /Query must be an object or a string/);
});

add_task(async function search_bookmark() {
  await PlacesUtils.bookmarks.eraseEverything();

  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  let bm2 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.org/",
                                                 title: "another bookmark" });
  let bm3 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.menuGuid,
                                                 url: "http://menu.org/",
                                                 title: "an on-menu bookmark" });
  let bm4 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                                 url: "http://toolbar.org/",
                                                 title: "an on-toolbar bookmark" });
  checkBookmarkObject(bm1);
  checkBookmarkObject(bm2);
  checkBookmarkObject(bm3);
  checkBookmarkObject(bm4);

  // finds a result by query
  let results = await PlacesUtils.bookmarks.search("example.com");
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm1, results[0]);

  // finds multiple results
  results = await PlacesUtils.bookmarks.search("example");
  Assert.equal(results.length, 2);
  checkBookmarkObject(results[0]);
  checkBookmarkObject(results[1]);

  // finds menu bookmarks
  results = await PlacesUtils.bookmarks.search("an on-menu bookmark");
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm3, results[0]);

  // finds toolbar bookmarks
  results = await PlacesUtils.bookmarks.search("an on-toolbar bookmark");
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm4, results[0]);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function search_bookmark_by_query_object() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  let bm2 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.org/",
                                                 title: "another bookmark" });
  checkBookmarkObject(bm1);
  checkBookmarkObject(bm2);

  let results = await PlacesUtils.bookmarks.search({query: "example.com"});
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);

  Assert.deepEqual(bm1, results[0]);

  results = await PlacesUtils.bookmarks.search({query: "example"});
  Assert.equal(results.length, 2);
  checkBookmarkObject(results[0]);
  checkBookmarkObject(results[1]);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function search_bookmark_by_url() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  let bm2 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.org/path",
                                                 title: "another bookmark" });
  let bm3 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.org/path",
                                                 title: "third bookmark" });
  checkBookmarkObject(bm1);
  checkBookmarkObject(bm2);
  checkBookmarkObject(bm3);

  // finds the correct result by url
  let results = await PlacesUtils.bookmarks.search({url: "http://example.com/"});
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm1, results[0]);

  // normalizes the url
  results = await PlacesUtils.bookmarks.search({url: "http:/example.com"});
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm1, results[0]);

  // returns multiple matches
  results = await PlacesUtils.bookmarks.search({url: "http://example.org/path"});
  Assert.equal(results.length, 2);

  // requires exact match
  results = await PlacesUtils.bookmarks.search({url: "http://example.org/"});
  Assert.equal(results.length, 0);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function search_bookmark_by_title() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  let bm2 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.org/path",
                                                 title: "another bookmark" });
  let bm3 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.net/",
                                                 title: "another bookmark" });
  checkBookmarkObject(bm1);
  checkBookmarkObject(bm2);
  checkBookmarkObject(bm3);

  // finds the correct result by title
  let results = await PlacesUtils.bookmarks.search({title: "a bookmark"});
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm1, results[0]);

  // returns multiple matches
  results = await PlacesUtils.bookmarks.search({title: "another bookmark"});
  Assert.equal(results.length, 2);

  // requires exact match
  results = await PlacesUtils.bookmarks.search({title: "bookmark"});
  Assert.equal(results.length, 0);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function search_bookmark_combinations() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  let bm2 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.org/path",
                                                 title: "another bookmark" });
  let bm3 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.net/",
                                                 title: "third bookmark" });
  checkBookmarkObject(bm1);
  checkBookmarkObject(bm2);
  checkBookmarkObject(bm3);

  // finds the correct result if title and url match
  let results = await PlacesUtils.bookmarks.search({url: "http://example.com/", title: "a bookmark"});
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm1, results[0]);

  // does not match if query is not matching but url and title match
  results = await PlacesUtils.bookmarks.search({url: "http://example.com/", title: "a bookmark", query: "nonexistent"});
  Assert.equal(results.length, 0);

  // does not match if one parameter is not matching
  results = await PlacesUtils.bookmarks.search({url: "http://what.ever", title: "a bookmark"});
  Assert.equal(results.length, 0);

  // query only matches if other fields match as well
  results = await PlacesUtils.bookmarks.search({query: "bookmark", url: "http://example.net/"});
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm3, results[0]);

  // non-matching query will also return no results
  results = await PlacesUtils.bookmarks.search({query: "nonexistent", url: "http://example.net/"});
  Assert.equal(results.length, 0);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function search_folder() {
  let folder = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.menuGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                 title: "a test folder" });
  let bm = await PlacesUtils.bookmarks.insert({ parentGuid: folder.guid,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(folder);
  checkBookmarkObject(bm);

  // also finds folders
  let results = await PlacesUtils.bookmarks.search("a test folder");
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.equal(folder.title, results[0].title);
  Assert.equal(folder.type, results[0].type);
  Assert.equal(folder.parentGuid, results[0].parentGuid);

  // finds elements in folders
  results = await PlacesUtils.bookmarks.search("example.com");
  Assert.equal(results.length, 1);
  checkBookmarkObject(results[0]);
  Assert.deepEqual(bm, results[0]);
  Assert.equal(folder.guid, results[0].parentGuid);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function search_includes_separators() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  let bm2 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_SEPARATOR });

  checkBookmarkObject(bm1);
  checkBookmarkObject(bm2);

  let results = await PlacesUtils.bookmarks.search({});
  Assert.ok(results.findIndex(bookmark => { return bookmark.guid == bm1.guid; }) > -1,
            "The bookmark was found in the results.");
  Assert.ok(results.findIndex(bookmark => { return bookmark.guid == bm2.guid; }) > -1,
            "The separator was included in the results.");

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function search_excludes_tags() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);
  PlacesUtils.tagging.tagURI(uri(bm1.url.href), ["Test Tag"]);

  let results = await PlacesUtils.bookmarks.search("example.com");
  // If tags are not being excluded, this would return two results, one representing the tag.
  Assert.equal(1, results.length, "A single object was returned from search.");
  Assert.deepEqual(bm1, results[0], "The bookmark was returned.");

  results = await PlacesUtils.bookmarks.search("Test Tag");
  Assert.equal(0, results.length, "The tag folder was not returned.");

  await PlacesUtils.bookmarks.eraseEverything();
});
