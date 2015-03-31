// Enable signature checks for these tests
Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);
// Disable update security
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const DATA = "data/signing_checks/";
const ADDONS = {
  bootstrap: {
    unsigned: "unsigned_bootstrap_2.xpi",
    badid: "signed_bootstrap_badid_2.xpi",
    preliminary: "preliminary_bootstrap_2.xpi",
    signed: "signed_bootstrap_2.xpi",
  },
};
const WORKING = "signed_bootstrap_1.xpi";
const ID = "test@tests.mozilla.org";

Components.utils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start(4444);

// Creates an add-on with a broken signature by changing an existing file
function createBrokenAddonModify(file) {
  let brokenFile = gTmpD.clone();
  brokenFile.append("broken.xpi");
  file.copyTo(brokenFile.parent, brokenFile.leafName);

  var stream = AM_Cc["@mozilla.org/io/string-input-stream;1"].
               createInstance(AM_Ci.nsIStringInputStream);
  stream.setData("FOOBAR", -1);
  var zipW = AM_Cc["@mozilla.org/zipwriter;1"].
             createInstance(AM_Ci.nsIZipWriter);
  zipW.open(brokenFile, FileUtils.MODE_RDWR | FileUtils.MODE_APPEND);
  zipW.removeEntry("test.txt", false);
  zipW.addEntryStream("test.txt", 0, AM_Ci.nsIZipWriter.COMPRESSION_NONE,
                      stream, false);
  zipW.close();

  return brokenFile;
}

// Creates an add-on with a broken signature by adding a new file
function createBrokenAddonAdd(file) {
  let brokenFile = gTmpD.clone();
  brokenFile.append("broken.xpi");
  file.copyTo(brokenFile.parent, brokenFile.leafName);

  var stream = AM_Cc["@mozilla.org/io/string-input-stream;1"].
               createInstance(AM_Ci.nsIStringInputStream);
  stream.setData("FOOBAR", -1);
  var zipW = AM_Cc["@mozilla.org/zipwriter;1"].
             createInstance(AM_Ci.nsIZipWriter);
  zipW.open(brokenFile, FileUtils.MODE_RDWR | FileUtils.MODE_APPEND);
  zipW.addEntryStream("test2.txt", 0, AM_Ci.nsIZipWriter.COMPRESSION_NONE,
                      stream, false);
  zipW.close();

  return brokenFile;
}

// Creates an add-on with a broken signature by removing an existing file
function createBrokenAddonRemove(file) {
  let brokenFile = gTmpD.clone();
  brokenFile.append("broken.xpi");
  file.copyTo(brokenFile.parent, brokenFile.leafName);

  var stream = AM_Cc["@mozilla.org/io/string-input-stream;1"].
               createInstance(AM_Ci.nsIStringInputStream);
  stream.setData("FOOBAR", -1);
  var zipW = AM_Cc["@mozilla.org/zipwriter;1"].
             createInstance(AM_Ci.nsIZipWriter);
  zipW.open(brokenFile, FileUtils.MODE_RDWR | FileUtils.MODE_APPEND);
  zipW.removeEntry("test.txt", false);
  zipW.close();

  return brokenFile;
}

function createInstall(url) {
  return new Promise(resolve => {
    AddonManager.getInstallForURL(url, resolve, "application/x-xpinstall");
  });
}

function serveUpdateRDF(leafName) {
  gServer.registerPathHandler("/update.rdf", function(request, response) {
    let updateData = {};
    updateData[ID] = [{
      version: "2.0",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "4",
        maxVersion: "6",
        updateLink: "http://localhost:4444/" + leafName
      }]
    }];

    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(createUpdateRDF(updateData));
  });
}


function* test_install_broken(file, expectedError) {
  gServer.registerFile("/" + file.leafName, file);

  let install = yield createInstall("http://localhost:4444/" + file.leafName);
  yield promiseCompleteAllInstalls([install]);

  do_check_eq(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  do_check_eq(install.error, expectedError);
  do_check_eq(install.addon, null);

  gServer.registerFile("/" + file.leafName, null);
}

function* test_install_working(file, expectedSignedState) {
  gServer.registerFile("/" + file.leafName, file);

  let install = yield createInstall("http://localhost:4444/" + file.leafName);
  yield promiseCompleteAllInstalls([install]);

  do_check_eq(install.state, AddonManager.STATE_INSTALLED);
  do_check_neq(install.addon, null);
  do_check_eq(install.addon.signedState, expectedSignedState);

  gServer.registerFile("/" + file.leafName, null);

  install.addon.uninstall();
}

function* test_update_broken(file, expectedError) {
  // First install the older version
  yield promiseInstallAllFiles([do_get_file(DATA + WORKING)]);

  gServer.registerFile("/" + file.leafName, file);
  serveUpdateRDF(file.leafName);

  let addon = yield promiseAddonByID(ID);
  let install = yield promiseFindAddonUpdates(addon);
  yield promiseCompleteAllInstalls([install]);

  do_check_eq(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  do_check_eq(install.error, expectedError);
  do_check_eq(install.addon, null);

  gServer.registerFile("/" + file.leafName, null);
  gServer.registerPathHandler("/update.rdf", null);

  addon.uninstall();
}

function* test_update_working(file, expectedSignedState) {
  // First install the older version
  yield promiseInstallAllFiles([do_get_file(DATA + WORKING)]);

  gServer.registerFile("/" + file.leafName, file);
  serveUpdateRDF(file.leafName);

  let addon = yield promiseAddonByID(ID);
  let install = yield promiseFindAddonUpdates(addon);
  yield promiseCompleteAllInstalls([install]);

  do_check_eq(install.state, AddonManager.STATE_INSTALLED);
  do_check_neq(install.addon, null);
  do_check_eq(install.addon.signedState, expectedSignedState);

  gServer.registerFile("/" + file.leafName, null);
  gServer.registerPathHandler("/update.rdf", null);

  install.addon.uninstall();
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");
  startupManager();

  run_next_test();
}

// Try to install a broken add-on
add_task(function*() {
  let file = createBrokenAddonModify(do_get_file(DATA + ADDONS.bootstrap.signed));
  yield test_install_broken(file, AddonManager.ERROR_CORRUPT_FILE);
  file.remove(true);
});

add_task(function*() {
  let file = createBrokenAddonAdd(do_get_file(DATA + ADDONS.bootstrap.signed));
  yield test_install_broken(file, AddonManager.ERROR_CORRUPT_FILE);
  file.remove(true);
});

add_task(function*() {
  let file = createBrokenAddonRemove(do_get_file(DATA + ADDONS.bootstrap.signed));
  yield test_install_broken(file, AddonManager.ERROR_CORRUPT_FILE);
  file.remove(true);
});

// Try to install an add-on with an incorrect ID
add_task(function*() {
  let file = do_get_file(DATA + ADDONS.bootstrap.badid);
  yield test_install_broken(file, AddonManager.ERROR_CORRUPT_FILE);
});

// Try to install an unsigned add-on
add_task(function*() {
  let file = do_get_file(DATA + ADDONS.bootstrap.unsigned);
  yield test_install_broken(file, AddonManager.ERROR_SIGNEDSTATE_REQUIRED);
});

// Try to install a preliminarily reviewed add-on
add_task(function*() {
  let file = do_get_file(DATA + ADDONS.bootstrap.preliminary);
  yield test_install_working(file, AddonManager.SIGNEDSTATE_PRELIMINARY);
});

// Try to install a signed add-on
add_task(function*() {
  let file = do_get_file(DATA + ADDONS.bootstrap.signed);
  yield test_install_working(file, AddonManager.SIGNEDSTATE_SIGNED);
});

// Try to update to a broken add-on
add_task(function*() {
  let file = createBrokenAddonModify(do_get_file(DATA + ADDONS.bootstrap.signed));
  yield test_update_broken(file, AddonManager.ERROR_CORRUPT_FILE);
  file.remove(true);
});

add_task(function*() {
  let file = createBrokenAddonAdd(do_get_file(DATA + ADDONS.bootstrap.signed));
  yield test_update_broken(file, AddonManager.ERROR_CORRUPT_FILE);
  file.remove(true);
});

add_task(function*() {
  let file = createBrokenAddonRemove(do_get_file(DATA + ADDONS.bootstrap.signed));
  yield test_update_broken(file, AddonManager.ERROR_CORRUPT_FILE);
  file.remove(true);
});

// Try to update to an add-on with an incorrect ID
add_task(function*() {
  let file = do_get_file(DATA + ADDONS.bootstrap.badid);
  yield test_update_broken(file, AddonManager.ERROR_CORRUPT_FILE);
});

// Try to update to an unsigned add-on
add_task(function*() {
  let file = do_get_file(DATA + ADDONS.bootstrap.unsigned);
  yield test_update_broken(file, AddonManager.ERROR_SIGNEDSTATE_REQUIRED);
});

// Try to update to a preliminarily reviewed add-on
add_task(function*() {
  let file = do_get_file(DATA + ADDONS.bootstrap.preliminary);
  yield test_update_working(file, AddonManager.SIGNEDSTATE_PRELIMINARY);
});

// Try to update to a signed add-on
add_task(function*() {
  let file = do_get_file(DATA + ADDONS.bootstrap.signed);
  yield test_update_working(file, AddonManager.SIGNEDSTATE_SIGNED);
});
