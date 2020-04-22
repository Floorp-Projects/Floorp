/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const SUGGESTIONS_ENGINE_NAME = "engine-suggestions.xml";

/**
 * Tests search engine aliases. See
 * browser/components/urlbar/tests/browser/browser_tokenAlias.js for tests of
 * the token alias list (i.e. showing all aliased engines on a "@" query).
 */
// Basic test that uses two engines, a GET engine and a POST engine, neither
// providing search suggestions.
add_task(async function basicGetAndPost() {
  // Note that head_autocomplete.js has already added a MozSearch engine.
  // Here we add another engine with a search alias.
  await Services.search.addEngineWithDetails("AliasedGETMozSearch", {
    alias: "get",
    method: "GET",
    template: "http://s.example.com/search",
  });
  await Services.search.addEngineWithDetails("AliasedPOSTMozSearch", {
    alias: "post",
    method: "POST",
    template: "http://s.example.com/search",
  });

  await PlacesTestUtils.addVisits("http://s.example.com/search?q=firefox");
  let historyMatch = {
    value: "http://s.example.com/search?q=firefox",
    comment: "test visit for http://s.example.com/search?q=firefox",
  };

  for (let alias of ["get", "post"]) {
    await check_autocomplete({
      search: alias,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} `, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "",
          alias,
          heuristic: true,
        }),
        historyMatch,
      ],
    });

    await check_autocomplete({
      search: `${alias} `,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} `, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "",
          alias,
          heuristic: true,
        }),
        historyMatch,
      ],
    });

    await check_autocomplete({
      search: `${alias} fire`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} fire`, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "fire",
          alias,
          heuristic: true,
        }),
        historyMatch,
      ],
    });

    await check_autocomplete({
      search: `${alias} mozilla`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} mozilla`, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "mozilla",
          alias,
          heuristic: true,
        }),
      ],
    });

    await check_autocomplete({
      search: `${alias} MoZiLlA`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} MoZiLlA`, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "MoZiLlA",
          alias,
          heuristic: true,
        }),
      ],
    });

    await check_autocomplete({
      search: `${alias} mozzarella mozilla`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} mozzarella mozilla`, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "mozzarella mozilla",
          alias,
          heuristic: true,
        }),
      ],
    });

    // When a restriction token is used before the alias, the alias should *not*
    // be recognized.  It should be treated as part of the search string.  Try
    // all the restriction tokens to test that.  We should get a single "search
    // with" heuristic result without an alias.
    for (let token of Object.values(UrlbarTokenizer.RESTRICT)) {
      let search = `${token} ${alias} query string`;
      let searchQuery =
        token == UrlbarTokenizer.RESTRICT.SEARCH &&
        search.startsWith(UrlbarTokenizer.RESTRICT.SEARCH)
          ? search.substring(2)
          : search;
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
  await cleanup();
});
