/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


add_task(function*() {
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://s.example.com/search");
  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.currentEngine = engine;
  Services.search.addEngineWithDetails("AliasedMozSearch", "", "doit", "",
                                       "GET", "http://s.example.com/search");


  yield check_autocomplete({
    search: "doit",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("searchengine", {engineName: "MozSearch", input: "doit", searchQuery: "doit"}), title: "MozSearch" }, ]
  });

  yield check_autocomplete({
    search: "doit mozilla",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("searchengine", {engineName: "AliasedMozSearch", input: "doit mozilla", searchQuery: "mozilla"}), title: "AliasedMozSearch" }, ]
  });

  yield check_autocomplete({
    search: "doit mozzarella mozilla",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("searchengine", {engineName: "AliasedMozSearch", input: "doit mozzarella mozilla", searchQuery: "mozzarella mozilla"}), title: "AliasedMozSearch" }, ]
  });

  yield cleanup();
});
