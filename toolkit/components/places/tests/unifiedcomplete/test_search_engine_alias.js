/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


add_task(async function() {
  // Note that head_autocomplete.js has already added a MozSearch engine.
  // Here we add another engine with a search alias.
  Services.search.addEngineWithDetails("AliasedGETMozSearch", "", "get", "",
                                       "GET", "http://s.example.com/search");
  Services.search.addEngineWithDetails("AliasedPOSTMozSearch", "", "post", "",
                                       "POST", "http://s.example.com/search");
  let histURI = NetUtil.newURI("http://s.example.com/search?q=firefox");
  await PlacesTestUtils.addVisits([{ uri: histURI, title: "History entry" }]);

  for (let alias of ["get", "post"]) {
    await check_autocomplete({
      search: alias,
      searchParam: "enable-actions",
      matches: [ makeSearchMatch(alias, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                          searchQuery: "", alias, heuristic: true }),
        { uri: histURI, title: "History entry" },
      ]
    });

    await check_autocomplete({
      search: `${alias} `,
      searchParam: "enable-actions",
      matches: [ makeSearchMatch(`${alias} `, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                                searchQuery: "", alias, heuristic: true }),
        { uri: histURI, title: "History entry" },
      ]
    });

    await check_autocomplete({
      search: `${alias} fire`,
      searchParam: "enable-actions",
      matches: [ makeSearchMatch(`${alias} fire`, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                                    searchQuery: "fire", alias, heuristic: true }),
        { uri: histURI, title: "History entry" },
      ]
    });

    await check_autocomplete({
      search: `${alias} mozilla`,
      searchParam: "enable-actions",
          matches: [ makeSearchMatch(`${alias} mozilla`, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                                           searchQuery: "mozilla", alias, heuristic: true }) ]
    });

    await check_autocomplete({
      search: `${alias} MoZiLlA`,
      searchParam: "enable-actions",
          matches: [ makeSearchMatch(`${alias} MoZiLlA`, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                                           searchQuery: "MoZiLlA", alias, heuristic: true }) ]
    });

    await check_autocomplete({
      search: `${alias} mozzarella mozilla`,
      searchParam: "enable-actions",
          matches: [ makeSearchMatch(`${alias} mozzarella mozilla`, { engineName: `Aliased${alias.toUpperCase()}MozSearch`,
                                                                      searchQuery: "mozzarella mozilla", alias, heuristic: true }) ]
    });
  }

  await cleanup();
});
