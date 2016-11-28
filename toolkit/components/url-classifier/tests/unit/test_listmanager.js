Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

// These tables share the same updateURL.
const TEST_TABLE_DATA_LIST = [
  // 0:
  {
    tableName: "test-listmanager0-digest256",
    providerName: "google",
    updateUrl: "http://localhost:4444/safebrowsing/update",
    gethashUrl: "http://localhost:4444/safebrowsing/gethash0",
  },

  // 1:
  {
    tableName: "test-listmanager1-digest256",
    providerName: "google",
    updateUrl: "http://localhost:4444/safebrowsing/update",
    gethashUrl: "http://localhost:4444/safebrowsing/gethash1",
  },

  // 2.
  {
    tableName: "test-listmanager2-digest256",
    providerName: "google",
    updateUrl: "http://localhost:4444/safebrowsing/update",
    gethashUrl: "http://localhost:4444/safebrowsing/gethash2",
  }
];

// These tables have a different update URL (for v4).
const TEST_TABLE_DATA_V4 = {
  tableName: "test-phish-proto",
  providerName: "google4",
  updateUrl: "http://localhost:5555/safebrowsing/update?",
  gethashUrl: "http://localhost:5555/safebrowsing/gethash-v4",
};
const TEST_TABLE_DATA_V4_DISABLED = {
  tableName: "test-unwanted-proto",
  providerName: "google4",
  updateUrl: "http://localhost:5555/safebrowsing/update?",
  gethashUrl: "http://localhost:5555/safebrowsing/gethash-v4",
};

const PREF_NEXTUPDATETIME = "browser.safebrowsing.provider.google.nextupdatetime";
const PREF_NEXTUPDATETIME_V4 = "browser.safebrowsing.provider.google4.nextupdatetime";

let gListManager = Cc["@mozilla.org/url-classifier/listmanager;1"]
                     .getService(Ci.nsIUrlListManager);

let gUrlUtils = Cc["@mozilla.org/url-classifier/utils;1"]
                   .getService(Ci.nsIUrlClassifierUtils);

// Global test server for serving safebrowsing updates.
let gHttpServ = null;
let gUpdateResponse = "";
let gExpectedUpdateRequest = "";
let gExpectedQueryV4 = "";

// Handles request for TEST_TABLE_DATA_V4.
let gHttpServV4 = null;

// These two variables are used to synchronize the last two racing updates
// (in terms of "update URL") in test_update_all_tables().
let gUpdatedCntForTableData = 0; // For TEST_TABLE_DATA_LIST.
let gIsV4Updated = false;   // For TEST_TABLE_DATA_V4.

const NEW_CLIENT_STATE = 'sta\0te';
const CHECKSUM = '\x30\x67\xc7\x2c\x5e\x50\x1c\x31\xe3\xfe\xca\x73\xf0\x47\xdc\x34\x1a\x95\x63\x99\xec\x70\x5e\x0a\xee\x9e\xfb\x17\xa1\x55\x35\x78';

prefBranch.setBoolPref("browser.safebrowsing.debug", true);

// The "\xFF\xFF" is to generate a base64 string with "/".
prefBranch.setCharPref("browser.safebrowsing.id", "Firefox\xFF\xFF");

// Register tables.
TEST_TABLE_DATA_LIST.forEach(function(t) {
  gListManager.registerTable(t.tableName,
                             t.providerName,
                             t.updateUrl,
                             t.gethashUrl);
});

gListManager.registerTable(TEST_TABLE_DATA_V4.tableName,
                           TEST_TABLE_DATA_V4.providerName,
                           TEST_TABLE_DATA_V4.updateUrl,
                           TEST_TABLE_DATA_V4.gethashUrl);

// To test Bug 1302044.
gListManager.registerTable(TEST_TABLE_DATA_V4_DISABLED.tableName,
                           TEST_TABLE_DATA_V4_DISABLED.providerName,
                           TEST_TABLE_DATA_V4_DISABLED.updateUrl,
                           TEST_TABLE_DATA_V4_DISABLED.gethashUrl);

const SERVER_INVOLVED_TEST_CASE_LIST = [
  // - Do table0 update.
  // - Server would respond "a:5:32:32\n[DATA]".
  function test_update_table0() {
    disableAllUpdates();

    gListManager.enableUpdate(TEST_TABLE_DATA_LIST[0].tableName);
    gExpectedUpdateRequest = TEST_TABLE_DATA_LIST[0].tableName + ";\n";

    gUpdateResponse = "n:1000\ni:" + TEST_TABLE_DATA_LIST[0].tableName + "\n";
    gUpdateResponse += readFileToString("data/digest2.chunk");

    forceTableUpdate();
  },

  // - Do table0 update again. Since chunk 5 was added to table0 in the last
  //   update, the expected request contains "a:5".
  // - Server would respond "s;2-12\n[DATA]".
  function test_update_table0_with_existing_chunks() {
    disableAllUpdates();

    gListManager.enableUpdate(TEST_TABLE_DATA_LIST[0].tableName);
    gExpectedUpdateRequest = TEST_TABLE_DATA_LIST[0].tableName + ";a:5\n";

    gUpdateResponse = "n:1000\ni:" + TEST_TABLE_DATA_LIST[0].tableName + "\n";
    gUpdateResponse += readFileToString("data/digest1.chunk");

    forceTableUpdate();
  },

  // - Do all-table update.
  // - Server would respond no chunk control.
  //
  // Note that this test MUST be the last one in the array since we rely on
  // the number of sever-involved test case to synchronize the racing last
  // two udpates for different URL.
  function test_update_all_tables() {
    disableAllUpdates();

    // Enable all tables including TEST_TABLE_DATA_V4!
    TEST_TABLE_DATA_LIST.forEach(function(t) {
      gListManager.enableUpdate(t.tableName);
    });

    // We register two v4 tables but only enable one of them
    // to verify that the disabled tables are not updated.
    // See Bug 1302044.
    gListManager.enableUpdate(TEST_TABLE_DATA_V4.tableName);
    gListManager.disableUpdate(TEST_TABLE_DATA_V4_DISABLED.tableName);

    // Expected results for v2.
    gExpectedUpdateRequest = TEST_TABLE_DATA_LIST[0].tableName + ";a:5:s:2-12\n" +
                             TEST_TABLE_DATA_LIST[1].tableName + ";\n" +
                             TEST_TABLE_DATA_LIST[2].tableName + ";\n";
    gUpdateResponse = "n:1000\n";

    // We test the request against the query string since v4 request
    // would be appened to the query string. The request is generated
    // by protobuf API (binary) then encoded to base64 format.
    let requestV4 = gUrlUtils.makeUpdateRequestV4([TEST_TABLE_DATA_V4.tableName],
                                                  [""],
                                                  1);
    gExpectedQueryV4 = "&$req=" + requestV4;

    forceTableUpdate();
  },

];

SERVER_INVOLVED_TEST_CASE_LIST.forEach(t => add_test(t));

add_test(function test_partialUpdateV4() {
  disableAllUpdates();

  gListManager.enableUpdate(TEST_TABLE_DATA_V4.tableName);

  // Since the new client state has been responded and saved in
  // test_update_all_tables, this update request should send
  // a partial update to the server.
  let requestV4 = gUrlUtils.makeUpdateRequestV4([TEST_TABLE_DATA_V4.tableName],
                                                [btoa(NEW_CLIENT_STATE)],
                                                1);
  gExpectedQueryV4 = "&$req=" + requestV4;

  forceTableUpdate();
});

// Tests nsIUrlListManager.getGethashUrl.
add_test(function test_getGethashUrl() {
  TEST_TABLE_DATA_LIST.forEach(function (t) {
    equal(gListManager.getGethashUrl(t.tableName), t.gethashUrl);
  });
  equal(gListManager.getGethashUrl(TEST_TABLE_DATA_V4.tableName),
        TEST_TABLE_DATA_V4.gethashUrl);
  run_next_test();
});

function run_test() {
  // Setup primary testing server.
  gHttpServ = new HttpServer();
  gHttpServ.registerDirectory("/", do_get_cwd());

  gHttpServ.registerPathHandler("/safebrowsing/update", function(request, response) {
    let body = NetUtil.readInputStreamToString(request.bodyInputStream,
                                               request.bodyInputStream.available());

    // Verify if the request is as expected.
    equal(body, gExpectedUpdateRequest);

    // Respond the update which is controlled by the test case.
    response.setHeader("Content-Type",
                       "application/vnd.google.safebrowsing-update", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(gUpdateResponse, gUpdateResponse.length);

    gUpdatedCntForTableData++;

    if (gUpdatedCntForTableData !== SERVER_INVOLVED_TEST_CASE_LIST.length) {
      // This is not the last test case so run the next once upon the
      // the update success.
      waitForUpdateSuccess(run_next_test);
      return;
    }

    if (gIsV4Updated) {
      run_next_test();  // All tests are done. Just finish.
      return;
    }

    do_print("Waiting for TEST_TABLE_DATA_V4 to be tested ...");
  });

  gHttpServ.start(4444);

  // Setup v4 testing server for the different update URL.
  gHttpServV4 = new HttpServer();
  gHttpServV4.registerDirectory("/", do_get_cwd());

  gHttpServV4.registerPathHandler("/safebrowsing/update", function(request, response) {
    // V4 update request body should be empty.
    equal(request.bodyInputStream.available(), 0);

    // Not on the spec. Found in Chromium source code...
    equal(request.getHeader("X-HTTP-Method-Override"), "POST");

    // V4 update request uses GET.
    equal(request.method, "GET");

    // V4 append the base64 encoded request to the query string.
    equal(request.queryString, gExpectedQueryV4);
    equal(request.queryString.indexOf('+'), -1);
    equal(request.queryString.indexOf('/'), -1);

    // Respond a V2 compatible content for now. In the future we can
    // send a meaningful response to test Bug 1284178 to see if the
    // update is successfully stored to database.
    response.setHeader("Content-Type",
                       "application/vnd.google.safebrowsing-update", false);
    response.setStatusLine(request.httpVersion, 200, "OK");

    // The protobuf binary represention of response:
    //
    // [
    //   {
    //     'threat_type': 2, // SOCIAL_ENGINEERING_PUBLIC
    //     'response_type': 2, // FULL_UPDATE
    //     'new_client_state': 'sta\x00te', // NEW_CLIENT_STATE
    //     'checksum': { "sha256": CHECKSUM }, // CHECKSUM
    //     'additions': { 'compression_type': RAW,
    //                    'prefix_size': 4,
    //                    'raw_hashes': "00000001000000020000000300000004"}
    //   }
    // ]
    //
    let content = "\x0A\x4A\x08\x02\x20\x02\x2A\x18\x08\x01\x12\x14\x08\x04\x12\x10\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x3A\x06\x73\x74\x61\x00\x74\x65\x42\x22\x0A\x20\x30\x67\xC7\x2C\x5E\x50\x1C\x31\xE3\xFE\xCA\x73\xF0\x47\xDC\x34\x1A\x95\x63\x99\xEC\x70\x5E\x0A\xEE\x9E\xFB\x17\xA1\x55\x35\x78\x12\x08\x08\x08\x10\x80\x94\xEB\xDC\x03";

    response.bodyOutputStream.write(content, content.length);

    if (gIsV4Updated) {
      // This falls to the case where test_partialUpdateV4 is running.
      // We are supposed to have verified the update request contains
      // the state we set in the previous request.
      run_next_test();
      return;
    }

    waitUntilMetaDataSaved(NEW_CLIENT_STATE, CHECKSUM, () => {
      gIsV4Updated = true;

      if (gUpdatedCntForTableData === SERVER_INVOLVED_TEST_CASE_LIST.length) {
        // All tests are done!
        run_next_test();
        return;
      }

      do_print("Wait for all sever-involved tests to be done ...");
    });

  });

  gHttpServV4.start(5555);

  run_next_test();
}

// A trick to force updating tables. However, before calling this, we have to
// call disableAllUpdates() first to clean up the updateCheckers in listmanager.
function forceTableUpdate() {
  prefBranch.setCharPref(PREF_NEXTUPDATETIME, "1");
  prefBranch.setCharPref(PREF_NEXTUPDATETIME_V4, "1");
  gListManager.maybeToggleUpdateChecking();
}

function disableAllUpdates() {
  TEST_TABLE_DATA_LIST.forEach(t => gListManager.disableUpdate(t.tableName));
  gListManager.disableUpdate(TEST_TABLE_DATA_V4.tableName);
}

// Since there's no public interface on listmanager to know the update success,
// we could only rely on the refresh of "nextupdatetime".
function waitForUpdateSuccess(callback) {
  let nextupdatetime = parseInt(prefBranch.getCharPref(PREF_NEXTUPDATETIME));
  do_print("nextupdatetime: " + nextupdatetime);
  if (nextupdatetime !== 1) {
    callback();
    return;
  }
  do_timeout(1000, waitForUpdateSuccess.bind(null, callback));
}

// Construct an update from a file.
function readFileToString(aFilename) {
  let f = do_get_file(aFilename);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"]
    .createInstance(Ci.nsIFileInputStream);
  stream.init(f, -1, 0, 0);
  let buf = NetUtil.readInputStreamToString(stream, stream.available());
  return buf;
}

function waitUntilMetaDataSaved(expectedState, expectedChecksum, callback) {
  let dbService = Cc["@mozilla.org/url-classifier/dbservice;1"]
                     .getService(Ci.nsIUrlClassifierDBService);

  dbService.getTables(metaData => {
    do_print("metadata: " + metaData);
    let didCallback = false;
    metaData.split("\n").some(line => {
      // Parse [tableName];[stateBase64]
      let p = line.indexOf(";");
      if (-1 === p) {
        return false; // continue.
      }
      let tableName = line.substring(0, p);
      let metadata = line.substring(p + 1).split(":");
      let stateBase64 = metadata[0];
      let checksumBase64 = metadata[1];

      if (tableName !== 'test-phish-proto') {
        return false; // continue.
      }

      if (stateBase64 === btoa(expectedState) &&
          checksumBase64 === btoa(expectedChecksum)) {
        do_print('State has been saved to disk!');

        // We slightly defer the callback to see if the in-memory
        // |getTables| caching works correctly.
        dbService.getTables(cachedMetadata => {
          equal(cachedMetadata, metaData);
          callback();
        });

        // Even though we haven't done callback at this moment
        // but we still claim "we have" in order to stop repeating
        // a new timer.
        didCallback = true;
      }

      return true; // break no matter whether the state is matching.
    });

    if (!didCallback) {
      do_timeout(1000, waitUntilMetaDataSaved.bind(null, expectedState,
                                                         expectedChecksum,
                                                         callback));
    }
  });
}
