const { Constructor: CC } = Components;

Cu.import("resource://testing-common/httpd.js");

const { OneCRLBlocklistClient } = Cu.import("resource://services-common/blocklist-clients.js", {});
const { Kinto } = Cu.import("resource://services-common/kinto-offline-client.js", {});
const { FirefoxAdapter } = Cu.import("resource://services-common/kinto-storage-adapter.js", {});

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");

let server;

// set up what we need to make storage adapters
let sqliteHandle;
const KINTO_FILENAME = "kinto.sqlite";

function do_get_kinto_collection(collectionName) {
  let config = {
    // Set the remote to be some server that will cause test failure when
    // hit since we should never hit the server directly, only via maybeSync()
    remote: "https://firefox.settings.services.mozilla.com/v1/",
    // Set up the adapter and bucket as normal
    adapter: FirefoxAdapter,
    adapterOptions: {sqliteHandle},
    bucket: "blocklists"
  };
  return new Kinto(config).collection(collectionName);
}

// Some simple tests to demonstrate that the logic inside maybeSync works
// correctly and that simple kinto operations are working as expected. There
// are more tests for core Kinto.js (and its storage adapter) in the
// xpcshell tests under /services/common
add_task(function* test_something() {
  const configPath = "/v1/";
  const recordsPath = "/v1/buckets/blocklists/collections/certificates/records";

  const dummyServerURL = `http://localhost:${server.identity.primaryPort}/v1`;
  Services.prefs.setCharPref("services.settings.server", dummyServerURL);

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
      do_print(e);
    }
  }
  server.registerPathHandler(configPath, handleResponse);
  server.registerPathHandler(recordsPath, handleResponse);

  // Test an empty db populates
  yield OneCRLBlocklistClient.maybeSync(2000, Date.now());

  sqliteHandle = yield FirefoxAdapter.openConnection({path: KINTO_FILENAME});
  const collection = do_get_kinto_collection("certificates");

  // Open the collection, verify it's been populated:
  let list = yield collection.list();
  // We know there will be initial values from the JSON dump.
  // (at least as many as in the dump shipped when this test was written).
  do_check_true(list.data.length >= 363);

  // No sync will be intented if maybeSync() is up-to-date.
  Services.prefs.clearUserPref("services.settings.server");
  Services.prefs.setIntPref("services.blocklist.onecrl.checked", 0);
  // Use any last_modified older than highest shipped in JSON dump.
  yield OneCRLBlocklistClient.maybeSync(123456, Date.now());
  // Last check value was updated.
  do_check_neq(0, Services.prefs.getIntPref("services.blocklist.onecrl.checked"));

  // Restore server pref.
  Services.prefs.setCharPref("services.settings.server", dummyServerURL);
  // clear the collection, save a non-zero lastModified so we don't do
  // import of initial data when we sync again.
  yield collection.clear();
  // a lastModified value of 1000 means we get a remote collection with a
  // single record
  yield collection.db.saveLastModified(1000);
  yield OneCRLBlocklistClient.maybeSync(2000, Date.now());

  // Open the collection, verify it's been updated:
  // Our test data now has two records; both should be in the local collection
  list = yield collection.list();
  do_check_eq(list.data.length, 1);

  // Test the db is updated when we call again with a later lastModified value
  yield OneCRLBlocklistClient.maybeSync(4000, Date.now());

  // Open the collection, verify it's been updated:
  // Our test data now has two records; both should be in the local collection
  list = yield collection.list();
  do_check_eq(list.data.length, 3);

  // Try to maybeSync with the current lastModified value - no connection
  // should be attempted.
  // Clear the kinto base pref so any connections will cause a test failure
  Services.prefs.clearUserPref("services.settings.server");
  yield OneCRLBlocklistClient.maybeSync(4000, Date.now());

  // Try again with a lastModified value at some point in the past
  yield OneCRLBlocklistClient.maybeSync(3000, Date.now());

  // Check the OneCRL check time pref is modified, even if the collection
  // hasn't changed
  Services.prefs.setIntPref("services.blocklist.onecrl.checked", 0);
  yield OneCRLBlocklistClient.maybeSync(3000, Date.now());
  let newValue = Services.prefs.getIntPref("services.blocklist.onecrl.checked");
  do_check_neq(newValue, 0);

  // Check that a sync completes even when there's bad data in the
  // collection. This will throw on fail, so just calling maybeSync is an
  // acceptible test.
  Services.prefs.setCharPref("services.settings.server", dummyServerURL);
  yield OneCRLBlocklistClient.maybeSync(5000, Date.now());
});

function run_test() {
  // Ensure that signature verification is disabled to prevent interference
  // with basic certificate sync tests
  Services.prefs.setBoolPref("services.blocklist.signing.enforced", false);

  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  do_register_cleanup(function() {
    server.stop(() => { });
    return sqliteHandle.close();
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
        "Server: waitress"
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": "null"
    },
    "GET:/v1/?": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress"
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({
        "settings": {
          "batch_max_requests": 25
        },
        "url": `http://localhost:${port}/v1/`,
        "documentation": "https://kinto.readthedocs.org/",
        "version": "1.5.1",
        "commit": "cbc6f58",
        "hello": "kinto"
      })
    },
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"1000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{}]})
    },
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified&_since=1000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "issuerName": "MEQxCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwx0aGF3dGUsIEluYy4xHjAcBgNVBAMTFXRoYXd0ZSBFViBTU0wgQ0EgLSBHMw==",
        "serialNumber": "CrTHPEE6AZSfI3jysin2bA==",
        "id": "78cf8900-fdea-4ce5-f8fb-b78710617718",
        "last_modified": 3000
      }]})
    },
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "issuerName": "MFkxCzAJBgNVBAYTAk5MMR4wHAYDVQQKExVTdGFhdCBkZXIgTmVkZXJsYW5kZW4xKjAoBgNVBAMTIVN0YWF0IGRlciBOZWRlcmxhbmRlbiBPdmVyaGVpZCBDQQ",
        "serialNumber": "ATFpsA==",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02c",
        "last_modified": 4000
      }, {
        "subject": "MCIxIDAeBgNVBAMMF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5",
        "pubKeyHash": "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02d",
        "last_modified": 4000
      }]})
    },
    "GET:/v1/buckets/blocklists/collections/certificates/records?_sort=-last_modified&_since=4000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"5000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "issuerName": "not a base64 encoded issuer",
        "serialNumber": "not a base64 encoded serial",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02e",
        "last_modified": 5000
      }, {
        "subject": "not a base64 encoded subject",
        "pubKeyHash": "not a base64 encoded pubKeyHash",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02f",
        "last_modified": 5000
      }, {
        "subject": "MCIxIDAeBgNVBAMMF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5",
        "pubKeyHash": "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=",
        "id": "dabafde9-df4a-ddba-2548-748da04cc02g",
        "last_modified": 5000
      }]})
    }
  };
  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[req.method];

}
