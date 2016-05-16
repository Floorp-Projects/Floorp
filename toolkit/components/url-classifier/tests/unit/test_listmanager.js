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

// This table has a different update URL.
const TEST_TABLE_DATA_ANOTHER = {
  tableName: "test-listmanageranother-digest256",
  providerName: "google",
  updateUrl: "http://localhost:5555/safebrowsing/update",
  gethashUrl: "http://localhost:5555/safebrowsing/gethash-another",
};

const PREF_NEXTUPDATETIME = "browser.safebrowsing.provider.google.nextupdatetime";

let gListManager = Cc["@mozilla.org/url-classifier/listmanager;1"]
                     .getService(Ci.nsIUrlListManager);

// Global test server for serving safebrowsing updates.
let gHttpServ = null;
let gUpdateResponse = "";
let gExpectedUpdateRequest = "";

// Handles request for TEST_TABLE_DATA_ANOTHER.
let gHttpServAnother = null;

// These two variables are used to synchronize the last two racing updates
// (in terms of "update URL") in test_update_all_tables().
let gUpdatedCntForTableData = 0; // For TEST_TABLE_DATA_LIST.
let gIsAnotherUpdated = false;   // For TEST_TABLE_DATA_ANOTHER.

prefBranch.setBoolPref("browser.safebrowsing.debug", true);

// Register tables.
TEST_TABLE_DATA_LIST.forEach(function(t) {
  gListManager.registerTable(t.tableName,
                             t.providerName,
                             t.updateUrl,
                             t.gethashUrl);
});
gListManager.registerTable(TEST_TABLE_DATA_ANOTHER.tableName,
                           TEST_TABLE_DATA_ANOTHER.providerName,
                           TEST_TABLE_DATA_ANOTHER.updateUrl,
                           TEST_TABLE_DATA_ANOTHER.gethashUrl);

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

    // Enable all tables including TEST_TABLE_DATA_ANOTHER!
    TEST_TABLE_DATA_LIST.forEach(function(t) {
      gListManager.enableUpdate(t.tableName);
    });
    gListManager.enableUpdate(TEST_TABLE_DATA_ANOTHER.tableName);

    gExpectedUpdateRequest = TEST_TABLE_DATA_LIST[0].tableName + ";a:5:s:2-12\n" +
                             TEST_TABLE_DATA_LIST[1].tableName + ";\n" +
                             TEST_TABLE_DATA_LIST[2].tableName + ";\n";
    gUpdateResponse = "n:1000\n";

    forceTableUpdate();
  },

];

SERVER_INVOLVED_TEST_CASE_LIST.forEach(t => add_test(t));

// Tests nsIUrlListManager.getGethashUrl.
add_test(function test_getGethashUrl() {
  TEST_TABLE_DATA_LIST.forEach(function (t) {
    equal(gListManager.getGethashUrl(t.tableName), t.gethashUrl);
  });
  equal(gListManager.getGethashUrl(TEST_TABLE_DATA_ANOTHER.tableName),
        TEST_TABLE_DATA_ANOTHER.gethashUrl);
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

    if (gIsAnotherUpdated) {
      run_next_test();  // All tests are done. Just finish.
      return;
    }

    do_print("Waiting for TEST_TABLE_DATA_ANOTHER to be tested ...");
  });

  gHttpServ.start(4444);

  // Setup another testing server for the different update URL.
  gHttpServAnother = new HttpServer();
  gHttpServAnother.registerDirectory("/", do_get_cwd());

  gHttpServAnother.registerPathHandler("/safebrowsing/update", function(request, response) {
    let body = NetUtil.readInputStreamToString(request.bodyInputStream,
                                               request.bodyInputStream.available());

    // Verify if the request is as expected.
    equal(body, TEST_TABLE_DATA_ANOTHER.tableName + ";\n");

    // Respond with no chunk control.
    response.setHeader("Content-Type",
                       "application/vnd.google.safebrowsing-update", false);
    response.setStatusLine(request.httpVersion, 200, "OK");

    let content = "n:1000\n";
    response.bodyOutputStream.write(content, content.length);

    gIsAnotherUpdated = true;

    if (gUpdatedCntForTableData === SERVER_INVOLVED_TEST_CASE_LIST.length) {
      // All tests are done!
      run_next_test();
      return;
    }

    do_print("Wait for all sever-involved tests to be done ...");
  });

  gHttpServAnother.start(5555);

  run_next_test();
}

// A trick to force updating tables. However, before calling this, we have to
// call disableAllUpdates() first to clean up the updateCheckers in listmanager.
function forceTableUpdate() {
  prefBranch.setCharPref(PREF_NEXTUPDATETIME, "1");
  gListManager.maybeToggleUpdateChecking();
}

function disableAllUpdates() {
  TEST_TABLE_DATA_LIST.forEach(t => gListManager.disableUpdate(t.tableName));
  gListManager.disableUpdate(TEST_TABLE_DATA_ANOTHER.tableName);
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
