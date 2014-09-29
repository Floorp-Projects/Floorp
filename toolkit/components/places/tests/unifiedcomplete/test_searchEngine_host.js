/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://testing-common/httpd.js");

function* addTestEngines(items) {
  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());
  let gDataUrl = "http://localhost:" + httpServer.identity.primaryPort + "/data/";
  do_register_cleanup(() => httpServer.stop(() => {}));

  let engines = [];

  for (let item of items) {
    do_print("Adding engine: " + item);
    yield new Promise(resolve => {
      Services.obs.addObserver(function obs(subject, topic, data) {
        let engine = subject.QueryInterface(Ci.nsISearchEngine);
        do_print("Observed " + data + " for " + engine.name);
        if (data != "engine-added" || engine.name != item) {
          return;
        }

        Services.obs.removeObserver(obs, "browser-search-engine-modified");
        engines.push(engine);
        resolve();
      }, "browser-search-engine-modified", false);

      do_print("`Adding engine from URL: " + gDataUrl + item);
      Services.search.addEngine(gDataUrl + item,
                                Ci.nsISearchEngine.DATA_XML, null, false);
    });
  }

  return engines;
}


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
  ok(frecencyForUrl(uri) > 10000, "Adeded URI should have expected high frecency");

  do_log_info("Check search domain is autoFilled even if there's an higher frecency match");
  yield check_autocomplete({
    search: "my",
    autofilled: "my.search.com",
    completed: "http://my.search.com"
  });

  yield cleanup();
});

add_task(function* test_searchEngine_noautoFill() {
  let engineName = "engine-rel-searchform.xml";
  let [engine] = yield addTestEngines([engineName]);
  do_register_cleanup(() => Services.search.removeEngine(engine));
  equal(engine.searchForm, "http://example.com/?search");

  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits(NetUtil.newURI("http://example.com/my/"));

  do_print("Check search domain is not autoFilled if it matches a visited domain");
  yield check_autocomplete({
    search: "example",
    autofilled: "example.com/",
    completed: "example.com/"
  });

  yield cleanup();
});

