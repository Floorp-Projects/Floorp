/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const gAppRep = Cc[
  "@mozilla.org/reputationservice/application-reputation-service;1"
].getService(Ci.nsIApplicationReputationService);

const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

var gHttpServ = null;
var gTables = {};
var gExpectedRemote = false;
var gExpectedRemoteRequestBody = "";

var whitelistedURI = createURI("http://foo:bar@whitelisted.com/index.htm#junk");
var exampleURI = createURI("http://user:password@example.com/i.html?foo=bar");
var exampleReferrerURI = createURI(
  "http://user:password@example.referrer.com/i.html?foo=bar"
);
var exampleRedirectURI = createURI(
  "http://user:password@example.redirect.com/i.html?foo=bar"
);
var blocklistedURI = createURI("http://baz:qux@blocklisted.com?xyzzy");

var binaryFile = "binaryFile.exe";
var nonBinaryFile = "nonBinaryFile.txt";

const appRepURLPref = "browser.safebrowsing.downloads.remote.url";

function createReferrerInfo(
  aURI,
  aRefererPolicy = Ci.nsIReferrerInfo.NO_REFERRER
) {
  return new ReferrerInfo(aRefererPolicy, true, aURI);
}

function readFileToString(aFilename) {
  let f = do_get_file(aFilename);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
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

add_task(async function test_setup() {
  // Set up a local HTTP server to return bad verdicts.
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  // Ensure safebrowsing is enabled for this test, even if the app
  // doesn't have it enabled.
  Services.prefs.setBoolPref("browser.safebrowsing.malware.enabled", true);
  Services.prefs.setBoolPref("browser.safebrowsing.downloads.enabled", true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.safebrowsing.malware.enabled");
    Services.prefs.clearUserPref("browser.safebrowsing.downloads.enabled");
  });

  // Set block and allow tables explicitly, since the allowlist is normally
  // disabled on non-Windows platforms.
  Services.prefs.setCharPref(
    "urlclassifier.downloadBlockTable",
    "goog-badbinurl-shavar"
  );
  Services.prefs.setCharPref(
    "urlclassifier.downloadAllowTable",
    "goog-downloadwhite-digest256"
  );
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("urlclassifier.downloadBlockTable");
    Services.prefs.clearUserPref("urlclassifier.downloadAllowTable");
  });

  gHttpServ = new HttpServer();
  gHttpServ.registerDirectory("/", do_get_cwd());
  gHttpServ.registerPathHandler("/download", function(request, response) {
    if (gExpectedRemote) {
      let body = NetUtil.readInputStreamToString(
        request.bodyInputStream,
        request.bodyInputStream.available()
      );
      Assert.equal(gExpectedRemoteRequestBody, body);
    } else {
      do_throw("This test should never make a remote lookup");
    }
  });
  gHttpServ.start(4444);

  registerCleanupFunction(function() {
    return (async function() {
      await new Promise(resolve => {
        gHttpServ.stop(resolve);
      });
    })();
  });
});

add_test(function test_nullSourceURI() {
  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.reason, InternalError, 1);

  gAppRep.queryReputation(
    {
      // No source URI
      fileSize: 12,
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_ERROR_UNEXPECTED, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);
      run_next_test();
    }
  );
});

add_test(function test_nullCallback() {
  let expected = get_telemetry_snapshot();

  try {
    gAppRep.queryReputation(
      {
        sourceURI: createURI("http://example.com"),
        fileSize: 12,
      },
      null
    );
    do_throw("Callback cannot be null");
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_INVALID_POINTER) {
      throw ex;
    }
    // We don't even increment the count here, because there's no callback.
    check_telemetry(expected);
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
    info("Returning update response: " + response);
    return response;
  }
  gHttpServ.registerPathHandler("/downloads", function(request, response) {
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

  // Load up some update chunks for the safebrowsing server to serve.
  // This chunk contains the hash of blocklisted.com/.
  registerTableUpdate("goog-badbinurl-shavar", "data/block_digest.chunk");
  // This chunk contains the hash of whitelisted.com/.
  registerTableUpdate("goog-downloadwhite-digest256", "data/digest.chunk");

  // Download some updates, and don't continue until the downloads are done.
  function updateSuccess(aEvent) {
    // Timeout of n:1000 is constructed in processUpdateRequest above and
    // passed back in the callback in nsIUrlClassifierStreamUpdater on success.
    Assert.equal("1000", aEvent);
    info("All data processed");
    run_next_test();
  }
  // Just throw if we ever get an update or download error.
  function handleError(aEvent) {
    do_throw("We didn't download or update correctly: " + aEvent);
  }
  streamUpdater.downloadUpdates(
    "goog-downloadwhite-digest256,goog-badbinurl-shavar",
    "goog-downloadwhite-digest256,goog-badbinurl-shavar;\n",
    true, // isPostRequest.
    "http://localhost:4444/downloads",
    updateSuccess,
    handleError,
    handleError
  );
});

add_test(function test_unlisted() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, NO_LIST, 1);
  add_telemetry_count(expected.reason, NonBinaryFile, 1);

  gAppRep.queryReputation(
    {
      sourceURI: exampleURI,
      fileSize: 12,
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);
      run_next_test();
    }
  );
});

add_test(function test_non_uri() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.reason, NonBinaryFile, 1);

  // No listcount is incremented, since the sourceURI is not an nsIURL
  let source = NetUtil.newURI("data:application/octet-stream,ABC");
  Assert.equal(false, source instanceof Ci.nsIURL);
  gAppRep.queryReputation(
    {
      sourceURI: source,
      fileSize: 12,
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);
      run_next_test();
    }
  );
});

add_test(function test_local_blacklist() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  expected.shouldBlock++;
  add_telemetry_count(expected.local, BLOCK_LIST, 1);
  add_telemetry_count(expected.reason, LocalBlocklist, 1);

  gAppRep.queryReputation(
    {
      sourceURI: blocklistedURI,
      fileSize: 12,
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(aShouldBlock);
      check_telemetry(expected);

      run_next_test();
    }
  );
});

add_test(async function test_referer_blacklist() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let testReferrerPolicies = [
    Ci.nsIReferrerInfo.EMPTY,
    Ci.nsIReferrerInfo.NO_REFERRER,
    Ci.nsIReferrerInfo.NO_REFERRER_WHEN_DOWNGRADE,
    Ci.nsIReferrerInfo.ORIGIN,
    Ci.nsIReferrerInfo.ORIGIN_WHEN_CROSS_ORIGIN,
    Ci.nsIReferrerInfo.UNSAFE_URL,
    Ci.nsIReferrerInfo.SAME_ORIGIN,
    Ci.nsIReferrerInfo.STRICT_ORIGIN,
    Ci.nsIReferrerInfo.STRICT_ORIGIN_WHEN_CROSS_ORIGIN,
  ];

  function runReferrerPolicyTest(referrerPolicy) {
    return new Promise(resolve => {
      let expected = get_telemetry_snapshot();
      expected.shouldBlock++;
      add_telemetry_count(expected.local, BLOCK_LIST, 1);
      add_telemetry_count(expected.local, NO_LIST, 1);
      add_telemetry_count(expected.reason, LocalBlocklist, 1);

      gAppRep.queryReputation(
        {
          sourceURI: exampleURI,
          referrerInfo: createReferrerInfo(blocklistedURI, referrerPolicy),
          fileSize: 12,
        },
        function onComplete(aShouldBlock, aStatus) {
          Assert.equal(Cr.NS_OK, aStatus);
          Assert.ok(aShouldBlock);
          check_telemetry(expected);
          resolve();
        }
      );
    });
  }

  // We run tests with referrer policies but download protection should use
  // "full URL" original referrer to block the download
  for (let i = 0; i < testReferrerPolicies.length; ++i) {
    await runReferrerPolicyTest(testReferrerPolicies[i]);
  }

  run_next_test();
});

add_test(function test_blocklist_trumps_allowlist() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  expected.shouldBlock++;
  add_telemetry_count(expected.local, BLOCK_LIST, 1);
  add_telemetry_count(expected.local, ALLOW_LIST, 1);
  add_telemetry_count(expected.reason, LocalBlocklist, 1);

  gAppRep.queryReputation(
    {
      sourceURI: whitelistedURI,
      referrerInfo: createReferrerInfo(blocklistedURI),
      suggestedFileName: binaryFile,
      fileSize: 12,
      signatureInfo: [],
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(aShouldBlock);
      check_telemetry(expected);

      run_next_test();
    }
  );
});

add_test(function test_redirect_on_blocklist() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  expected.shouldBlock++;
  add_telemetry_count(expected.local, BLOCK_LIST, 1);
  add_telemetry_count(expected.local, ALLOW_LIST, 1);
  add_telemetry_count(expected.local, NO_LIST, 1);
  add_telemetry_count(expected.reason, LocalBlocklist, 1);
  let secman = Services.scriptSecurityManager;
  let badRedirects = Cc["@mozilla.org/array;1"].createInstance(
    Ci.nsIMutableArray
  );

  let redirect1 = {
    QueryInterface: ChromeUtils.generateQI(["nsIRedirectHistoryEntry"]),
    principal: secman.createContentPrincipal(exampleURI, {}),
  };
  badRedirects.appendElement(redirect1);

  let redirect2 = {
    QueryInterface: ChromeUtils.generateQI(["nsIRedirectHistoryEntry"]),
    principal: secman.createContentPrincipal(blocklistedURI, {}),
  };
  badRedirects.appendElement(redirect2);

  // Add a whitelisted URI that will not be looked up against the
  // whitelist (i.e. it will match NO_LIST).
  let redirect3 = {
    QueryInterface: ChromeUtils.generateQI(["nsIRedirectHistoryEntry"]),
    principal: secman.createContentPrincipal(whitelistedURI, {}),
  };
  badRedirects.appendElement(redirect3);

  gAppRep.queryReputation(
    {
      sourceURI: whitelistedURI,
      referrerInfo: createReferrerInfo(exampleURI),
      redirects: badRedirects,
      suggestedFileName: binaryFile,
      fileSize: 12,
      signatureInfo: [],
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(aShouldBlock);
      check_telemetry(expected);
      run_next_test();
    }
  );
});

add_test(function test_whitelisted_source() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, ALLOW_LIST, 1);
  add_telemetry_count(expected.reason, LocalWhitelist, 1);
  gAppRep.queryReputation(
    {
      sourceURI: whitelistedURI,
      suggestedFileName: binaryFile,
      fileSize: 12,
      signatureInfo: [],
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);
      run_next_test();
    }
  );
});

add_test(function test_whitelisted_non_binary_source() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, NO_LIST, 1);
  add_telemetry_count(expected.reason, NonBinaryFile, 1);
  gAppRep.queryReputation(
    {
      sourceURI: whitelistedURI,
      suggestedFileName: nonBinaryFile,
      fileSize: 12,
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);
      run_next_test();
    }
  );
});

add_test(function test_whitelisted_referrer() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, NO_LIST, 2);
  add_telemetry_count(expected.reason, NonBinaryFile, 1);

  gAppRep.queryReputation(
    {
      sourceURI: exampleURI,
      referrerInfo: createReferrerInfo(exampleURI),
      fileSize: 12,
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);
      run_next_test();
    }
  );
});

add_test(function test_whitelisted_redirect() {
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, NO_LIST, 3);
  add_telemetry_count(expected.reason, NonBinaryFile, 1);
  let secman = Services.scriptSecurityManager;
  let okayRedirects = Cc["@mozilla.org/array;1"].createInstance(
    Ci.nsIMutableArray
  );

  let redirect1 = {
    QueryInterface: ChromeUtils.generateQI(["nsIRedirectHistoryEntry"]),
    principal: secman.createContentPrincipal(exampleURI, {}),
  };
  okayRedirects.appendElement(redirect1);

  // Add a whitelisted URI that will not be looked up against the
  // whitelist (i.e. it will match NO_LIST).
  let redirect2 = {
    QueryInterface: ChromeUtils.generateQI(["nsIRedirectHistoryEntry"]),
    principal: secman.createContentPrincipal(whitelistedURI, {}),
  };
  okayRedirects.appendElement(redirect2);

  gAppRep.queryReputation(
    {
      sourceURI: exampleURI,
      redirects: okayRedirects,
      fileSize: 12,
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);
      run_next_test();
    }
  );
});

add_test(function test_remote_lookup_protocolbuf() {
  // This long hard-coded string is the contents of the request generated by
  // the Application Reputation component, converted to the binary protobuf format.
  // If this test is changed, or we add anything to the remote lookup requests
  // in ApplicationReputation.cpp, then we'll need to update this hard-coded string.
  gExpectedRemote = true;
  gExpectedRemoteRequestBody =
    "\x0A\x19\x68\x74\x74\x70\x3A\x2F\x2F\x65\x78\x61\x6D\x70\x6C\x65\x2E\x63\x6F\x6D\x2F\x69\x2E\x68\x74\x6D\x6C\x12\x22\x0A\x20\x61\x62\x63\x00\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x64\x65\x18\x0C\x22\x41\x0A\x19\x68\x74\x74\x70\x3A\x2F\x2F\x65\x78\x61\x6D\x70\x6C\x65\x2E\x63\x6F\x6D\x2F\x69\x2E\x68\x74\x6D\x6C\x10\x00\x22\x22\x68\x74\x74\x70\x3A\x2F\x2F\x65\x78\x61\x6D\x70\x6C\x65\x2E\x72\x65\x66\x65\x72\x72\x65\x72\x2E\x63\x6F\x6D\x2F\x69\x2E\x68\x74\x6D\x6C\x22\x26\x0A\x22\x68\x74\x74\x70\x3A\x2F\x2F\x65\x78\x61\x6D\x70\x6C\x65\x2E\x72\x65\x64\x69\x72\x65\x63\x74\x2E\x63\x6F\x6D\x2F\x69\x2E\x68\x74\x6D\x6C\x10\x01\x30\x01\x4A\x0E\x62\x69\x6E\x61\x72\x79\x46\x69\x6C\x65\x2E\x65\x78\x65\x50\x00\x5A\x05\x65\x6E\x2D\x55\x53";
  Services.prefs.setCharPref(appRepURLPref, "http://localhost:4444/download");
  let secman = Services.scriptSecurityManager;
  let expected = get_telemetry_snapshot();
  add_telemetry_count(expected.local, NO_LIST, 3);
  add_telemetry_count(expected.reason, VerdictSafe, 1);

  // Redirects
  let redirects = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  let redirect1 = {
    QueryInterface: ChromeUtils.generateQI(["nsIRedirectHistoryEntry"]),
    principal: secman.createContentPrincipal(exampleRedirectURI, {}),
  };
  redirects.appendElement(redirect1);

  // Insert null(\x00) in the middle of the hash to test we won't truncate it.
  let sha256Hash = "abc\x00" + "de".repeat(14);

  gAppRep.queryReputation(
    {
      sourceURI: exampleURI,
      referrerInfo: createReferrerInfo(exampleReferrerURI),
      suggestedFileName: binaryFile,
      sha256Hash,
      redirects,
      fileSize: 12,
      signatureInfo: [],
    },
    function onComplete(aShouldBlock, aStatus) {
      Assert.equal(Cr.NS_OK, aStatus);
      Assert.ok(!aShouldBlock);
      check_telemetry(expected);

      gExpectedRemote = false;
      run_next_test();
    }
  );
});
