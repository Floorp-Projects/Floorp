/* import-globals-from ../../../common/tests/unit/head_helpers.js */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { UptakeTelemetry } = ChromeUtils.import(
  "resource://services-common/uptake-telemetry.js"
);

const PREF_SETTINGS_SERVER = "services.settings.server";
const PREF_SIGNATURE_ROOT = "security.content.signature.root_hash";
const SIGNER_NAME = "onecrl.content-signature.mozilla.org";

const CERT_DIR = "test_remote_settings_signatures/";
const CHAIN_FILES = [
  "collection_signing_ee.pem",
  "collection_signing_int.pem",
  "collection_signing_root.pem",
];

function getFileData(file) {
  const stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
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
  const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
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

let server;
let client;

function run_test() {
  // Signature verification is enabled by default. We use a custom signer
  // because these tests were originally written for OneCRL.
  client = RemoteSettings("signed", { signerName: SIGNER_NAME });

  Services.prefs.setCharPref("services.settings.loglevel", "debug");

  // set the content signing root to our test root
  setRoot();

  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  registerCleanupFunction(() => server.stop(() => {}));
}

add_task(async function test_check_signatures() {
  // First, perform a signature verification with known data and signature
  // to ensure things are working correctly
  let verifier = Cc[
    "@mozilla.org/security/contentsignatureverifier;1"
  ].createInstance(Ci.nsIContentSignatureVerifier);

  const emptyData = "[]";
  const emptySignature =
    "p384ecdsa=zbugm2FDitsHwk5-IWsas1PpWwY29f0Fg5ZHeqD8fzep7AVl2vfcaHA7LdmCZ28qZLOioGKvco3qT117Q4-HlqFTJM7COHzxGyU2MMJ0ZTnhJrPOC1fP3cVQjU1PTWi9";

  ok(
    await verifier.asyncVerifyContentSignature(
      emptyData,
      emptySignature,
      getCertChain(),
      SIGNER_NAME
    )
  );

  const collectionData =
    '[{"details":{"bug":"https://bugzilla.mozilla.org/show_bug.cgi?id=1155145","created":"2016-01-18T14:43:37Z","name":"GlobalSign certs","who":".","why":"."},"enabled":true,"id":"97fbf7c4-3ef2-f54f-0029-1ba6540c63ea","issuerName":"MHExKDAmBgNVBAMTH0dsb2JhbFNpZ24gUm9vdFNpZ24gUGFydG5lcnMgQ0ExHTAbBgNVBAsTFFJvb3RTaWduIFBhcnRuZXJzIENBMRkwFwYDVQQKExBHbG9iYWxTaWduIG52LXNhMQswCQYDVQQGEwJCRQ==","last_modified":2000,"serialNumber":"BAAAAAABA/A35EU="},{"details":{"bug":"https://bugzilla.mozilla.org/show_bug.cgi?id=1155145","created":"2016-01-18T14:48:11Z","name":"GlobalSign certs","who":".","why":"."},"enabled":true,"id":"e3bd531e-1ee4-7407-27ce-6fdc9cecbbdc","issuerName":"MIGBMQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTElMCMGA1UECxMcUHJpbWFyeSBPYmplY3QgUHVibGlzaGluZyBDQTEwMC4GA1UEAxMnR2xvYmFsU2lnbiBQcmltYXJ5IE9iamVjdCBQdWJsaXNoaW5nIENB","last_modified":3000,"serialNumber":"BAAAAAABI54PryQ="}]';
  const collectionSignature =
    "p384ecdsa=f4pA2tYM5jQgWY6YUmhUwQiBLj6QO5sHLD_5MqLePz95qv-7cNCuQoZnPQwxoptDtW8hcWH3kLb0quR7SB-r82gkpR9POVofsnWJRA-ETb0BcIz6VvI3pDT49ZLlNg3p";

  ok(
    await verifier.asyncVerifyContentSignature(
      collectionData,
      collectionSignature,
      getCertChain(),
      SIGNER_NAME
    )
  );
});

add_task(async function test_check_synchronization_with_signatures() {
  const port = server.identity.primaryPort;

  const x5u = `http://localhost:${port}/test_remote_settings_signatures/test_cert_chain.pem`;

  // Telemetry reports.
  const TELEMETRY_HISTOGRAM_KEY = client.identifier;

  function registerHandlers(responses) {
    function handleResponse(serverTimeMillis, request, response) {
      const key = `${request.method}:${request.path}?${request.queryString}`;
      const available = responses[key];
      const sampled = available.length > 1 ? available.shift() : available[0];
      if (!sampled) {
        do_throw(
          `unexpected ${request.method} request for ${request.path}?${request.queryString}`
        );
      }

      response.setStatusLine(
        null,
        sampled.status.status,
        sampled.status.statusText
      );
      // send the headers
      for (let headerLine of sampled.sampleHeaders) {
        let headerElements = headerLine.split(":");
        response.setHeader(headerElements[0], headerElements[1].trimLeft());
      }

      // set the server date
      response.setHeader("Date", new Date(serverTimeMillis).toUTCString());

      response.write(sampled.responseBody);
    }

    for (let key of Object.keys(responses)) {
      const keyParts = key.split(":");
      const valueParts = keyParts[1].split("?");
      const path = valueParts[0];

      server.registerPathHandler(path, handleResponse.bind(null, 2000));
    }
  }

  // set up prefs so the kinto updater talks to the test server
  Services.prefs.setCharPref(
    PREF_SETTINGS_SERVER,
    `http://localhost:${server.identity.primaryPort}/v1`
  );

  // These are records we'll use in the test collections
  const RECORD1 = {
    details: {
      bug: "https://bugzilla.mozilla.org/show_bug.cgi?id=1155145",
      created: "2016-01-18T14:43:37Z",
      name: "GlobalSign certs",
      who: ".",
      why: ".",
    },
    enabled: true,
    id: "97fbf7c4-3ef2-f54f-0029-1ba6540c63ea",
    issuerName:
      "MHExKDAmBgNVBAMTH0dsb2JhbFNpZ24gUm9vdFNpZ24gUGFydG5lcnMgQ0ExHTAbBgNVBAsTFFJvb3RTaWduIFBhcnRuZXJzIENBMRkwFwYDVQQKExBHbG9iYWxTaWduIG52LXNhMQswCQYDVQQGEwJCRQ==",
    last_modified: 2000,
    serialNumber: "BAAAAAABA/A35EU=",
  };

  const RECORD2 = {
    details: {
      bug: "https://bugzilla.mozilla.org/show_bug.cgi?id=1155145",
      created: "2016-01-18T14:48:11Z",
      name: "GlobalSign certs",
      who: ".",
      why: ".",
    },
    enabled: true,
    id: "e3bd531e-1ee4-7407-27ce-6fdc9cecbbdc",
    issuerName:
      "MIGBMQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTElMCMGA1UECxMcUHJpbWFyeSBPYmplY3QgUHVibGlzaGluZyBDQTEwMC4GA1UEAxMnR2xvYmFsU2lnbiBQcmltYXJ5IE9iamVjdCBQdWJsaXNoaW5nIENB",
    last_modified: 3000,
    serialNumber: "BAAAAAABI54PryQ=",
  };

  const RECORD3 = {
    details: {
      bug: "https://bugzilla.mozilla.org/show_bug.cgi?id=1155145",
      created: "2016-01-18T14:48:11Z",
      name: "GlobalSign certs",
      who: ".",
      why: ".",
    },
    enabled: true,
    id: "c7c49b69-a4ab-418e-92a9-e1961459aa7f",
    issuerName:
      "MIGBMQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTElMCMGA1UECxMcUHJpbWFyeSBPYmplY3QgUHVibGlzaGluZyBDQTEwMC4GA1UEAxMnR2xvYmFsU2lnbiBQcmltYXJ5IE9iamVjdCBQdWJsaXNoaW5nIENB",
    last_modified: 4000,
    serialNumber: "BAAAAAABI54PryQ=",
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
    sampleHeaders: ["Content-Type: text/plain; charset=UTF-8"],
    status: { status: 200, statusText: "OK" },
    responseBody: getCertChain(),
  };

  // A server settings response. This is used in each sync.
  const RESPONSE_SERVER_SETTINGS = {
    comment: "RESPONSE_SERVER_SETTINGS",
    sampleHeaders: [
      "Access-Control-Allow-Origin: *",
      "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
      "Content-Type: application/json; charset=UTF-8",
      "Server: waitress",
    ],
    status: { status: 200, statusText: "OK" },
    responseBody: JSON.stringify({
      settings: {
        batch_max_requests: 25,
      },
      url: `http://localhost:${port}/v1/`,
      documentation: "https://kinto.readthedocs.org/",
      version: "1.5.1",
      commit: "cbc6f58",
      hello: "kinto",
    }),
  };

  // This is the initial, empty state of the collection. This is only used
  // for the first sync.
  const RESPONSE_EMPTY_INITIAL = {
    comment: "RESPONSE_EMPTY_INITIAL",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      'ETag: "1000"',
    ],
    status: { status: 200, statusText: "OK" },
    responseBody: JSON.stringify({
      timestamp: 1000,
      metadata: {
        signature: {
          x5u,
          signature:
            "vxuAg5rDCB-1pul4a91vqSBQRXJG_j7WOYUTswxRSMltdYmbhLRH8R8brQ9YKuNDF56F-w6pn4HWxb076qgKPwgcEBtUeZAO_RtaHXRkRUUgVzAr86yQL4-aJTbv3D6u",
        },
      },
      changes: [],
    }),
  };

  // Here, we map request method and path to the available responses
  const emptyCollectionResponses = {
    "GET:/test_remote_settings_signatures/test_cert_chain.pem?": [
      RESPONSE_CERT_CHAIN,
    ],
    "GET:/v1/?": [RESPONSE_SERVER_SETTINGS],
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=1000": [
      RESPONSE_EMPTY_INITIAL,
    ],
  };

  //
  // 1.
  // - collection: undefined -> []
  // - timestamp: undefined -> 1000
  //

  // .. and use this map to register handlers for each path
  registerHandlers(emptyCollectionResponses);

  let startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);

  // With all of this set up, we attempt a sync. This will resolve if all is
  // well and throw if something goes wrong.
  await client.maybeSync(1000);

  equal((await client.get()).length, 0);

  let endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);

  // ensure that a success histogram is tracked when a succesful sync occurs.
  let expectedIncrements = { [UptakeTelemetry.STATUS.SUCCESS]: 1 };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);

  //
  // 2.
  // - collection: [] -> [RECORD2, RECORD1]
  // - timestamp: 1000 -> 3000
  //
  // Check that some additions (2 records) to the collection have a valid
  // signature.

  // This response adds two entries (RECORD1 and RECORD2) to the collection
  const RESPONSE_TWO_ADDED = {
    comment: "RESPONSE_TWO_ADDED",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      'ETag: "3000"',
    ],
    status: { status: 200, statusText: "OK" },
    responseBody: JSON.stringify({
      timestamp: 3000,
      metadata: {
        signature: {
          x5u,
          signature:
            "dwhJeypadNIyzGj3QdI0KMRTPnHhFPF_j73mNrsPAHKMW46S2Ftf4BzsPMvPMB8h0TjDus13wo_R4l432DHe7tYyMIWXY0PBeMcoe5BREhFIxMxTsh9eGVXBD1e3UwRy",
        },
      },
      changes: [RECORD2, RECORD1],
    }),
  };

  const twoItemsResponses = {
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=3000&_since=%221000%22": [
      RESPONSE_TWO_ADDED,
    ],
  };
  registerHandlers(twoItemsResponses);
  await client.maybeSync(3000);

  equal((await client.get()).length, 2);

  //
  // 3.
  // - collection: [RECORD2, RECORD1] -> [RECORD2, RECORD3]
  // - timestamp: 3000 -> 4000
  //
  // Check the collection with one addition and one removal has a valid
  // signature
  const THREE_ITEMS_SIG =
    "MIEmNghKnkz12UodAAIc3q_Y4a3IJJ7GhHF4JYNYmm8avAGyPM9fYU7NzVo94pzjotG7vmtiYuHyIX2rTHTbT587w0LdRWxipgFd_PC1mHiwUyjFYNqBBG-kifYk7kEw";

  // Remove RECORD1, add RECORD3
  const RESPONSE_ONE_ADDED_ONE_REMOVED = {
    comment: "RESPONSE_ONE_ADDED_ONE_REMOVED ",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      'ETag: "4000"',
    ],
    status: { status: 200, statusText: "OK" },
    responseBody: JSON.stringify({
      timestamp: 4000,
      metadata: {
        signature: {
          x5u,
          signature: THREE_ITEMS_SIG,
        },
      },
      changes: [RECORD3, RECORD1_DELETION],
    }),
  };

  const oneAddedOneRemovedResponses = {
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=4000&_since=%223000%22": [
      RESPONSE_ONE_ADDED_ONE_REMOVED,
    ],
  };
  registerHandlers(oneAddedOneRemovedResponses);
  await client.maybeSync(4000);

  equal((await client.get()).length, 2);

  //
  // 4.
  // - collection: [RECORD2, RECORD3] -> [RECORD2, RECORD3]
  // - timestamp: 4000 -> 4100
  //
  // Check the signature is still valid with no operation (no changes)

  // Leave the collection unchanged
  const RESPONSE_EMPTY_NO_UPDATE = {
    comment: "RESPONSE_EMPTY_NO_UPDATE ",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      'ETag: "4000"',
    ],
    status: { status: 200, statusText: "OK" },
    responseBody: JSON.stringify({
      timestamp: 4000,
      metadata: {
        signature: {
          x5u,
          signature: THREE_ITEMS_SIG,
        },
      },
      changes: [],
    }),
  };

  const noOpResponses = {
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=4100&_since=%224000%22": [
      RESPONSE_EMPTY_NO_UPDATE,
    ],
  };
  registerHandlers(noOpResponses);
  await client.maybeSync(4100);

  equal((await client.get()).length, 2);

  console.info("---------------------------------------------------------");
  //
  // 5.
  // - collection: [RECORD2, RECORD3] -> [RECORD2, RECORD3]
  // - timestamp: 4000 -> 5000
  //
  // Check the collection is reset when the signature is invalid.
  // Client will:
  //   - Fetch metadata (with bad signature)
  //   - Perform the sync (fetch empty changes)
  //   - Refetch the metadata and the whole collection
  //   - Validate signature successfully, but with no changes to emit.

  const RESPONSE_COMPLETE_INITIAL = {
    comment: "RESPONSE_COMPLETE_INITIAL ",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      'ETag: "4000"',
    ],
    status: { status: 200, statusText: "OK" },
    responseBody: JSON.stringify({
      timestamp: 4000,
      metadata: {
        signature: {
          x5u,
          signature: THREE_ITEMS_SIG,
        },
      },
      changes: [RECORD2, RECORD3],
    }),
  };

  const RESPONSE_EMPTY_NO_UPDATE_BAD_SIG = {
    ...RESPONSE_EMPTY_NO_UPDATE,
    responseBody: JSON.stringify({
      timestamp: 4000,
      metadata: {
        signature: {
          x5u,
          signature: "aW52YWxpZCBzaWduYXR1cmUK",
        },
      },
      changes: [],
    }),
  };

  const badSigGoodSigResponses = {
    // The first collection state is the three item collection (since
    // there was sync with no updates before) - but, since the signature is wrong,
    // another request will be made...
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=5000&_since=%224000%22": [
      RESPONSE_EMPTY_NO_UPDATE_BAD_SIG,
    ],
    // Subsequent signature returned is a valid one for the three item
    // collection.
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=5000": [
      RESPONSE_COMPLETE_INITIAL,
    ],
  };

  registerHandlers(badSigGoodSigResponses);

  startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);

  let syncEventSent = false;
  client.on("sync", ({ data }) => {
    syncEventSent = true;
  });

  await client.maybeSync(5000);

  equal((await client.get()).length, 2);

  endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);

  // since we only fixed the signature, and no data was changed, the sync event
  // was not sent.
  equal(syncEventSent, false);

  // ensure that the failure count is incremented for a succesful sync with an
  // (initial) bad signature - only SERVICES_SETTINGS_SYNC_SIG_FAIL should
  // increment.
  expectedIncrements = { [UptakeTelemetry.STATUS.SIGNATURE_ERROR]: 1 };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);

  //
  // 6.
  // - collection: [RECORD2, RECORD3] -> [RECORD2, RECORD3]
  // - timestamp: 4000 -> 5000
  //
  // Check the collection is reset when the signature is invalid.
  // Client will:
  //   - Fetch metadata (with bad signature)
  //   - Perform the sync (fetch empty changes)
  //   - Refetch the whole collection and metadata
  //   - Sync will be no-op since local is equal to server, no changes to emit.

  const badSigGoodOldResponses = {
    // The first collection state is the current state (since there's no update
    // - but, since the signature is wrong, another request will be made)
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=5000&_since=%224000%22": [
      RESPONSE_EMPTY_NO_UPDATE_BAD_SIG,
    ],
    // The next request is for the full collection. This will be
    // checked against the valid signature and last_modified times will be
    // compared. Sync should be a no-op, even though the signature is good,
    // because the local collection is newer.
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=5000": [
      RESPONSE_EMPTY_INITIAL,
    ],
  };

  // ensure our collection hasn't been replaced with an older, empty one
  equal((await client.get()).length, 2);

  registerHandlers(badSigGoodOldResponses);

  syncEventSent = false;
  client.on("sync", ({ data }) => {
    syncEventSent = true;
  });

  await client.maybeSync(5000);

  // Local data was unchanged, since it was never than the one returned by the server,
  // thus the sync event is not sent.
  equal(syncEventSent, false);

  //
  // 7.
  // - collection: [RECORD2, RECORD3] -> [RECORD2, RECORD3]
  // - timestamp: 4000 -> 5000
  //
  // Check that a tampered local DB will be overwritten and
  // sync event contain the appropriate data.

  const RESPONSE_COMPLETE_BAD_SIG = {
    ...RESPONSE_EMPTY_NO_UPDATE,
    responseBody: JSON.stringify({
      timestamp: 5000,
      metadata: {
        signature: {
          x5u,
          signature: "aW52YWxpZCBzaWduYXR1cmUK",
        },
      },
      changes: [RECORD2, RECORD3],
    }),
  };

  const badLocalContentGoodSigResponses = {
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=5000": [
      RESPONSE_COMPLETE_BAD_SIG,
      RESPONSE_COMPLETE_INITIAL,
    ],
  };

  registerHandlers(badLocalContentGoodSigResponses);

  // we create a local state manually here, in order to test that the sync event data
  // properly contains created, updated, and deleted records.
  // the local DB contains same id as RECORD2 and a fake record.
  // the final server collection contains RECORD2 and RECORD3
  await client.db.clear();
  await client.db.create(
    { ...RECORD2, last_modified: 1234567890, serialNumber: "abc" },
    { synced: true, useRecordId: true }
  );
  const localId = "0602b1b2-12ab-4d3a-b6fb-593244e7b035";
  await client.db.create({ id: localId }, { synced: true, useRecordId: true });

  let syncData = null;
  client.on("sync", ({ data }) => {
    syncData = data;
  });

  await client.maybeSync(5000);

  // Local data was replaced. But we use records IDs to determine
  // what was created and deleted. So fake local data will appear
  // in the sync event.
  equal(syncData.current.length, 2);
  equal(syncData.created.length, 1);
  equal(syncData.created[0].id, RECORD3.id);
  equal(syncData.updated.length, 1);
  equal(syncData.updated[0].old.serialNumber, "abc");
  equal(syncData.updated[0].new.serialNumber, RECORD2.serialNumber);
  equal(syncData.deleted.length, 1);
  equal(syncData.deleted[0].id, localId);

  //
  // 8.
  // - collection: [RECORD2, RECORD3] -> [RECORD2, RECORD3] (unchanged because of error)
  // - timestamp: 4000 -> 6000
  //
  // Check that a failing signature throws after retry, and that sync changes
  // are not applied.

  const RESPONSE_ONLY_RECORD4_BAD_SIG = {
    comment: "Delete RECORD3, create RECORD4",
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      'ETag: "6000"',
    ],
    status: { status: 200, statusText: "OK" },
    responseBody: JSON.stringify({
      timestamp: 6000,
      metadata: {
        signature: {
          x5u,
          signature: "wrong-sig-here-too",
        },
      },
      changes: [
        {
          id: "f765df30-b2f1-42f6-9803-7bd5a07b5098",
          last_modified: 6000,
        },
      ],
    }),
  };
  const allBadSigResponses = {
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=6000&_since=%224000%22": [
      RESPONSE_EMPTY_NO_UPDATE_BAD_SIG,
    ],
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=6000": [
      RESPONSE_ONLY_RECORD4_BAD_SIG,
    ],
  };

  startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  registerHandlers(allBadSigResponses);
  try {
    await client.maybeSync(6000);
    do_throw("Sync should fail (the signature is intentionally bad)");
  } catch (e) {
    ok(true, "Sync failed as expected (bad signature after retry)");
  }

  // Ensure that the failure is reflected in the accumulated telemetry:
  endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  expectedIncrements = { [UptakeTelemetry.STATUS.SIGNATURE_RETRY_ERROR]: 1 };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);

  // When signature fails after retry, the local data present before sync
  // should be maintained.
  ok(
    arrayEqual(
      (await client.get()).map(r => r.id),
      [RECORD3.id, RECORD2.id]
    ),
    "Remote changes were not changed"
  );
  // And local data should still be valid.
  await client.get({ verifySignature: true }); // Not raising.

  //
  // 9.
  // - collection: [RECORD2, RECORD3] -> [] (cleared)
  // - timestamp: 4000 -> 6000
  //
  // Check that local data is cleared during sync if signature is not valid.

  await client.db.create({
    id: "c6b19c67-2e0e-4a82-b7f7-1777b05f3e81",
    last_modified: 42,
    tampered: true,
  });

  try {
    await client.maybeSync(6000);
    do_throw("Sync should fail (the signature is intentionally bad)");
  } catch (e) {
    ok(true, "Sync failed as expected (bad signature after retry)");
  }
  // Since local data was tampered, it was cleared.
  equal((await client.get()).length, 0, "Local database is now empty.");

  //
  // 11.
  // - collection: [] -> []
  // - timestamp: 4000 -> 6000
  //
  // Check that we don't apply changes when signature is missing in remote.

  const RESPONSE_NO_SIG = {
    sampleHeaders: [
      "Content-Type: application/json; charset=UTF-8",
      `ETag: \"123456\"`,
    ],
    status: { status: 200, statusText: "OK" },
    responseBody: JSON.stringify({
      metadata: {
        last_modified: 123456,
      },
      changes: [],
      timestamp: 123456,
    }),
  };

  const missingSigResponses = {
    // In this test, we deliberately serve metadata without the signature attribute.
    // As if the collection was not signed.
    "GET:/v1/buckets/main/collections/signed/changeset?_expected=6000": [
      RESPONSE_NO_SIG,
    ],
  };

  // Local data was empty after last test.
  equal((await client.get()).length, 0);

  startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  registerHandlers(missingSigResponses);
  try {
    await client.maybeSync(6000);
    do_throw("Sync should fail (the signature is missing)");
  } catch (e) {
    equal((await client.get()).length, 0, "Local remains empty");
  }

  // Ensure that the failure is reflected in the accumulated telemetry:
  endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  expectedIncrements = {
    [UptakeTelemetry.STATUS.SIGNATURE_ERROR]: 1,
    [UptakeTelemetry.STATUS.SIGNATURE_RETRY_ERROR]: 0, // Not retried since missing.
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
