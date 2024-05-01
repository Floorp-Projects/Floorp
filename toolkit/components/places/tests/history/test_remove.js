/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// Tests for `History.remove`, as implemented in History.sys.mjs

"use strict";

// Test removing a single page
add_task(async function test_remove_single() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  let WITNESS_URI = NetUtil.newURI(
    "http://mozilla.com/test_browserhistory/test_remove/" + Math.random()
  );
  await PlacesTestUtils.addVisits(WITNESS_URI);
  Assert.ok(page_in_database(WITNESS_URI));

  let remover = async function (name, filter, options) {
    info(name);
    info(JSON.stringify(options));
    info("Setting up visit");

    let uri = NetUtil.newURI(
      "http://mozilla.com/test_browserhistory/test_remove/" + Math.random()
    );
    let title = "Visit " + Math.random();
    await PlacesTestUtils.addVisits({ uri, title });
    Assert.ok(visits_in_database(uri), "History entry created");

    let removeArg = await filter(uri);

    if (options.addBookmark) {
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: uri,
        title: "test bookmark",
      });
    }

    let shouldRemove = !options.addBookmark;
    let placesEventListener;
    let promiseObserved = new Promise((resolve, reject) => {
      placesEventListener = events => {
        for (const event of events) {
          switch (event.type) {
            case "page-title-changed": {
              reject(
                "Unexpected page-title-changed event happens on " + event.url
              );
              break;
            }
            case "history-cleared": {
              reject("Unexpected history-cleared event happens");
              break;
            }
            case "pages-rank-changed": {
              try {
                Assert.ok(!shouldRemove, "Observing pages-rank-changed event");
              } finally {
                resolve();
              }
              break;
            }
            case "page-removed": {
              Assert.equal(
                event.isRemovedFromStore,
                shouldRemove,
                "Observe page-removed event with right removal type"
              );
              Assert.equal(
                event.url,
                uri.spec,
                "Observing effect on the right uri"
              );
              resolve();
              break;
            }
          }
        }
      };
    });
    PlacesObservers.addListener(
      [
        "page-title-changed",
        "history-cleared",
        "pages-rank-changed",
        "page-removed",
      ],
      placesEventListener
    );

    info("Performing removal");
    let removed = false;
    if (options.useCallback) {
      let onRowCalled = false;
      let guid = await PlacesTestUtils.getDatabaseValue("moz_places", "guid", {
        url: uri,
      });
      removed = await PlacesUtils.history.remove(removeArg, page => {
        Assert.equal(onRowCalled, false, "Callback has not been called yet");
        onRowCalled = true;
        Assert.equal(
          page.url.href,
          uri.spec,
          "Callback provides the correct url"
        );
        Assert.equal(page.guid, guid, "Callback provides the correct guid");
        Assert.equal(page.title, title, "Callback provides the correct title");
      });
      Assert.ok(onRowCalled, "Callback has been called");
    } else {
      removed = await PlacesUtils.history.remove(removeArg);
    }

    await promiseObserved;
    PlacesObservers.removeListener(
      [
        "page-title-changed",
        "history-cleared",
        "pages-rank-changed",
        "page-removed",
      ],
      placesEventListener
    );

    Assert.equal(visits_in_database(uri), 0, "History entry has disappeared");
    Assert.notEqual(
      visits_in_database(WITNESS_URI),
      0,
      "Witness URI still has visits"
    );
    Assert.notEqual(
      page_in_database(WITNESS_URI),
      0,
      "Witness URI is still here"
    );
    if (shouldRemove) {
      Assert.ok(removed, "Something was removed");
      Assert.equal(page_in_database(uri), 0, "Page has disappeared");
    } else {
      Assert.ok(!removed, "The page was not removed, as there was a bookmark");
      Assert.notEqual(page_in_database(uri), 0, "The page is still present");
    }
  };

  try {
    for (let useCallback of [false, true]) {
      for (let addBookmark of [false, true]) {
        let options = { useCallback, addBookmark };
        await remover(
          "Testing History.remove() with a single URI",
          x => x,
          options
        );
        await remover(
          "Testing History.remove() with a single string url",
          x => x.spec,
          options
        );
        await remover(
          "Testing History.remove() with a single string guid",
          async x =>
            PlacesTestUtils.getDatabaseValue("moz_places", "guid", { url: x }),
          options
        );
        await remover(
          "Testing History.remove() with a single URI in an array",
          x => [x],
          options
        );
        await remover(
          "Testing History.remove() with a single string url in an array",
          x => [x.spec],
          options
        );
        await remover(
          "Testing History.remove() with a single string guid in an array",
          x =>
            PlacesTestUtils.getDatabaseValue("moz_places", "guid", { url: x }),
          options
        );
      }
    }
  } finally {
    await PlacesUtils.history.clear();
  }
});

add_task(async function cleanup() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
});

// Test the various error cases
add_task(async function test_error_cases() {
  Assert.throws(
    () => PlacesUtils.history.remove(),
    /TypeError: Invalid url/,
    "History.remove with no argument should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove(null),
    /TypeError: Invalid url/,
    "History.remove with `null` should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove(undefined),
    /TypeError: Invalid url/,
    "History.remove with `undefined` should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove("not a guid, obviously"),
    /TypeError: .* is not a valid URL/,
    "History.remove with an ill-formed guid/url argument should throw a TypeError"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.remove({
        "not the kind of object we know how to handle": true,
      }),
    /TypeError: Invalid url/,
    "History.remove with an unexpected object should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove([]),
    /TypeError: Expected at least one page/,
    "History.remove with an empty array should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove([null]),
    /TypeError: Invalid url or guid/,
    "History.remove with an array containing null should throw a TypeError"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.remove([
        "http://example.org",
        "not a guid, obviously",
      ]),
    /TypeError: .* is not a valid URL/,
    "History.remove with an array containing an ill-formed guid/url argument should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove(["0123456789ab" /* valid guid*/, null]),
    /TypeError: Invalid url or guid: null/,
    "History.remove with an array containing a guid and a second argument that is null should throw a TypeError"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.remove([
        "http://example.org",
        { "not the kind of object we know how to handle": true },
      ]),
    /TypeError: Invalid url/,
    "History.remove with an array containing an unexpected objecgt should throw a TypeError"
  );
  Assert.throws(
    () =>
      PlacesUtils.history.remove(
        "http://example.org",
        "not a function, obviously"
      ),
    /TypeError: Invalid function/,
    "History.remove with a second argument that is not a function argument should throw a TypeError"
  );
  try {
    PlacesUtils.history.remove(
      "http://example.org/I/have/clearly/not/been/added",
      null
    );
    Assert.ok(true, "History.remove should ignore `null` as a second argument");
  } catch (ex) {
    Assert.ok(
      false,
      "History.remove should ignore `null` as a second argument"
    );
  }
});

add_task(async function test_orphans() {
  let uri = NetUtil.newURI("http://moz.org/");
  await PlacesTestUtils.addVisits({ uri });

  PlacesUtils.favicons.setAndFetchFaviconForPage(
    uri,
    SMALLPNG_DATA_URI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );
  // Also create a root icon.
  let faviconURI = Services.io.newURI(uri.spec + "favicon.ico");
  await PlacesTestUtils.setFaviconForPage(uri, faviconURI, SMALLPNG_DATA_URI);

  await PlacesUtils.history.update({
    url: uri,
    annotations: new Map([["test", "restval"]]),
  });

  await PlacesUtils.history.remove(uri);
  Assert.ok(
    !(await PlacesTestUtils.isPageInDB(uri)),
    "Page should have been removed"
  );

  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`SELECT (SELECT count(*) FROM moz_annos) +
                                      (SELECT count(*) FROM moz_icons) +
                                      (SELECT count(*) FROM moz_pages_w_icons) +
                                      (SELECT count(*) FROM moz_icons_to_pages) AS count`);
  Assert.equal(rows[0].getResultByName("count"), 0, "Should not find orphans");
});

add_task(async function test_remove_backslash() {
  // Backslash is an escape char in Sqlite, we must take care of that when
  // removing a url containing a backslash.
  const url = "https://www.mozilla.org/?test=\u005C";
  await PlacesTestUtils.addVisits(url);
  Assert.ok(await PlacesUtils.history.remove(url), "A page should be removed");
  Assert.deepEqual(
    await PlacesUtils.history.fetch(url),
    null,
    "The page should not be found"
  );
});

add_task(async function test_url_with_apices() {
  // Apices may confuse code and cause injection if mishandled.
  // The ideal test would be with a javascript url, because it would not be
  // encoded by URL(), unfortunately it would also not be added to history.
  const url = `http://mozilla.org/\u0022\u0027`;
  await PlacesTestUtils.addVisits(url);
  Assert.ok(await PlacesUtils.history.remove(url), "A page should be removed");
  Assert.deepEqual(
    await PlacesUtils.history.fetch(url),
    null,
    "The page should not be found"
  );
});
