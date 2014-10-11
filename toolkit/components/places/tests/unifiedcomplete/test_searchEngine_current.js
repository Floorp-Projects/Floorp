/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


add_task(function*() {
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://s.example.com/search");
  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.currentEngine = engine;
  Services.search.addEngineWithDetails("AliasedMozSearch", "", "doit", "",
                                       "GET", "http://s.example.com/search");

  do_log_info("search engine");
  yield check_autocomplete({
    search: "mozilla",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("searchengine", {engineName: "MozSearch", input: "mozilla", searchQuery: "mozilla"}), title: "MozSearch" }, ]
  });

  do_log_info("search engine, uri-like input");
  yield check_autocomplete({
    search: "http:///",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("searchengine", {engineName: "MozSearch", input: "http:///", searchQuery: "http:///"}), title: "MozSearch" }, ]
  });

  do_log_info("search engine, multiple words");
  yield check_autocomplete({
    search: "mozzarella cheese",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("searchengine", {engineName: "MozSearch", input: "mozzarella cheese", searchQuery: "mozzarella cheese"}), title: "MozSearch" }, ]
  });

  do_log_info("search engine, after current engine has changed");
  Services.search.addEngineWithDetails("MozSearch2", "", "", "", "GET",
                                       "http://s.example.com/search2");
  engine = Services.search.getEngineByName("MozSearch2");
  notEqual(Services.search.currentEngine, engine, "New engine shouldn't be the current engine yet");
  Services.search.currentEngine = engine;
  yield check_autocomplete({
    search: "mozilla",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("searchengine", {engineName: "MozSearch2", input: "mozilla", searchQuery: "mozilla"}), title: "MozSearch2" }, ]
  });

  yield cleanup();
});
