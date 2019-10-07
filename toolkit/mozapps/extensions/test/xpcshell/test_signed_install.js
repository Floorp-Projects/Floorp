// Enable signature checks for these tests
gUseRealCertChecks = true;

const DATA = "data/signing_checks/";
const ADDONS = {
  unsigned: "unsigned.xpi",
  signed1: "signed1.xpi",
  signed2: "signed2.xpi",
  privileged: "privileged.xpi",

  // Bug 1509093
  // sha256Signed: "signed_bootstrap_sha256_1.xpi",
};

// The ID in signed1.xpi and signed2.xpi
const ID = "test@somewhere.com";
const PR_USEC_PER_MSEC = 1000;

let testserver = createHttpServer({ hosts: ["example.com"] });

Services.prefs.setCharPref(
  "extensions.update.background.url",
  "http://example.com/update.json"
);
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

// Creates an add-on with a broken signature by changing an existing file
function createBrokenAddonModify(file) {
  let brokenFile = gTmpD.clone();
  brokenFile.append("broken.xpi");
  file.copyTo(brokenFile.parent, brokenFile.leafName);

  var stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.setData("FOOBAR", -1);
  var zipW = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  zipW.open(brokenFile, FileUtils.MODE_RDWR | FileUtils.MODE_APPEND);
  zipW.removeEntry("test.txt", false);
  zipW.addEntryStream(
    "test.txt",
    new Date() * PR_USEC_PER_MSEC,
    Ci.nsIZipWriter.COMPRESSION_NONE,
    stream,
    false
  );
  zipW.close();

  return brokenFile;
}

// Creates an add-on with a broken signature by adding a new file
function createBrokenAddonAdd(file) {
  let brokenFile = gTmpD.clone();
  brokenFile.append("broken.xpi");
  file.copyTo(brokenFile.parent, brokenFile.leafName);

  var stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.setData("FOOBAR", -1);
  var zipW = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  zipW.open(brokenFile, FileUtils.MODE_RDWR | FileUtils.MODE_APPEND);
  zipW.addEntryStream(
    "test2.txt",
    new Date() * PR_USEC_PER_MSEC,
    Ci.nsIZipWriter.COMPRESSION_NONE,
    stream,
    false
  );
  zipW.close();

  return brokenFile;
}

// Creates an add-on with a broken signature by removing an existing file
function createBrokenAddonRemove(file) {
  let brokenFile = gTmpD.clone();
  brokenFile.append("broken.xpi");
  file.copyTo(brokenFile.parent, brokenFile.leafName);

  var stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.setData("FOOBAR", -1);
  var zipW = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  zipW.open(brokenFile, FileUtils.MODE_RDWR | FileUtils.MODE_APPEND);
  zipW.removeEntry("test.txt", false);
  zipW.close();

  return brokenFile;
}

function serveUpdate(filename) {
  const RESPONSE = {
    addons: {
      [ID]: {
        updates: [
          {
            version: "2.0",
            update_link: `http://example.com/${filename}`,
            applications: {
              gecko: {
                strict_min_version: "4",
                advisory_max_version: "6",
              },
            },
          },
        ],
      },
    },
  };
  AddonTestUtils.registerJSON(testserver, "/update.json", RESPONSE);
}

async function test_install_broken(file, expectedError) {
  let install = await AddonManager.getInstallForFile(file);
  await Assert.rejects(
    install.install(),
    /Install failed/,
    "Install of an improperly signed extension should throw"
  );

  Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  Assert.equal(install.error, expectedError);
  Assert.equal(install.addon, null);
}

async function test_install_working(file, expectedSignedState) {
  let install = await AddonManager.getInstallForFile(file);
  await install.install();

  Assert.equal(install.state, AddonManager.STATE_INSTALLED);
  Assert.notEqual(install.addon, null);
  Assert.equal(install.addon.signedState, expectedSignedState);

  await install.addon.uninstall();
}

async function test_update_broken(file1, file2, expectedError) {
  // First install the older version
  await Promise.all([
    promiseInstallFile(file1),
    promiseWebExtensionStartup(ID),
  ]);

  testserver.registerFile("/" + file2.leafName, file2);
  serveUpdate(file2.leafName);

  let addon = await promiseAddonByID(ID);
  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;
  await Assert.rejects(
    install.install(),
    /Install failed/,
    "Update to an improperly signed extension should throw"
  );

  Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
  Assert.equal(install.error, expectedError);
  Assert.equal(install.addon, null);

  testserver.registerFile("/" + file2.leafName, null);
  testserver.registerPathHandler("/update.json", null);

  await addon.uninstall();
}

async function test_update_working(file1, file2, expectedSignedState) {
  // First install the older version
  await promiseInstallFile(file1);

  testserver.registerFile("/" + file2.leafName, file2);
  serveUpdate(file2.leafName);

  let addon = await promiseAddonByID(ID);
  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;
  await Promise.all([install.install(), promiseWebExtensionStartup(ID)]);

  Assert.equal(install.state, AddonManager.STATE_INSTALLED);
  Assert.notEqual(install.addon, null);
  Assert.equal(install.addon.signedState, expectedSignedState);

  testserver.registerFile("/" + file2.leafName, null);
  testserver.registerPathHandler("/update.json", null);

  await install.addon.uninstall();
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "4");
  await promiseStartupManager();
});

// Try to install a broken add-on
add_task(async function test_install_invalid_modified() {
  let file = createBrokenAddonModify(do_get_file(DATA + ADDONS.signed1));
  await test_install_broken(file, AddonManager.ERROR_CORRUPT_FILE);
  file.remove(true);
});

add_task(async function test_install_invalid_added() {
  let file = createBrokenAddonAdd(do_get_file(DATA + ADDONS.signed1));
  await test_install_broken(file, AddonManager.ERROR_CORRUPT_FILE);
  file.remove(true);
});

add_task(async function test_install_invalid_removed() {
  let file = createBrokenAddonRemove(do_get_file(DATA + ADDONS.signed1));
  await test_install_broken(file, AddonManager.ERROR_CORRUPT_FILE);
  file.remove(true);
});

// Try to install an unsigned add-on
add_task(async function test_install_invalid_unsigned() {
  let file = do_get_file(DATA + ADDONS.unsigned);
  await test_install_broken(file, AddonManager.ERROR_SIGNEDSTATE_REQUIRED);
});

// Try to install a signed add-on
add_task(async function test_install_valid() {
  let file = do_get_file(DATA + ADDONS.signed1);
  await test_install_working(file, AddonManager.SIGNEDSTATE_SIGNED);
});

// Try to install an add-on signed with SHA-256
add_task(async function test_install_valid_sha256() {
  // Bug 1509093
  // let file = do_get_file(DATA + ADDONS.sha256Signed);
  // await test_install_working(file, AddonManager.SIGNEDSTATE_SIGNED);
});

// Try to install an add-on with the "Mozilla Extensions" OU
add_task(async function test_install_valid_privileged() {
  let file = do_get_file(DATA + ADDONS.privileged);
  await test_install_working(file, AddonManager.SIGNEDSTATE_PRIVILEGED);
});

// Try to update to a broken add-on
add_task(async function test_update_invalid_modified() {
  let file1 = do_get_file(DATA + ADDONS.signed1);
  let file2 = createBrokenAddonModify(do_get_file(DATA + ADDONS.signed2));
  await test_update_broken(file1, file2, AddonManager.ERROR_CORRUPT_FILE);
  file2.remove(true);
});

add_task(async function test_update_invalid_added() {
  let file1 = do_get_file(DATA + ADDONS.signed1);
  let file2 = createBrokenAddonAdd(do_get_file(DATA + ADDONS.signed2));
  await test_update_broken(file1, file2, AddonManager.ERROR_CORRUPT_FILE);
  file2.remove(true);
});

add_task(async function test_update_invalid_removed() {
  let file1 = do_get_file(DATA + ADDONS.signed1);
  let file2 = createBrokenAddonRemove(do_get_file(DATA + ADDONS.signed2));
  await test_update_broken(file1, file2, AddonManager.ERROR_CORRUPT_FILE);
  file2.remove(true);
});

// Try to update to an unsigned add-on
add_task(async function test_update_invalid_unsigned() {
  let file1 = do_get_file(DATA + ADDONS.signed1);
  let file2 = do_get_file(DATA + ADDONS.unsigned);
  await test_update_broken(
    file1,
    file2,
    AddonManager.ERROR_SIGNEDSTATE_REQUIRED
  );
});

// Try to update to a signed add-on
add_task(async function test_update_valid() {
  let file1 = do_get_file(DATA + ADDONS.signed1);
  let file2 = do_get_file(DATA + ADDONS.signed2);
  await test_update_working(file1, file2, AddonManager.SIGNEDSTATE_SIGNED);
});

add_task(() => promiseShutdownManager());
