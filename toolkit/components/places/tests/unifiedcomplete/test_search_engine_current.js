/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


add_task(async function() {
  // Note that head_autocomplete.js has already added a MozSearch engine.
  // Here we add another engine with a search alias.
  Services.search.addEngineWithDetails("AliasedMozSearch", "", "doit", "",
                                       "GET", "http://s.example.com/search");

  info("search engine");
  await check_autocomplete({
    search: "mozilla",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("mozilla", { heuristic: true }) ]
  });

  info("search engine, uri-like input");
  await check_autocomplete({
    search: "http:///",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("http:///", { heuristic: true }) ]
  });

  info("search engine, multiple words");
  await check_autocomplete({
    search: "mozzarella cheese",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("mozzarella cheese", { heuristic: true }) ]
  });

  info("search engine, after current engine has changed");
  Services.search.addEngineWithDetails("MozSearch2", "", "", "", "GET",
                                       "http://s.example.com/search2");
  let engine = Services.search.getEngineByName("MozSearch2");
  notEqual(Services.search.currentEngine, engine, "New engine shouldn't be the current engine yet");
  Services.search.currentEngine = engine;
  await check_autocomplete({
    search: "mozilla",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("mozilla", { engineName: "MozSearch2", heuristic: true }) ]
  });

  await cleanup();
});
