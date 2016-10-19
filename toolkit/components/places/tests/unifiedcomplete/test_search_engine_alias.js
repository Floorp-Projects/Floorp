/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


add_task(function*() {
  // Note that head_autocomplete.js has already added a MozSearch engine.
  // Here we add another engine with a search alias.
  Services.search.addEngineWithDetails("AliasedGETMozSearch", "", "get", "",
                                       "GET", "http://s.example.com/search");
  Services.search.addEngineWithDetails("AliasedPOSTMozSearch", "", "post", "",
                                       "POST", "http://s.example.com/search");

  for (let alias of ["get", "post"]) {
    yield check_autocomplete({
      search: alias,
      searchParam: "enable-actions",
      matches: [ makeSearchMatch(alias, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                          searchQuery: "", alias, heuristic: true }) ]
    });

    yield check_autocomplete({
      search: `${alias} `,
      searchParam: "enable-actions",
      matches: [ makeSearchMatch(`${alias} `, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                                searchQuery: "", alias, heuristic: true }) ]
    });

    yield check_autocomplete({
      search: `${alias} mozilla`,
      searchParam: "enable-actions",
          matches: [ makeSearchMatch(`${alias} mozilla`, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                                           searchQuery: "mozilla", alias, heuristic: true }) ]
    });

    yield check_autocomplete({
      search: `${alias} MoZiLlA`,
      searchParam: "enable-actions",
          matches: [ makeSearchMatch(`${alias} MoZiLlA`, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                                           searchQuery: "MoZiLlA", alias, heuristic: true }) ]
    });

    yield check_autocomplete({
      search: `${alias} mozzarella mozilla`,
      searchParam: "enable-actions",
          matches: [ makeSearchMatch(`${alias} mozzarella mozilla`, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                                                      searchQuery: "mozzarella mozilla", alias, heuristic: true }) ]
    });
  }

  yield cleanup();
});
