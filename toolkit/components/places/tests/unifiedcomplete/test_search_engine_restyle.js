/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_searchEngine() {
  let engine = await Services.search.addEngineWithDetails("SearchEngine", {
    method: "GET",
    template: "http://s.example.com/search",
    searchGetParams: "q={searchTerms}",
  });
  registerCleanupFunction(async () => Services.search.removeEngine(engine));

  let uri = NetUtil.newURI("http://s.example.com/search?q=Terms");
  await PlacesTestUtils.addVisits({
    uri,
    title: "Terms - SearchEngine Search",
  });

  info("Past search terms should be styled.");
  Services.prefs.setBoolPref("browser.urlbar.restyleSearches", true);
  await check_autocomplete({
    search: "term",
    matches: [
      makeSearchMatch("Terms", {
        engineName: "SearchEngine",
        searchSuggestion: "Terms",
        isSearchHistory: true,
        style: ["favicon"],
      }),
    ],
  });

  info("Bookmarked past searches should not be restyled");
  await PlacesTestUtils.addBookmarkWithDetails({
    uri,
    title: "Terms - SearchEngine Search",
  });

  await check_autocomplete({
    search: "term",
    matches: [
      {
        uri,
        title: "Terms - SearchEngine Search",
        style: ["bookmark"],
      },
    ],
  });

  await PlacesUtils.bookmarks.eraseEverything();

  info("Past search terms should not be styled if restyling is disabled");
  Services.prefs.setBoolPref("browser.urlbar.restyleSearches", false);
  await check_autocomplete({
    search: "term",
    matches: [{ uri, title: "Terms - SearchEngine Search" }],
  });

  await cleanup();
});
