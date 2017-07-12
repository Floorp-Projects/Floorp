"use strict"

async function check_keyword(aExpectExists, aHref, aKeyword, aPostData = null) {
  // Check case-insensitivity.
  aKeyword = aKeyword.toUpperCase();

  let entry = await PlacesUtils.keywords.fetch(aKeyword);

  Assert.deepEqual(entry, await PlacesUtils.keywords.fetch({ keyword: aKeyword }));

  if (aExpectExists) {
    Assert.ok(!!entry, "A keyword should exist");
    Assert.equal(entry.url.href, aHref);
    Assert.equal(entry.postData, aPostData);
    Assert.deepEqual(entry, await PlacesUtils.keywords.fetch({ keyword: aKeyword, url: aHref }));
    let entries = [];
    await PlacesUtils.keywords.fetch({ url: aHref }, e => entries.push(e));
    Assert.ok(entries.some(e => e.url.href == aHref && e.keyword == aKeyword.toLowerCase()));
  } else {
    Assert.ok(!entry || entry.url.href != aHref,
              "The given keyword entry should not exist");
    if (aHref) {
      Assert.equal(null, await PlacesUtils.keywords.fetch({ keyword: aKeyword, url: aHref }));
    } else {
      Assert.equal(null, await PlacesUtils.keywords.fetch({ keyword: aKeyword }));
    }
  }
}

/**
 * Polls the keywords cache waiting for the given keyword entry.
 */
async function promiseKeyword(keyword, expectedHref) {
  let href = null;
  do {
    await new Promise(resolve => do_timeout(100, resolve));
    let entry = await PlacesUtils.keywords.fetch(keyword);
    if (entry)
      href = entry.url.href;
  } while (href != expectedHref);
}

async function check_no_orphans() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.executeCached(
    `SELECT id FROM moz_keywords k
     WHERE NOT EXISTS (SELECT 1 FROM moz_places WHERE id = k.place_id)
    `);
  Assert.equal(rows.length, 0);
}

function expectBookmarkNotifications() {
  let notifications = [];
  let observer = new Proxy(NavBookmarkObserver, {
    get(target, name) {
      if (name == "check") {
        PlacesUtils.bookmarks.removeObserver(observer);
        return expectedNotifications =>
          Assert.deepEqual(notifications, expectedNotifications);
      }

      if (name.startsWith("onItemChanged")) {
        return function(itemId, property) {
          if (property != "keyword")
            return;
          let args = Array.from(arguments, arg => {
            if (arg && arg instanceof Ci.nsIURI)
              return new URL(arg.spec);
            if (arg && typeof(arg) == "number" && arg >= Date.now() * 1000)
              return new Date(parseInt(arg / 1000));
            return arg;
          });
          notifications.push({ name, arguments: args });
        }
      }

      if (name in target)
        return target[name];
      return undefined;
    }
  });
  PlacesUtils.bookmarks.addObserver(observer);
  return observer;
}

add_task(async function test_invalid_input() {
  Assert.throws(() => PlacesUtils.keywords.fetch(null),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.fetch(5),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.fetch(undefined),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.fetch({ keyword: null }),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.fetch({ keyword: {} }),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.fetch({ keyword: 5 }),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.fetch({}),
                /At least keyword or url must be provided/);
  Assert.throws(() => PlacesUtils.keywords.fetch({ keyword: "test" }, "test"),
                /onResult callback must be a valid function/);
  Assert.throws(() => PlacesUtils.keywords.fetch({ url: "test" }),
                /is not a valid URL/);
  Assert.throws(() => PlacesUtils.keywords.fetch({ url: {} }),
                /is not a valid URL/);
  Assert.throws(() => PlacesUtils.keywords.fetch({ url: null }),
                /is not a valid URL/);
  Assert.throws(() => PlacesUtils.keywords.fetch({ url: "" }),
                /is not a valid URL/);

  Assert.throws(() => PlacesUtils.keywords.insert(null),
                /Input should be a valid object/);
  Assert.throws(() => PlacesUtils.keywords.insert("test"),
                /Input should be a valid object/);
  Assert.throws(() => PlacesUtils.keywords.insert(undefined),
                /Input should be a valid object/);
  Assert.throws(() => PlacesUtils.keywords.insert({ }),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: null }),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: 5 }),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: "" }),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: "test", postData: 5 }),
                /Invalid POST data/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: "test", postData: {} }),
                /Invalid POST data/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: "test" }),
                /is not a valid URL/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: "test", url: 5 }),
                /is not a valid URL/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: "test", url: "" }),
                /is not a valid URL/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: "test", url: null }),
                /is not a valid URL/);
  Assert.throws(() => PlacesUtils.keywords.insert({ keyword: "test", url: "mozilla" }),
                /is not a valid URL/);

  Assert.throws(() => PlacesUtils.keywords.remove(null),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.remove(""),
                /Invalid keyword/);
  Assert.throws(() => PlacesUtils.keywords.remove(5),
                /Invalid keyword/);
});

add_task(async function test_addKeyword() {
  await check_keyword(false, "http://example.com/", "keyword");
  let fc = await foreign_count("http://example.com/");
  let observer = expectBookmarkNotifications();

  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example.com/" });
  observer.check([]);

  await check_keyword(true, "http://example.com/", "keyword");
  Assert.equal((await foreign_count("http://example.com/")), fc + 1); // +1 keyword

  // Now remove the keyword.
  observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.remove("keyword");
  observer.check([]);

  await check_keyword(false, "http://example.com/", "keyword");
  Assert.equal((await foreign_count("http://example.com/")), fc); // -1 keyword

  // Check using URL.
  await PlacesUtils.keywords.insert({ keyword: "keyword", url: new URL("http://example.com/") });
  await check_keyword(true, "http://example.com/", "keyword");
  await PlacesUtils.keywords.remove("keyword");
  await check_keyword(false, "http://example.com/", "keyword");

  await check_no_orphans();
});

add_task(async function test_addBookmarkAndKeyword() {
  await check_keyword(false, "http://example.com/", "keyword");
  let fc = await foreign_count("http://example.com/");
  let bookmark = await PlacesUtils.bookmarks.insert({ url: "http://example.com/",
                                                      parentGuid: PlacesUtils.bookmarks.unfiledGuid });

  let observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example.com/" });

  observer.check([{ name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark.guid)),
                                 "keyword", false, "keyword",
                                 bookmark.lastModified * 1000, bookmark.type,
                                 (await PlacesUtils.promiseItemId(bookmark.parentGuid)),
                                 bookmark.guid, bookmark.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] } ]);

  await check_keyword(true, "http://example.com/", "keyword");
  Assert.equal((await foreign_count("http://example.com/")), fc + 2); // +1 bookmark +1 keyword

  // Now remove the keyword.
  observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.remove("keyword");

  observer.check([{ name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark.guid)),
                                 "keyword", false, "",
                                 bookmark.lastModified * 1000, bookmark.type,
                                 (await PlacesUtils.promiseItemId(bookmark.parentGuid)),
                                 bookmark.guid, bookmark.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] } ]);

  await check_keyword(false, "http://example.com/", "keyword");
  Assert.equal((await foreign_count("http://example.com/")), fc + 1); // -1 keyword

  // Add again the keyword, then remove the bookmark.
  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example.com/" });

  observer = expectBookmarkNotifications();
  await PlacesUtils.bookmarks.remove(bookmark.guid);
  // the notification is synchronous but the removal process is async.
  // Unfortunately there's nothing explicit we can wait for.
  while ((await foreign_count("http://example.com/")));
  // We don't get any itemChanged notification since the bookmark has been
  // removed already.
  observer.check([]);

  await check_keyword(false, "http://example.com/", "keyword");

  await check_no_orphans();
});

add_task(async function test_addKeywordToURIHavingKeyword() {
  await check_keyword(false, "http://example.com/", "keyword");
  let fc = await foreign_count("http://example.com/");

  let observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example.com/" });
  observer.check([]);

  await check_keyword(true, "http://example.com/", "keyword");
  Assert.equal((await foreign_count("http://example.com/")), fc + 1); // +1 keyword

  await PlacesUtils.keywords.insert({ keyword: "keyword2", url: "http://example.com/" });

  await check_keyword(true, "http://example.com/", "keyword");
  await check_keyword(true, "http://example.com/", "keyword2");
  Assert.equal((await foreign_count("http://example.com/")), fc + 2); // +1 keyword
  let entries = [];
  let entry = await PlacesUtils.keywords.fetch({ url: "http://example.com/" }, e => entries.push(e));
  Assert.equal(entries.length, 2);
  Assert.deepEqual(entries[0], entry);

  // Now remove the keywords.
  observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.remove("keyword");
  await PlacesUtils.keywords.remove("keyword2");
  observer.check([]);

  await check_keyword(false, "http://example.com/", "keyword");
  await check_keyword(false, "http://example.com/", "keyword2");
  Assert.equal((await foreign_count("http://example.com/")), fc); // -1 keyword

  await check_no_orphans();
});

add_task(async function test_addBookmarkToURIHavingKeyword() {
  await check_keyword(false, "http://example.com/", "keyword");
  let fc = await foreign_count("http://example.com/");
  let observer = expectBookmarkNotifications();

  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example.com/" });
  observer.check([]);

  await check_keyword(true, "http://example.com/", "keyword");
  Assert.equal((await foreign_count("http://example.com/")), fc + 1); // +1 keyword

  observer = expectBookmarkNotifications();
  let bookmark = await PlacesUtils.bookmarks.insert({ url: "http://example.com/",
                                                      parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  Assert.equal((await foreign_count("http://example.com/")), fc + 2); // +1 bookmark
  observer.check([]);

  observer = expectBookmarkNotifications();
  await PlacesUtils.bookmarks.remove(bookmark.guid);
  // the notification is synchronous but the removal process is async.
  // Unfortunately there's nothing explicit we can wait for.
  while ((await foreign_count("http://example.com/")));
  // We don't get any itemChanged notification since the bookmark has been
  // removed already.
  observer.check([]);

  await check_keyword(false, "http://example.com/", "keyword");

  await check_no_orphans();
});

add_task(async function test_sameKeywordDifferentURL() {
  let fc1 = await foreign_count("http://example1.com/");
  let bookmark1 = await PlacesUtils.bookmarks.insert({ url: "http://example1.com/",
                                                       parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let fc2 = await foreign_count("http://example2.com/");
  let bookmark2 = await PlacesUtils.bookmarks.insert({ url: "http://example2.com/",
                                                       parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example1.com/" });

  await check_keyword(true, "http://example1.com/", "keyword");
  Assert.equal((await foreign_count("http://example1.com/")), fc1 + 2); // +1 bookmark +1 keyword
  await check_keyword(false, "http://example2.com/", "keyword");
  Assert.equal((await foreign_count("http://example2.com/")), fc2 + 1); // +1 bookmark

  // Assign the same keyword to another url.
  let observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example2.com/" });

  observer.check([{ name: "onItemChanged",
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
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] } ]);

  await check_keyword(false, "http://example1.com/", "keyword");
  Assert.equal((await foreign_count("http://example1.com/")), fc1 + 1); // -1 keyword
  await check_keyword(true, "http://example2.com/", "keyword");
  Assert.equal((await foreign_count("http://example2.com/")), fc2 + 2); // +1 keyword

  // Now remove the keyword.
  observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.remove("keyword");
  observer.check([{ name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark2.guid)),
                                 "keyword", false, "",
                                 bookmark2.lastModified * 1000, bookmark2.type,
                                 (await PlacesUtils.promiseItemId(bookmark2.parentGuid)),
                                 bookmark2.guid, bookmark2.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] } ]);

  await check_keyword(false, "http://example1.com/", "keyword");
  await check_keyword(false, "http://example2.com/", "keyword");
  Assert.equal((await foreign_count("http://example1.com/")), fc1 + 1);
  Assert.equal((await foreign_count("http://example2.com/")), fc2 + 1); // -1 keyword

  await PlacesUtils.bookmarks.remove(bookmark1);
  await PlacesUtils.bookmarks.remove(bookmark2);
  Assert.equal((await foreign_count("http://example1.com/")), fc1); // -1 bookmark
  while ((await foreign_count("http://example2.com/"))); // -1 keyword

  await check_no_orphans();
});

add_task(async function test_sameURIDifferentKeyword() {
  let fc = await foreign_count("http://example.com/");

  let observer = expectBookmarkNotifications();
  let bookmark = await PlacesUtils.bookmarks.insert({ url: "http://example.com/",
                                                      parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  await PlacesUtils.keywords.insert({keyword: "keyword", url: "http://example.com/" });

  await check_keyword(true, "http://example.com/", "keyword");
  Assert.equal((await foreign_count("http://example.com/")), fc + 2); // +1 bookmark +1 keyword

  observer.check([{ name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark.guid)),
                                 "keyword", false, "keyword",
                                 bookmark.lastModified * 1000, bookmark.type,
                                 (await PlacesUtils.promiseItemId(bookmark.parentGuid)),
                                 bookmark.guid, bookmark.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] } ]);

  observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.insert({ keyword: "keyword2", url: "http://example.com/" });
  await check_keyword(true, "http://example.com/", "keyword");
  await check_keyword(true, "http://example.com/", "keyword2");
  Assert.equal((await foreign_count("http://example.com/")), fc + 3); // +1 keyword
  observer.check([{ name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark.guid)),
                                 "keyword", false, "keyword2",
                                 bookmark.lastModified * 1000, bookmark.type,
                                 (await PlacesUtils.promiseItemId(bookmark.parentGuid)),
                                 bookmark.guid, bookmark.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] } ]);

  // Add a third keyword.
  await PlacesUtils.keywords.insert({ keyword: "keyword3", url: "http://example.com/" });
  await check_keyword(true, "http://example.com/", "keyword");
  await check_keyword(true, "http://example.com/", "keyword2");
  await check_keyword(true, "http://example.com/", "keyword3");
  Assert.equal((await foreign_count("http://example.com/")), fc + 4); // +1 keyword

  // Remove one of the keywords.
  observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.remove("keyword");
  await check_keyword(false, "http://example.com/", "keyword");
  await check_keyword(true, "http://example.com/", "keyword2");
  await check_keyword(true, "http://example.com/", "keyword3");
  observer.check([{ name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark.guid)),
                                 "keyword", false, "",
                                 bookmark.lastModified * 1000, bookmark.type,
                                 (await PlacesUtils.promiseItemId(bookmark.parentGuid)),
                                 bookmark.guid, bookmark.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] } ]);
  Assert.equal((await foreign_count("http://example.com/")), fc + 3); // -1 keyword

  // Now remove the bookmark.
  await PlacesUtils.bookmarks.remove(bookmark);
  while ((await foreign_count("http://example.com/")));
  await check_keyword(false, "http://example.com/", "keyword");
  await check_keyword(false, "http://example.com/", "keyword2");
  await check_keyword(false, "http://example.com/", "keyword3");

  await check_no_orphans();
});

add_task(async function test_deleteKeywordMultipleBookmarks() {
  let fc = await foreign_count("http://example.com/");

  let observer = expectBookmarkNotifications();
  let bookmark1 = await PlacesUtils.bookmarks.insert({ url: "http://example.com/",
                                                       parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let bookmark2 = await PlacesUtils.bookmarks.insert({ url: "http://example.com/",
                                                       parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example.com/" });

  await check_keyword(true, "http://example.com/", "keyword");
  Assert.equal((await foreign_count("http://example.com/")), fc + 3); // +2 bookmark +1 keyword
  observer.check([{ name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark2.guid)),
                                 "keyword", false, "keyword",
                                 bookmark2.lastModified * 1000, bookmark2.type,
                                 (await PlacesUtils.promiseItemId(bookmark2.parentGuid)),
                                 bookmark2.guid, bookmark2.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                  { name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark1.guid)),
                                 "keyword", false, "keyword",
                                 bookmark1.lastModified * 1000, bookmark1.type,
                                 (await PlacesUtils.promiseItemId(bookmark1.parentGuid)),
                                 bookmark1.guid, bookmark1.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] } ]);

  observer = expectBookmarkNotifications();
  await PlacesUtils.keywords.remove("keyword");
  await check_keyword(false, "http://example.com/", "keyword");
  Assert.equal((await foreign_count("http://example.com/")), fc + 2); // -1 keyword
  observer.check([{ name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark2.guid)),
                                 "keyword", false, "",
                                 bookmark2.lastModified * 1000, bookmark2.type,
                                 (await PlacesUtils.promiseItemId(bookmark2.parentGuid)),
                                 bookmark2.guid, bookmark2.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                  { name: "onItemChanged",
                    arguments: [ (await PlacesUtils.promiseItemId(bookmark1.guid)),
                                 "keyword", false, "",
                                 bookmark1.lastModified * 1000, bookmark1.type,
                                 (await PlacesUtils.promiseItemId(bookmark1.parentGuid)),
                                 bookmark1.guid, bookmark1.parentGuid, "",
                                 Ci.nsINavBookmarksService.SOURCE_DEFAULT ] } ]);

  // Now remove the bookmarks.
  await PlacesUtils.bookmarks.remove(bookmark1);
  await PlacesUtils.bookmarks.remove(bookmark2);
  Assert.equal((await foreign_count("http://example.com/")), fc); // -2 bookmarks

  await check_no_orphans();
});

add_task(async function test_multipleKeywordsSamePostData() {
  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example.com/", postData: "postData1" });
  await check_keyword(true, "http://example.com/", "keyword", "postData1");
  // Add another keyword with same postData, should fail.
  await Assert.rejects(PlacesUtils.keywords.insert({ keyword: "keyword2", url: "http://example.com/", postData: "postData1" }),
                       /constraint failed/);
  await check_keyword(false, "http://example.com/", "keyword2", "postData1");

  await PlacesUtils.keywords.remove("keyword");

  await check_no_orphans();
});

add_task(async function test_oldPostDataAPI() {
  let bookmark = await PlacesUtils.bookmarks.insert({ url: "http://example.com/",
                                                      parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example.com/" });
  let itemId = await PlacesUtils.promiseItemId(bookmark.guid);
  await PlacesUtils.setPostDataForBookmark(itemId, "postData");
  await check_keyword(true, "http://example.com/", "keyword", "postData");
  Assert.equal(PlacesUtils.getPostDataForBookmark(itemId), "postData");

  await PlacesUtils.keywords.remove("keyword");
  await PlacesUtils.bookmarks.remove(bookmark);

  await check_no_orphans();
});

add_task(async function test_oldKeywordsAPI() {
  let bookmark = await PlacesUtils.bookmarks.insert({ url: "http://example.com/",
                                                    parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  await check_keyword(false, "http://example.com/", "keyword");
  let itemId = await PlacesUtils.promiseItemId(bookmark.guid);

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");
  await promiseKeyword("keyword", "http://example.com/");

  // Remove the keyword.
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "");
  await promiseKeyword("keyword", null);

  await PlacesUtils.keywords.insert({ keyword: "keyword", url: "http://example.com" });
  Assert.equal(PlacesUtils.bookmarks.getKeywordForBookmark(itemId), "keyword");

  let entry = await PlacesUtils.keywords.fetch("keyword");
  Assert.equal(entry.url, "http://example.com/");

  await PlacesUtils.bookmarks.remove(bookmark);

  await check_no_orphans();
});

add_task(async function test_bookmarkURLChange() {
  let fc1 = await foreign_count("http://example1.com/");
  let fc2 = await foreign_count("http://example2.com/");
  let bookmark = await PlacesUtils.bookmarks.insert({ url: "http://example1.com/",
                                                      parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  await PlacesUtils.keywords.insert({ keyword: "keyword",
                                      url: "http://example1.com/" });

  await check_keyword(true, "http://example1.com/", "keyword");
  Assert.equal((await foreign_count("http://example1.com/")), fc1 + 2); // +1 bookmark +1 keyword

  await PlacesUtils.bookmarks.update({ guid: bookmark.guid,
                                       url: "http://example2.com/"});
  await promiseKeyword("keyword", "http://example2.com/");

  await check_keyword(false, "http://example1.com/", "keyword");
  await check_keyword(true, "http://example2.com/", "keyword");
  Assert.equal((await foreign_count("http://example1.com/")), fc1); // -1 bookmark -1 keyword
  Assert.equal((await foreign_count("http://example2.com/")), fc2 + 2); // +1 bookmark +1 keyword
});
