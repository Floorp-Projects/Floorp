const URI1 = NetUtil.newURI("http://test1.mozilla.org/");
const URI2 = NetUtil.newURI("http://test2.mozilla.org/");
const URI3 = NetUtil.newURI("http://test3.mozilla.org/");

function check_keyword(aURI, aKeyword) {
  if (aKeyword)
    aKeyword = aKeyword.toLowerCase();

  for (let bm of PlacesUtils.getBookmarksForURI(aURI)) {
    let keyword = PlacesUtils.bookmarks.getKeywordForBookmark(bm);
    if (keyword && !aKeyword) {
      throw (`${aURI.spec} should not have a keyword`);
    } else if (aKeyword && keyword == aKeyword) {
      Assert.equal(keyword, aKeyword);
    }
  }

  if (aKeyword) {
    let uri = PlacesUtils.bookmarks.getURIForKeyword(aKeyword);
    Assert.equal(uri.spec, aURI.spec);
    // Check case insensitivity.
    uri = PlacesUtils.bookmarks.getURIForKeyword(aKeyword.toUpperCase());
    Assert.equal(uri.spec, aURI.spec);
  }
}

function* check_orphans() {
  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.executeCached(
    `SELECT id FROM moz_keywords k
     WHERE NOT EXISTS (SELECT 1 FROM moz_places WHERE id = k.place_id)
    `);
  Assert.equal(rows.length, 0);
}

function expectNotifications() {
  let notifications = [];
  let observer = new Proxy(NavBookmarkObserver, {
    get(target, name) {
      if (name == "check") {
        PlacesUtils.bookmarks.removeObserver(observer);
        return expectedNotifications =>
          Assert.deepEqual(notifications, expectedNotifications);
      }

      if (name.startsWith("onItemChanged")) {
        return function(id, prop, isAnno, val, lastMod, itemType, parentId, guid, parentGuid, oldVal) {
          if (prop != "keyword")
            return;
          let args = Array.from(arguments, arg => {
            if (arg && arg instanceof Ci.nsIURI)
              return new URL(arg.spec);
            if (arg && typeof(arg) == "number" && arg >= Date.now() * 1000)
              return new Date(parseInt(arg / 1000));
            return arg;
          });
          notifications.push({ name: name, arguments: args });
        }
      }

      return target[name];
    }
  });
  PlacesUtils.bookmarks.addObserver(observer, false);
  return observer;
}

add_task(function test_invalid_input() {
  Assert.throws(() => PlacesUtils.bookmarks.getURIForKeyword(null),
                /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => PlacesUtils.bookmarks.getURIForKeyword(""),
                /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => PlacesUtils.bookmarks.getKeywordForBookmark(null),
                /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => PlacesUtils.bookmarks.getKeywordForBookmark(0),
                /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => PlacesUtils.bookmarks.setKeywordForBookmark(null, "k"),
                /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => PlacesUtils.bookmarks.setKeywordForBookmark(0, "k"),
                /NS_ERROR_ILLEGAL_VALUE/);
});

add_task(function* test_addBookmarkAndKeyword() {
  check_keyword(URI1, null);
  let fc = yield foreign_count(URI1);
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI1,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");
  let bookmark = yield PlacesUtils.bookmarks.fetch({ url: URI1 });
  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "keyword", false, "keyword",
                                  bookmark.lastModified, bookmark.type,
                                  (yield PlacesUtils.promiseItemId(bookmark.parentGuid)),
                                  bookmark.guid, bookmark.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
  yield PlacesTestUtils.promiseAsyncUpdates();

  check_keyword(URI1, "keyword");
  Assert.equal((yield foreign_count(URI1)), fc + 2); // + 1 bookmark + 1 keyword

  yield PlacesTestUtils.promiseAsyncUpdates();
  yield check_orphans();
});

add_task(function* test_addBookmarkToURIHavingKeyword() {
  // The uri has already a keyword.
  check_keyword(URI1, "keyword");
  let fc = yield foreign_count(URI1);

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI1,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");
  check_keyword(URI1, "keyword");
  Assert.equal((yield foreign_count(URI1)), fc + 1); // + 1 bookmark

  PlacesUtils.bookmarks.removeItem(itemId);
  yield PlacesTestUtils.promiseAsyncUpdates();
  check_orphans();
});

add_task(function* test_sameKeywordDifferentURI() {
  let fc1 = yield foreign_count(URI1);
  let fc2 = yield foreign_count(URI2);
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI2,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test2");
  check_keyword(URI1, "keyword");
  check_keyword(URI2, null);

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "kEyWoRd");

  let bookmark1 = yield PlacesUtils.bookmarks.fetch({ url: URI1 });
  let bookmark2 = yield PlacesUtils.bookmarks.fetch({ url: URI2 });
  observer.check([ { name: "onItemChanged",
                     arguments: [ (yield PlacesUtils.promiseItemId(bookmark1.guid)),
                                  "keyword", false, "",
                                  bookmark1.lastModified, bookmark1.type,
                                  (yield PlacesUtils.promiseItemId(bookmark1.parentGuid)),
                                  bookmark1.guid, bookmark1.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ itemId, "keyword", false, "keyword",
                                  bookmark2.lastModified, bookmark2.type,
                                  (yield PlacesUtils.promiseItemId(bookmark2.parentGuid)),
                                  bookmark2.guid, bookmark2.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
  yield PlacesTestUtils.promiseAsyncUpdates();

  // The keyword should have been "moved" to the new URI.
  check_keyword(URI1, null);
  Assert.equal((yield foreign_count(URI1)), fc1 - 1); // - 1 keyword
  check_keyword(URI2, "keyword");
  Assert.equal((yield foreign_count(URI2)), fc2 + 2); // + 1 bookmark + 1 keyword

  yield PlacesTestUtils.promiseAsyncUpdates();
  check_orphans();
});

add_task(function* test_sameURIDifferentKeyword() {
  let fc = yield foreign_count(URI2);
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI2,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test2");
  check_keyword(URI2, "keyword");

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword2");

  let bookmarks = [];
  yield PlacesUtils.bookmarks.fetch({ url: URI2 }, bookmark => bookmarks.push(bookmark));
  observer.check([ { name: "onItemChanged",
                     arguments: [ (yield PlacesUtils.promiseItemId(bookmarks[0].guid)),
                                  "keyword", false, "keyword2",
                                  bookmarks[0].lastModified, bookmarks[0].type,
                                  (yield PlacesUtils.promiseItemId(bookmarks[0].parentGuid)),
                                  bookmarks[0].guid, bookmarks[0].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (yield PlacesUtils.promiseItemId(bookmarks[1].guid)),
                                  "keyword", false, "keyword2",
                                  bookmarks[1].lastModified, bookmarks[1].type,
                                  (yield PlacesUtils.promiseItemId(bookmarks[1].parentGuid)),
                                  bookmarks[1].guid, bookmarks[1].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
  yield PlacesTestUtils.promiseAsyncUpdates();

  check_keyword(URI2, "keyword2");
  Assert.equal((yield foreign_count(URI2)), fc + 2); // + 1 bookmark + 1 keyword

  yield PlacesTestUtils.promiseAsyncUpdates();
  check_orphans();
});

add_task(function* test_removeBookmarkWithKeyword() {
  let fc = yield foreign_count(URI2);
  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI2,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");

   // The keyword should not be removed, since there are other bookmarks yet.
   PlacesUtils.bookmarks.removeItem(itemId);

  check_keyword(URI2, "keyword2");
  Assert.equal((yield foreign_count(URI2)), fc); // + 1 bookmark - 1 bookmark

  yield PlacesTestUtils.promiseAsyncUpdates();
  check_orphans();
});

add_task(function* test_unsetKeyword() {
  let fc = yield foreign_count(URI2);
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI2,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");

  // The keyword should be removed from any bookmark.
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, null);

  let bookmarks = [];
  yield PlacesUtils.bookmarks.fetch({ url: URI2 }, bookmark => bookmarks.push(bookmark));
  do_print(bookmarks.length);
  observer.check([ { name: "onItemChanged",
                     arguments: [ (yield PlacesUtils.promiseItemId(bookmarks[0].guid)),
                                  "keyword", false, "",
                                  bookmarks[0].lastModified, bookmarks[0].type,
                                  (yield PlacesUtils.promiseItemId(bookmarks[0].parentGuid)),
                                  bookmarks[0].guid, bookmarks[0].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (yield PlacesUtils.promiseItemId(bookmarks[1].guid)),
                                  "keyword", false, "",
                                  bookmarks[1].lastModified, bookmarks[1].type,
                                  (yield PlacesUtils.promiseItemId(bookmarks[1].parentGuid)),
                                  bookmarks[1].guid, bookmarks[1].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (yield PlacesUtils.promiseItemId(bookmarks[2].guid)),
                                  "keyword", false, "",
                                  bookmarks[2].lastModified, bookmarks[2].type,
                                  (yield PlacesUtils.promiseItemId(bookmarks[2].parentGuid)),
                                  bookmarks[2].guid, bookmarks[2].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);

  check_keyword(URI1, null);
  check_keyword(URI2, null);
  Assert.equal((yield foreign_count(URI2)), fc - 1); // + 1 bookmark - 2 keyword

  yield PlacesTestUtils.promiseAsyncUpdates();
  check_orphans();
});

add_task(function* test_addRemoveBookmark() {
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI3,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test3");

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");
  let bookmark = yield PlacesUtils.bookmarks.fetch({ url: URI3 });
  let parentId = yield PlacesUtils.promiseItemId(bookmark.parentGuid);
  PlacesUtils.bookmarks.removeItem(itemId);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId,
                                  "keyword", false, "keyword",
                                  bookmark.lastModified, bookmark.type,
                                  parentId,
                                  bookmark.guid, bookmark.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);

  check_keyword(URI3, null);
  // Don't check the foreign count since the process is async.
  // The new test_keywords.js in unit is checking this though.

  yield PlacesTestUtils.promiseAsyncUpdates();
  check_orphans();
});

function run_test() {
  run_next_test();
}
