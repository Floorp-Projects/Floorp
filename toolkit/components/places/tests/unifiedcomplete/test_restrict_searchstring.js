/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that restriction tokens are not removed from the search string, apart
 * from a leading search restriction token.
 */

add_task(async function test_searchstring() {
  for (let token of Object.values(UrlbarTokenizer.RESTRICT)) {
    for (let search of [`${token} query`, `query ${token}`]) {
      let searchQuery =
        token == UrlbarTokenizer.RESTRICT.SEARCH &&
        search.startsWith(UrlbarTokenizer.RESTRICT.SEARCH)
          ? search.substring(2)
          : search;
      info(`Searching for "${search}", expecting "${searchQuery}"`);
      await check_autocomplete({
        search,
        searchParam: "enable-actions",
        matches: [
          makeSearchMatch(search, {
            engineName: "MozSearch",
            searchQuery,
            heuristic: true,
          }),
        ],
      });
    }
  }
});
