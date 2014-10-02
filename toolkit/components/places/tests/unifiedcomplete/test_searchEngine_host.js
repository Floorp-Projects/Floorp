/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* test_searchEngine_autoFill() {
  Services.search.addEngineWithDetails("MySearchEngine", "", "", "",
                                       "GET", "http://my.search.com/");
  let engine = Services.search.getEngineByName("MySearchEngine");
  do_register_cleanup(() => Services.search.removeEngine(engine));

  // Add an uri that matches the search string with high frecency.
  let uri = NetUtil.newURI("http://www.example.com/my/");
  let visits = [];
  for (let i = 0; i < 100; ++i) {
    visits.push({ uri , title: "Terms - SearchEngine Search" });
  }
  yield promiseAddVisits(visits);
  addBookmark({ uri: uri, title: "Example bookmark" });
  Assert.ok(frecencyForUrl(uri) > 10000);

  do_log_info("Check search domain is autoFilled even if there's an higher frecency match");
  yield check_autocomplete({
    search: "my",
    autofilled: "my.search.com",
    completed: "http://my.search.com"
  });

  yield cleanup();
});
