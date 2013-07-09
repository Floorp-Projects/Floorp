/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let Cu = Components.utils;
let Ci = Components.interfaces;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://testing-common/httpd.js");

/*
 * test_nocache: Start search engine
 * - without search.json
 *
 * Ensure that :
 * - nothing explodes;
 * - search.json is created.
 */


function run_test()
{
  removeCache();
  updateAppInfo();
  do_load_manifest("data/chrome.manifest");

  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());

  let search = Services.search;

  do_test_pending();

  // Check that cache is created at startup
  afterCache(function cacheCreated() {
    // Check that search.json has been created.
    let cache = gProfD.clone();
    cache.append("search.json");
    do_check_true(cache.exists());
  });

  // Perform initialization
  search.init(function ss_initialized(rv) {
    do_check_true(Components.isSuccessCode(rv));

    do_print("Setting up observer");
    function observer(aSubject, aTopic, aData) {
      do_print("Observing topic " + aTopic);
      if ("engine-added" == aData) {
        let engine = search.getEngineByName("Test search engine");
        if (!engine) {
          return;
        }
        Services.obs.removeObserver(observer, "browser-search-engine-modified");
        do_print("Engine has been added, let's wait for the cache to be built");
        afterCache(function() {
          do_print("Success");

          Task.spawn(function task() {
            do_print("Searching test engine in cache");
            try {
              let path = OS.Path.join(OS.Constants.Path.profileDir, "search.json");
              let data = yield OS.File.read(path);
              let text = new TextDecoder().decode(data);
              let cache = JSON.parse(text);
              let found = false;
              for (let dirName in cache.directories) {
                for (let engine of cache.directories[dirName].engines) {
                  if (engine._id == "[app]/test-search-engine.xml") {
                    found = true;
                    break;
                  }
                }
                if (found) {
                  break;
                }
              }
              do_check_true(found);
            } catch (ex) {
              do_throw(ex);
            } finally {
              removeCache();
              httpServer.stop(function() {
                // httpServer doesn't seem to stop cleanly if there is no callback
              });
              do_test_finished();
            }
          });
        });
      }
    };
    Services.obs.addObserver(observer, "browser-search-engine-modified", false);

    // Add an engine, check if it appears in the cache
    search.addEngine("http://localhost:" + httpServer.identity.primaryPort +
                     "/data/engine.xml",
                     Ci.nsISearchEngine.DATA_XML,
                     null, false);
  });
}
