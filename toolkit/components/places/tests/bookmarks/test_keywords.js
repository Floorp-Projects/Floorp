const URI1 = NetUtil.newURI("http://test1.mozilla.org/");
const URI2 = NetUtil.newURI("http://test2.mozilla.org/");
const URI3 = NetUtil.newURI("http://test3.mozilla.org/");

async function check_keyword(aURI, aKeyword) {
  if (aKeyword)
    aKeyword = aKeyword.toLowerCase();

  let bms = [];
  await PlacesUtils.bookmarks.fetch({ url: aURI }, bm => bms.push(bm));
  for (let bm of bms) {
    let itemId = await PlacesUtils.promiseItemId(bm.guid);
    let keyword = PlacesUtils.bookmarks.getKeywordForBookmark(itemId);
    if (keyword && !aKeyword) {
      throw (`${aURI.spec} should not have a keyword`);
    } else if (aKeyword && keyword == aKeyword) {
      Assert.equal(keyword, aKeyword);
    }
  }

  if (aKeyword) {
    let uri = await PlacesUtils.keywords.fetch(aKeyword);
    Assert.equal(uri.url, aURI.spec);
    // Check case insensitivity.
    uri = await PlacesUtils.keywords.fetch(aKeyword.toUpperCase());
    Assert.equal(uri.url, aURI.spec);
  }
}

async function check_orphans() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.executeCached(
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
          notifications.push({ name, arguments: args });
        };
      }

      return target[name];
    }
  });
  PlacesUtils.bookmarks.addObserver(observer);
  return observer;
}

add_task(function test_invalid_input() {
  Assert.throws(() => PlacesUtils.bookmarks.getKeywordForBookmark(null),
                /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => PlacesUtils.bookmarks.getKeywordForBookmark(0),
                /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => PlacesUtils.bookmarks.setKeywordForBookmark(null, "k"),
                /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => PlacesUtils.bookmarks.setKeywordForBookmark(0, "k"),
                /NS_ERROR_ILLEGAL_VALUE/);
});

add_task(async function test_addBookmarkAndKeyword() {
  await check_keyword(URI1, null);
  let fc = await foreign_count(URI1);
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI1,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");
  let bookmark = await PlacesUtils.bookmarks.fetch({ url: URI1 });
  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "keyword", false, "keyword",
                                  bookmark.lastModified * 1000, bookmark.type,
                                  (await PlacesUtils.promiseItemId(bookmark.parentGuid)),
                                  bookmark.guid, bookmark.parentGuid,
                                  bookmark.url.href,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
  await PlacesTestUtils.promiseAsyncUpdates();

  await check_keyword(URI1, "keyword");
  Assert.equal((await foreign_count(URI1)), fc + 2); // + 1 bookmark + 1 keyword

  await PlacesTestUtils.promiseAsyncUpdates();
  await check_orphans();
});

add_task(async function test_addBookmarkToURIHavingKeyword() {
  // The uri has already a keyword.
  await check_keyword(URI1, "keyword");
  let fc = await foreign_count(URI1);

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI1,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");
  await check_keyword(URI1, "keyword");
  Assert.equal((await foreign_count(URI1)), fc + 1); // + 1 bookmark

  PlacesUtils.bookmarks.removeItem(itemId);
  await PlacesTestUtils.promiseAsyncUpdates();
  await check_orphans();
});

add_task(async function test_sameKeywordDifferentURI() {
  let fc1 = await foreign_count(URI1);
  let fc2 = await foreign_count(URI2);
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI2,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test2");
  await check_keyword(URI1, "keyword");
  await check_keyword(URI2, null);

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "kEyWoRd");

  let bookmark1 = await PlacesUtils.bookmarks.fetch({ url: URI1 });
  let bookmark2 = await PlacesUtils.bookmarks.fetch({ url: URI2 });
  observer.check([ { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmark1.guid)),
                                  "keyword", false, "",
                                  bookmark1.lastModified * 1000, bookmark1.type,
                                  (await PlacesUtils.promiseItemId(bookmark1.parentGuid)),
                                  bookmark1.guid, bookmark1.parentGuid,
                                  bookmark1.url.href,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ itemId, "keyword", false, "keyword",
                                  bookmark2.lastModified * 1000, bookmark2.type,
                                  (await PlacesUtils.promiseItemId(bookmark2.parentGuid)),
                                  bookmark2.guid, bookmark2.parentGuid,
                                  bookmark2.url.href,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
  await PlacesTestUtils.promiseAsyncUpdates();

  // The keyword should have been "moved" to the new URI.
  await check_keyword(URI1, null);
  Assert.equal((await foreign_count(URI1)), fc1 - 1); // - 1 keyword
  await check_keyword(URI2, "keyword");
  Assert.equal((await foreign_count(URI2)), fc2 + 2); // + 1 bookmark + 1 keyword

  await PlacesTestUtils.promiseAsyncUpdates();
  await check_orphans();
});

add_task(async function test_sameURIDifferentKeyword() {
  let fc = await foreign_count(URI2);
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI2,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test2");
  await check_keyword(URI2, "keyword");

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword2");

  let bookmarks = [];
  await PlacesUtils.bookmarks.fetch({ url: URI2 }, bookmark => bookmarks.push(bookmark));
  observer.check([ { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[0].guid)),
                                  "keyword", false, "keyword2",
                                  bookmarks[0].lastModified * 1000, bookmarks[0].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[0].parentGuid)),
                                  bookmarks[0].guid, bookmarks[0].parentGuid,
                                  bookmarks[0].url.href,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[1].guid)),
                                  "keyword", false, "keyword2",
                                  bookmarks[1].lastModified * 1000, bookmarks[1].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[1].parentGuid)),
                                  bookmarks[1].guid, bookmarks[1].parentGuid,
                                  bookmarks[0].url.href,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
  await PlacesTestUtils.promiseAsyncUpdates();

  await check_keyword(URI2, "keyword2");
  Assert.equal((await foreign_count(URI2)), fc + 2); // + 1 bookmark + 1 keyword

  await PlacesTestUtils.promiseAsyncUpdates();
  await check_orphans();
});

add_task(async function test_removeBookmarkWithKeyword() {
  let fc = await foreign_count(URI2);
  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI2,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");

   // The keyword should not be removed, since there are other bookmarks yet.
   PlacesUtils.bookmarks.removeItem(itemId);

  await check_keyword(URI2, "keyword2");
  Assert.equal((await foreign_count(URI2)), fc); // + 1 bookmark - 1 bookmark

  await PlacesTestUtils.promiseAsyncUpdates();
  await check_orphans();
});

add_task(async function test_unsetKeyword() {
  let fc = await foreign_count(URI2);
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI2,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");

  // The keyword should be removed from any bookmark.
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, null);

  let bookmarks = [];
  await PlacesUtils.bookmarks.fetch({ url: URI2 }, bookmark => bookmarks.push(bookmark));
  do_print(bookmarks.length);
  observer.check([ { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[0].guid)),
                                  "keyword", false, "",
                                  bookmarks[0].lastModified * 1000, bookmarks[0].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[0].parentGuid)),
                                  bookmarks[0].guid, bookmarks[0].parentGuid,
                                  bookmarks[0].url.href,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[1].guid)),
                                  "keyword", false, "",
                                  bookmarks[1].lastModified * 1000, bookmarks[1].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[1].parentGuid)),
                                  bookmarks[1].guid, bookmarks[1].parentGuid,
                                  bookmarks[1].url.href,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[2].guid)),
                                  "keyword", false, "",
                                  bookmarks[2].lastModified * 1000, bookmarks[2].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[2].parentGuid)),
                                  bookmarks[2].guid, bookmarks[2].parentGuid,
                                  bookmarks[2].url.href,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);

  await check_keyword(URI1, null);
  await check_keyword(URI2, null);
  Assert.equal((await foreign_count(URI2)), fc - 1); // + 1 bookmark - 2 keyword

  await PlacesTestUtils.promiseAsyncUpdates();
  await check_orphans();
});

add_task(async function test_addRemoveBookmark() {
  let observer = expectNotifications();

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URI3,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test3");

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");
  let bookmark = await PlacesUtils.bookmarks.fetch({ url: URI3 });
  let parentId = await PlacesUtils.promiseItemId(bookmark.parentGuid);
  PlacesUtils.bookmarks.removeItem(itemId);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId,
                                  "keyword", false, "keyword",
                                  bookmark.lastModified * 1000, bookmark.type,
                                  parentId,
                                  bookmark.guid, bookmark.parentGuid,
                                  bookmark.url.href,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);

  await check_keyword(URI3, null);
  // Don't check the foreign count since the process is async.
  // The new test_keywords.js in unit is checking this though.

  await PlacesTestUtils.promiseAsyncUpdates();
  await check_orphans();
});
