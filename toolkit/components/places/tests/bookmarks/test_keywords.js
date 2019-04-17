/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");

const URI1 = "http://test1.mozilla.org/";
const URI2 = "http://test2.mozilla.org/";
const URI3 = "http://test3.mozilla.org/";

async function check_keyword(aURI, aKeyword) {
  if (aKeyword)
    aKeyword = aKeyword.toLowerCase();

  if (aKeyword) {
    let uri = await PlacesUtils.keywords.fetch(aKeyword);
    Assert.equal(uri.url, aURI);
    // Check case insensitivity.
    uri = await PlacesUtils.keywords.fetch(aKeyword.toUpperCase());
    Assert.equal(uri.url, aURI);
  } else {
    let entry = await PlacesUtils.keywords.fetch({ url: aURI });
    if (entry) {
      throw new Error(`${aURI.spec} should not have a keyword`);
    }
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
    },
  });
  PlacesUtils.bookmarks.addObserver(observer);
  return observer;
}

add_task(function test_invalid_input() {
});

add_task(async function test_addBookmarkAndKeyword() {
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function() {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  await check_keyword(URI1, null);
  let fc = await foreign_count(URI1);
  let observer = expectNotifications();

  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: URI1,
    title: "test",
  });
  await PlacesUtils.keywords.insert({url: URI1, keyword: "keyword"});
  let itemId = await PlacesUtils.promiseItemId(bookmark.guid);
  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "keyword", false, "keyword",
                                  bookmark.lastModified * 1000, bookmark.type,
                                  (await PlacesUtils.promiseItemId(bookmark.parentGuid)),
                                  bookmark.guid, bookmark.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                 ]);
  await check_keyword(URI1, "keyword");
  Assert.equal((await foreign_count(URI1)), fc + 2); // + 1 bookmark + 1 keyword
  await check_orphans();
});

add_task(async function test_addBookmarkToURIHavingKeyword() {
  // The uri has already a keyword.
  await check_keyword(URI1, "keyword");
  let fc = await foreign_count(URI1);

  let bookmark = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: URI1,
      title: "test",
    });
  await check_keyword(URI1, "keyword");
  Assert.equal((await foreign_count(URI1)), fc + 1); // + 1 bookmark
  await PlacesUtils.bookmarks.remove(bookmark);
  await check_orphans();
});

add_task(async function test_sameKeywordDifferentURI() {
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function() {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  let fc1 = await foreign_count(URI1);
  let fc2 = await foreign_count(URI2);
  let observer = expectNotifications();

  let bookmark2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: URI2,
    title: "test2",
  });
  await check_keyword(URI1, "keyword");
  await check_keyword(URI2, null);

  await PlacesUtils.keywords.insert({ url: URI2, keyword: "kEyWoRd" });

  let bookmark1 = await PlacesUtils.bookmarks.fetch({ url: URI1 });
  observer.check([ { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmark1.guid)),
                                  "keyword", false, "",
                                  bookmark1.lastModified * 1000, bookmark1.type,
                                  (await PlacesUtils.promiseItemId(bookmark1.parentGuid)),
                                  bookmark1.guid, bookmark1.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmark2.guid)),
                                  "keyword", false, "keyword",
                                  bookmark2.lastModified * 1000, bookmark2.type,
                                  (await PlacesUtils.promiseItemId(bookmark2.parentGuid)),
                                  bookmark2.guid, bookmark2.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                 ]);

  // The keyword should have been "moved" to the new URI.
  await check_keyword(URI1, null);
  Assert.equal((await foreign_count(URI1)), fc1 - 1); // - 1 keyword
  await check_keyword(URI2, "keyword");
  Assert.equal((await foreign_count(URI2)), fc2 + 2); // + 1 bookmark + 1 keyword
  await check_orphans();
});

add_task(async function test_sameURIDifferentKeyword() {
  let fc = await foreign_count(URI2);
  let observer = expectNotifications();

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: URI2,
    title: "test2",
  });
  await check_keyword(URI2, "keyword");

  await PlacesUtils.keywords.insert({ url: URI2, keyword: "keyword2" });
  let bookmarks = [];
  await PlacesUtils.bookmarks.fetch({ url: URI2 }, bm => bookmarks.push(bm));
  observer.check([ { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[0].guid)),
                                  "keyword", false, "keyword2",
                                  bookmarks[0].lastModified * 1000, bookmarks[0].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[0].parentGuid)),
                                  bookmarks[0].guid, bookmarks[0].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[1].guid)),
                                  "keyword", false, "keyword2",
                                  bookmarks[1].lastModified * 1000, bookmarks[1].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[1].parentGuid)),
                                  bookmarks[1].guid, bookmarks[1].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                 ]);
  await check_keyword(URI2, "keyword2");
  Assert.equal((await foreign_count(URI2)), fc + 1); // + 1 bookmark - 1 keyword + 1 keyword
  await check_orphans();
});

add_task(async function test_removeBookmarkWithKeyword() {
  let fc = await foreign_count(URI2);
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: URI2,
    title: "test",
  });

   // The keyword should not be removed, since there are other bookmarks yet.
   await PlacesUtils.bookmarks.remove(bookmark);

  await check_keyword(URI2, "keyword2");
  Assert.equal((await foreign_count(URI2)), fc); // + 1 bookmark - 1 bookmark
  await check_orphans();
});

add_task(async function test_unsetKeyword() {
  let fc = await foreign_count(URI2);
  let observer = expectNotifications();

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: URI2,
    title: "test",
  });

  // The keyword should be removed from any bookmark.
  await PlacesUtils.keywords.remove("keyword2");

  let bookmarks = [];
  await PlacesUtils.bookmarks.fetch({ url: URI2 }, bookmark => bookmarks.push(bookmark));
  Assert.equal(bookmarks.length, 3, "Check number of bookmarks");
  observer.check([ { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[0].guid)),
                                  "keyword", false, "",
                                  bookmarks[0].lastModified * 1000, bookmarks[0].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[0].parentGuid)),
                                  bookmarks[0].guid, bookmarks[0].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[1].guid)),
                                  "keyword", false, "",
                                  bookmarks[1].lastModified * 1000, bookmarks[1].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[1].parentGuid)),
                                  bookmarks[1].guid, bookmarks[1].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemChanged",
                     arguments: [ (await PlacesUtils.promiseItemId(bookmarks[2].guid)),
                                  "keyword", false, "",
                                  bookmarks[2].lastModified * 1000, bookmarks[2].type,
                                  (await PlacesUtils.promiseItemId(bookmarks[2].parentGuid)),
                                  bookmarks[2].guid, bookmarks[2].parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                 ]);

  await check_keyword(URI1, null);
  await check_keyword(URI2, null);
  Assert.equal((await foreign_count(URI2)), fc); // + 1 bookmark - 1 keyword
  await check_orphans();
});

add_task(async function test_addRemoveBookmark() {
  let fc = await foreign_count(URI3);
  let observer = expectNotifications();

  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: URI3,
    title: "test3",
  });
  let itemId = (await PlacesUtils.promiseItemId(bookmark.guid));
  await PlacesUtils.keywords.insert({ url: URI3, keyword: "keyword" });
  await PlacesUtils.bookmarks.remove(bookmark);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId,
                                  "keyword", false, "keyword",
                                  bookmark.lastModified * 1000, bookmark.type,
                                  (await PlacesUtils.promiseItemId(bookmark.parentGuid)),
                                  bookmark.guid, bookmark.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                 ]);

  await check_keyword(URI3, null);
  Assert.equal((await foreign_count(URI3)), fc); // +- 1 bookmark +- 1 keyword
  await check_orphans();
});

add_task(async function test_reassign() {
  // Should move keywords from old URL to new URL.
  info("Old URL with keywords; new URL without keywords");
  {
    let oldURL = "http://example.com/1/kw";
    let oldBmk = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: oldURL,
    });
    await PlacesUtils.keywords.insert({
      url: oldURL,
      keyword: "kw1-1",
      postData: "a=b",
    });
    await PlacesUtils.keywords.insert({
      url: oldURL,
      keyword: "kw1-2",
      postData: "c=d",
    });
    let oldFC = await foreign_count(oldURL);
    equal(oldFC, 3);

    let newURL = "http://example.com/2/no-kw";
    let newBmk = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: newURL,
    });
    let newFC = await foreign_count(newURL);
    equal(newFC, 1);

    let observer = expectNotifications();
    await PlacesUtils.keywords.reassign(oldURL, newURL);
    observer.check([ { name: "onItemChanged",
                       arguments: [ (await PlacesUtils.promiseItemId(oldBmk.guid)),
                                    "keyword", false, "",
                                    oldBmk.lastModified * 1000, oldBmk.type,
                                    (await PlacesUtils.promiseItemId(oldBmk.parentGuid)),
                                    oldBmk.guid, oldBmk.parentGuid, "",
                                    Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                     { name: "onItemChanged",
                       arguments: [ (await PlacesUtils.promiseItemId(newBmk.guid)),
                                    "keyword", false, "",
                                    newBmk.lastModified * 1000, newBmk.type,
                                    (await PlacesUtils.promiseItemId(newBmk.parentGuid)),
                                    newBmk.guid, newBmk.parentGuid, "",
                                    Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                     { name: "onItemChanged",
                       arguments: [ (await PlacesUtils.promiseItemId(newBmk.guid)),
                                    "keyword", false, "kw1-1",
                                    newBmk.lastModified * 1000, newBmk.type,
                                    (await PlacesUtils.promiseItemId(newBmk.parentGuid)),
                                    newBmk.guid, newBmk.parentGuid, "",
                                    Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                     { name: "onItemChanged",
                       arguments: [ (await PlacesUtils.promiseItemId(newBmk.guid)),
                                    "keyword", false, "kw1-2",
                                    newBmk.lastModified * 1000, newBmk.type,
                                    (await PlacesUtils.promiseItemId(newBmk.parentGuid)),
                                    newBmk.guid, newBmk.parentGuid, "",
                                    Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   ]);


    await check_keyword(oldURL, null);
    await check_keyword(newURL, "kw1-1");
    await check_keyword(newURL, "kw1-2");

    equal(await foreign_count(oldURL), oldFC - 2); // Removed both keywords.
    equal(await foreign_count(newURL), newFC + 2); // Added two keywords.
  }

  // Should not remove any keywords from new URL.
  info("Old URL without keywords; new URL with keywords");
  {
    let oldURL = "http://example.com/3/no-kw";
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: oldURL,
    });
    let oldFC = await foreign_count(oldURL);
    equal(oldFC, 1);

    let newURL = "http://example.com/4/kw";
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: newURL,
    });
    await PlacesUtils.keywords.insert({
      url: newURL,
      keyword: "kw4-1",
    });
    let newFC = await foreign_count(newURL);
    equal(newFC, 2);

    let observer = expectNotifications();
    await PlacesUtils.keywords.reassign(oldURL, newURL);
    observer.check([]);

    await check_keyword(newURL, "kw4-1");

    equal(await foreign_count(oldURL), oldFC);
    equal(await foreign_count(newURL), newFC);
  }

  // Should remove all keywords from new URL, then move keywords from old URL.
  info("Old URL with keywords; new URL with keywords");
  {
    let oldURL = "http://example.com/8/kw";
    let oldBmk = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: oldURL,
    });
    await PlacesUtils.keywords.insert({
      url: oldURL,
      keyword: "kw8-1",
      postData: "a=b",
    });
    await PlacesUtils.keywords.insert({
      url: oldURL,
      keyword: "kw8-2",
      postData: "c=d",
    });
    let oldFC = await foreign_count(oldURL);
    equal(oldFC, 3);

    let newURL = "http://example.com/9/kw";
    let newBmk = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: newURL,
    });
    await PlacesUtils.keywords.insert({
      url: newURL,
      keyword: "kw9-1",
    });
    let newFC = await foreign_count(newURL);
    equal(newFC, 2);

    let observer = expectNotifications();
    await PlacesUtils.keywords.reassign(oldURL, newURL);
    observer.check([ { name: "onItemChanged",
                       arguments: [ (await PlacesUtils.promiseItemId(oldBmk.guid)),
                                    "keyword", false, "",
                                    oldBmk.lastModified * 1000, oldBmk.type,
                                    (await PlacesUtils.promiseItemId(oldBmk.parentGuid)),
                                    oldBmk.guid, oldBmk.parentGuid, "",
                                    Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                     { name: "onItemChanged",
                       arguments: [ (await PlacesUtils.promiseItemId(newBmk.guid)),
                                    "keyword", false, "",
                                    newBmk.lastModified * 1000, newBmk.type,
                                    (await PlacesUtils.promiseItemId(newBmk.parentGuid)),
                                    newBmk.guid, newBmk.parentGuid, "",
                                    Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                     { name: "onItemChanged",
                       arguments: [ (await PlacesUtils.promiseItemId(newBmk.guid)),
                                    "keyword", false, "kw8-1",
                                    newBmk.lastModified * 1000, newBmk.type,
                                    (await PlacesUtils.promiseItemId(newBmk.parentGuid)),
                                    newBmk.guid, newBmk.parentGuid, "",
                                    Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                     { name: "onItemChanged",
                       arguments: [ (await PlacesUtils.promiseItemId(newBmk.guid)),
                                    "keyword", false, "kw8-2",
                                    newBmk.lastModified * 1000, newBmk.type,
                                    (await PlacesUtils.promiseItemId(newBmk.parentGuid)),
                                    newBmk.guid, newBmk.parentGuid, "",
                                    Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   ]);

    await check_keyword(oldURL, null);
    await check_keyword(newURL, "kw8-1");
    await check_keyword(newURL, "kw8-2");

    equal(await foreign_count(oldURL), oldFC - 2); // Removed both keywords.
    equal(await foreign_count(newURL), newFC + 1); // Removed old keyword; added two keywords.
  }

  // Should do nothing.
  info("Old URL without keywords; new URL without keywords");
  {
    let oldURL = "http://example.com/10/no-kw";
    let oldFC = await foreign_count(oldURL);

    let newURL = "http://example.com/11/no-kw";
    let newFC = await foreign_count(newURL);

    let observer = expectNotifications();
    await PlacesUtils.keywords.reassign(oldURL, newURL);
    observer.check([]);

    equal(await foreign_count(oldURL), oldFC);
    equal(await foreign_count(newURL), newFC);
  }

  await check_orphans();
});

add_task(async function test_invalidation() {
  info("Insert bookmarks");
  let fx = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "Get Firefox!",
    url: "http://getfirefox.com",
  });
  let tb = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "Get Thunderbird!",
    url: "http://getthunderbird.com",
  });

  info("Set keywords for bookmarks");
  await PlacesUtils.keywords.insert({ url: fx.url, keyword: "fx" });
  await PlacesUtils.keywords.insert({ url: tb.url, keyword: "tb" });

  info("Invalidate cached keywords");
  await PlacesUtils.keywords.invalidateCachedKeywords();

  info("Change URL of bookmark with keyword");
  let promiseNotification = PlacesTestUtils.waitForNotification(
    "onItemChanged",
    (id, prop, isAnnoProp, newValue, lastModified, type, parentId, guid) =>
      guid == fx.guid && prop == "keyword" && newValue == "fx"
  );
  await PlacesUtils.bookmarks.update({
    guid: fx.guid,
    url: "https://www.mozilla.org/firefox",
  });
  await promiseNotification;

  let entriesByKeyword = [];
  await PlacesUtils.keywords.fetch({ keyword: "fx" }, e => entriesByKeyword.push(e.url.href));
  deepEqual(entriesByKeyword, ["https://www.mozilla.org/firefox"],
    "Should return new URL for keyword");

  ok(!(await PlacesUtils.keywords.fetch({ url: "http://getfirefox.com" })),
    "Should not return keywords for old URL");

  let entiresByURL = [];
  await PlacesUtils.keywords.fetch({ url: "https://www.mozilla.org/firefox" },
    e => entiresByURL.push(e.keyword));
  deepEqual(entiresByURL, ["fx"], "Should return keyword for new URL");

  info("Invalidate cached keywords");
  await PlacesUtils.keywords.invalidateCachedKeywords();

  info("Remove bookmark with keyword");
  await PlacesUtils.bookmarks.remove(tb.guid);

  ok(!(await PlacesUtils.keywords.fetch({ url: "http://getthunderbird.com" })),
    "Should not return keywords for removed bookmark URL");

  ok(!(await PlacesUtils.keywords.fetch({ keyword: "tb" })),
    "Should not return URL for removed bookmark keyword");
  await check_orphans();

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_eraseAllBookmarks() {
  info("Insert bookmarks");
  let fx = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "Get Firefox!",
    url: "http://getfirefox.com",
  });
  let tb = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "Get Thunderbird!",
    url: "http://getthunderbird.com",
  });

  info("Set keywords for bookmarks");
  await PlacesUtils.keywords.insert({ url: fx.url, keyword: "fx" });
  await PlacesUtils.keywords.insert({ url: tb.url, keyword: "tb" });

  info("Erase everything");
  await PlacesUtils.bookmarks.eraseEverything();

  ok(!(await PlacesUtils.keywords.fetch({ keyword: "fx" })),
    "Should remove Firefox keyword");

  ok(!(await PlacesUtils.keywords.fetch({ keyword: "tb" })),
    "Should remove Thunderbird keyword");
});
