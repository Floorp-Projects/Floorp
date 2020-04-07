/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const SUGGESTIONS_ENGINE_NAME = "engine-suggestions.xml";

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

// When the search is simply "@", the results should be a list of all the "@"
// alias engines.
add_task(async function tokenAliasEngines() {
  await Services.search.init();
  // Tell the search service we are running in the US.  This also has the
  // desired side-effect of preventing our geoip lookup.
  Services.prefs.setCharPref("browser.search.region", "US");
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);

  Services.search.restoreDefaultEngines();
  Services.search.resetToOriginalDefaultEngine();

  let tokenEngines = [];
  for (let engine of await Services.search.getEngines()) {
    let aliases = [];
    if (engine.alias) {
      aliases.push(engine.alias);
    }
    aliases.push(...engine.wrappedJSObject._internalAliases);
    let tokenAliases = aliases.filter(a => a.startsWith("@"));
    if (tokenAliases.length) {
      tokenEngines.push({ engine, tokenAliases });
    }
  }
  if (!tokenEngines.length) {
    Assert.ok(true, "No token alias engines, skipping task.");
    return;
  }
  info(
    "Got token alias engines: " + tokenEngines.map(({ engine }) => engine.name)
  );

  await check_autocomplete({
    search: "@",
    searchParam: "enable-actions",
    matches: tokenEngines.map(({ engine, tokenAliases }) => {
      let alias = tokenAliases[0];
      return makeSearchMatch(alias + " ", {
        engineName: engine.name,
        alias,
        searchQuery: "",
      });
    }),
  });

  await cleanup();
});
