/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
});

const URI1 = "http://test1.mozilla.org/";
const URI2 = "http://test2.mozilla.org/";
const URI3 = "http://test3.mozilla.org/";

async function check_keyword(aURI, aKeyword) {
  if (aKeyword) {
    aKeyword = aKeyword.toLowerCase();
  }

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
    `
  );
  Assert.equal(rows.length, 0);
}

function expectNotifications() {
  const observer = {
    notifications: [],
    _start() {
      this._handle = this._handle.bind(this);
      PlacesUtils.observers.addListener(
        ["bookmark-keyword-changed"],
        this._handle
      );
    },
    _handle(events) {
      for (const event of events) {
        this.notifications.push({
          type: event.type,
          id: event.id,
          itemType: event.itemType,
          url: event.url,
          guid: event.guid,
          parentGuid: event.parentGuid,
          keyword: event.keyword,
          lastModified: new Date(event.lastModified),
          source: event.source,
          isTagging: event.isTagging,
        });
      }
    },
    check(expected) {
      PlacesUtils.observers.removeListener(
        ["bookmark-keyword-changed"],
        this._handle
      );
      Assert.deepEqual(this.notifications, expected);
    },
  };
  observer._start();
  return observer;
}

add_task(function test_invalid_input() {});

add_task(async function test_addBookmarkAndKeyword() {
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function () {
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
  await PlacesUtils.keywords.insert({ url: URI1, keyword: "keyword" });
  let itemId = await PlacesTestUtils.promiseItemId(bookmark.guid);
  observer.check([
    {
      type: "bookmark-keyword-changed",
      id: itemId,
      itemType: bookmark.type,
      url: bookmark.url,
      guid: bookmark.guid,
      parentGuid: bookmark.parentGuid,
      keyword: "keyword",
      lastModified: new Date(bookmark.lastModified),
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
  await check_keyword(URI1, "keyword");
  Assert.equal(await foreign_count(URI1), fc + 2); // + 1 bookmark + 1 keyword
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
  Assert.equal(await foreign_count(URI1), fc + 1); // + 1 bookmark
  await PlacesUtils.bookmarks.remove(bookmark);
  await check_orphans();
});

add_task(async function test_sameKeywordDifferentURI() {
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function () {
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
  observer.check([
    {
      type: "bookmark-keyword-changed",
      id: await PlacesTestUtils.promiseItemId(bookmark1.guid),
      itemType: bookmark1.type,
      url: bookmark1.url,
      guid: bookmark1.guid,
      parentGuid: bookmark1.parentGuid,
      keyword: "",
      lastModified: new Date(bookmark1.lastModified),
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
    {
      type: "bookmark-keyword-changed",
      id: await PlacesTestUtils.promiseItemId(bookmark2.guid),
      itemType: bookmark2.type,
      url: bookmark2.url,
      guid: bookmark2.guid,
      parentGuid: bookmark2.parentGuid,
      keyword: "keyword",
      lastModified: new Date(bookmark2.lastModified),
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);

  // The keyword should have been "moved" to the new URI.
  await check_keyword(URI1, null);
  Assert.equal(await foreign_count(URI1), fc1 - 1); // - 1 keyword
  await check_keyword(URI2, "keyword");
  Assert.equal(await foreign_count(URI2), fc2 + 2); // + 1 bookmark + 1 keyword
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
  observer.check([
    {
      type: "bookmark-keyword-changed",
      id: await PlacesTestUtils.promiseItemId(bookmarks[0].guid),
      itemType: bookmarks[0].type,
      url: bookmarks[0].url,
      guid: bookmarks[0].guid,
      parentGuid: bookmarks[0].parentGuid,
      keyword: "keyword2",
      lastModified: new Date(bookmarks[0].lastModified),
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
    {
      type: "bookmark-keyword-changed",
      id: await PlacesTestUtils.promiseItemId(bookmarks[1].guid),
      itemType: bookmarks[1].type,
      url: bookmarks[1].url,
      guid: bookmarks[1].guid,
      parentGuid: bookmarks[1].parentGuid,
      keyword: "keyword2",
      lastModified: new Date(bookmarks[1].lastModified),
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
  await check_keyword(URI2, "keyword2");
  Assert.equal(await foreign_count(URI2), fc + 1); // + 1 bookmark - 1 keyword + 1 keyword
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
  Assert.equal(await foreign_count(URI2), fc); // + 1 bookmark - 1 bookmark
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
  await PlacesUtils.bookmarks.fetch({ url: URI2 }, bookmark =>
    bookmarks.push(bookmark)
  );
  Assert.equal(bookmarks.length, 3, "Check number of bookmarks");
  observer.check([
    {
      type: "bookmark-keyword-changed",
      id: await PlacesTestUtils.promiseItemId(bookmarks[0].guid),
      itemType: bookmarks[0].type,
      url: bookmarks[0].url,
      guid: bookmarks[0].guid,
      parentGuid: bookmarks[0].parentGuid,
      keyword: "",
      lastModified: new Date(bookmarks[0].lastModified),
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
    {
      type: "bookmark-keyword-changed",
      id: await PlacesTestUtils.promiseItemId(bookmarks[1].guid),
      itemType: bookmarks[1].type,
      url: bookmarks[1].url,
      guid: bookmarks[1].guid,
      parentGuid: bookmarks[1].parentGuid,
      keyword: "",
      lastModified: new Date(bookmarks[1].lastModified),
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
    {
      type: "bookmark-keyword-changed",
      id: await PlacesTestUtils.promiseItemId(bookmarks[2].guid),
      itemType: bookmarks[2].type,
      url: bookmarks[2].url,
      guid: bookmarks[2].guid,
      parentGuid: bookmarks[2].parentGuid,
      keyword: "",
      lastModified: new Date(bookmarks[2].lastModified),
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);

  await check_keyword(URI1, null);
  await check_keyword(URI2, null);
  Assert.equal(await foreign_count(URI2), fc); // + 1 bookmark - 1 keyword
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
  let itemId = await PlacesTestUtils.promiseItemId(bookmark.guid);
  await PlacesUtils.keywords.insert({ url: URI3, keyword: "keyword" });
  await PlacesUtils.bookmarks.remove(bookmark);

  observer.check([
    {
      type: "bookmark-keyword-changed",
      id: itemId,
      itemType: bookmark.type,
      url: bookmark.url,
      guid: bookmark.guid,
      parentGuid: bookmark.parentGuid,
      keyword: "keyword",
      lastModified: new Date(bookmark.lastModified),
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);

  await check_keyword(URI3, null);
  Assert.equal(await foreign_count(URI3), fc); // +- 1 bookmark +- 1 keyword
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
    observer.check([
      {
        type: "bookmark-keyword-changed",
        id: await PlacesTestUtils.promiseItemId(oldBmk.guid),
        itemType: oldBmk.type,
        url: oldBmk.url,
        guid: oldBmk.guid,
        parentGuid: oldBmk.parentGuid,
        keyword: "",
        lastModified: new Date(oldBmk.lastModified),
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      },
      {
        type: "bookmark-keyword-changed",
        id: await PlacesTestUtils.promiseItemId(newBmk.guid),
        itemType: newBmk.type,
        url: newBmk.url,
        guid: newBmk.guid,
        parentGuid: newBmk.parentGuid,
        keyword: "",
        lastModified: new Date(newBmk.lastModified),
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      },
      {
        type: "bookmark-keyword-changed",
        id: await PlacesTestUtils.promiseItemId(newBmk.guid),
        itemType: newBmk.type,
        url: newBmk.url,
        guid: newBmk.guid,
        parentGuid: newBmk.parentGuid,
        keyword: "kw1-1",
        lastModified: new Date(newBmk.lastModified),
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      },
      {
        type: "bookmark-keyword-changed",
        id: await PlacesTestUtils.promiseItemId(newBmk.guid),
        itemType: newBmk.type,
        url: newBmk.url,
        guid: newBmk.guid,
        parentGuid: newBmk.parentGuid,
        keyword: "kw1-2",
        lastModified: new Date(newBmk.lastModified),
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      },
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
    observer.check([
      {
        type: "bookmark-keyword-changed",
        id: await PlacesTestUtils.promiseItemId(oldBmk.guid),
        itemType: oldBmk.type,
        url: oldBmk.url,
        guid: oldBmk.guid,
        parentGuid: oldBmk.parentGuid,
        keyword: "",
        lastModified: new Date(oldBmk.lastModified),
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      },
      {
        type: "bookmark-keyword-changed",
        id: await PlacesTestUtils.promiseItemId(newBmk.guid),
        itemType: newBmk.type,
        url: newBmk.url,
        guid: newBmk.guid,
        parentGuid: newBmk.parentGuid,
        keyword: "",
        lastModified: new Date(newBmk.lastModified),
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      },
      {
        type: "bookmark-keyword-changed",
        id: await PlacesTestUtils.promiseItemId(newBmk.guid),
        itemType: newBmk.type,
        url: newBmk.url,
        guid: newBmk.guid,
        parentGuid: newBmk.parentGuid,
        keyword: "kw8-1",
        lastModified: new Date(newBmk.lastModified),
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      },
      {
        type: "bookmark-keyword-changed",
        id: await PlacesTestUtils.promiseItemId(newBmk.guid),
        itemType: newBmk.type,
        url: newBmk.url,
        guid: newBmk.guid,
        parentGuid: newBmk.parentGuid,
        keyword: "kw8-2",
        lastModified: new Date(newBmk.lastModified),
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      },
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
    "bookmark-keyword-changed",
    events =>
      events.some(event => event.guid === fx.guid && event.keyword === "fx")
  );
  await PlacesUtils.bookmarks.update({
    guid: fx.guid,
    url: "https://www.mozilla.org/firefox",
  });
  await promiseNotification;

  let entriesByKeyword = [];
  await PlacesUtils.keywords.fetch({ keyword: "fx" }, e =>
    entriesByKeyword.push(e.url.href)
  );
  deepEqual(
    entriesByKeyword,
    ["https://www.mozilla.org/firefox"],
    "Should return new URL for keyword"
  );

  ok(
    !(await PlacesUtils.keywords.fetch({ url: "http://getfirefox.com" })),
    "Should not return keywords for old URL"
  );

  let entiresByURL = [];
  await PlacesUtils.keywords.fetch(
    { url: "https://www.mozilla.org/firefox" },
    e => entiresByURL.push(e.keyword)
  );
  deepEqual(entiresByURL, ["fx"], "Should return keyword for new URL");

  info("Invalidate cached keywords");
  await PlacesUtils.keywords.invalidateCachedKeywords();

  info("Remove bookmark with keyword");
  await PlacesUtils.bookmarks.remove(tb.guid);

  ok(
    !(await PlacesUtils.keywords.fetch({ url: "http://getthunderbird.com" })),
    "Should not return keywords for removed bookmark URL"
  );

  ok(
    !(await PlacesUtils.keywords.fetch({ keyword: "tb" })),
    "Should not return URL for removed bookmark keyword"
  );
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

  ok(
    !(await PlacesUtils.keywords.fetch({ keyword: "fx" })),
    "Should remove Firefox keyword"
  );

  ok(
    !(await PlacesUtils.keywords.fetch({ keyword: "tb" })),
    "Should remove Thunderbird keyword"
  );
});
