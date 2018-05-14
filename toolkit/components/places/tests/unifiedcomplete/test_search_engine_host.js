/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_searchEngine_autoFill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  Services.search.addEngineWithDetails("MySearchEngine", "", "", "",
                                       "GET", "http://my.search.com/");
  let engine = Services.search.getEngineByName("MySearchEngine");
  registerCleanupFunction(() => Services.search.removeEngine(engine));

  // Add an uri that matches the search string with high frecency.
  let uri = NetUtil.newURI("http://www.example.com/my/");
  let visits = [];
  for (let i = 0; i < 100; ++i) {
    visits.push({ uri, title: "Terms - SearchEngine Search" });
  }
  await PlacesTestUtils.addVisits(visits);
  await addBookmark({ uri, title: "Example bookmark" });
  await PlacesTestUtils.promiseAsyncUpdates();
  ok(frecencyForUrl(uri) > 10000, "Added URI should have expected high frecency");

  info("Check search domain is autoFilled even if there's an higher frecency match");
  await check_autocomplete({
    search: "my",
    autofilled: "my.search.com/",
    completed: "http://my.search.com/"
  });

  await cleanup();
});

add_task(async function test_searchEngine_noautoFill() {
  let engineName = "engine-rel-searchform.xml";
  let engine = await addTestEngine(engineName);
  equal(engine.searchForm, "http://example.com/?search");

  await PlacesTestUtils.addVisits(NetUtil.newURI("http://example.com/my/"));

  info("Check search domain is not autoFilled if it matches a visited domain");
  await check_autocomplete({
    search: "example",
    autofilled: "example.com/",
    completed: "http://example.com/"
  });

  await cleanup();
});

