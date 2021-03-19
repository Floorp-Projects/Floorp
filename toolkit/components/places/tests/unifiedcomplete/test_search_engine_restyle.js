/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const engineDomain = "s.example.com";
add_task(async function setup() {
  Services.prefs.setBoolPref("browser.urlbar.restyleSearches", true);
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.restyleSearches");
  });
});
add_task(async function test_searchEngine() {
  let uri = Services.io.newURI(`https://${engineDomain}/search?q=Terms`);
  await PlacesTestUtils.addVisits({
    uri,
    title: "Terms - SearchEngine Search",
  });

  info("Past search terms should be styled.");

  await check_autocomplete({
    search: "term",
    matches: [
      makeSearchMatch("Terms", {
        engineName: "MozSearch",
        searchSuggestion: "Terms",
        isSearchHistory: true,
        style: ["favicon"],
      }),
    ],
  });

  info(
    "Searching for a superset of the search string in history should not restyle."
  );
  await check_autocomplete({
    search: "Terms Foo",
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
  Services.prefs.setBoolPref("browser.urlbar.restyleSearches", true);

  await cleanup();
});

add_task(async function test_extraneousParameters() {
  info("SERPs in history with extraneous parameters should not be restyled.");
  let uri = Services.io.newURI(
    `https://${engineDomain}/search?q=Terms&p=2&type=img`
  );
  await PlacesTestUtils.addVisits({
    uri,
    title: "Terms - SearchEngine Search",
  });

  await check_autocomplete({
    search: "term",
    matches: [{ uri, title: "Terms - SearchEngine Search" }],
  });
});
