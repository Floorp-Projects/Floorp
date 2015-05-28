/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


add_task(function*() {
  // Note that head_autocomplete.js has already added a MozSearch engine.
  // Here we add another engine with a search alias.
  Services.search.addEngineWithDetails("AliasedMozSearch", "", "doit", "",
                                       "GET", "http://s.example.com/search");


  yield check_autocomplete({
    search: "doit",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("doit") ]
  });

  yield check_autocomplete({
    search: "doit mozilla",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("doit mozilla", { engineName: "AliasedMozSearch", searchQuery: "mozilla", alias: "doit" }) ]
  });

  yield check_autocomplete({
    search: "doit mozzarella mozilla",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("doit mozzarella mozilla", { engineName: "AliasedMozSearch", searchQuery: "mozzarella mozilla", alias: "doit" }) ]
  });

  yield cleanup();
});
