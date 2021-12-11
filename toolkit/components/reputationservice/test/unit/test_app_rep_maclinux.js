/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Globals

ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);

const gAppRep = Cc[
  "@mozilla.org/reputationservice/application-reputation-service;1"
].getService(Ci.nsIApplicationReputationService);
var gStillRunning = true;
var gTables = {};
var gHttpServer = null;

const appRepURLPref = "browser.safebrowsing.downloads.remote.url";
const remoteEnabledPref = "browser.safebrowsing.downloads.remote.enabled";

function readFileToString(aFilename) {
  let f = do_get_file(aFilename);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  stream.init(f, -1, 0, 0);
  let buf = NetUtil.readInputStreamToString(stream, stream.available());
  return buf;
}

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

  gHttpServer.registerPathHandler(redirectPath, function(request, response) {
    info("Mock safebrowsing server handling request for " + redirectPath);
    let contents = readFileToString(aFilename);
    info("Length of " + aFilename + ": " + contents.length);
    response.setHeader(
      "Content-Type",
      "application/vnd.google.safebrowsing-update",
      false
    );
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(contents, contents.length);
  });
}

// Tests

add_task(function test_setup() {
  // Wait 10 minutes, that is half of the external xpcshell timeout.
  do_timeout(10 * 60 * 1000, function() {
    if (gStillRunning) {
      do_throw("Test timed out.");
    }
  });
  // Set up a local HTTP server to return bad verdicts.
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  // Ensure safebrowsing is enabled for this test, even if the app
  // doesn't have it enabled.
  Services.prefs.setBoolPref("browser.safebrowsing.malware.enabled", true);
  Services.prefs.setBoolPref("browser.safebrowsing.downloads.enabled", true);
  // Set block table explicitly, no need for the allow table though
  Services.prefs.setCharPref(
    "urlclassifier.downloadBlockTable",
    "goog-badbinurl-shavar"
  );
  // SendRemoteQueryInternal needs locale preference.
  let originalReqLocales = Services.locale.requestedLocales;
  Services.locale.requestedLocales = ["en-US"];

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.safebrowsing.malware.enabled");
    Services.prefs.clearUserPref("browser.safebrowsing.downloads.enabled");
    Services.prefs.clearUserPref("urlclassifier.downloadBlockTable");
    Services.locale.requestedLocales = originalReqLocales;
  });

  gHttpServer = new HttpServer();
  gHttpServer.registerDirectory("/", do_get_cwd());

  function createVerdict(aShouldBlock) {
    // We can't programmatically create a protocol buffer here, so just
    // hardcode some already serialized ones.
    let blob = String.fromCharCode(parseInt(0x08, 16));
    if (aShouldBlock) {
      // A safe_browsing::ClientDownloadRequest with a DANGEROUS verdict
      blob += String.fromCharCode(parseInt(0x01, 16));
    } else {
      // A safe_browsing::ClientDownloadRequest with a SAFE verdict
      blob += String.fromCharCode(parseInt(0x00, 16));
    }
    return blob;
  }

  gHttpServer.registerPathHandler("/throw", function(request, response) {
    do_throw("We shouldn't be getting here");
  });

  gHttpServer.registerPathHandler("/download", function(request, response) {
    info("Querying remote server for verdict");
    response.setHeader("Content-Type", "application/octet-stream", false);
    let buf = NetUtil.readInputStreamToString(
      request.bodyInputStream,
      request.bodyInputStream.available()
    );
    info("Request length: " + buf.length);
    // A garbage response. By default this produces NS_CANNOT_CONVERT_DATA as
    // the callback status.
    let blob =
      "this is not a serialized protocol buffer (the length doesn't match our hard-coded values)";
    // We can't actually parse the protocol buffer here, so just switch on the
    // length instead of inspecting the contents.
    if (buf.length == 67) {
      // evil.com
      blob = createVerdict(true);
    } else if (buf.length == 73) {
      // mozilla.com
      blob = createVerdict(false);
    }
    response.bodyOutputStream.write(blob, blob.length);
  });

  gHttpServer.start(4444);

  registerCleanupFunction(function() {
    return (async function() {
      await new Promise(resolve => {
        gHttpServer.stop(resolve);
      });
    })();
  });
});

// Construct a response with redirect urls.
function processUpdateRequest() {
  let response = "n:1000\n";
  for (let table in gTables) {
    response += "i:" + table + "\n";
    for (let i = 0; i < gTables[table].length; ++i) {
      response += "u:" + gTables[table][i] + "\n";
    }
  }
  info("Returning update response: " + response);
  return response;
}

// Set up the local whitelist.
function waitForUpdates() {
  return new Promise((resolve, reject) => {
    gHttpServer.registerPathHandler("/downloads", function(request, response) {
      let blob = processUpdateRequest();
      response.setHeader(
        "Content-Type",
        "application/vnd.google.safebrowsing-update",
        false
      );
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(blob, blob.length);
    });

    let streamUpdater = Cc[
      "@mozilla.org/url-classifier/streamupdater;1"
    ].getService(Ci.nsIUrlClassifierStreamUpdater);

    // Load up some update chunks for the safebrowsing server to serve. This
    // particular chunk contains the hash of whitelisted.com/ and
    // sb-ssl.google.com/safebrowsing/csd/certificate/.
    registerTableUpdate("goog-downloadwhite-digest256", "data/digest.chunk");

    // Resolve the promise once processing the updates is complete.
    function updateSuccess(aEvent) {
      // Timeout of n:1000 is constructed in processUpdateRequest above and
      // passed back in the callback in nsIUrlClassifierStreamUpdater on success.
      Assert.equal("1000", aEvent);
      info("All data processed");
      resolve(true);
    }
    // Just throw if we ever get an update or download error.
    function handleError(aEvent) {
      do_throw("We didn't download or update correctly: " + aEvent);
      reject();
    }
    streamUpdater.downloadUpdates(
      "goog-downloadwhite-digest256",
      "goog-downloadwhite-digest256;\n",
      true,
      "http://localhost:4444/downloads",
      updateSuccess,
      handleError,
      handleError
    );
  });
}

function promiseQueryReputation(query, expected) {
  return new Promise(resolve => {
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      check_telemetry(expected);
      resolve(true);
    }
    gAppRep.queryReputation(query, onComplete);
  });
}

add_task(async function() {
  // Wait for Safebrowsing local list updates to complete.
  await waitForUpdates();
});

add_task(async function test_blocked_binary() {
  // We should reach the remote server for a verdict.
  Services.prefs.setBoolPref(remoteEnabledPref, true);
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  expected.shouldBlock++;
  add_telemetry_count(expected.local, NO_LIST, 1);
  add_telemetry_count(expected.reason, VerdictDangerous, 1);

  // evil.com should return a malware verdict from the remote server.
  await promiseQueryReputation(
    {
      sourceURI: createURI("http://evil.com"),
      suggestedFileName: "noop.bat",
      fileSize: 12,
      signatureInfo: [],
    },
    expected
  );
});

add_task(async function test_non_binary() {
  // We should not reach the remote server for a verdict for non-binary files.
  Services.prefs.setBoolPref(remoteEnabledPref, true);
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/throw");

  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, NO_LIST, 1);
  add_telemetry_count(expected.reason, NonBinaryFile, 1);

  await promiseQueryReputation(
    {
      sourceURI: createURI("http://evil.com"),
      suggestedFileName: "noop.txt",
      fileSize: 12,
      signatureInfo: [],
    },
    expected
  );
});

add_task(async function test_good_binary() {
  // We should reach the remote server for a verdict.
  Services.prefs.setBoolPref(remoteEnabledPref, true);
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");

  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, NO_LIST, 1);
  add_telemetry_count(expected.reason, VerdictSafe, 1);

  // mozilla.com should return a not-guilty verdict from the remote server.
  await promiseQueryReputation(
    {
      sourceURI: createURI("http://mozilla.com"),
      suggestedFileName: "noop.bat",
      fileSize: 12,
      signatureInfo: [],
    },
    expected
  );
});

add_task(async function test_disabled() {
  // Explicitly disable remote checks
  Services.prefs.setBoolPref(remoteEnabledPref, false);
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/throw");

  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, NO_LIST, 1);
  add_telemetry_count(expected.reason, RemoteLookupDisabled, 1);

  let query = {
    sourceURI: createURI("http://example.com"),
    suggestedFileName: "noop.bat",
    fileSize: 12,
    signatureInfo: [],
  };
  await new Promise(resolve => {
    gAppRep.queryReputation(query, function onComplete(aShouldBlock, aStatus) {
      // We should be getting NS_ERROR_NOT_AVAILABLE if the service is disabled
      Assert.equal(Cr.NS_ERROR_NOT_AVAILABLE, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);
      resolve(true);
    });
  });
});

add_task(async function test_disabled_through_lists() {
  Services.prefs.setBoolPref(remoteEnabledPref, false);
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  Services.prefs.setCharPref("urlclassifier.downloadBlockTable", "");

  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, NO_LIST, 1);
  add_telemetry_count(expected.reason, RemoteLookupDisabled, 1);

  let query = {
    sourceURI: createURI("http://example.com"),
    suggestedFileName: "noop.bat",
    fileSize: 12,
    signatureInfo: [],
  };
  await new Promise(resolve => {
    gAppRep.queryReputation(query, function onComplete(aShouldBlock, aStatus) {
      // We should be getting NS_ERROR_NOT_AVAILABLE if the service is disabled
      Assert.equal(Cr.NS_ERROR_NOT_AVAILABLE, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);
      resolve(true);
    });
  });
});
add_task(async function test_teardown() {
  gStillRunning = false;
});
