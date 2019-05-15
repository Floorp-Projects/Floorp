
// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";
do_get_profile(); // must be called before getting nsIX509CertDB

const {RemoteSettings} = ChromeUtils.import("resource://services-settings/remote-settings.js");
const {TestUtils} = ChromeUtils.import("resource://testing-common/TestUtils.jsm");
const {TelemetryTestUtils} = ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm");

let remoteSecSetting;
if (AppConstants.MOZ_NEW_CERT_STORAGE) {
  const {RemoteSecuritySettings} = ChromeUtils.import("resource://gre/modules/psm/RemoteSecuritySettings.jsm");
  remoteSecSetting = new RemoteSecuritySettings();
  remoteSecSetting.client.verifySignature = false;
}

let server;

let intermediate1Data;
let intermediate2Data;

const INTERMEDIATES_DL_PER_POLL_PREF     = "security.remote_settings.intermediates.downloads_per_poll";
const INTERMEDIATES_ENABLED_PREF         = "security.remote_settings.intermediates.enabled";

function cyclingIteratorGenerator(items, count = null) {
  return () => cyclingIterator(items, count);
}

function* cyclingIterator(items, count = null) {
  if (count == null) {
    count = items.length;
  }
  for (let i = 0; i < count; i++) {
    yield items[i % items.length];
  }
}

function getHashCommon(aStr, useBase64) {
  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  hasher.init(Ci.nsICryptoHash.SHA256);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(Ci.nsIStringInputStream);
  stringStream.data = aStr;
  hasher.updateFromStream(stringStream, -1);

  return hasher.finish(useBase64);
}

// Get a hexified SHA-256 hash of the given string.
function getHash(aStr) {
  return hexify(getHashCommon(aStr, false));
}

function countTelemetryReports(histogram) {
  let count = 0;
  for (let x in histogram.values) {
    count += histogram.values[x];
  }
  return count;
}

function clearTelemetry() {
  Services.telemetry.getHistogramById("INTERMEDIATE_PRELOADING_ERRORS")
    .clear();
  Services.telemetry.getHistogramById("INTERMEDIATE_PRELOADING_UPDATE_TIME_MS")
    .clear();
  Services.telemetry.clearScalars();
}

function syncAndPromiseUpdate() {
  let updatedPromise = TestUtils.topicObserved("remote-security-settings:intermediates-updated");

  // sync() requires us to implement the whole kinto changes-observing interface,
  // so let's use maybeSync().
  return remoteSecSetting.maybeSync()
  // maybeSync() doesn't send the poll-end notification, so we have to fake it
  .then(r => Services.obs.notifyObservers(null, "remote-settings:changes-poll-end"))
  .then(r => updatedPromise)
  // topicObserved gives back a 2-array
  .then(results => results[1]);
}

function setupKintoPreloadServer(certGenerator, options = {
  attachmentCB: null,
  hashFunc: null,
  lengthFunc: null,
}) {
  const dummyServerURL = `http://localhost:${server.identity.primaryPort}/v1`;
  Services.prefs.setCharPref("services.settings.server", dummyServerURL);

  const configPath = "/v1/";
  const metadataPath = "/v1/buckets/security-state/collections/intermediates";
  const recordsPath = "/v1/buckets/security-state/collections/intermediates/records";
  const attachmentsPath = "/attachments/";

  if (options.hashFunc == null) {
    options.hashFunc = getHash;
  }
  if (options.lengthFunc == null) {
    options.lengthFunc = arr => arr.length;
  }

  function setHeader(response, headers) {
    for (let headerLine of headers) {
      let headerElements = headerLine.split(":");
      response.setHeader(headerElements[0], headerElements[1].trimLeft());
    }
    response.setHeader("Date", (new Date()).toUTCString());
  }

  // Basic server information, all static
  const handler = (request, response) => {
    try {
      const respData = getResponseData(request, server.identity.primaryPort);
      if (!respData) {
        do_throw(`unexpected ${request.method} request for ${request.path}?${request.queryString}`);
        return;
      }

      response.setStatusLine(null, respData.status.status,
                             respData.status.statusText);
      setHeader(response, respData.responseHeaders);
      response.write(respData.responseBody);
    } catch (e) {
      info(e);
    }
  };
  server.registerPathHandler(configPath, handler);
  server.registerPathHandler(metadataPath, handler);

  // Lists of certs
  server.registerPathHandler(recordsPath, (request, response) => {
    response.setStatusLine(null, 200, "OK");
    setHeader(response, [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"1000\"",
    ]);

    let output = [];
    let count = 1;

    let certIterator = certGenerator();
    let result = certIterator.next();
    while (!result.done) {
      let certBytes = result.value;

      output.push({
        "details": {
          "who": "",
          "why": "",
          "name": "",
          "created": "",
        },
        "subject": "",
        "attachment": {
          "hash": options.hashFunc(certBytes),
          "size": options.lengthFunc(certBytes),
          "filename": `intermediate certificate #${count}.pem`,
          "location": `int${count}`,
          "mimetype": "application/x-pem-file",
        },
        "whitelist": false,
        // "pubKeyHash" is actually just the hash of the DER bytes of the certificate
        "pubKeyHash": getHashCommon(atob(pemToBase64(certBytes)), true),
        "crlite_enrolled": true,
        "id": `78cf8900-fdea-4ce5-f8fb-${count}`,
        "last_modified": Date.now(),
      });

      count++;
      result = certIterator.next();
    }

    response.write(JSON.stringify({ data: output }));
  });

  // Certificate data
  server.registerPrefixHandler(attachmentsPath, (request, response) => {
    setHeader(response, [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/x-pem-file; charset=UTF-8",
        "Server: waitress",
        "Etag: \"1000\"",
    ]);

    let identifier = request.path.match(/\d+$/)[0];
    let count = 1;

    let certIterator = certGenerator();
    let result = certIterator.next();
    while (!result.done) {
      // Could do the modulus of the certIterator to get the right data,
      // but that requires plumbing through knowledge of those offsets, so
      // let's just loop. It's not that slow.

      if (count == identifier) {
        response.setStatusLine(null, 200, "OK");
        response.write(result.value);
        if (options.attachmentCB) {
          options.attachmentCB(identifier, true);
        }
        return;
      }

      count++;
      result = certIterator.next();
    }

    response.setStatusLine(null, 404, `Identifier ${identifier} Not Found`);
    if (options.attachmentCB) {
      options.attachmentCB(identifier, false);
    }
  });
}

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function test_preload_empty() {
  Services.prefs.setBoolPref(INTERMEDIATES_ENABLED_PREF, true);

  let countDownloadAttempts = 0;
  setupKintoPreloadServer(
    cyclingIteratorGenerator([]),
    found => { countDownloadAttempts++; }
  );

  let certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

  // load the first root and end entity, ignore the initial intermediate
  addCertFromFile(certDB, "test_intermediate_preloads/ca.pem", "CTu,,");

  let ee_cert = constructCertFromFile("test_intermediate_preloads/ee.pem");
  notEqual(ee_cert, null, "EE cert should have successfully loaded");

  equal(await syncAndPromiseUpdate(), "success", "Preloading update should have run");

  equal(countDownloadAttempts, 0, "There should have been no downloads");

  // check that ee cert 1 is unknown
  await checkCertErrorGeneric(certDB, ee_cert, SEC_ERROR_UNKNOWN_ISSUER,
                              certificateUsageSSLServer);
});

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function test_preload_disabled() {
  Services.prefs.setBoolPref(INTERMEDIATES_ENABLED_PREF, false);

  let countDownloadAttempts = 0;
  setupKintoPreloadServer(
    cyclingIteratorGenerator([intermediate1Data]),
    {attachmentCB: (identifier, attachmentFound) => { countDownloadAttempts++; }}
  );

  equal(await syncAndPromiseUpdate(), "disabled", "Preloading update should not have run");

  equal(countDownloadAttempts, 0, "There should have been no downloads");
});

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function test_preload_invalid_hash() {
  Services.prefs.setBoolPref(INTERMEDIATES_ENABLED_PREF, true);
  const invalidHash = "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d";

  let countDownloadAttempts = 0;
  setupKintoPreloadServer(
    cyclingIteratorGenerator([intermediate1Data]),
    {
      attachmentCB: (identifier, attachmentFound) => { countDownloadAttempts++; },
      hashFunc: data => invalidHash,
    }
  );

  clearTelemetry();

  equal(await syncAndPromiseUpdate(), "success", "Preloading update should have run");

  let errors_histogram = Services.telemetry
                          .getHistogramById("INTERMEDIATE_PRELOADING_ERRORS")
                          .snapshot();

  equal(countTelemetryReports(errors_histogram), 1, "There should be one error report");
  equal(errors_histogram.values[7], 1, "There should be one invalid hash error");

  equal(countDownloadAttempts, 1, "There should have been one download attempt");

  let certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

  // load the first root and end entity, ignore the initial intermediate
  addCertFromFile(certDB, "test_intermediate_preloads/ca.pem", "CTu,,");

  let ee_cert = constructCertFromFile("test_intermediate_preloads/ee.pem");
  notEqual(ee_cert, null, "EE cert should have successfully loaded");

  // We should still have a missing intermediate.
  await checkCertErrorGeneric(certDB, ee_cert, SEC_ERROR_UNKNOWN_ISSUER,
                              certificateUsageSSLServer);
});

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function test_preload_invalid_length() {
  Services.prefs.setBoolPref(INTERMEDIATES_ENABLED_PREF, true);

  let countDownloadAttempts = 0;
  setupKintoPreloadServer(
    cyclingIteratorGenerator([intermediate1Data]),
    {
      attachmentCB: (identifier, attachmentFound) => { countDownloadAttempts++; },
      lengthFunc: data => 42,
    }
  );

  clearTelemetry();

  equal(await syncAndPromiseUpdate(), "success", "Preloading update should have run");

  let errors_histogram = Services.telemetry
                          .getHistogramById("INTERMEDIATE_PRELOADING_ERRORS")
                          .snapshot();

  equal(countTelemetryReports(errors_histogram), 1, "There should be only one error report");
  equal(errors_histogram.values[8], 1, "There should be one invalid length error");

  equal(countDownloadAttempts, 1, "There should have been one download attempt");

  let certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

  // load the first root and end entity, ignore the initial intermediate
  addCertFromFile(certDB, "test_intermediate_preloads/ca.pem", "CTu,,");

  let ee_cert = constructCertFromFile("test_intermediate_preloads/ee.pem");
  notEqual(ee_cert, null, "EE cert should have successfully loaded");

  // We should still have a missing intermediate.
  await checkCertErrorGeneric(certDB, ee_cert, SEC_ERROR_UNKNOWN_ISSUER,
                              certificateUsageSSLServer);
});

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function test_preload_basic() {
  Services.prefs.setBoolPref(INTERMEDIATES_ENABLED_PREF, true);
  Services.prefs.setIntPref(INTERMEDIATES_DL_PER_POLL_PREF, 100);

  let countDownloadAttempts = 0;
  setupKintoPreloadServer(
    cyclingIteratorGenerator([intermediate1Data, intermediate2Data]),
    {attachmentCB: (identifier, attachmentFound) => { countDownloadAttempts++; }}
  );

  let certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

  // load the first root and end entity, ignore the initial intermediate
  addCertFromFile(certDB, "test_intermediate_preloads/ca.pem", "CTu,,");

  let ee_cert = constructCertFromFile("test_intermediate_preloads/ee.pem");
  notEqual(ee_cert, null, "EE cert should have successfully loaded");

  // load the second end entity, ignore both intermediate and root
  let ee_cert_2 = constructCertFromFile("test_intermediate_preloads/ee2.pem");
  notEqual(ee_cert_2, null, "EE cert 2 should have successfully loaded");

  // check that the missing intermediate causes an unknown issuer error, as
  // expected, in both cases
  await checkCertErrorGeneric(certDB, ee_cert, SEC_ERROR_UNKNOWN_ISSUER,
                              certificateUsageSSLServer);
  await checkCertErrorGeneric(certDB, ee_cert_2, SEC_ERROR_UNKNOWN_ISSUER,
                              certificateUsageSSLServer);

  equal(await syncAndPromiseUpdate(), "success", "Preloading update should have run");

  equal(countDownloadAttempts, 2, "There should have been 2 downloads");

  // check that ee cert 1 verifies now the update has happened and there is
  // an intermediate
  await checkCertErrorGeneric(certDB, ee_cert, PRErrorCodeSuccess,
                              certificateUsageSSLServer);

  // check that ee cert 2 does not verify - since we don't know the issuer of
  // this certificate
  await checkCertErrorGeneric(certDB, ee_cert_2, SEC_ERROR_UNKNOWN_ISSUER,
                              certificateUsageSSLServer);
});


add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function test_preload_200() {
  Services.prefs.setBoolPref(INTERMEDIATES_ENABLED_PREF, true);
  Services.prefs.setIntPref(INTERMEDIATES_DL_PER_POLL_PREF, 100);

  let countDownloadedAttachments = 0;
  let countMissingAttachments = 0;
  setupKintoPreloadServer(
    cyclingIteratorGenerator([intermediate1Data, intermediate2Data], 200),
    {
      attachmentCB: (identifier, attachmentFound) => {
        if (!attachmentFound) {
          countMissingAttachments++;
        } else {
          countDownloadedAttachments++;
        }
      },
    }
  );

  clearTelemetry();

  equal(await syncAndPromiseUpdate(), "success", "Preloading update should have run");

  equal(countMissingAttachments, 0, "There should have been no missing attachments");
  equal(countDownloadedAttachments, 100, "There should have been only 100 downloaded");

  const scalars = TelemetryTestUtils.getProcessScalars("parent");
  TelemetryTestUtils.assertScalar(scalars, "security.intermediate_preloading_num_preloaded",
                                  102, "Should have preloaded 102 certs (2 from earlier test)");
  TelemetryTestUtils.assertScalar(scalars, "security.intermediate_preloading_num_pending",
                                  98, "Should report 98 pending");

  let time_histogram = Services.telemetry
                         .getHistogramById("INTERMEDIATE_PRELOADING_UPDATE_TIME_MS")
                         .snapshot();
  let errors_histogram = Services.telemetry
                           .getHistogramById("INTERMEDIATE_PRELOADING_ERRORS")
                           .snapshot();
  equal(countTelemetryReports(time_histogram), 1, "Should report time once");
  equal(countTelemetryReports(errors_histogram), 0, "There should be no error reports");

  equal(await syncAndPromiseUpdate(), "success", "Preloading update should have run");

  equal(countMissingAttachments, 0, "There should have been no missing attachments");
  equal(countDownloadedAttachments, 198,
        "There should have been now 198 downloaded, because 2 existed in an earlier test");
});


function run_test() {
  // Ensure that signature verification is disabled to prevent interference
  // with basic certificate sync tests
  Services.prefs.setBoolPref("services.blocklist.signing.enforced", false);

  let intermediate1File = do_get_file("test_intermediate_preloads/int.pem", false);
  intermediate1Data = readFile(intermediate1File);

  let intermediate2File = do_get_file("test_intermediate_preloads/int2.pem", false);
  intermediate2Data = readFile(intermediate2File);

  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  registerCleanupFunction(function() {
    server.stop(() => { });
  });
}

// get a response for a given request from sample data
function getResponseData(req, port) {
  info(`Resource requested: ${req.method}:${req.path}?${req.queryString}\n\n`);
  const cannedResponses = {
    "OPTIONS": {
      "responseHeaders": [
        "Access-Control-Allow-Headers: Content-Length,Expires,Backoff,Retry-After,Last-Modified,Total-Records,ETag,Pragma,Cache-Control,authorization,content-type,if-none-match,Alert,Next-Page",
        "Access-Control-Allow-Methods: GET,HEAD,OPTIONS,POST,DELETE,OPTIONS",
        "Access-Control-Allow-Origin: *",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": "null",
    },
    "GET:/v1/": {
      "responseHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({
        "settings": {
          "batch_max_requests": 25,
        },
        "url": `http://localhost:${port}/v1/`,
        "documentation": "https://kinto.readthedocs.org/",
        "version": "1.5.1",
        "commit": "cbc6f58",
        "hello": "kinto",
        "capabilities": {
          "attachments": {
            "base_url": `http://localhost:${port}/attachments/`,
          },
        },
      }),
    },
    "GET:/v1/buckets/security-state/collections/intermediates?": {
      "responseHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"1234\"",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": JSON.stringify({
        "data": {
          "id": "intermediates",
          "last_modified": 1234,
        },
      }),
    },
  };
  let result = cannedResponses[`${req.method}:${req.path}?${req.queryString}`] ||
               cannedResponses[`${req.method}:${req.path}`] ||
               cannedResponses[req.method];
  return result;
}
