/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { Constructor: CC } = Components;

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const IS_ANDROID = AppConstants.platform == "android";

const { RemoteSettings } = ChromeUtils.import("resource://services-settings/remote-settings.js");
const { Utils } = ChromeUtils.import("resource://services-settings/Utils.jsm");
const { UptakeTelemetry } = ChromeUtils.import("resource://services-common/uptake-telemetry.js");
const { TelemetryTestUtils } = ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm");

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");

let server;
let client;
let clientWithDump;

async function clear_state() {
  // Clear local DB.
  const collection = await client.openCollection();
  await collection.clear();
  // Reset event listeners.
  client._listeners.set("sync", []);

  const collectionWithDump = await clientWithDump.openCollection();
  await collectionWithDump.clear();

  Services.prefs.clearUserPref("services.settings.default_bucket");

  // Clear events snapshot.
  TelemetryTestUtils.assertEvents([], {}, { process: "dummy" });
}


function run_test() {
  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  // Point the blocklist clients to use this local HTTP server.
  Services.prefs.setCharPref("services.settings.server",
                             `http://localhost:${server.identity.primaryPort}/v1`);

  client = RemoteSettings("password-fields");
  client.verifySignature = false;

  clientWithDump = RemoteSettings("language-dictionaries");
  clientWithDump.verifySignature = false;

  // Setup server fake responses.
  function handleResponse(request, response) {
    try {
      const sample = getSampleResponse(request, server.identity.primaryPort);
      if (!sample) {
        do_throw(`unexpected ${request.method} request for ${request.path}?${request.queryString}`);
      }

      response.setStatusLine(null, sample.status.status,
                             sample.status.statusText);
      // send the headers
      for (let headerLine of sample.sampleHeaders) {
        let headerElements = headerLine.split(":");
        response.setHeader(headerElements[0], headerElements[1].trimLeft());
      }
      response.setHeader("Date", (new Date()).toUTCString());

      const body = typeof sample.responseBody == "string" ? sample.responseBody
                                                          : JSON.stringify(sample.responseBody);
      response.write(body);
      response.finish();
    } catch (e) {
      info(e);
    }
  }
  const configPath = "/v1/";
  const changesPath = "/v1/buckets/monitor/collections/changes/records";
  const metadataPath = "/v1/buckets/main/collections/password-fields";
  const recordsPath  = "/v1/buckets/main/collections/password-fields/records";
  server.registerPathHandler(configPath, handleResponse);
  server.registerPathHandler(changesPath, handleResponse);
  server.registerPathHandler(metadataPath, handleResponse);
  server.registerPathHandler(recordsPath, handleResponse);
  server.registerPathHandler("/fake-x5u", handleResponse);

  run_next_test();

  registerCleanupFunction(function() {
    server.stop(() => { });
  });
}

add_task(async function test_records_obtained_from_server_are_stored_in_db() {
  // Test an empty db populates
  await client.maybeSync(2000);

  // Open the collection, verify it's been populated:
  // Our test data has a single record; it should be in the local collection
  const list = await client.get();
  equal(list.length, 1);
});
add_task(clear_state);

add_task(async function test_records_can_have_local_fields() {
  const c = RemoteSettings("password-fields", { localFields: ["accepted"] });
  await c.maybeSync(2000);

  const col = await c.openCollection();
  await col.update({ id: "9d500963-d80e-3a91-6e74-66f3811b99cc", accepted: true });

  await c.maybeSync(2000); // Does not fail.
});
add_task(clear_state);

add_task(async function test_records_changes_are_overwritten_by_server_changes() {
  // Create some local conflicting data, and make sure it syncs without error.
  const collection = await client.openCollection();
  await collection.create({
    "website": "",
    "id": "9d500963-d80e-3a91-6e74-66f3811b99cc",
  }, { useRecordId: true });

  await client.maybeSync(2000);

  const data = await client.get();
  equal(data[0].website, "https://some-website.com");
});
add_task(clear_state);

add_task(async function test_get_loads_default_records_from_a_local_dump_when_database_is_empty() {
  if (IS_ANDROID) {
    // Skip test: we don't ship remote settings dumps on Android (see package-manifest).
    return;
  }

  // When collection has a dump in services/settings/dumps/{bucket}/{collection}.json
  const data = await clientWithDump.get();
  notEqual(data.length, 0);
  // No synchronization happened (responses are not mocked).
});
add_task(clear_state);

add_task(async function test_get_does_not_sync_if_empty_dump_is_provided() {
  if (IS_ANDROID) {
    // Skip test: we don't ship remote settings dumps on Android (see package-manifest).
    return;
  }

  const clientWithEmptyDump = RemoteSettings("example");
  Assert.ok(!(await Utils.hasLocalData(clientWithEmptyDump)));

  const data = await clientWithEmptyDump.get();

  equal(data.length, 0);
  Assert.ok(await Utils.hasLocalData(clientWithEmptyDump));
});

add_task(async function test_get_synchronization_can_be_disabled() {
  const data = await client.get({ syncIfEmpty: false });

  equal(data.length, 0);
});
add_task(clear_state);

add_task(async function test_get_triggers_synchronization_when_database_is_empty() {
  // The "password-fields" collection has no local dump, and no local data.
  // Therefore a synchronization will happen.
  const data = await client.get();

  // Data comes from mocked HTTP response (see below).
  equal(data.length, 1);
  equal(data[0].selector, "#webpage[field-pwd]");
});
add_task(clear_state);

add_task(async function test_get_ignores_synchronization_errors() {
  // The monitor endpoint won't contain any information about this collection.
  let data = await RemoteSettings("some-unknown-key").get();
  equal(data.length, 0);
  // The sync endpoints are not mocked, this fails internally.
  data = await RemoteSettings("no-mocked-responses").get();
  equal(data.length, 0);
});
add_task(clear_state);

add_task(async function test_get_can_verify_signature() {
  // No signature in metadata.
  let error;
  try {
    await client.get({ verifySignature: true, syncIfEmpty: false });
  } catch (e) {
    error = e;
  }
  equal(error.message, "Missing signature (main/password-fields)");

  // Populate the local DB (record and metadata)
  await client.maybeSync(2000);

  // It validates signature that was stored in local DB.
  let calledSignature;
  client._verifier = {
    async asyncVerifyContentSignature(serialized, signature) {
      calledSignature = signature;
      return JSON.parse(serialized).data.length == 1;
    },
  };
  await client.get({ verifySignature: true });
  ok(calledSignature.endsWith("abcdef"));

  // It throws when signature does not verify.
  const col = await client.openCollection();
  await col.delete("9d500963-d80e-3a91-6e74-66f3811b99cc");
  try {
    await client.get({ verifySignature: true });
  } catch (e) {
    error = e;
  }
  equal(error.message, "Invalid content signature (main/password-fields)");
});
add_task(clear_state);

add_task(async function test_sync_event_provides_information_about_records() {
  let eventData;
  client.on("sync", ({ data }) => eventData = data);

  await client.maybeSync(2000);
  equal(eventData.current.length, 1);

  await client.maybeSync(3001);
  equal(eventData.current.length, 2);
  equal(eventData.created.length, 1);
  equal(eventData.created[0].website, "https://www.other.org/signin");
  equal(eventData.updated.length, 1);
  equal(eventData.updated[0].old.website, "https://some-website.com");
  equal(eventData.updated[0].new.website, "https://some-website.com/login");
  equal(eventData.deleted.length, 0);

  await client.maybeSync(4001);
  equal(eventData.current.length, 1);
  equal(eventData.created.length, 0);
  equal(eventData.updated.length, 0);
  equal(eventData.deleted.length, 1);
  equal(eventData.deleted[0].website, "https://www.other.org/signin");
});
add_task(clear_state);

add_task(async function test_inspect_method() {
  // Synchronize the `password-fields` collection in order to have
  // some local data when .inspect() is called.
  await client.maybeSync(2000);

  const inspected = await RemoteSettings.inspect();

  // Assertion for global attributes.
  const { mainBucket, serverURL, defaultSigner, collections, serverTimestamp } = inspected;
  const rsSigner = "remote-settings.content-signature.mozilla.org";
  equal(mainBucket, "main");
  equal(serverURL, `http://localhost:${server.identity.primaryPort}/v1`);
  equal(defaultSigner, rsSigner);
  equal(serverTimestamp, '"5000"');

  // A collection is listed in .inspect() if it has local data or if there
  // is a JSON dump for it.
  // "password-fields" has no dump but was synchronized above and thus has local data.
  let col = collections.pop();
  equal(col.collection, "password-fields");
  equal(col.serverTimestamp, 3000);
  equal(col.localTimestamp, 3000);

  if (!IS_ANDROID) {
    // "language-dictionaries" has a local dump (not on Android)
    col = collections.pop();
    equal(col.collection, "language-dictionaries");
    equal(col.serverTimestamp, 4000);
    ok(!col.localTimestamp); // not synchronized.
  }
});
add_task(clear_state);

add_task(async function test_listeners_are_not_deduplicated() {
  let count = 0;
  const plus1 = () => { count += 1; };

  client.on("sync", plus1);
  client.on("sync", plus1);
  client.on("sync", plus1);

  await client.maybeSync(2000);

  equal(count, 3);
});
add_task(clear_state);

add_task(async function test_listeners_can_be_removed() {
  let count = 0;
  const onSync = () => { count += 1; };

  client.on("sync", onSync);
  client.off("sync", onSync);

  await client.maybeSync(2000);

  equal(count, 0);
});
add_task(clear_state);

add_task(async function test_all_listeners_are_executed_if_one_fails() {
  let count = 0;
  client.on("sync", () => { count += 1; });
  client.on("sync", () => { throw new Error("boom"); });
  client.on("sync", () => { count += 2; });

  let error;
  try {
    await client.maybeSync(2000);
  } catch (e) {
    error = e;
  }

  equal(count, 3);
  equal(error.message, "boom");
});
add_task(clear_state);

add_task(async function test_telemetry_reports_up_to_date() {
  await client.maybeSync(2000);
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  await client.maybeSync(3000);

  // No Telemetry was sent.
  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.UP_TO_DATE]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_if_sync_succeeds() {
  // We test each client because Telemetry requires preleminary declarations.
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  await client.maybeSync(2000);

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.SUCCESS]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_synchronization_duration_is_reported_in_uptake_status() {
  await withFakeChannel("nightly", async () => {
    await client.maybeSync(2000);

    TelemetryTestUtils.assertEvents([
      ["uptake.remotecontent.result", "uptake", "remotesettings", UptakeTelemetry.STATUS.SUCCESS,
        { source: client.identifier, duration: (v) => v > 0, trigger: "manual" }],
    ]);
  });
});
add_task(clear_state);

add_task(async function test_telemetry_reports_if_application_fails() {
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);
  client.on("sync", () => { throw new Error("boom"); });

  try {
    await client.maybeSync(2000);
  } catch (e) {}

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.APPLY_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_reports_if_sync_fails() {
  const collection = await client.openCollection();
  await collection.db.saveLastModified(9999);

  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  try {
    await client.maybeSync(10000);
  } catch (e) {}

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.SERVER_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_reports_if_parsing_fails() {
  const collection = await client.openCollection();
  await collection.db.saveLastModified(10000);

  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  try {
    await client.maybeSync(10001);
  } catch (e) { }

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = { [UptakeTelemetry.STATUS.PARSE_ERROR]: 1 };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_reports_if_fetching_signature_fails() {
  const collection = await client.openCollection();
  await collection.db.saveLastModified(11000);

  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  try {
    await client.maybeSync(11001);
  } catch (e) { }

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = { [UptakeTelemetry.STATUS.SERVER_ERROR]: 1 };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_reports_unknown_errors() {
  const backup = client.openCollection;
  client.openCollection = () => { throw new Error("Internal"); };
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  try {
    await client.maybeSync(2000);
  } catch (e) {}

  client.openCollection = backup;
  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.UNKNOWN_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_telemetry_reports_indexeddb_as_custom_1() {
  const backup = client.openCollection;
  const msg = "IndexedDB getLastModified() The operation failed for reasons unrelated to the database itself";
  client.openCollection = () => { throw new Error(msg); };
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  try {
    await client.maybeSync(2000);
  } catch (e) { }

  client.openCollection = backup;
  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.CUSTOM_1_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_bucketname_changes_when_bucket_pref_changes() {
  equal(client.bucketName, "main");

  Services.prefs.setCharPref("services.settings.default_bucket", "main-preview");

  equal(client.bucketName, "main-preview");
});
add_task(clear_state);

add_task(async function test_inspect_changes_the_list_when_bucket_pref_is_changed() {
  if (IS_ANDROID) {
     // Skip test: we don't ship remote settings dumps on Android (see package-manifest).
    return;
  }
  // Register a client only listed in -preview...
  RemoteSettings("crash-rate");

  const { collections: before } = await RemoteSettings.inspect();

  // These two collections are listed in the main bucket in monitor/changes (one with dump, one registered).
  deepEqual(before.map(c => c.collection).sort(), ["language-dictionaries", "password-fields"]);

  // Switch to main-preview bucket.
  Services.prefs.setCharPref("services.settings.default_bucket", "main-preview");
  const { collections: after, mainBucket } = await RemoteSettings.inspect();

  // These two collections are listed in the main bucket in monitor/changes (both are registered).
  deepEqual(after.map(c => c.collection).sort(), ["crash-rate", "password-fields"]);
  equal(mainBucket, "main-preview");
});
add_task(clear_state);

// get a response for a given request from sample data
function getSampleResponse(req, port) {
  const responses = {
    "OPTIONS": {
      "sampleHeaders": [
        "Access-Control-Allow-Headers: Content-Length,Expires,Backoff,Retry-After,Last-Modified,Total-Records,ETag,Pragma,Cache-Control,authorization,content-type,if-none-match,Alert,Next-Page",
        "Access-Control-Allow-Methods: GET,HEAD,OPTIONS,POST,DELETE,OPTIONS",
        "Access-Control-Allow-Origin: *",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": null,
    },
    "GET:/v1/": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": {
        "settings": {
          "batch_max_requests": 25,
        },
        "url": `http://localhost:${port}/v1/`,
        "documentation": "https://kinto.readthedocs.org/",
        "version": "1.5.1",
        "commit": "cbc6f58",
        "hello": "kinto",
      },
    },
    "GET:/v1/buckets/monitor/collections/changes/records": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        `Date: ${new Date().toUTCString()}`,
        "Etag: \"5000\"",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": {
        "data": [{
          "id": "4676f0c7-9757-4796-a0e8-b40a5a37a9c9",
          "bucket": "main",
          "collection": "unknown-locally",
          "last_modified": 5000,
        }, {
          "id": "4676f0c7-9757-4796-a0e8-b40a5a37a9c9",
          "bucket": "main",
          "collection": "language-dictionaries",
          "last_modified": 4000,
        }, {
          "id": "0af8da0b-3e03-48fb-8d0d-2d8e4cb7514d",
          "bucket": "main",
          "collection": "password-fields",
          "last_modified": 3000,
        }, {
          "id": "4acda969-3bd3-4074-a678-ff311eeb076e",
          "bucket": "main-preview",
          "collection": "password-fields",
          "last_modified": 2000,
        }, {
          "id": "58697bd1-315f-4185-9bee-3371befc2585",
          "bucket": "main-preview",
          "collection": "crash-rate",
          "last_modified": 1000,
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"1234\"",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": JSON.stringify({
        "data": {
          "id": "password-fields",
          "last_modified": 1234,
          "signature": {
            "signature": "abcdef",
            "x5u": `http://localhost:${port}/fake-x5u`,
          },
        },
      }),
    },
    "GET:/fake-x5u": {
      "sampleHeaders": [
        "Content-Type: /octet-stream",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": `-----BEGIN CERTIFICATE-----
MIIGYTCCBEmgAwIBAgIBATANBgkqhkiG9w0BAQwFADB9MQswCQYDVQQGEwJVU
ZARKjbu1TuYQHf0fs+GwID8zeLc2zJL7UzcHFwwQ6Nda9OJN4uPAuC/BKaIpxCLL
26b24/tRam4SJjqpiq20lynhUrmTtt6hbG3E1Hpy3bmkt2DYnuMFwEx2gfXNcnbT
wNuvFqc=
-----END CERTIFICATE-----`,
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=2000&_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": {
        "data": [{
          "id": "9d500963-d80e-3a91-6e74-66f3811b99cc",
          "last_modified": 3000,
          "website": "https://some-website.com",
          "selector": "#user[password]",
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=3001&_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": {
        "data": [{
          "id": "aabad965-e556-ffe7-4191-074f5dee3df3",
          "last_modified": 4000,
          "website": "https://www.other.org/signin",
          "selector": "#signinpassword",
        }, {
          "id": "9d500963-d80e-3a91-6e74-66f3811b99cc",
          "last_modified": 3500,
          "website": "https://some-website.com/login",
          "selector": "input#user[password]",
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=4001&_sort=-last_modified&_since=4000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"5000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": {
        "data": [{
          "id": "aabad965-e556-ffe7-4191-074f5dee3df3",
          "deleted": true,
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=10000&_sort=-last_modified&_since=9999": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 503, statusText: "Service Unavailable"},
      "responseBody": {
        code: 503,
        errno: 999,
        error: "Service Unavailable",
      },
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=10001&_sort=-last_modified&_since=10000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"10001\"",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": "<invalid json",
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=11001&_sort=-last_modified&_since=11000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": { status: 503, statusText: "Service Unavailable" },
      "responseBody": {
        "data": [{
          "id": "c4f021e3-f68c-4269-ad2a-d4ba87762b35",
          "last_modified": 4000,
          "website": "https://www.eff.org",
          "selector": "#pwd",
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields?_expected=11001": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": { status: 503, statusText: "Service Unavailable" },
      "responseBody": {
        code: 503,
        errno: 999,
        error: "Service Unavailable",
      },
    },
    "GET:/v1/buckets/monitor/collections/changes/records?collection=password-fields&bucket=main": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        `Date: ${new Date().toUTCString()}`,
        "Etag: \"1338\"",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": {
        "data": [{
          "id": "fe5758d0-c67a-42d0-bb4f-8f2d75106b65",
          "bucket": "main",
          "collection": "password-fields",
          "last_modified": 1337,
        }],
      },
    },
    "GET:/v1/buckets/main/collections/password-fields/records?_expected=1337&_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\"",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": {
        "data": [{
          "id": "312cc78d-9c1f-4291-a4fa-a1be56f6cc69",
          "last_modified": 3000,
          "website": "https://some-website.com",
          "selector": "#webpage[field-pwd]",
        }],
      },
    },
    "GET:/v1/buckets/monitor/collections/changes/records?collection=no-mocked-responses&bucket=main": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        `Date: ${new Date().toUTCString()}`,
        "Etag: \"713705\"",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": {
        "data": [{
          "id": "07a98d1b-7c62-4344-ab18-76856b3facd8",
          "bucket": "main",
          "collection": "no-mocked-responses",
          "last_modified": 713705,
        }],
      },
    },
  };
  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[`${req.method}:${req.path}`] ||
         responses[req.method];
}
