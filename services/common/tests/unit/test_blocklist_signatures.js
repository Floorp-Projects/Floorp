"use strict";

Cu.import("resource://services-common/blocklist-updater.js");
Cu.import("resource://testing-common/httpd.js");

const { Kinto } = Cu.import("resource://services-common/kinto-offline-client.js", {});
const { FirefoxAdapter } = Cu.import("resource://services-common/kinto-storage-adapter.js", {});
const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const { OneCRLBlocklistClient } = Cu.import("resource://services-common/blocklist-clients.js", {});

let server;

const PREF_BLOCKLIST_BUCKET            = "services.blocklist.bucket";
const PREF_BLOCKLIST_ENFORCE_SIGNING   = "services.blocklist.signing.enforced";
const PREF_BLOCKLIST_ONECRL_COLLECTION = "services.blocklist.onecrl.collection";
const PREF_SETTINGS_SERVER             = "services.settings.server";
const PREF_SIGNATURE_ROOT              = "security.content.signature.root_hash";

const kintoFilename = "kinto.sqlite";

const CERT_DIR = "test_blocklist_signatures/";
const CHAIN_FILES =
    ["collection_signing_ee.pem",
     "collection_signing_int.pem",
     "collection_signing_root.pem"];

function getFileData(file) {
  const stream = Cc["@mozilla.org/network/file-input-stream;1"]
                   .createInstance(Ci.nsIFileInputStream);
  stream.init(file, -1, 0, 0);
  const data = NetUtil.readInputStreamToString(stream, stream.available());
  stream.close();
  return data;
}

function setRoot() {
  const filename = CERT_DIR + CHAIN_FILES[0];

  const certFile = do_get_file(filename, false);
  const b64cert = getFileData(certFile)
                    .replace(/-----BEGIN CERTIFICATE-----/, "")
                    .replace(/-----END CERTIFICATE-----/, "")
                    .replace(/[\r\n]/g, "");
  const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                   .getService(Ci.nsIX509CertDB);
  const cert = certdb.constructX509FromBase64(b64cert);
  Services.prefs.setCharPref(PREF_SIGNATURE_ROOT, cert.sha256Fingerprint);
}

function getCertChain() {
  const chain = [];
  for (let file of CHAIN_FILES) {
    chain.push(getFileData(do_get_file(CERT_DIR + file)));
  }
  return chain.join("\n");
}

function* checkRecordCount(count) {
  // open the collection manually
  const base = Services.prefs.getCharPref(PREF_SETTINGS_SERVER);
  const bucket = Services.prefs.getCharPref(PREF_BLOCKLIST_BUCKET);
  const collectionName =
      Services.prefs.getCharPref(PREF_BLOCKLIST_ONECRL_COLLECTION);

  const sqliteHandle = yield FirefoxAdapter.openConnection({path: kintoFilename});
  const config = {
    remote: base,
    bucket,
    adapter: FirefoxAdapter,
    adapterOptions: {sqliteHandle},
  };

  const db = new Kinto(config);
  const collection = db.collection(collectionName);

  // Check we have the expected number of records
  let records = yield collection.list();
  do_check_eq(count, records.data.length);

  // Close the collection so the test can exit cleanly
  yield sqliteHandle.close();
}

// Check to ensure maybeSync is called with correct values when a changes
// document contains information on when a collection was last modified
add_task(function* test_check_signatures() {
  const port = server.identity.primaryPort;

  // a response to give the client when the cert chain is expected
  function makeMetaResponseBody(lastModified, signature) {
    return {
      data: {
        id: "certificates",
        last_modified: lastModified,
        signature: {
          x5u: `http://localhost:${port}/test_blocklist_signatures/test_cert_chain.pem`,
          public_key: "fake",
          "content-signature": `x5u=http://localhost:${port}/test_blocklist_signatures/test_cert_chain.pem;p384ecdsa=${signature}`,
          signature_encoding: "rs_base64url",
          signature,
          hash_algorithm: "sha384",
          ref: "1yryrnmzou5rf31ou80znpnq8n"
        }
      }
    };
  }

  function makeMetaResponse(eTag, body, comment) {
    return {
      comment,
      sampleHeaders: [
        "Content-Type: application/json; charset=UTF-8",
        `ETag: \"${eTag}\"`
      ],
      status: {status: 200, statusText: "OK"},
      responseBody: JSON.stringify(body)
    };
  }

  function registerHandlers(responses) {
    function handleResponse(serverTimeMillis, request, response) {
      const key = `${request.method}:${request.path}?${request.queryString}`;
      const available = responses[key];
      const sampled = available.length > 1 ? available.shift() : available[0];

      if (!sampled) {
        do_throw(`unexpected ${request.method} request for ${request.path}?${request.queryString}`);
      }

      response.setStatusLine(null, sampled.status.status,
                            sampled.status.statusText);
      // send the headers
      for (let headerLine of sampled.sampleHeaders) {
        let headerElements = headerLine.split(":");
        response.setHeader(headerElements[0], headerElements[1].trimLeft());
      }

      // set the server date
      response.setHeader("Date", (new Date(serverTimeMillis)).toUTCString());

      response.write(sampled.responseBody);
    }

    for (let key of Object.keys(responses)) {
      const keyParts = key.split(":");
      const valueParts = keyParts[1].split("?");
      const path = valueParts[0];

      server.registerPathHandler(path, handleResponse.bind(null, 2000));
    }
  }

  // First, perform a signature verification with known data and signature
  // to ensure things are working correctly
  let verifier = Cc["@mozilla.org/security/contentsignatureverifier;1"]
                   .createInstance(Ci.nsIContentSignatureVerifier);

  const emptyData = "[]";
  const emptySignature = "p384ecdsa=zbugm2FDitsHwk5-IWsas1PpWwY29f0Fg5ZHeqD8fzep7AVl2vfcaHA7LdmCZ28qZLOioGKvco3qT117Q4-HlqFTJM7COHzxGyU2MMJ0ZTnhJrPOC1fP3cVQjU1PTWi9";
  const name = "onecrl.content-signature.mozilla.org";
  ok(verifier.verifyContentSignature(emptyData, emptySignature,
                                     getCertChain(), name));

  verifier = Cc["@mozilla.org/security/contentsignatureverifier;1"]
               .createInstance(Ci.nsIContentSignatureVerifier);

  const collectionData = '[{"details":{"bug":"https://bugzilla.mozilla.org/show_bug.cgi?id=1155145","created":"2016-01-18T14:43:37Z","name":"GlobalSign certs","who":".","why":"."},"enabled":true,"id":"97fbf7c4-3ef2-f54f-0029-1ba6540c63ea","issuerName":"MHExKDAmBgNVBAMTH0dsb2JhbFNpZ24gUm9vdFNpZ24gUGFydG5lcnMgQ0ExHTAbBgNVBAsTFFJvb3RTaWduIFBhcnRuZXJzIENBMRkwFwYDVQQKExBHbG9iYWxTaWduIG52LXNhMQswCQYDVQQGEwJCRQ==","last_modified":2000,"serialNumber":"BAAAAAABA/A35EU="},{"details":{"bug":"https://bugzilla.mozilla.org/show_bug.cgi?id=1155145","created":"2016-01-18T14:48:11Z","name":"GlobalSign certs","who":".","why":"."},"enabled":true,"id":"e3bd531e-1ee4-7407-27ce-6fdc9cecbbdc","issuerName":"MIGBMQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTElMCMGA1UECxMcUHJpbWFyeSBPYmplY3QgUHVibGlzaGluZyBDQTEwMC4GA1UEAxMnR2xvYmFsU2lnbiBQcmltYXJ5IE9iamVjdCBQdWJsaXNoaW5nIENB","last_modified":3000,"serialNumber":"BAAAAAABI54PryQ="}]';
  const collectionSignature = "p384ecdsa=f4pA2tYM5jQgWY6YUmhUwQiBLj6QO5sHLD_5MqLePz95qv-7cNCuQoZnPQwxoptDtW8hcWH3kLb0quR7SB-r82gkpR9POVofsnWJRA-ETb0BcIz6VvI3pDT49ZLlNg3p";

  ok(verifier.verifyContentSignature(collectionData, collectionSignature, getCertChain(), name));

  // set up prefs so the kinto updater talks to the test server
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER,
    `http://localhost:${server.identity.primaryPort}/v1`);

  // Set up some data we need for our test
  let startTime = Date.now();

  // These are records we'll use in the test collections
  const RECORD1 = {
    details: {
      bug: "https://bugzilla.mozilla.org/show_bug.cgi?id=1155145",
      created: "2016-01-18T14:43:37Z",
      name: "GlobalSign certs",
      who: ".",
      why: "."
    },
    enabled: true,
    id: "97fbf7c4-3ef2-f54f-0029-1ba6540c63ea",
    issuerName: "MHExKDAmBgNVBAMTH0dsb2JhbFNpZ24gUm9vdFNpZ24gUGFydG5lcnMgQ0ExHTAbBgNVBAsTFFJvb3RTaWduIFBhcnRuZXJzIENBMRkwFwYDVQQKExBHbG9iYWxTaWduIG52LXNhMQswCQYDVQQGEwJCRQ==",
    last_modified: 2000,
    serialNumber: "BAAAAAABA/A35EU="
  };

  const RECORD2 = {
    details: {
      bug: "https://bugzilla.mozilla.org/show_bug.cgi?id=1155145",
      created: "2016-01-18T14:48:11Z",
      name: "GlobalSign certs",
      who: ".",
      why: "."
    },
    enabled: true,
    id: "e3bd531e-1ee4-7407-27ce-6fdc9cecbbdc",
    issuerName: "MIGBMQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTElMCMGA1UECxMcUHJpbWFyeSBPYmplY3QgUHVibGlzaGluZyBDQTEwMC4GA1UEAxMnR2xvYmFsU2lnbiBQcmltYXJ5IE9iamVjdCBQdWJsaXNoaW5nIENB",
    last_modified: 3000,
    serialNumber: "BAAAAAABI54PryQ="
  };

  const RECORD3 = {
    details: {
      bug: "https://bugzilla.mozilla.org/show_bug.cgi?id=1155145",
      created: "2016-01-18T14:48:11Z",
      name: "GlobalSign certs",
      who: ".",
      why: "."
    },
    enabled: true,
    id: "c7c49b69-a4ab-418e-92a9-e1961459aa7f",
    issuerName: "MIGBMQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTElMCMGA1UECxMcUHJpbWFyeSBPYmplY3QgUHVibGlzaGluZyBDQTEwMC4GA1UEAxMnR2xvYmFsU2lnbiBQcmltYXJ5IE9iamVjdCBQdWJsaXNoaW5nIENB",
    last_modified: 4000,
    serialNumber: "BAAAAAABI54PryQ="
  };

  const RECORD1_DELETION = {
    deleted: true,
    enabled: true,
    id: "97fbf7c4-3ef2-f54f-0029-1ba6540c63ea",
    last_modified: 3500,
  };

  // Check that a signature on an empty collection is OK
  // We need to set up paths on the HTTP server to return specific data from
  // specific paths for each test. Here we prepare data for each response.

  // A cert chain response (this the cert chain that contains the signing
  // cert, the root and any intermediates in between). This is used in each
  // sync.
  const RESPONSE_CERT_CHAIN = {
    comment: "RESPONSE_CERT_CHAIN",
    sampleHeaders: [
      "Content-Type: text/plain; charset=UTF-8"
    ],
    status: {status: 200, statusText: "OK"},
    responseBody: getCertChain()
  };

  // A server settings response. This is used in each sync.
  const RESPONSE_SERVER_SETTINGS = {
    comment: "RESPONSE_SERVER_SETTINGS",
    sampleHeaders: [
      "Access-Control-Allow-Origin: *",
      "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
      "Content-Type: application/json; charset=UTF-8",
      "Server: waitress"
    ],
    status: {status: 200, statusText: "OK"},
    responseBody: JSON.stringify({"settings":{"batch_max_requests":25}, "url":`http://localhost:${port}/v1/`, "documentation":"https://kinto.readthedocs.org/", "version":"1.5.1", "commit":"cbc6f58", "hello":"kinto"})
  };

  // This is the initial, empty state of the collection. This is only used
  // for the first sync.
  const RESPONSE_EMPTY_INITIAL = {
    comment: "RESPONSE_EMPTY_INITIAL",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      "ETag: \"1000\""
    ],
    status: {status: 200, statusText: "OK"},
    responseBody: JSON.stringify({"data": []})
  };

  const RESPONSE_BODY_META_EMPTY_SIG = makeMetaResponseBody(1000,
    "vxuAg5rDCB-1pul4a91vqSBQRXJG_j7WOYUTswxRSMltdYmbhLRH8R8brQ9YKuNDF56F-w6pn4HWxb076qgKPwgcEBtUeZAO_RtaHXRkRUUgVzAr86yQL4-aJTbv3D6u");

  // The collection metadata containing the signature for the empty
  // collection.
  const RESPONSE_META_EMPTY_SIG =
    makeMetaResponse(1000, RESPONSE_BODY_META_EMPTY_SIG,
                     "RESPONSE_META_EMPTY_SIG");

  // Here, we map request method and path to the available responses
  const emptyCollectionResponses = {
    "GET:/test_blocklist_signatures/test_cert_chain.pem?":[RESPONSE_CERT_CHAIN],
    "GET:/v1/?": [RESPONSE_SERVER_SETTINGS],
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified":
      [RESPONSE_EMPTY_INITIAL],
    "GET:/v1/buckets/blocklists/collections/certificates?":
      [RESPONSE_META_EMPTY_SIG]
  };

  // .. and use this map to register handlers for each path
  registerHandlers(emptyCollectionResponses);

  // With all of this set up, we attempt a sync. This will resolve if all is
  // well and throw if something goes wrong.
  yield OneCRLBlocklistClient.maybeSync(1000, startTime);

  // Check that some additions (2 records) to the collection have a valid
  // signature.

  // This response adds two entries (RECORD1 and RECORD2) to the collection
  const RESPONSE_TWO_ADDED = {
    comment: "RESPONSE_TWO_ADDED",
    sampleHeaders: [
        "Content-Type: application/json; charset=UTF-8",
        "ETag: \"3000\""
    ],
    status: {status: 200, statusText: "OK"},
    responseBody: JSON.stringify({"data": [RECORD2, RECORD1]})
  };

  const RESPONSE_BODY_META_TWO_ITEMS_SIG = makeMetaResponseBody(3000,
    "dwhJeypadNIyzGj3QdI0KMRTPnHhFPF_j73mNrsPAHKMW46S2Ftf4BzsPMvPMB8h0TjDus13wo_R4l432DHe7tYyMIWXY0PBeMcoe5BREhFIxMxTsh9eGVXBD1e3UwRy");

  // A signature response for the collection containg RECORD1 and RECORD2
  const RESPONSE_META_TWO_ITEMS_SIG =
    makeMetaResponse(3000, RESPONSE_BODY_META_TWO_ITEMS_SIG,
                     "RESPONSE_META_TWO_ITEMS_SIG");

  const twoItemsResponses = {
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified&_since=1000":
      [RESPONSE_TWO_ADDED],
    "GET:/v1/buckets/blocklists/collections/certificates?":
      [RESPONSE_META_TWO_ITEMS_SIG]
  };
  registerHandlers(twoItemsResponses);
  yield OneCRLBlocklistClient.maybeSync(3000, startTime);

  // Check the collection with one addition and one removal has a valid
  // signature

  // Remove RECORD1, add RECORD3
  const RESPONSE_ONE_ADDED_ONE_REMOVED = {
    comment: "RESPONSE_ONE_ADDED_ONE_REMOVED ",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      "ETag: \"4000\""
    ],
    status: {status: 200, statusText: "OK"},
    responseBody: JSON.stringify({"data": [RECORD3, RECORD1_DELETION]})
  };

  const RESPONSE_BODY_META_THREE_ITEMS_SIG = makeMetaResponseBody(4000,
    "MIEmNghKnkz12UodAAIc3q_Y4a3IJJ7GhHF4JYNYmm8avAGyPM9fYU7NzVo94pzjotG7vmtiYuHyIX2rTHTbT587w0LdRWxipgFd_PC1mHiwUyjFYNqBBG-kifYk7kEw");

  // signature response for the collection containing RECORD2 and RECORD3
  const RESPONSE_META_THREE_ITEMS_SIG =
    makeMetaResponse(4000, RESPONSE_BODY_META_THREE_ITEMS_SIG,
                     "RESPONSE_META_THREE_ITEMS_SIG");

  const oneAddedOneRemovedResponses = {
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified&_since=3000":
      [RESPONSE_ONE_ADDED_ONE_REMOVED],
    "GET:/v1/buckets/blocklists/collections/certificates?":
      [RESPONSE_META_THREE_ITEMS_SIG]
  };
  registerHandlers(oneAddedOneRemovedResponses);
  yield OneCRLBlocklistClient.maybeSync(4000, startTime);

  // Check the signature is still valid with no operation (no changes)

  // Leave the collection unchanged
  const RESPONSE_EMPTY_NO_UPDATE = {
    comment: "RESPONSE_EMPTY_NO_UPDATE ",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      "ETag: \"4000\""
    ],
    status: {status: 200, statusText: "OK"},
    responseBody: JSON.stringify({"data": []})
  };

  const noOpResponses = {
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified&_since=4000":
      [RESPONSE_EMPTY_NO_UPDATE],
    "GET:/v1/buckets/blocklists/collections/certificates?":
      [RESPONSE_META_THREE_ITEMS_SIG]
  };
  registerHandlers(noOpResponses);
  yield OneCRLBlocklistClient.maybeSync(4100, startTime);

  // Check the collection is reset when the signature is invalid

  // Prepare a (deliberately) bad signature to check the collection state is
  // reset if something is inconsistent
  const RESPONSE_COMPLETE_INITIAL = {
    comment: "RESPONSE_COMPLETE_INITIAL ",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      "ETag: \"4000\""
    ],
    status: {status: 200, statusText: "OK"},
    responseBody: JSON.stringify({"data": [RECORD2, RECORD3]})
  };

  const RESPONSE_COMPLETE_INITIAL_SORTED_BY_ID = {
    comment: "RESPONSE_COMPLETE_INITIAL ",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      "ETag: \"4000\""
    ],
    status: {status: 200, statusText: "OK"},
    responseBody: JSON.stringify({"data": [RECORD3, RECORD2]})
  };

  const RESPONSE_BODY_META_BAD_SIG = makeMetaResponseBody(4000,
      "aW52YWxpZCBzaWduYXR1cmUK");

  const RESPONSE_META_BAD_SIG =
      makeMetaResponse(4000, RESPONSE_BODY_META_BAD_SIG, "RESPONSE_META_BAD_SIG");

  const badSigGoodSigResponses = {
    // In this test, we deliberately serve a bad signature initially. The
    // subsequent signature returned is a valid one for the three item
    // collection.
    "GET:/v1/buckets/blocklists/collections/certificates?":
      [RESPONSE_META_BAD_SIG, RESPONSE_META_THREE_ITEMS_SIG],
    // The first collection state is the three item collection (since
    // there's a sync with no updates) - but, since the signature is wrong,
    // another request will be made...
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified&_since=4000":
      [RESPONSE_EMPTY_NO_UPDATE],
    // The next request is for the full collection. This will be checked
    // against the valid signature - so the sync should succeed.
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified":
      [RESPONSE_COMPLETE_INITIAL],
    // The next request is for the full collection sorted by id. This will be
    // checked against the valid signature - so the sync should succeed.
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=id":
      [RESPONSE_COMPLETE_INITIAL_SORTED_BY_ID]
  };

  registerHandlers(badSigGoodSigResponses);
  yield OneCRLBlocklistClient.maybeSync(5000, startTime);

  const badSigGoodOldResponses = {
    // In this test, we deliberately serve a bad signature initially. The
    // subsequent sitnature returned is a valid one for the three item
    // collection.
    "GET:/v1/buckets/blocklists/collections/certificates?":
      [RESPONSE_META_BAD_SIG, RESPONSE_META_EMPTY_SIG],
    // The first collection state is the current state (since there's no update
    // - but, since the signature is wrong, another request will be made)
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified&_since=4000":
      [RESPONSE_EMPTY_NO_UPDATE],
    // The next request is for the full collection sorted by id. This will be
    // checked against the valid signature and last_modified times will be
    // compared. Sync should fail, even though the signature is good,
    // because the local collection is newer.
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=id":
      [RESPONSE_EMPTY_INITIAL],
  };

  // ensure our collection hasn't been replaced with an older, empty one
  yield checkRecordCount(2);

  registerHandlers(badSigGoodOldResponses);
  yield OneCRLBlocklistClient.maybeSync(5000, startTime);

  const allBadSigResponses = {
    // In this test, we deliberately serve only a bad signature.
    "GET:/v1/buckets/blocklists/collections/certificates?":
      [RESPONSE_META_BAD_SIG],
    // The first collection state is the three item collection (since
    // there's a sync with no updates) - but, since the signature is wrong,
    // another request will be made...
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified&_since=4000":
      [RESPONSE_EMPTY_NO_UPDATE],
    // The next request is for the full collection sorted by id. This will be
    // checked against the valid signature - so the sync should succeed.
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=id":
      [RESPONSE_COMPLETE_INITIAL_SORTED_BY_ID]
  };

  registerHandlers(allBadSigResponses);
  try {
    yield OneCRLBlocklistClient.maybeSync(6000, startTime);
    do_throw("Sync should fail (the signature is intentionally bad)");
  } catch (e) {
    yield checkRecordCount(2);
  }
});

function run_test() {
  // ensure signatures are enforced
  Services.prefs.setBoolPref(PREF_BLOCKLIST_ENFORCE_SIGNING, true);

  // get a signature verifier to ensure nsNSSComponent is initialized
  Cc["@mozilla.org/security/contentsignatureverifier;1"]
    .createInstance(Ci.nsIContentSignatureVerifier);

  // set the content signing root to our test root
  setRoot();

  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  do_register_cleanup(function() {
    server.stop(function() { });
  });
}
