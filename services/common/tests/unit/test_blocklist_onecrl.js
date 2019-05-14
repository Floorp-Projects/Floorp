const { Constructor: CC } = Components;

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {BlocklistClients} = ChromeUtils.import("resource://services-common/blocklist-clients.js");

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");

let server;

// Some simple tests to demonstrate that the logic inside maybeSync works
// correctly and that simple kinto operations are working as expected. There
// are more tests for core Kinto.js (and its storage adapter) in the
// xpcshell tests under /services/common
add_task(async function test_something() {
  const dummyServerURL = `http://localhost:${server.identity.primaryPort}/v1`;
  Services.prefs.setCharPref("services.settings.server", dummyServerURL);

  const {OneCRLBlocklistClient} = BlocklistClients.initialize({verifySignature: false});

  // register a handler
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

      response.write(sample.responseBody);
    } catch (e) {
      info(e);
    }
  }
  server.registerPathHandler("/v1/", handleResponse);
  server.registerPathHandler("/v1/buckets/security-state/collections/onecrl", handleResponse);
  server.registerPathHandler("/v1/buckets/security-state/collections/onecrl/records", handleResponse);

  // Test an empty db populates from JSON dump.
  await OneCRLBlocklistClient.maybeSync(42);

  // Open the collection, verify it's been populated:
  const list = await OneCRLBlocklistClient.get();
  // We know there will be initial values from the JSON dump.
  // (at least as many as in the dump shipped when this test was written).
  Assert.ok(list.length >= 363);

  // No sync will be intented if maybeSync() is up-to-date.
  Services.prefs.clearUserPref("services.settings.server");
  Services.prefs.setIntPref("services.settings.security.onecrl.checked", 0);
  // Use any last_modified older than highest shipped in JSON dump.
  await OneCRLBlocklistClient.maybeSync(123456);

  // Restore server pref.
  Services.prefs.setCharPref("services.settings.server", dummyServerURL);

  // clear the collection, save a non-zero lastModified so we don't do
  // import of initial data when we sync again.
  const collection = await OneCRLBlocklistClient.openCollection();
  await collection.clear();
  // a lastModified value of 1000 means we get a remote collection with a
  // single record
  await collection.db.saveLastModified(1000);

  await OneCRLBlocklistClient.maybeSync(2000);

  // Open the collection, verify it's been updated:
  // Our test data now has two records; both should be in the local collection
  const before = await OneCRLBlocklistClient.get();
  Assert.equal(before.length, 1);

  // Test the db is updated when we call again with a later lastModified value
  await OneCRLBlocklistClient.maybeSync(4000);

  // Open the collection, verify it's been updated:
  // Our test data now has two records; both should be in the local collection
  const after = await OneCRLBlocklistClient.get();
  Assert.equal(after.length, 3);

  // Try to maybeSync with the current lastModified value - no connection
  // should be attempted.
  // Clear the kinto base pref so any connections will cause a test failure
  Services.prefs.clearUserPref("services.settings.server");
  await OneCRLBlocklistClient.maybeSync(4000);

  // Try again with a lastModified value at some point in the past
  await OneCRLBlocklistClient.maybeSync(3000);

  // Check that a sync completes even when there's bad data in the
  // collection. This will throw on fail, so just calling maybeSync is an
  // acceptible test.
  Services.prefs.setCharPref("services.settings.server", dummyServerURL);
  await OneCRLBlocklistClient.maybeSync(5000);
});

function run_test() {
  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  registerCleanupFunction(function() {
    server.stop(() => { });
  });
}

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
      "responseBody": "null",
    },
    "GET:/v1/?": {
      "sampleHeaders": [
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
      }),
    },
    "GET:/v1/buckets/security-state/collections/onecrl": {
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
          "id": "onecrl",
          "last_modified": 1234,
        },
      }),
    },
    "GET:/v1/buckets/security-state/collections/onecrl/records?_expected=2000&_sort=-last_modified&_since=1000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "issuerName": "MEQxCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwx0aGF3dGUsIEluYy4xHjAcBgNVBAMTFXRoYXd0ZSBFViBTU0wgQ0EgLSBHMw==",
        "serialNumber": "CrTHPEE6AZSfI3jysin2bA==",
        "id": "78cf8900-fdea-4ce5-f8fb-b78710617718",
        "last_modified": 3000,
      }]}),
    },
    "GET:/v1/buckets/security-state/collections/onecrl/records?_expected=4000&_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "issuerName": "MFkxCzAJBgNVBAYTAk5MMR4wHAYDVQQKExVTdGFhdCBkZXIgTmVkZXJsYW5kZW4xKjAoBgNVBAMTIVN0YWF0IGRlciBOZWRlcmxhbmRlbiBPdmVyaGVpZCBDQQ",
        "serialNumber": "ATFpsA==",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02c",
        "last_modified": 4000,
      }, {
        "subject": "MCIxIDAeBgNVBAMMF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5",
        "pubKeyHash": "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02d",
        "last_modified": 4000,
      }]}),
    },
    "GET:/v1/buckets/security-state/collections/onecrl/records?_expected=5000&_sort=-last_modified&_since=4000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"5000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "issuerName": "not a base64 encoded issuer",
        "serialNumber": "not a base64 encoded serial",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02e",
        "last_modified": 5000,
      }, {
        "subject": "not a base64 encoded subject",
        "pubKeyHash": "not a base64 encoded pubKeyHash",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02f",
        "last_modified": 5000,
      }, {
        "subject": "MCIxIDAeBgNVBAMMF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5",
        "pubKeyHash": "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02g",
        "last_modified": 5000,
      }]}),
    },
  };
  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[`${req.method}:${req.path}`] ||
         responses[req.method];
}
