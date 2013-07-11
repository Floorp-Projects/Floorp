/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * test_save_sorted_engines: Start search engine
 * - without search-metadata.json
 * - without search.sqlite
 *
 * Ensure that search-metadata.json is correct after:
 * - moving an engine
 * - removing an engine
 * - adding a new engine
 *
 * Notes:
 * - we install the search engines of test "test_downloadAndAddEngines.js"
 * to ensure that this test is independent from locale, commercial agreements
 * and configuration of Firefox.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

function run_test() {
  do_print("Preparing test");
  removeMetadata();
  updateAppInfo();

  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());
  let baseUrl = "http://localhost:" + httpServer.identity.primaryPort;

  function getSearchMetadata() {
    // Check that search-metadata.json has been created
    let metadata = gProfD.clone();
    metadata.append("search-metadata.json");
    do_check_true(metadata.exists());

    let stream = NetUtil.newChannel(metadata).open();
    do_print("Parsing metadata");
    let json = parseJsonFromStream(stream);
    stream.close(); // Stream must be closed under Windows

    return json;
  }

  let search = Services.search;

  do_print("Setting up observer");
  function observer(aSubject, aTopic, aData) {
    do_print("Observing topic " + aTopic);
    if ("engine-added" != aData) {
      return;
    }

    let engine1 = search.getEngineByName("Test search engine");
    let engine2 = search.getEngineByName("Sherlock test search engine");
    do_print("Currently, engine1 is " + engine1);
    do_print("Currently, engine2 is " + engine2);
    if (!engine1 || !engine2) {
      return;
    }

    // Test moving the engines
    search.moveEngine(engine1, 0);
    search.moveEngine(engine2, 1);

    // Changes should be commited immediately
    afterCommit(function() {
      do_print("Commit complete after moveEngine");

      // Check that the entries are placed as specified correctly
      let json = getSearchMetadata();
      do_check_eq(json["[app]/test-search-engine.xml"].order, 1);
      do_check_eq(json["[profile]/sherlock-test-search-engine.xml"].order, 2);

      // Test removing an engine
      search.removeEngine(engine1);
      afterCommit(function() {
        do_print("Commit complete after removeEngine");

        // Check that the order of the remaining engine was updated correctly
        let json = getSearchMetadata();
        do_check_eq(json["[profile]/sherlock-test-search-engine.xml"].order, 1);

        // Test adding a new engine
        search.addEngineWithDetails("foo", "", "foo", "", "GET", "http://searchget/?search={searchTerms}");
        afterCommit(function() {
          do_print("Commit complete after addEngineWithDetails");

          // Check that engine was added to the list of sorted engines
          let json = getSearchMetadata();
          do_check_eq(json["[profile]/foo.xml"].alias, "foo");
          do_check_true(json["[profile]/foo.xml"].order > 0);

          do_print("Cleaning up");
          Services.obs.removeObserver(observer, "browser-search-engine-modified");
          httpServer.stop(function() {});
          removeMetadata();
          do_test_finished();
        });
      });
    });
  };
  Services.obs.addObserver(observer, "browser-search-engine-modified", false);

  do_test_pending();

  search.addEngine(baseUrl + "/data/engine.xml",
                   Ci.nsISearchEngine.DATA_XML,
                   null, false);
  search.addEngine(baseUrl + "/data/engine.src",
                   Ci.nsISearchEngine.DATA_TEXT,
                   baseUrl + "/data/ico-size-16x16-png.ico",
                   false);

  do_timeout(120000, function() {
    do_throw("Timeout");
  });
}
