/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * This testcase performs 3 requests against the offline cache.  They
 * are
 *
 *  - start_cache_nonpinned_app1()
 *
 *    - Request nsOfflineCacheService to skip pages (4) of app1 on
 *      the cache storage.
 *
 *    - The offline cache storage is empty at this monent.
 *
 *  - start_cache_nonpinned_app2_for_partial()
 *
 *     - Request nsOfflineCacheService to skip pages of app2 on the
 *       cache storage.
 *
 *     - The offline cache storage has only enough space for one more
 *       additional page.  Only first of pages is really in the cache.
 *
 *  - start_cache_pinned_app2_for_success()
 *
 *     - Request nsOfflineCacheService to skip pages of app2 on the
 *       cache storage.
 *
 *     - The offline cache storage has only enough space for one
 *       additional page.  But, this is a pinned request,
 *       nsOfflineCacheService will make more space for this request
 *       by discarding app1 (non-pinned)
 *
 */

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

// const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const kNS_OFFLINECACHEUPDATESERVICE_CONTRACTID =
  "@mozilla.org/offlinecacheupdate-service;1";

const kManifest1 =
  "CACHE MANIFEST\n" +
  "/pages/foo1\n" +
  "/pages/foo2\n" +
  "/pages/foo3\n" +
  "/pages/foo4\n";
const kManifest2 =
  "CACHE MANIFEST\n" +
  "/pages/foo5\n" +
  "/pages/foo6\n" +
  "/pages/foo7\n" +
  "/pages/foo8\n";

const kDataFileSize = 1024; // file size for each content page
const kCacheSize = kDataFileSize * 5; // total space for offline cache storage

XPCOMUtils.defineLazyGetter(this, "kHttpLocation", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + "/";
});

XPCOMUtils.defineLazyGetter(this, "kHttpLocation_ip", function() {
  return "http://127.0.0.1:" + httpServer.identity.primaryPort + "/";
});

function manifest1_handler(metadata, response) {
  info("manifest1\n");
  response.setHeader("content-type", "text/cache-manifest");

  response.write(kManifest1);
}

function manifest2_handler(metadata, response) {
  info("manifest2\n");
  response.setHeader("content-type", "text/cache-manifest");

  response.write(kManifest2);
}

function app_handler(metadata, response) {
  info("app_handler\n");
  response.setHeader("content-type", "text/html");

  response.write("<html></html>");
}

function datafile_handler(metadata, response) {
  info("datafile_handler\n");
  let data = "";

  while (data.length < kDataFileSize) {
    data =
      data +
      Math.random()
        .toString(36)
        .substring(2, 15);
  }

  response.setHeader("content-type", "text/plain");
  response.write(data.substring(0, kDataFileSize));
}

var httpServer;

function init_profile() {
  var ps = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );
  dump(ps.getBoolPref("browser.cache.offline.enable"));
  ps.setBoolPref("browser.cache.offline.enable", true);
  ps.setComplexValue(
    "browser.cache.offline.parent_directory",
    Ci.nsIFile,
    do_get_profile()
  );
}

function init_http_server() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/app1", app_handler);
  httpServer.registerPathHandler("/app2", app_handler);
  httpServer.registerPathHandler("/app1.appcache", manifest1_handler);
  httpServer.registerPathHandler("/app2.appcache", manifest2_handler);
  for (i = 1; i <= 8; i++) {
    httpServer.registerPathHandler("/pages/foo" + i, datafile_handler);
  }
  httpServer.start(-1);
}

function init_cache_capacity() {
  let prefs = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );
  prefs.setIntPref("browser.cache.offline.capacity", kCacheSize / 1024);
}

function clean_app_cache() {
  evict_cache_entries("appcache");
}

function do_app_cache(manifestURL, pageURL, pinned) {
  let update_service = Cc[kNS_OFFLINECACHEUPDATESERVICE_CONTRACTID].getService(
    Ci.nsIOfflineCacheUpdateService
  );

  PermissionTestUtils.add(
    manifestURL,
    "pin-app",
    pinned
      ? Ci.nsIPermissionManager.ALLOW_ACTION
      : Ci.nsIPermissionManager.DENY_ACTION
  );

  let update = update_service.scheduleUpdate(
    manifestURL,
    pageURL,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null
  ); /* no window */

  return update;
}

function watch_update(update, stateChangeHandler, cacheAvailHandler) {
  let observer = {
    QueryInterface: ChromeUtils.generateQI([]),

    updateStateChanged: stateChangeHandler,
    applicationCacheAvailable: cacheAvailHandler,
  };
  update.addObserver(observer);

  return update;
}

function start_and_watch_app_cache(
  manifestURL,
  pageURL,
  pinned,
  stateChangeHandler,
  cacheAvailHandler
) {
  let ioService = Cc["@mozilla.org/network/io-service;1"].getService(
    Ci.nsIIOService
  );
  let update = do_app_cache(
    ioService.newURI(manifestURL),
    ioService.newURI(pageURL),
    pinned
  );
  watch_update(update, stateChangeHandler, cacheAvailHandler);
  return update;
}

const {
  STATE_FINISHED: STATE_FINISHED,
  STATE_CHECKING: STATE_CHECKING,
  STATE_ERROR: STATE_ERROR,
} = Ci.nsIOfflineCacheUpdateObserver;

/*
 * Start caching app1 as a non-pinned app.
 */
function start_cache_nonpinned_app() {
  info("Start non-pinned App1");
  start_and_watch_app_cache(
    kHttpLocation + "app1.appcache",
    kHttpLocation + "app1",
    false,
    function(update, state) {
      switch (state) {
        case STATE_FINISHED:
          start_cache_nonpinned_app2_for_partial();
          break;

        case STATE_ERROR:
          do_throw("App1 cache state = " + state);
          break;
      }
    },
    function(appcahe) {
      info("app1 avail " + appcache + "\n");
    }
  );
}

/*
 * Start caching app2 as a non-pinned app.
 *
 * This cache request is supposed to be saved partially in the cache
 * storage for running out of the cache storage.  The offline cache
 * storage can hold 5 files at most.  (kDataFileSize bytes for each
 * file)
 */
function start_cache_nonpinned_app2_for_partial() {
  let error_count = [0];
  info("Start non-pinned App2 for partial\n");
  start_and_watch_app_cache(
    kHttpLocation_ip + "app2.appcache",
    kHttpLocation_ip + "app2",
    false,
    function(update, state) {
      switch (state) {
        case STATE_FINISHED:
          start_cache_pinned_app2_for_success();
          break;

        case STATE_ERROR:
          do_throw("App2 cache state = " + state);
          break;
      }
    },
    function(appcahe) {}
  );
}

/*
 * Start caching app2 as a pinned app.
 *
 * This request use IP address (127.0.0.1) as the host name instead of
 * the one used by app1.  Because, app1 is also pinned when app2 is
 * pinned if they have the same host name (localhost).
 */
function start_cache_pinned_app2_for_success() {
  let error_count = [0];
  info("Start pinned App2 for success\n");
  start_and_watch_app_cache(
    kHttpLocation_ip + "app2.appcache",
    kHttpLocation_ip + "app2",
    true,
    function(update, state) {
      switch (state) {
        case STATE_FINISHED:
          Assert.ok(error_count[0] == 0, "Do not discard app1?");
          httpServer.stop(do_test_finished);
          break;

        case STATE_ERROR:
          info("STATE_ERROR\n");
          error_count[0]++;
          break;
      }
    },
    function(appcahe) {
      info("app2 avail " + appcache + "\n");
    }
  );
}

function run_test() {
  if (typeof _XPCSHELL_PROCESS == "undefined" || _XPCSHELL_PROCESS != "child") {
    init_profile();
    init_cache_capacity();
    clean_app_cache();
  }

  init_http_server();
  start_cache_nonpinned_app();
  do_test_pending();
}
