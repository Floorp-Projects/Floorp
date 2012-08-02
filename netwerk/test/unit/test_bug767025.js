/* -*- Mode: Javasript; indent-tab-mode: nil; js-indent-level: 2 -*- */
do_load_httpd_js();
/**
 * This is testcase do following steps to make sure bug767025 removing
 * files as expection.
 *
 * STEPS:
 *  - Schedule a offline cache update for app.manifest.
 *    - pages/foo1, pages/foo2, pages/foo3, and pages/foo4 are cached.
 *  - Activate pages/foo1
 *  - Doom pages/foo1, and pages/foo2.
 *    - pages/foo1 should keep alive while pages/foo2 was gone.
 *  - Activate pages/foo3
 *  - Evict all documents.
 *    - all documents except pages/foo1 are gone since pages/foo1 & pages/foo3
 *      are activated.
 */

Cu.import("resource://gre/modules/Services.jsm");

const kNS_OFFLINECACHEUPDATESERVICE_CONTRACTID =
  "@mozilla.org/offlinecacheupdate-service;1";
const kNS_CACHESERVICE_CONTRACTID =
  "@mozilla.org/network/cache-service;1";
const kNS_APPLICATIONCACHESERVICE_CONTRACTID =
  "@mozilla.org/network/application-cache-service;1";

const kManifest = "CACHE MANIFEST\n" +
  "/pages/foo1\n" +
  "/pages/foo2\n" +
  "/pages/foo3\n" +
  "/pages/foo4\n";

const kDataFileSize = 1024;	// file size for each content page
const kHttpLocation = "http://localhost:4444/";

function manifest_handler(metadata, response) {
  do_print("manifest\n");
  response.setHeader("content-type", "text/cache-manifest");

  response.write(kManifest);
}

function datafile_handler(metadata, response) {
  do_print("datafile_handler\n");
  let data = "";

  while(data.length < kDataFileSize) {
    data = data + Math.random().toString(36).substring(2, 15);
  }

  response.setHeader("content-type", "text/plain");
  response.write(data.substring(0, kDataFileSize));
}

function app_handler(metadata, response) {
  do_print("app_handler\n");
  response.setHeader("content-type", "text/html");

  response.write("<html></html>");
}

var httpServer;

function init_profile() {
  var ps = Cc["@mozilla.org/preferences-service;1"]
    .getService(Ci.nsIPrefBranch);
  dump(ps.getBoolPref("browser.cache.offline.enable"));
  ps.setBoolPref("browser.cache.offline.enable", true);
  ps.setComplexValue("browser.cache.offline.parent_directory",
		     Ci.nsILocalFile, do_get_profile());
  do_print("profile " + do_get_profile());
}

function init_http_server() {
  httpServer = new nsHttpServer();
  httpServer.registerPathHandler("/app.appcache", manifest_handler);
  httpServer.registerPathHandler("/app", app_handler);
  for (i = 1; i <= 4; i++) {
    httpServer.registerPathHandler("/pages/foo" + i, datafile_handler);
  }
  httpServer.start(4444);
}

function clean_app_cache() {
  let cache_service = Cc[kNS_CACHESERVICE_CONTRACTID].
    getService(Ci.nsICacheService);
  cache_service.evictEntries(Ci.nsICache.STORE_OFFLINE);
}

function do_app_cache(manifestURL, pageURL) {
  let update_service = Cc[kNS_OFFLINECACHEUPDATESERVICE_CONTRACTID].
    getService(Ci.nsIOfflineCacheUpdateService);

  Services.perms.add(manifestURL,
		     "offline-app",
                     Ci.nsIPermissionManager.ALLOW_ACTION);

  let update =
    update_service.scheduleUpdate(manifestURL,
                                  pageURL,
                                  null); /* no window */

  return update;
}

function watch_update(update, stateChangeHandler, cacheAvailHandler) {
  let observer = {
    QueryInterface: function QueryInterface(iftype) {
      return this;
    },

    updateStateChanged: stateChangeHandler,
    applicationCacheAvailable: cacheAvailHandler
  };~
  update.addObserver(observer, false);

  return update;
}

function start_and_watch_app_cache(manifestURL,
                                 pageURL,
                                 stateChangeHandler,
                                 cacheAvailHandler) {
  let ioService = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);
  let update = do_app_cache(ioService.newURI(manifestURL, null, null),
                            ioService.newURI(pageURL, null, null));
  watch_update(update, stateChangeHandler, cacheAvailHandler);
  return update;
}

const {STATE_FINISHED: STATE_FINISHED,
       STATE_CHECKING: STATE_CHECKING,
       STATE_ERROR: STATE_ERROR } = Ci.nsIOfflineCacheUpdateObserver;

/*
 * Start caching app1 as a non-pinned app.
 */
function start_cache_nonpinned_app() {
  do_print("Start non-pinned App1");
  start_and_watch_app_cache(kHttpLocation + "app.appcache",
                          kHttpLocation + "app",
                          function (update, state) {
                            switch(state) {
                            case STATE_FINISHED:
			      check_bug();
                              break;

                            case STATE_ERROR:
                              do_throw("App cache state = " + state);
                              break;
                            }
                          },
                          function (appcahe) {
                            do_print("app avail " + appcache + "\n");
                          });
}

var hold_entry_foo1 = null;

function check_bug() {
  let appcache_service = Cc[kNS_APPLICATIONCACHESERVICE_CONTRACTID].
    getService(Ci.nsIApplicationCacheService);
  let appcache =
    appcache_service.chooseApplicationCache(kHttpLocation + "pages/foo1");
  let clientID = appcache.clientID;
  // activate foo1
  asyncOpenCacheEntry(
    kHttpLocation + "pages/foo1",
    clientID,
    Ci.nsICache.STORE_OFFLINE,
    Ci.nsICache.ACCESS_READ,
    function(status, entry) {
      var count = 0;

      let listener = {
        onCacheEntryDoomed: function (status) {
          do_print("doomed " + count);
          do_check_eq(status, 0);
          count = count + 1;
          do_check_true(count <= 2, "too much callbacks");
          if (count == 2) {
            do_timeout(100, function () { check_evict_cache(clientID); });
          }
        },
      };

      let session = get_cache_service().createSession(clientID,
                                                      Ci.nsICache.STORE_OFFLINE,
                                                      Ci.nsICache.STREAM_BASED);

      // Doom foo1 & foo2
      session.doomEntry(kHttpLocation + "pages/foo1", listener);
      session.doomEntry(kHttpLocation + "pages/foo2", listener);

      hold_entry_foo1 = entry;
    });
}

function check_evict_cache(clientID) {
  // Only foo2 should be removed.
  let file = do_get_profile().clone();
  file.append("OfflineCache");
  file.append("5");
  file.append("9");
  file.append("8379C6596B8CA4-0");
  do_check_eq(file.exists(), true);

  file = do_get_profile().clone();
  file.append("OfflineCache");
  file.append("C");
  file.append("2");
  file.append("5F356A168B5E3B-0");
  do_check_eq(file.exists(), false);

  // activate foo3
  asyncOpenCacheEntry(
    kHttpLocation + "pages/foo3",
    clientID,
    Ci.nsICache.STORE_OFFLINE,
    Ci.nsICache.ACCESS_READ,
    function(status, entry) {
      hold_entry_foo3 = entry;

      // evict all documents.
      get_cache_service().createSession(clientID,
                                        Ci.nsICache.STORE_OFFLINE,
                                        Ci.nsICache.STREAM_BASED).
                          evictEntries();

      // All documents are removed except foo1 & foo3.

      // foo1
      let file = do_get_profile().clone();
      file.append("OfflineCache");
      file.append("5");
      file.append("9");
      file.append("8379C6596B8CA4-0");
      do_check_eq(file.exists(), true);

      file = do_get_profile().clone();
      file.append("OfflineCache");
      file.append("0");
      file.append("0");
      file.append("61FEE819921D39-0");
      do_check_eq(file.exists(), false);

      file = do_get_profile().clone();
      file.append("OfflineCache");
      file.append("3");
      file.append("9");
      file.append("0D8759F1DE5452-0");
      do_check_eq(file.exists(), false);

      file = do_get_profile().clone();
      file.append("OfflineCache");
      file.append("C");
      file.append("2");
      file.append("5F356A168B5E3B-0");
      do_check_eq(file.exists(), false);

      // foo3
      file = do_get_profile().clone();
      file.append("OfflineCache");
      file.append("D");
      file.append("C");
      file.append("1ADCCC843B5C00-0");
      do_check_eq(file.exists(), true);

      file = do_get_profile().clone();
      file.append("OfflineCache");
      file.append("F");
      file.append("0");
      file.append("FC3E6D6C1164E9-0");
      do_check_eq(file.exists(), false);

      httpServer.stop(do_test_finished);
    });
}

function run_test() {
  if (typeof _XPCSHELL_PROCESS == "undefined" ||
      _XPCSHELL_PROCESS != "child") {
    init_profile();
    clean_app_cache();
  }

  init_http_server();
  start_cache_nonpinned_app();
  do_test_pending();
}
