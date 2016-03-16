/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/kinto-updater.js")
Cu.import("resource://testing-common/httpd.js");

var server;

const PREF_KINTO_BASE = "services.kinto.base";
const PREF_LAST_UPDATE = "services.kinto.last_update_seconds";
const PREF_CLOCK_SKEW_SECONDS = "services.kinto.clock_skew_seconds";

// Check to ensure maybeSync is called with correct values when a changes
// document contains information on when a collection was last modified
add_task(function* test_check_maybeSync(){
  const changesPath = "/v1/buckets/monitor/collections/changes/records";

  // register a handler
  function handleResponse (request, response) {
    try {
      const sampled = getSampleResponse(request, server.identity.primaryPort);
      if (!sampled) {
        do_throw(`unexpected ${request.method} request for ${request.path}?${request.queryString}`);
      }

      response.setStatusLine(null, sampled.status.status,
                             sampled.status.statusText);
      // send the headers
      for (let headerLine of sampled.sampleHeaders) {
        let headerElements = headerLine.split(':');
        response.setHeader(headerElements[0], headerElements[1].trimLeft());
      }

      // set the
      response.setHeader("Date", (new Date(2000)).toUTCString());

      response.write(sampled.responseBody);
    } catch (e) {
      dump(`${e}\n`);
    }
  }

  server.registerPathHandler(changesPath, handleResponse);

  // set up prefs so the kinto updater talks to the test server
  Services.prefs.setCharPref(PREF_KINTO_BASE,
    `http://localhost:${server.identity.primaryPort}/v1`);

  // set some initial values so we can check these are updated appropriately
  Services.prefs.setIntPref("services.kinto.last_update", 0);
  Services.prefs.setIntPref("services.kinto.clock_difference", 0);


  let startTime = Date.now();

  let syncPromise = new Promise(function(resolve, reject) {
    let updater = Cu.import("resource://services-common/kinto-updater.js");
    // add a test kinto client that will respond to lastModified information
    // for a collection called 'test-collection'
    updater.addTestKintoClient("test-collection", {
      "maybeSync": function(lastModified, serverTime){
        // ensire the lastModified and serverTime values are as expected
        do_check_eq(lastModified, 1000);
        do_check_eq(serverTime, 2000);
        resolve();
      }
    });
    updater.checkVersions();
  });

  // ensure we get the maybeSync call
  yield syncPromise;

  // check the last_update is updated
  do_check_eq(Services.prefs.getIntPref(PREF_LAST_UPDATE), 2);

  // How does the clock difference look?
  let endTime = Date.now();
  let clockDifference = Services.prefs.getIntPref(PREF_CLOCK_SKEW_SECONDS);
  // we previously set the serverTime to 2 (seconds past epoch)
  do_check_eq(clockDifference <= endTime / 1000
              && clockDifference >= Math.floor(startTime / 1000) - 2, true);
});

function run_test() {
  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  do_register_cleanup(function() {
    server.stop(function() { });
  });
}

// get a response for a given request from sample data
function getSampleResponse(req, port) {
  const responses = {
    "GET:/v1/buckets/monitor/collections/changes/records?": {
      "sampleHeaders": [
        "Content-Type: application/json; charset=UTF-8"
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data":[{"host":"localhost","last_modified":1000,"bucket":"blocklists","id":"330a0c5f-fadf-ff0b-40c8-4eb0d924ff6a","collection":"test-collection"}]})
    }
  };
  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[req.method];
}
