/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests functionality of the mozIAsyncLivemarks interface.

const FEED_URI = NetUtil.newURI("http://feed.rss/");
const SITE_URI = NetUtil.newURI("http://site.org/");

// This test must be the first one, since it's testing the cache.
add_task(function* test_livemark_cache() {
  // Add a livemark through other APIs.
  let folder = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "test",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid
  });
  let id = yield PlacesUtils.promiseItemId(folder.guid);
  PlacesUtils.annotations
             .setItemAnnotation(id, PlacesUtils.LMANNO_FEEDURI,
                                "http://example.com/feed",
                                0, PlacesUtils.annotations.EXPIRE_NEVER);
  PlacesUtils.annotations
             .setItemAnnotation(id, PlacesUtils.LMANNO_SITEURI,
                                "http://example.com/site",
                                0, PlacesUtils.annotations.EXPIRE_NEVER);

  let livemark = yield PlacesUtils.livemarks.getLivemark({ guid: folder.guid });
  Assert.equal(folder.guid, livemark.guid);
  Assert.equal(folder.dateAdded * 1000, livemark.dateAdded);
  Assert.equal(folder.parentGuid, livemark.parentGuid);
  Assert.equal(folder.index, livemark.index);
  Assert.equal(folder.title, livemark.title);
  Assert.equal(id, livemark.id);
  Assert.equal(PlacesUtils.unfiledBookmarksFolderId, livemark.parentId);
  Assert.equal("http://example.com/feed", livemark.feedURI.spec);
  Assert.equal("http://example.com/site", livemark.siteURI.spec);

  yield PlacesUtils.livemarks.removeLivemark(livemark);
});

add_task(function* test_addLivemark_noArguments_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark();
    do_throw("Invoking addLivemark with no arguments should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_XPC_NOT_ENOUGH_ARGS);
  }
});

add_task(function* test_addLivemark_emptyObject_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark({});
    do_throw("Invoking addLivemark with empty object should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_badParentId_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark({ parentId: "test" });
    do_throw("Invoking addLivemark with a bad parent id should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_invalidParentId_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark({ parentId: -2 });
    do_throw("Invoking addLivemark with an invalid parent id should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_noIndex_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark({
      parentId: PlacesUtils.unfiledBookmarksFolderId });
    do_throw("Invoking addLivemark with no index should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_badIndex_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark(
      { parentId: PlacesUtils.unfiledBookmarksFolderId
      , index: "test" });
    do_throw("Invoking addLivemark with a bad index should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_invalidIndex_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark(
      { parentId: PlacesUtils.unfiledBookmarksFolderId
      , index: -2
      });
    do_throw("Invoking addLivemark with an invalid index should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_noFeedURI_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark(
      { parentGuid: PlacesUtils.bookmarks.unfiledGuid });
    do_throw("Invoking addLivemark with no feedURI should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_badFeedURI_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark(
      { parentGuid: PlacesUtils.bookmarks.unfiledGuid
      , feedURI: "test" });
    do_throw("Invoking addLivemark with a bad feedURI should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_badSiteURI_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark(
      { parentGuid: PlacesUtils.bookmarks.unfiledGuid
      , feedURI: FEED_URI
      , siteURI: "test" });
    do_throw("Invoking addLivemark with a bad siteURI should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_badGuid_throws() {
  try {
    yield PlacesUtils.livemarks.addLivemark(
      { parentGuid: PlacesUtils.bookmarks.unfileGuid
      , feedURI: FEED_URI
      , guid: "123456" });
    do_throw("Invoking addLivemark with a bad guid should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_parentId_succeeds() {
  let onItemAddedCalled = false;
  PlacesUtils.bookmarks.addObserver({
    __proto__: NavBookmarkObserver.prototype,
    onItemAdded: function onItemAdded(aItemId, aParentId, aIndex, aItemType,
                                      aURI, aTitle)
    {
      onItemAddedCalled = true;
      PlacesUtils.bookmarks.removeObserver(this);
      do_check_eq(aParentId, PlacesUtils.unfiledBookmarksFolderId);
      do_check_eq(aIndex, 0);
      do_check_eq(aItemType, Ci.nsINavBookmarksService.TYPE_FOLDER);
      do_check_eq(aTitle, "test");
    }
  }, false);

  yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentId: PlacesUtils.unfiledBookmarksFolderId
    , feedURI: FEED_URI });
  do_check_true(onItemAddedCalled);
});


add_task(function* test_addLivemark_noSiteURI_succeeds() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    });
  do_check_true(livemark.id > 0);
  do_check_valid_places_guid(livemark.guid);
  do_check_eq(livemark.title, "test");
  do_check_eq(livemark.parentId, PlacesUtils.unfiledBookmarksFolderId);
  do_check_eq(livemark.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  do_check_true(livemark.feedURI.equals(FEED_URI));
  do_check_eq(livemark.siteURI, null);
  do_check_true(livemark.lastModified > 0);
  do_check_true(is_time_ordered(livemark.dateAdded, livemark.lastModified));

  let bookmark = yield PlacesUtils.bookmarks.fetch(livemark.guid);
  do_check_eq(livemark.index, bookmark.index);
  do_check_eq(livemark.dateAdded, bookmark.dateAdded * 1000);
});

add_task(function* test_addLivemark_succeeds() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    , siteURI: SITE_URI
    });

  do_check_true(livemark.id > 0);
  do_check_valid_places_guid(livemark.guid);
  do_check_eq(livemark.title, "test");
  do_check_eq(livemark.parentId, PlacesUtils.unfiledBookmarksFolderId);
  do_check_eq(livemark.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  do_check_true(livemark.feedURI.equals(FEED_URI));
  do_check_true(livemark.siteURI.equals(SITE_URI));
  do_check_true(PlacesUtils.annotations
                           .itemHasAnnotation(livemark.id,
                                              PlacesUtils.LMANNO_FEEDURI));
  do_check_true(PlacesUtils.annotations
                           .itemHasAnnotation(livemark.id,
                                              PlacesUtils.LMANNO_SITEURI));
});

add_task(function* test_addLivemark_bogusid_succeeds() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { id: 100 // Should be ignored.
    , title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    , siteURI: SITE_URI
    });
  do_check_true(livemark.id > 0);
  do_check_neq(livemark.id, 100);
});

add_task(function* test_addLivemark_bogusParentId_fails() {
  try {
    yield PlacesUtils.livemarks.addLivemark(
      { title: "test"
      , parentId: 187
      , feedURI: FEED_URI
      });
    do_throw("Adding a livemark with a bogus parent should fail");
  } catch(ex) {}
});

add_task(function* test_addLivemark_bogusParentGuid_fails() {
  try {
    yield PlacesUtils.livemarks.addLivemark(
      { title: "test"
      , parentGuid: "123456789012"
      , feedURI: FEED_URI
      });
    do_throw("Adding a livemark with a bogus parent should fail");
  } catch(ex) {}
})

add_task(function* test_addLivemark_intoLivemark_fails() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    });

  try {
    yield PlacesUtils.livemarks.addLivemark(
      { title: "test"
      , parentGuid: livemark.guid
      , feedURI: FEED_URI
      });
    do_throw("Adding a livemark into a livemark should fail");
  } catch(ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_addLivemark_forceGuid_succeeds() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    , guid: "1234567890AB"
    });
    do_check_eq(livemark.guid, "1234567890AB");
    do_check_guid_for_bookmark(livemark.id, "1234567890AB");
});

add_task(function* test_addLivemark_dateAdded_succeeds() {
  let dateAdded = new Date("2013-03-01T01:10:00") * 1000;
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    , dateAdded
    });
  do_check_eq(livemark.dateAdded, dateAdded);
});

add_task(function* test_addLivemark_lastModified_succeeds() {
  let now = Date.now() * 1000;
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    , lastModified: now
    });
  do_check_eq(livemark.dateAdded, now);
  do_check_eq(livemark.lastModified, now);
});

add_task(function* test_removeLivemark_emptyObject_throws() {
  try {
    yield PlacesUtils.livemarks.removeLivemark({});
    do_throw("Invoking removeLivemark with empty object should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_removeLivemark_noValidId_throws() {
  try {
    yield PlacesUtils.livemarks.removeLivemark({ id: -10, guid: "test"});
    do_throw("Invoking removeLivemark with no valid id should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_removeLivemark_nonExistent_fails() {
  try {
    yield PlacesUtils.livemarks.removeLivemark({ id: 1337 });
    do_throw("Removing a non-existent livemark should fail");
  }
  catch(ex) {
  }
});

add_task(function* test_removeLivemark_guid_succeeds() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    , guid: "234567890ABC"
  });

  do_check_eq(livemark.guid, "234567890ABC");

  yield PlacesUtils.livemarks.removeLivemark({
    id: 789, guid: "234567890ABC"
  });

  do_check_eq((yield PlacesUtils.bookmarks.fetch("234567890ABC")), null);
});

add_task(function* test_removeLivemark_id_succeeds() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
  });

  yield PlacesUtils.livemarks.removeLivemark({ id: livemark.id });

  do_check_eq((yield PlacesUtils.bookmarks.fetch("234567890ABC")), null);
});

add_task(function* test_getLivemark_emptyObject_throws() {
  try {
    yield PlacesUtils.livemarks.getLivemark({});
    do_throw("Invoking getLivemark with empty object should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_getLivemark_noValidId_throws() {
  try {
    yield PlacesUtils.livemarks.getLivemark({ id: -10, guid: "test"});
    do_throw("Invoking getLivemark with no valid id should throw");
  } catch (ex) {
    do_check_eq(ex.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_getLivemark_nonExistentId_fails() {
  try {
    yield PlacesUtils.livemarks.getLivemark({ id: 1234 });
    do_throw("getLivemark for a non existent id should fail");
  } catch (ex) {}
});

add_task(function* test_getLivemark_nonExistentGUID_fails() {
  try {
    yield PlacesUtils.livemarks.getLivemark({ guid: "34567890ABCD" });
    do_throw("getLivemark for a non-existent guid should fail");
  } catch (ex) {}
});

add_task(function* test_getLivemark_guid_succeeds() {
  yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    , guid: "34567890ABCD" });

  // invalid id to check the guid wins.
  let livemark =
    yield PlacesUtils.livemarks.getLivemark({ id: 789, guid: "34567890ABCD" });

  do_check_eq(livemark.title, "test");
  do_check_eq(livemark.parentId, PlacesUtils.unfiledBookmarksFolderId);
  do_check_eq(livemark.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  do_check_true(livemark.feedURI.equals(FEED_URI));
  do_check_eq(livemark.siteURI, null);
  do_check_eq(livemark.guid, "34567890ABCD");

  let bookmark = yield PlacesUtils.bookmarks.fetch("34567890ABCD");
  do_check_eq(livemark.index, bookmark.index);
});

add_task(function* test_getLivemark_id_succeeds() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    });

  livemark = yield PlacesUtils.livemarks.getLivemark({ id: livemark.id });

  do_check_eq(livemark.title, "test");
  do_check_eq(livemark.parentId, PlacesUtils.unfiledBookmarksFolderId);
  do_check_eq(livemark.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  do_check_true(livemark.feedURI.equals(FEED_URI));
  do_check_eq(livemark.siteURI, null);
  do_check_guid_for_bookmark(livemark.id, livemark.guid);

  let bookmark = yield PlacesUtils.bookmarks.fetch(livemark.guid);
  do_check_eq(livemark.index, bookmark.index);
});

add_task(function* test_getLivemark_removeItem_contention() {
  // do not yield.
  PlacesUtils.livemarks.addLivemark({ title: "test"
                                    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
                                    , feedURI: FEED_URI
                                    });
  yield PlacesUtils.bookmarks.eraseEverything();
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    });

  livemark = yield PlacesUtils.livemarks.getLivemark({ guid: livemark.guid });

  do_check_eq(livemark.title, "test");
  do_check_eq(livemark.parentId, PlacesUtils.unfiledBookmarksFolderId);
  do_check_true(livemark.feedURI.equals(FEED_URI));
  do_check_eq(livemark.siteURI, null);
  do_check_guid_for_bookmark(livemark.id, livemark.guid);
});

add_task(function* test_title_change() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI
    });

  yield PlacesUtils.bookmarks.update({ guid: livemark.guid,
                                       title: "test2" });
  // Poll for the title change.
  while (true) {
    let lm = yield PlacesUtils.livemarks.getLivemark({ guid: livemark.guid });
    if (lm.title == "test2")
      break;
    yield new Promise(resolve => do_timeout(resolve, 100));
  }
});

add_task(function* test_livemark_move() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI } );

  yield PlacesUtils.bookmarks.update({ guid: livemark.guid,
                                       parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                       index: PlacesUtils.bookmarks.DEFAULT_INDEX });
  // Poll for the parent change.
  while (true) {
    let lm = yield PlacesUtils.livemarks.getLivemark({ guid: livemark.guid });
    if (lm.parentGuid == PlacesUtils.bookmarks.toolbarGuid)
      break;
    yield new Promise(resolve => do_timeout(resolve, 100));
  }
});

add_task(function* test_livemark_removed() {
  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "test"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , feedURI: FEED_URI } );

  yield PlacesUtils.bookmarks.remove(livemark.guid);
  // Poll for the livemark removal.
  while (true) {
    try {
      yield PlacesUtils.livemarks.getLivemark({ guid: livemark.guid });
    } catch (ex) {
      break;
    }
    yield new Promise(resolve => do_timeout(resolve, 100));
  }
});
