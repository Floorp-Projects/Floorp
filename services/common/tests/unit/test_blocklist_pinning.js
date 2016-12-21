"use strict"

const { Constructor: CC } = Components;

Cu.import("resource://testing-common/httpd.js");

const { Kinto } = Cu.import("resource://services-common/kinto-offline-client.js");
const { FirefoxAdapter } = Cu.import("resource://services-common/kinto-storage-adapter.js");

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");

const PREF_BLOCKLIST_PINNING_COLLECTION = "services.blocklist.pinning.collection";
const COLLECTION_NAME = "pins";
const KINTO_STORAGE_PATH    = "kinto.sqlite";

// First, we need to setup appInfo or we can't do version checks
var id = "xpcshell@tests.mozilla.org";
var appName = "XPCShell";
var version = "1";
var platformVersion = "1.9.2";
Cu.import("resource://testing-common/AppInfo.jsm", this);

updateAppInfo({
  name: appName,
  ID: id,
  version: version,
  platformVersion: platformVersion ? platformVersion : "1.0",
  crashReporter: true,
});

let server;


function do_get_kinto_collection(connection, collectionName) {
  let config = {
    // Set the remote to be some server that will cause test failure when
    // hit since we should never hit the server directly (any non-local
    // request causes failure in tests), only via maybeSync()
    remote: "https://firefox.settings.services.mozilla.com/v1/",
    // Set up the adapter and bucket as normal
    adapter: FirefoxAdapter,
    adapterOptions: {sqliteHandle: connection},
    bucket: "pinning"
  };
  let kintoClient = new Kinto(config);
  return kintoClient.collection(collectionName);
}

// Some simple tests to demonstrate that the core preload sync operations work
// correctly and that simple kinto operations are working as expected.
add_task(function* test_something(){
  // set the collection name explicitly - since there will be version
  // specific collection names in prefs
  Services.prefs.setCharPref(PREF_BLOCKLIST_PINNING_COLLECTION,
                             COLLECTION_NAME);

  const { PinningPreloadClient } = Cu.import("resource://services-common/blocklist-clients.js");

  const configPath = "/v1/";
  const recordsPath = "/v1/buckets/pinning/collections/pins/records";

  Services.prefs.setCharPref("services.settings.server",
                             `http://localhost:${server.identity.primaryPort}/v1`);

  // register a handler
  function handleResponse (request, response) {
    try {
      const sample = getSampleResponse(request, server.identity.primaryPort);
      if (!sample) {
        do_throw(`unexpected ${request.method} request for ${request.path}?${request.queryString}`);
      }

      response.setStatusLine(null, sample.status.status,
                             sample.status.statusText);
      // send the headers
      for (let headerLine of sample.sampleHeaders) {
        let headerElements = headerLine.split(':');
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

  let sss = Cc["@mozilla.org/ssservice;1"]
              .getService(Ci.nsISiteSecurityService);

  // ensure our pins are all missing before we start
  ok(!sss.isSecureHost(sss.HEADER_HPKP, "one.example.com", 0));
  ok(!sss.isSecureHost(sss.HEADER_HPKP, "two.example.com", 0));
  ok(!sss.isSecureHost(sss.HEADER_HPKP, "three.example.com", 0));
  ok(!sss.isSecureHost(sss.HEADER_HSTS, "five.example.com", 0));

  // Test an empty db populates
  let result = yield PinningPreloadClient.maybeSync(2000, Date.now());

  let connection = yield FirefoxAdapter.openConnection({path: KINTO_STORAGE_PATH});

  // Open the collection, verify it's been populated:
  // Our test data has a single record; it should be in the local collection
  let collection = do_get_kinto_collection(connection, COLLECTION_NAME);
  let list = yield collection.list();
  do_check_eq(list.data.length, 1);

  // check that a pin exists for one.example.com
  ok(sss.isSecureHost(sss.HEADER_HPKP, "one.example.com", 0));

  // Test the db is updated when we call again with a later lastModified value
  result = yield PinningPreloadClient.maybeSync(4000, Date.now());

  // Open the collection, verify it's been updated:
  // Our data now has four new records; all should be in the local collection
  collection = do_get_kinto_collection(connection, COLLECTION_NAME);
  list = yield collection.list();
  do_check_eq(list.data.length, 5);
  yield connection.close();

  // check that a pin exists for two.example.com and three.example.com
  ok(sss.isSecureHost(sss.HEADER_HPKP, "two.example.com", 0));
  ok(sss.isSecureHost(sss.HEADER_HPKP, "three.example.com", 0));

  // check that a pin does not exist for four.example.com - it's in the
  // collection but the version should not match
  ok(!sss.isSecureHost(sss.HEADER_HPKP, "four.example.com", 0));

  // Try to maybeSync with the current lastModified value - no connection
  // should be attempted.
  // Clear the kinto base pref so any connections will cause a test failure
  Services.prefs.clearUserPref("services.settings.server");
  yield PinningPreloadClient.maybeSync(4000, Date.now());

  // Try again with a lastModified value at some point in the past
  yield PinningPreloadClient.maybeSync(3000, Date.now());

  // Check the pinning check time pref is modified, even if the collection
  // hasn't changed
  Services.prefs.setIntPref("services.blocklist.onecrl.checked", 0);
  yield PinningPreloadClient.maybeSync(3000, Date.now());
  let newValue = Services.prefs.getIntPref("services.blocklist.pinning.checked");
  do_check_neq(newValue, 0);

  // Check that the HSTS preload added to the collection works...
  ok(sss.isSecureHost(sss.HEADER_HSTS, "five.example.com", 0));
  // ...and that includeSubdomains is honored
  ok(!sss.isSecureHost(sss.HEADER_HSTS, "subdomain.five.example.com", 0));

  // Check that a sync completes even when there's bad data in the
  // collection. This will throw on fail, so just calling maybeSync is an
  // acceptible test (the data below with last_modified of 300 is nonsense).
  Services.prefs.setCharPref("services.settings.server",
                             `http://localhost:${server.identity.primaryPort}/v1`);
  yield PinningPreloadClient.maybeSync(5000, Date.now());

  // The STS entry for five.example.com now has includeSubdomains set;
  // ensure that the new includeSubdomains value is honored.
  ok(sss.isSecureHost(sss.HEADER_HSTS, "subdomain.five.example.com", 0));
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
  });
}

// get a response for a given request from sample data
function getSampleResponse(req, port) {
  const appInfo = Cc["@mozilla.org/xre/app-info;1"]
                     .getService(Ci.nsIXULAppInfo);

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
      "responseBody": JSON.stringify({"settings":{"batch_max_requests":25}, "url":`http://localhost:${port}/v1/`, "documentation":"https://kinto.readthedocs.org/", "version":"1.5.1", "commit":"cbc6f58", "hello":"kinto"})
    },
    "GET:/v1/buckets/pinning/collections/pins/records?_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data":[{
        "pinType": "KeyPin",
        "hostName": "one.example.com",
        "includeSubdomains": false,
        "expires": new Date().getTime() + 1000000,
        "pins" : ["cUPcTAZWKaASuYWhhneDttWpY3oBAkE3h2+soZS7sWs=",
                  "M8HztCzM3elUxkcjR2S5P4hhyBNf6lHkmjAHKhpGPWE="],
        "versions" : [appInfo.version],
        "id":"78cf8900-fdea-4ce5-f8fb-b78710617718",
        "last_modified":3000
      }]})
    },
    "GET:/v1/buckets/pinning/collections/pins/records?_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data":[{
        "pinType": "KeyPin",
        "hostName": "two.example.com",
        "includeSubdomains": false,
        "expires": new Date().getTime() + 1000000,
        "pins" : ["cUPcTAZWKaASuYWhhneDttWpY3oBAkE3h2+soZS7sWs=",
                  "M8HztCzM3elUxkcjR2S5P4hhyBNf6lHkmjAHKhpGPWE="],
        "versions" : [appInfo.version],
        "id":"dabafde9-df4a-ddba-2548-748da04cc02c",
        "last_modified":4000
      },{
        "pinType": "KeyPin",
        "hostName": "three.example.com",
        "includeSubdomains": false,
        "expires": new Date().getTime() + 1000000,
        "pins" : ["cUPcTAZWKaASuYWhhneDttWpY3oBAkE3h2+soZS7sWs=",
                  "M8HztCzM3elUxkcjR2S5P4hhyBNf6lHkmjAHKhpGPWE="],
        "versions" : [appInfo.version, "some other version that won't match"],
        "id":"dabafde9-df4a-ddba-2548-748da04cc02d",
        "last_modified":4000
      },{
        "pinType": "KeyPin",
        "hostName": "four.example.com",
        "includeSubdomains": false,
        "expires": new Date().getTime() + 1000000,
        "pins" : ["cUPcTAZWKaASuYWhhneDttWpY3oBAkE3h2+soZS7sWs=",
                  "M8HztCzM3elUxkcjR2S5P4hhyBNf6lHkmjAHKhpGPWE="],
        "versions" : ["some version that won't match"],
        "id":"dabafde9-df4a-ddba-2548-748da04cc02e",
        "last_modified":4000
      },{
        "pinType": "STSPin",
        "hostName": "five.example.com",
        "includeSubdomains": false,
        "expires": new Date().getTime() + 1000000,
        "versions" : [appInfo.version, "some version that won't match"],
        "id":"dabafde9-df4a-ddba-2548-748da04cc032",
        "last_modified":4000
      }]})
    },
    "GET:/v1/buckets/pinning/collections/pins/records?_sort=-last_modified&_since=4000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"5000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data":[{
        "irrelevant":"this entry looks nothing whatsoever like a pin preload",
        "pinType": "KeyPin",
        "id":"dabafde9-df4a-ddba-2548-748da04cc02f",
        "last_modified":5000
      },{
        "irrelevant":"this entry has data of the wrong type",
        "pinType": "KeyPin",
        "hostName": 3,
        "includeSubdomains": "nonsense",
        "expires": "more nonsense",
        "pins" : [1,2,3,4],
        "id":"dabafde9-df4a-ddba-2548-748da04cc030",
        "last_modified":5000
      },{
        "irrelevant":"this entry is missing the actual pins",
        "pinType": "KeyPin",
        "hostName": "missingpins.example.com",
        "includeSubdomains": false,
        "expires": new Date().getTime() + 1000000,
        "versions" : [appInfo.version],
        "id":"dabafde9-df4a-ddba-2548-748da04cc031",
        "last_modified":5000
      },{
        "pinType": "STSPin",
        "hostName": "five.example.com",
        "includeSubdomains": true,
        "expires": new Date().getTime() + 1000000,
        "versions" : [appInfo.version, "some version that won't match"],
        "id":"dabafde9-df4a-ddba-2548-748da04cc032",
        "last_modified":5000
      }]})
    }
  };
  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[req.method];

}
