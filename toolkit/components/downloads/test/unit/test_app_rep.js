/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  do_register_cleanup(function() {
    Services.prefs.clearUserPref("browser.safebrowsing.malware.enabled");
  });

  gHttpServ = new HttpServer();
  gHttpServ.registerDirectory("/", do_get_cwd());

  function createVerdict(aShouldBlock) {
    // We can't programmatically create a protocol buffer here, so just
    // hardcode some already serialized ones.
    blob = String.fromCharCode(parseInt(0x08, 16));
    if (aShouldBlock) {
      // A safe_browsing::ClientDownloadRequest with a DANGEROUS verdict
      blob += String.fromCharCode(parseInt(0x01, 16));
    } else {
      // A safe_browsing::ClientDownloadRequest with a SAFE verdict
      blob += String.fromCharCode(parseInt(0x00, 16));
    }
    return blob;
  }

  gHttpServ.registerPathHandler("/download", function(request, response) {
    response.setHeader("Content-Type", "application/octet-stream", false);
    let buf = NetUtil.readInputStreamToString(
      request.bodyInputStream,
      request.bodyInputStream.available());
    do_print("Request length: " + buf.length);
    // A garbage response. By default this produces NS_CANNOT_CONVERT_DATA as
    // the callback status.
    let blob = "this is not a serialized protocol buffer";
    // We can't actually parse the protocol buffer here, so just switch on the
    // length instead of inspecting the contents.
    if (buf.length == 35) {
      // evil.com
      blob = createVerdict(true);
    } else if (buf.length == 38) {
      // mozilla.com
      blob = createVerdict(false);
    }
    response.bodyOutputStream.write(blob, blob.length);
  });

  gHttpServ.start(4444);

  run_next_test();
}

/*
// Uncomment when remote lookups are enabled (bug 933432)
add_test(function test_shouldBlock() {
  gAppRep.queryReputation({
    sourceURI: createURI("http://evil.com"),
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_true(aShouldBlock);
    do_check_eq(Cr.NS_OK, aStatus);
    run_next_test();
  });
});

add_test(function test_shouldNotBlock() {
  gAppRep.queryReputation({
    sourceURI: createURI("http://mozilla.com"),
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_OK, aStatus);
    do_check_false(aShouldBlock);
    run_next_test();
  });
});
*/

add_test(function test_nullSourceURI() {
  gAppRep.queryReputation({
    // No source URI
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_ERROR_UNEXPECTED, aStatus);
    do_check_false(aShouldBlock);
    run_next_test();
  });
});

add_test(function test_nullCallback() {
  try {
    gAppRep.queryReputation({
      sourceURI: createURI("http://example.com"),
      fileSize: 12,
    }, null);
    do_throw("Callback cannot be null");
  } catch (ex if ex.result == Cr.NS_ERROR_INVALID_POINTER) {
    run_next_test();
  }
});

add_test(function test_disabled() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL", "");
  gAppRep.queryReputation({
    sourceURI: createURI("http://example.com"),
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    // We should be getting NS_ERROR_NOT_AVAILABLE if the service is disabled
    do_check_eq(Cr.NS_ERROR_NOT_AVAILABLE, aStatus);
    do_check_false(aShouldBlock);
    run_next_test();
  });
});

/*
// Uncomment when remote lookups are enabled (bug 933432)
add_test(function test_garbage() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL",
                             "http://localhost:4444/download");
  gAppRep.queryReputation({
    sourceURI: createURI("http://whitelisted.com"),
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    // We should be getting the garbage response.
    do_check_eq(Cr.NS_ERROR_CANNOT_CONVERT_DATA, aStatus);
    do_check_false(aShouldBlock);
    run_next_test();
  });
});
*/

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
  streamUpdater.updateUrl = "http://localhost:4444/downloads";

  // Load up some update chunks for the safebrowsing server to serve. This
  // particular chunk contains the hash of whitelisted.com/.
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
    "goog-downloadwhite-digest256",
    "goog-downloadwhite-digest256;\n",
    updateSuccess, handleError, handleError);
});

// After being whitelisted, we shouldn't throw.
add_test(function test_local_whitelist() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL",
                             "http://localhost:4444/download");
  gAppRep.queryReputation({
    sourceURI: createURI("http://whitelisted.com"),
    fileSize: 12,
  }, function onComplete(aShouldBlock, aStatus) {
    // We would get garbage if this query made it to the remote server.
    do_check_eq(Cr.NS_OK, aStatus);
    do_check_false(aShouldBlock);
    run_next_test();
  });
});
