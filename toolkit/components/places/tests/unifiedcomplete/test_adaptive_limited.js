/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that top adaptive results are limited, remaining ones are enqueued.

add_task(async function() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  let n = 10;
  let uris = Array(n).fill(0).map((e, i) => "http://site.tld/" + i);

  // Add a bookmark to one url.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test_book",
    url: uris.shift(),
  });

  // Make remaining ones adaptive results.
  for (let uri of uris) {
    await PlacesTestUtils.addVisits(uri);
    await addAdaptiveFeedback(uri, "book");
  }

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
  });

  let matches = uris.map(uri => ({ uri: Services.io.newURI(uri),
                                   title: "test visit for " + uri }));
  let book_index = Math.ceil(Services.prefs.getIntPref("browser.urlbar.maxRichResults") / 4);
  matches.splice(book_index, 0, { uri: Services.io.newURI(bm.url.href),
                                  title: "test_book", "style": ["bookmark"] });

  await check_autocomplete({
    search: "book",
    matches,
    checkSorting: true,
  });
});
