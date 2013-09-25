/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import('resource://gre/modules/NetUtil.jsm');
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const ApplicationReputationQuery = Components.Constructor(
      "@mozilla.org/downloads/application-reputation-query;1",
      "nsIApplicationReputationQuery");

const gAppRep = Cc["@mozilla.org/downloads/application-reputation-service;1"].
                  getService(Ci.nsIApplicationReputationService);
let gHttpServ = null;

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
    // A garbage response.
    let blob = "this is not a serialized protocol buffer";
    // We can't actually parse the protocol buffer here, so just switch on the
    // length instead of inspecting the contents.
    if (buf.length == 35) {
      blob = createVerdict(true);
    } else if (buf.length == 38) {
      blob = createVerdict(false);
    }
    response.bodyOutputStream.write(blob, blob.length);
  });

  gHttpServ.start(4444);

  run_next_test();
}

add_test(function test_shouldBlock() {
  let query = new ApplicationReputationQuery();
  query.sourceURI = createURI("http://evil.com");
  query.fileSize = 12;

  gAppRep.queryReputation(query, function onComplete(aShouldBlock, aStatus) {
    do_check_true(aShouldBlock);
    do_check_eq(Cr.NS_OK, aStatus);
    run_next_test();
  });
});

add_test(function test_shouldNotBlock() {
  let query = new ApplicationReputationQuery();
  query.sourceURI = createURI("http://mozilla.com");
  query.fileSize = 12;

  gAppRep.queryReputation(query, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_OK, aStatus);
    do_check_false(aShouldBlock);
    run_next_test();
  });
});

add_test(function test_garbage() {
  let query = new ApplicationReputationQuery();
  query.sourceURI = createURI("http://thisisagarbageurl.com");
  query.fileSize = 12;

  gAppRep.queryReputation(query, function onComplete(aShouldBlock, aStatus) {
    // We should be getting the garbage response.
    do_check_eq(Cr.NS_ERROR_CANNOT_CONVERT_DATA, aStatus);
    do_check_false(aShouldBlock);
    run_next_test();
  });
});

add_test(function test_nullSourceURI() {
  let query = new ApplicationReputationQuery();
  query.fileSize = 12;
  // No source URI
  gAppRep.queryReputation(query, function onComplete(aShouldBlock, aStatus) {
    do_check_eq(Cr.NS_ERROR_UNEXPECTED, aStatus);
    do_check_false(aShouldBlock);
    run_next_test();
  });
});

add_test(function test_nullCallback() {
  let query = new ApplicationReputationQuery();
  query.fileSize = 12;
  try {
    gAppRep.queryReputation(query, null);
    do_throw("Callback cannot be null");
  } catch (ex if ex.result == Cr.NS_ERROR_INVALID_POINTER) {
    run_next_test();
  }
});

add_test(function test_disabled() {
  Services.prefs.setCharPref("browser.safebrowsing.appRepURL", "");
  let query = new ApplicationReputationQuery();
  query.sourceURI = createURI("http://example.com");
  query.fileSize = 12;
  gAppRep.queryReputation(query, function onComplete(aShouldBlock, aStatus) {
    // We should be getting NS_ERROR_NOT_AVAILABLE if the service is disabled
    do_check_eq(Cr.NS_ERROR_NOT_AVAILABLE, aStatus);
    do_check_false(aShouldBlock);
    run_next_test();
  });
});
