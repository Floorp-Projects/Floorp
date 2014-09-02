/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import('resource://gre/modules/NetUtil.jsm');
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const gAppRep = Cc["@mozilla.org/downloads/application-reputation-service;1"].
                  getService(Ci.nsIApplicationReputationService);
let gHttpServ = null;
let gTables = {};

let ALLOW_LIST = 0;
let BLOCK_LIST = 1;
let NO_LIST = 2;

let whitelistedURI = createURI("http://foo:bar@whitelisted.com/index.htm#junk");
let exampleURI = createURI("http://user:password@example.com/i.html?foo=bar");
let blocklistedURI = createURI("http://baz:qux@blocklisted.com?xyzzy");

function readFileToString(aFilename) {
  let f = do_get_file(aFilename);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"]
                 .createInstance(Ci.nsIFileInputStream);
  stream.init(f, -1, 0, 0);
  let buf = NetUtil.readInputStreamToString(stream, stream.available());
  return buf;
}

// Registers a table for which to serve update chunks. Returns a promise that
// resolves when that chunk has been downloaded.
function registerTableUpdate(aTable, aFilename) {
  // If we haven't been given an update for this table yet, add it to the map
  if (!(aTable in gTables)) {
    gTables[aTable] = [];
  }

  // The number of chunks associated with this table.
  let numChunks = gTables[aTable].length + 1;
  let redirectPath = "/" + aTable + "-" + numChunks;
  let redirectUrl = "localhost:4444" + redirectPath;

  // Store redirect url for that table so we can return it later when we
  // process an update request.
  gTables[aTable].push(redirectUrl);

  gHttpServ.registerPathHandler(redirectPath, function(request, response) {
    do_print("Mock safebrowsing server handling request for " + redirectPath);
    let contents = readFileToString(aFilename);
    do_print("Length of " + aFilename + ": " + contents.length);
    response.setHeader("Content-Type",
                       "application/vnd.google.safebrowsing-update", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(contents, contents.length);
  });
}

function run_test() {
  // Set up a local HTTP server to return bad verdicts.
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL",
                             "http://localhost:4444/download");
  // Ensure safebrowsing is enabled for this test, even if the app
  // doesn't have it enabled.
  Services.prefs.setBoolPref("browser.safebrowsing.malware.enabled", true);
  Services.prefs.setBoolPref("browser.safebrowsing.downloads.enabled", true);
  do_register_cleanup(function() {
    Services.prefs.clearUserPref("browser.safebrowsing.malware.enabled");
    Services.prefs.clearUserPref("browser.safebrowsing.downloads.enabled");
  });

  // Set block and allow tables explicitly, since the allowlist is normally
  // disabled on non-Windows platforms.
  Services.prefs.setCharPref("urlclassifier.downloadBlockTable",
                             "goog-badbinurl-shavar");
  Services.prefs.setCharPref("urlclassifier.downloadAllowTable",
                             "goog-downloadwhite-digest256");
  do_register_cleanup(function() {
    Services.prefs.clearUserPref("urlclassifier.downloadBlockTable");
    Services.prefs.clearUserPref("urlclassifier.downloadAllowTable");
  });

  gHttpServ = new HttpServer();
  gHttpServ.registerDirectory("/", do_get_cwd());
  gHttpServ.registerPathHandler("/download", function(request, response) {
    do_throw("This test should never make a remote lookup");
  });
  gHttpServ.start(4444);

  run_next_test();
}

function check_telemetry(aCount,
                         aShouldBlockCount,
                         aListCounts) {
  let count = Cc["@mozilla.org/base/telemetry;1"]
                .getService(Ci.nsITelemetry)
                .getHistogramById("APPLICATION_REPUTATION_COUNT")
                .snapshot();
  do_check_eq(count.counts[1], aCount);
  let local = Cc["@mozilla.org/base/telemetry;1"]
                .getService(Ci.nsITelemetry)
                .getHistogramById("APPLICATION_REPUTATION_LOCAL")
                .snapshot();
  do_check_eq(local.counts[ALLOW_LIST], aListCounts[ALLOW_LIST],
              "Allow list counts don't match");
  do_check_eq(local.counts[BLOCK_LIST], aListCounts[BLOCK_LIST],
              "Block list counts don't match");
  do_check_eq(local.counts[NO_LIST], aListCounts[NO_LIST],
              "No list counts don't match");

  let shouldBlock = Cc["@mozilla.org/base/telemetry;1"]
                .getService(Ci.nsITelemetry)
                .getHistogramById("APPLICATION_REPUTATION_SHOULD_BLOCK")
                .snapshot();
  // SHOULD_BLOCK = true
  do_check_eq(shouldBlock.counts[1], aShouldBlockCount);
  // Sanity check that SHOULD_BLOCK total adds up to the COUNT.
  do_check_eq(shouldBlock.counts[0] + shouldBlock.counts[1], aCount);
}

function get_telemetry_counts() {
  let count = Cc["@mozilla.org/base/telemetry;1"]
                .getService(Ci.nsITelemetry)
                .getHistogramById("APPLICATION_REPUTATION_COUNT")
                .snapshot();
  let local = Cc["@mozilla.org/base/telemetry;1"]
                .getService(Ci.nsITelemetry)
                .getHistogramById("APPLICATION_REPUTATION_LOCAL")
                .snapshot();
  let shouldBlock = Cc["@mozilla.org/base/telemetry;1"]
                .getService(Ci.nsITelemetry)
                .getHistogramById("APPLICATION_REPUTATION_SHOULD_BLOCK")
                .snapshot();
  return { total: count.counts[1],
           shouldBlock: shouldBlock.counts[1],
           listCounts: local.counts };
}

add_test(function test_nullSourceURI() {
  let counts = get_telemetry_counts();
  gAppRep.queryReputation({
    // No source URI
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_ERROR_UNEXPECTED, aStatus);
    do_check_false(aShouldBlock);
    check_telemetry(counts.total + 1, counts.shouldBlock, counts.listCounts);
    run_next_test();
  });
});

add_test(function test_nullCallback() {
  let counts = get_telemetry_counts();
  try {
    gAppRep.queryReputation({
      sourceURI: createURI("http://example.com"),
      fileSize: 12,
    }, null);
    do_throw("Callback cannot be null");
  } catch (ex if ex.result == Cr.NS_ERROR_INVALID_POINTER) {
    // We don't even increment the count here, because there's no callback.
    check_telemetry(counts.total, counts.shouldBlock, counts.listCounts);
    run_next_test();
  }
});

// Set up the local whitelist.
add_test(function test_local_list() {
  // Construct a response with redirect urls.
  function processUpdateRequest() {
    let response = "n:1000\n";
    for (let table in gTables) {
      response += "i:" + table + "\n";
      for (let i = 0; i < gTables[table].length; ++i) {
        response += "u:" + gTables[table][i] + "\n";
      }
    }
    do_print("Returning update response: " + response);
    return response;
  }
  gHttpServ.registerPathHandler("/downloads", function(request, response) {
    let buf = NetUtil.readInputStreamToString(request.bodyInputStream,
      request.bodyInputStream.available());
    let blob = processUpdateRequest();
    response.setHeader("Content-Type",
                       "application/vnd.google.safebrowsing-update", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(blob, blob.length);
  });

  let streamUpdater = Cc["@mozilla.org/url-classifier/streamupdater;1"]
    .getService(Ci.nsIUrlClassifierStreamUpdater);

  // Load up some update chunks for the safebrowsing server to serve.
  // This chunk contains the hash of blocklisted.com/.
  registerTableUpdate("goog-badbinurl-shavar", "data/block_digest.chunk");
  // This chunk contains the hash of whitelisted.com/.
  registerTableUpdate("goog-downloadwhite-digest256", "data/digest.chunk");

  // Download some updates, and don't continue until the downloads are done.
  function updateSuccess(aEvent) {
    // Timeout of n:1000 is constructed in processUpdateRequest above and
    // passed back in the callback in nsIUrlClassifierStreamUpdater on success.
    do_check_eq("1000", aEvent);
    do_print("All data processed");
    run_next_test();
  }
  // Just throw if we ever get an update or download error.
  function handleError(aEvent) {
    do_throw("We didn't download or update correctly: " + aEvent);
  }
  streamUpdater.downloadUpdates(
    "goog-downloadwhite-digest256,goog-badbinurl-shavar",
    "goog-downloadwhite-digest256,goog-badbinurl-shavar;\n",
    "http://localhost:4444/downloads",
    updateSuccess, handleError, handleError);
});

add_test(function test_unlisted() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL",
                             "http://localhost:4444/download");
  let counts = get_telemetry_counts();
  let listCounts = counts.listCounts;
  listCounts[NO_LIST]++;
  gAppRep.queryReputation({
    sourceURI: exampleURI,
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_OK, aStatus);
    do_check_false(aShouldBlock);
    check_telemetry(counts.total + 1, counts.shouldBlock, listCounts);
    run_next_test();
  });
});

add_test(function test_non_uri() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL",
                             "http://localhost:4444/download");
  let counts = get_telemetry_counts();
  let listCounts = counts.listCounts;
  // No listcount is incremented, since the sourceURI is not an nsIURL
  let source = NetUtil.newURI("data:application/octet-stream,ABC");
  do_check_false(source instanceof Ci.nsIURL);
  gAppRep.queryReputation({
    sourceURI: source,
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_OK, aStatus);
    do_check_false(aShouldBlock);
    check_telemetry(counts.total + 1, counts.shouldBlock, listCounts);
    run_next_test();
  });
});

add_test(function test_local_blacklist() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL",
                             "http://localhost:4444/download");
  let counts = get_telemetry_counts();
  let listCounts = counts.listCounts;
  listCounts[BLOCK_LIST]++;
  gAppRep.queryReputation({
    sourceURI: blocklistedURI,
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_OK, aStatus);
    do_check_true(aShouldBlock);
    check_telemetry(counts.total + 1, counts.shouldBlock + 1, listCounts);
    run_next_test();
  });
});

add_test(function test_referer_blacklist() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL",
                             "http://localhost:4444/download");
  let counts = get_telemetry_counts();
  let listCounts = counts.listCounts;
  listCounts[BLOCK_LIST]++;
  gAppRep.queryReputation({
    sourceURI: exampleURI,
    referrerURI: blocklistedURI,
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_OK, aStatus);
    do_check_true(aShouldBlock);
    check_telemetry(counts.total + 1, counts.shouldBlock + 1, listCounts);
    run_next_test();
  });
});

add_test(function test_blocklist_trumps_allowlist() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL",
                             "http://localhost:4444/download");
  let counts = get_telemetry_counts();
  let listCounts = counts.listCounts;
  listCounts[BLOCK_LIST]++;
  gAppRep.queryReputation({
    sourceURI: whitelistedURI,
    referrerURI: blocklistedURI,
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_OK, aStatus);
    do_check_true(aShouldBlock);
    check_telemetry(counts.total + 1, counts.shouldBlock + 1, listCounts);
    run_next_test();
  });
});

add_test(function test_redirect_on_blocklist() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL",
                             "http://localhost:4444/download");
  let counts = get_telemetry_counts();
  let listCounts = counts.listCounts;
  listCounts[BLOCK_LIST]++;
  listCounts[ALLOW_LIST]++;
  let secman = Services.scriptSecurityManager;
  let badRedirects = Cc["@mozilla.org/array;1"]
                       .createInstance(Ci.nsIMutableArray);
  badRedirects.appendElement(secman.getNoAppCodebasePrincipal(exampleURI),
                             false);
  badRedirects.appendElement(secman.getNoAppCodebasePrincipal(blocklistedURI),
                             false);
  badRedirects.appendElement(secman.getNoAppCodebasePrincipal(whitelistedURI),
                             false);
  gAppRep.queryReputation({
    sourceURI: whitelistedURI,
    referrerURI: exampleURI,
    redirects: badRedirects,
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_OK, aStatus);
    do_check_true(aShouldBlock);
    check_telemetry(counts.total + 1, counts.shouldBlock + 1, listCounts);
    run_next_test();
  });
});
