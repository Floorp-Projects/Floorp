/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const SUGGESTIONS_ENGINE_NAME = "engine-suggestions.xml";

add_task(async function init() {
  // This history result would match some of the searches below were it not for
  // the fact that don't include history results when the user has used an
  // engine alias.  Therefore, this result should never appear below.
  await PlacesTestUtils.addVisits("http://s.example.com/search?q=firefox");
});


// Basic test that uses two engines, a GET engine and a POST engine, neither
// providing search suggestions.
add_task(async function getPost() {
  // Note that head_autocomplete.js has already added a MozSearch engine.
  // Here we add another engine with a search alias.
  Services.search.addEngineWithDetails("AliasedGETMozSearch", "", "get", "",
                                       "GET", "http://s.example.com/search");
  Services.search.addEngineWithDetails("AliasedPOSTMozSearch", "", "post", "",
                                       "POST", "http://s.example.com/search");

  for (let alias of ["get", "post"]) {
    await check_autocomplete({
      search: alias,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(alias, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          searchQuery: "",
          alias,
          heuristic: true,
        }),
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
  }

  await cleanup();
});


// Uses an engine that provides search suggestions.
add_task(async function engineWithSuggestions() {
  let engine = await addTestSuggestionsEngine();

  // Use a normal alias and then one with an "@", the latter to simulate the
  // built-in "@" engine aliases (e.g., "@google").
  for (let alias of ["moz", "@moz"]) {
    engine.alias = alias;

    await check_autocomplete({
      search: `${alias}`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(alias, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          searchQuery: "",
          heuristic: true,
        }),
      ],
    });

    await check_autocomplete({
      search: `${alias} `,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} `, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          searchQuery: "",
          heuristic: true,
        }),
      ],
    });

    await check_autocomplete({
      search: `${alias} fire`,
      searchParam: "enable-actions",
      matches: [
        makeSearchMatch(`${alias} fire`, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          searchQuery: "fire",
          heuristic: true,
        }),
        makeSearchMatch(`fire foo`, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          searchQuery: "fire",
          searchSuggestion: "fire foo",
        }),
        makeSearchMatch(`fire bar`, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          searchQuery: "fire",
          searchSuggestion: "fire bar",
        }),
      ],
    });
  }

  engine.alias = "";
  await cleanup();
});


// When the search is simply "@", the results should be a list of all the "@"
// alias engines.
add_task(async function tokenAliasEngines() {
  let tokenEngines = [];
  for (let engine of Services.search.getEngines()) {
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
  info("Got token alias engines: " +
       tokenEngines.map(({ engine }) => engine.name));

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
