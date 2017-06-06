const { Constructor: CC } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Timer.jsm");
const { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
const { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});

const { Kinto } = Cu.import("resource://services-common/kinto-offline-client.js", {});
const { FirefoxAdapter } = Cu.import("resource://services-common/kinto-storage-adapter.js", {});
const BlocklistClients = Cu.import("resource://services-common/blocklist-clients.js", {});
const { UptakeTelemetry } = Cu.import("resource://services-common/uptake-telemetry.js", {});

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");
const kintoFilename = "kinto.sqlite";

const gBlocklistClients = [
  {client: BlocklistClients.AddonBlocklistClient, testData: ["i808", "i720", "i539"]},
  {client: BlocklistClients.PluginBlocklistClient, testData: ["p1044", "p32", "p28"]},
  {client: BlocklistClients.GfxBlocklistClient, testData: ["g204", "g200", "g36"]},
];


let server;

function kintoCollection(collectionName, sqliteHandle) {
  const config = {
    // Set the remote to be some server that will cause test failure when
    // hit since we should never hit the server directly, only via maybeSync()
    remote: "https://firefox.settings.services.mozilla.com/v1/",
    adapter: FirefoxAdapter,
    adapterOptions: {sqliteHandle},
    bucket: "blocklists"
  };
  return new Kinto(config).collection(collectionName);
}

function* readJSON(filepath) {
  const binaryData = yield OS.File.read(filepath);
  const textData = (new TextDecoder()).decode(binaryData);
  return Promise.resolve(JSON.parse(textData));
}

function* clear_state() {
  for (let {client} of gBlocklistClients) {
    // Remove last server times.
    Services.prefs.clearUserPref(client.lastCheckTimePref);

    // Clear local DB.
    let sqliteHandle;
    try {
      sqliteHandle = yield FirefoxAdapter.openConnection({path: kintoFilename});
      const collection = kintoCollection(client.collectionName, sqliteHandle);
      yield collection.clear();
    } finally {
      yield sqliteHandle.close();
    }

    // Remove JSON dumps folders in profile dir.
    const dumpFile = OS.Path.join(OS.Constants.Path.profileDir, client.filename);
    const folder = OS.Path.dirname(dumpFile);
    yield OS.File.removeDir(folder, { ignoreAbsent: true });
  }
}


function run_test() {
  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  // Point the blocklist clients to use this local HTTP server.
  Services.prefs.setCharPref("services.settings.server",
                             `http://localhost:${server.identity.primaryPort}/v1`);

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

      response.write(sample.responseBody);
      response.finish();
    } catch (e) {
      do_print(e);
    }
  }
  const configPath = "/v1/";
  const addonsRecordsPath  = "/v1/buckets/blocklists/collections/addons/records";
  const gfxRecordsPath     = "/v1/buckets/blocklists/collections/gfx/records";
  const pluginsRecordsPath = "/v1/buckets/blocklists/collections/plugins/records";
  server.registerPathHandler(configPath, handleResponse);
  server.registerPathHandler(addonsRecordsPath, handleResponse);
  server.registerPathHandler(gfxRecordsPath, handleResponse);
  server.registerPathHandler(pluginsRecordsPath, handleResponse);


  run_next_test();

  do_register_cleanup(function() {
    server.stop(() => { });
  });
}

add_task(function* test_records_obtained_from_server_are_stored_in_db() {
  for (let {client} of gBlocklistClients) {
    // Test an empty db populates
    yield client.maybeSync(2000, Date.now(), {loadDump: false});

    // Open the collection, verify it's been populated:
    // Our test data has a single record; it should be in the local collection
    const sqliteHandle = yield FirefoxAdapter.openConnection({path: kintoFilename});
    let collection = kintoCollection(client.collectionName, sqliteHandle);
    let list = yield collection.list();
    equal(list.data.length, 1);
    yield sqliteHandle.close();
  }
});
add_task(clear_state);

add_task(function* test_list_is_written_to_file_in_profile() {
  for (let {client, testData} of gBlocklistClients) {
    const filePath = OS.Path.join(OS.Constants.Path.profileDir, client.filename);
    const profFile = new FileUtils.File(filePath);
    strictEqual(profFile.exists(), false);

    yield client.maybeSync(2000, Date.now(), {loadDump: false});

    strictEqual(profFile.exists(), true);
    const content = yield readJSON(profFile.path);
    equal(content.data[0].blockID, testData[testData.length - 1]);
  }
});
add_task(clear_state);

add_task(function* test_current_server_time_is_saved_in_pref() {
  for (let {client} of gBlocklistClients) {
    const serverTime = Date.now();
    yield client.maybeSync(2000, serverTime);
    const after = Services.prefs.getIntPref(client.lastCheckTimePref);
    equal(after, Math.round(serverTime / 1000));
  }
});
add_task(clear_state);

add_task(function* test_update_json_file_when_addons_has_changes() {
  for (let {client, testData} of gBlocklistClients) {
    yield client.maybeSync(2000, Date.now() - 1000, {loadDump: false});
    const filePath = OS.Path.join(OS.Constants.Path.profileDir, client.filename);
    const profFile = new FileUtils.File(filePath);
    const fileLastModified = profFile.lastModifiedTime = profFile.lastModifiedTime - 1000;
    const serverTime = Date.now();

    yield client.maybeSync(3001, serverTime);

    // File was updated.
    notEqual(fileLastModified, profFile.lastModifiedTime);
    const content = yield readJSON(profFile.path);
    deepEqual(content.data.map((r) => r.blockID), testData);
    // Server time was updated.
    const after = Services.prefs.getIntPref(client.lastCheckTimePref);
    equal(after, Math.round(serverTime / 1000));
  }
});
add_task(clear_state);

add_task(function* test_sends_reload_message_when_blocklist_has_changes() {
  for (let {client} of gBlocklistClients) {
    let received = yield new Promise((resolve, reject) => {
      Services.ppmm.addMessageListener("Blocklist:reload-from-disk", {
        receiveMessage(aMsg) { resolve(aMsg) }
      });

      client.maybeSync(2000, Date.now() - 1000, {loadDump: false});
    });

    equal(received.data.filename, client.filename);
  }
});
add_task(clear_state);

add_task(function* test_telemetry_reports_up_to_date() {
  for (let {client} of gBlocklistClients) {
    yield client.maybeSync(2000, Date.now() - 1000, {loadDump: false});
    const filePath = OS.Path.join(OS.Constants.Path.profileDir, client.filename);
    const profFile = new FileUtils.File(filePath);
    const fileLastModified = profFile.lastModifiedTime = profFile.lastModifiedTime - 1000;
    const serverTime = Date.now();
    const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

    yield client.maybeSync(3000, serverTime);

    // File was not updated.
    equal(fileLastModified, profFile.lastModifiedTime);
    // Server time was updated.
    const after = Services.prefs.getIntPref(client.lastCheckTimePref);
    equal(after, Math.round(serverTime / 1000));
    // No Telemetry was sent.
    const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
    const expectedIncrements = {[UptakeTelemetry.STATUS.UP_TO_DATE]: 1};
    checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
  }
});
add_task(clear_state);

add_task(function* test_telemetry_if_sync_succeeds() {
  // We test each client because Telemetry requires preleminary declarations.
  for (let {client} of gBlocklistClients) {
    const serverTime = Date.now();
    const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

    yield client.maybeSync(2000, serverTime, {loadDump: false});

    const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
    const expectedIncrements = {[UptakeTelemetry.STATUS.SUCCESS]: 1};
    checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
  }
});
add_task(clear_state);

add_task(function* test_telemetry_reports_if_application_fails() {
  const {client} = gBlocklistClients[0];
  const serverTime = Date.now();
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const backup = client.processCallback;
  client.processCallback = () => { throw new Error("boom"); };

  try {
    yield client.maybeSync(2000, serverTime, {loadDump: false});
  } catch (e) {}

  client.processCallback = backup;

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.APPLY_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(function* test_telemetry_reports_if_sync_fails() {
  const {client} = gBlocklistClients[0];
  const serverTime = Date.now();

  const sqliteHandle = yield FirefoxAdapter.openConnection({path: kintoFilename});
  const collection = kintoCollection(client.collectionName, sqliteHandle);
  yield collection.db.saveLastModified(9999);
  yield sqliteHandle.close();

  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  try {
    yield client.maybeSync(10000, serverTime);
  } catch (e) {}

  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.SYNC_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(function* test_telemetry_reports_unknown_errors() {
  const {client} = gBlocklistClients[0];
  const serverTime = Date.now();
  const backup = FirefoxAdapter.openConnection;
  FirefoxAdapter.openConnection = () => { throw new Error("Internal"); };
  const startHistogram = getUptakeTelemetrySnapshot(client.identifier);

  try {
    yield client.maybeSync(2000, serverTime);
  } catch (e) {}

  FirefoxAdapter.openConnection = backup;
  const endHistogram = getUptakeTelemetrySnapshot(client.identifier);
  const expectedIncrements = {[UptakeTelemetry.STATUS.UNKNOWN_ERROR]: 1};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
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
    "GET:/v1/buckets/blocklists/collections/addons/records?_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "prefs": [],
        "blockID": "i539",
        "last_modified": 3000,
        "versionRange": [{
          "targetApplication": [],
          "maxVersion": "*",
          "minVersion": "0",
          "severity": "1"
        }],
        "guid": "ScorpionSaver@jetpack",
        "id": "9d500963-d80e-3a91-6e74-66f3811b99cc"
      }]})
    },
    "GET:/v1/buckets/blocklists/collections/plugins/records?_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "matchFilename": "NPFFAddOn.dll",
        "blockID": "p28",
        "id": "7b1e0b3c-e390-a817-11b6-a6887f65f56e",
        "last_modified": 3000,
        "versionRange": []
      }]})
    },
    "GET:/v1/buckets/blocklists/collections/gfx/records?_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "driverVersionComparator": "LESS_THAN_OR_EQUAL",
        "driverVersion": "8.17.12.5896",
        "vendor": "0x10de",
        "blockID": "g36",
        "feature": "DIRECT3D_9_LAYERS",
        "devices": ["0x0a6c"],
        "featureStatus": "BLOCKED_DRIVER_VERSION",
        "last_modified": 3000,
        "os": "WINNT 6.1",
        "id": "3f947f16-37c2-4e96-d356-78b26363729b"
      }]})
    },
    "GET:/v1/buckets/blocklists/collections/addons/records?_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "prefs": [],
        "blockID": "i808",
        "last_modified": 4000,
        "versionRange": [{
          "targetApplication": [],
          "maxVersion": "*",
          "minVersion": "0",
          "severity": "3"
        }],
        "guid": "{c96d1ae6-c4cf-4984-b110-f5f561b33b5a}",
        "id": "9ccfac91-e463-c30c-f0bd-14143794a8dd"
      }, {
        "prefs": ["browser.startup.homepage"],
        "blockID": "i720",
        "last_modified": 3500,
        "versionRange": [{
          "targetApplication": [],
          "maxVersion": "*",
          "minVersion": "0",
          "severity": "1"
        }],
        "guid": "FXqG@xeeR.net",
        "id": "cf9b3129-a97e-dbd7-9525-a8575ac03c25"
      }]})
    },
    "GET:/v1/buckets/blocklists/collections/plugins/records?_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "infoURL": "https://get.adobe.com/flashplayer/",
        "blockID": "p1044",
        "matchFilename": "libflashplayer\\.so",
        "last_modified": 4000,
        "versionRange": [{
          "targetApplication": [],
          "minVersion": "11.2.202.509",
          "maxVersion": "11.2.202.539",
          "severity": "0",
          "vulnerabilityStatus": "1"
        }],
        "os": "Linux",
        "id": "aabad965-e556-ffe7-4191-074f5dee3df3"
      }, {
        "matchFilename": "npViewpoint.dll",
        "blockID": "p32",
        "id": "1f48af42-c508-b8ef-b8d5-609d48e4f6c9",
        "last_modified": 3500,
        "versionRange": [{
          "targetApplication": [{
            "minVersion": "3.0",
            "guid": "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}",
            "maxVersion": "*"
          }]
        }]
      }]})
    },
    "GET:/v1/buckets/blocklists/collections/gfx/records?_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "vendor": "0x8086",
        "blockID": "g204",
        "feature": "WEBGL_MSAA",
        "devices": [],
        "id": "c96bca82-e6bd-044d-14c4-9c1d67e9283a",
        "last_modified": 4000,
        "os": "Darwin 10",
        "featureStatus": "BLOCKED_DEVICE"
      }, {
        "vendor": "0x10de",
        "blockID": "g200",
        "feature": "WEBGL_MSAA",
        "devices": [],
        "id": "c3a15ba9-e0e2-421f-e399-c995e5b8d14e",
        "last_modified": 3500,
        "os": "Darwin 11",
        "featureStatus": "BLOCKED_DEVICE"
      }]})
    },
    "GET:/v1/buckets/blocklists/collections/addons/records?_sort=-last_modified&_since=9999": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 503, statusText: "Service Unavailable"},
      "responseBody": JSON.stringify({
        code: 503,
        errno: 999,
        error: "Service Unavailable",
      })
    }
  };
  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[req.method];

}
