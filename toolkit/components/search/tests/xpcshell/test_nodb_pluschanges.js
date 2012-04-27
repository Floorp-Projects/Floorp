/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


/*
 * test_nodb: Start search engine
 * - without search-metadata.json
 * - without search.sqlite
 *
 * Ensure that :
 * - nothing explodes;
 * - if we change the order, search-metadata.json is created;
 * - this search-medata.json can be parsed;
 * - the order stored in search-metadata.json is consistent.
 *
 * Notes:
 * - we install the search engines of test "test_downloadAndAddEngines.js"
 * to ensure that this test is independent from locale, commercial agreements
 * and configuration of Firefox.
 */

do_load_httpd_js();

function run_test()
{
  do_print("Preparing test");
  removeMetadata();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");
  do_load_manifest("data/chrome.manifest");

  let httpServer = new nsHttpServer();
  httpServer.start(4444);
  httpServer.registerDirectory("/", do_get_cwd());

  let search = Services.search;

  do_print("Setting up observer");
  function observer(aSubject, aTopic, aData) {
    do_print("Observing topic " + aTopic);
    if ("engine-added" == aData) {
      let engine1 = search.getEngineByName("Test search engine");
      let engine2 = search.getEngineByName("Sherlock test search engine");
      do_print("Currently, engine1 is " + engine1);
      do_print("Currently, engine2 is " + engine2);
      if(engine1 && engine2)
      {
        search.moveEngine(engine1, 0);
        search.moveEngine(engine2, 1);
        do_print("Next step is forcing flush");
        do_timeout(0,
                   function() {
                     do_print("Forcing flush");
                     // Force flush
                     // Note: the timeout is needed, to avoid some reentrency
                     // issues in nsSearchService.
                     search.QueryInterface(Ci.nsIObserver).
                       observe(observer, "quit-application", "<no verb>");
                   });
        afterCommit(
          function()
          {
            do_print("Commit complete");
            // Check that search-metadata.json has been created
            let metadata = gProfD.clone();
            metadata.append("search-metadata.json");
            do_check_true(metadata.exists());

            // Check that the entries are placed as specified correctly
            let stream = NetUtil.newChannel(metadata).open();
            do_print("Parsing metadata");
            let json = parseJsonFromStream(stream);
            do_check_eq(json["[app]/test-search-engine.xml"].order, 1);
            do_check_eq(json["[profile]/sherlock-test-search-engine.xml"].order, 2);

            do_print("Cleaning up");
            httpServer.stop(function() {});
            stream.close(); // Stream must be closed under Windows
            removeMetadata();
            do_test_finished();
          }
        );
      }
    }
  };
  Services.obs.addObserver(observer, "browser-search-engine-modified",
                           false);

  do_test_pending();

  search.addEngine("http://localhost:4444/data/engine.xml",
                   Ci.nsISearchEngine.DATA_XML,
                   null, false);
  search.addEngine("http://localhost:4444/data/engine.src",
                   Ci.nsISearchEngine.DATA_TEXT,
                   "http://localhost:4444/data/ico-size-16x16-png.ico",
                   false);

  do_timeout(120000, function() {
    do_throw("Timeout");
  });
}
