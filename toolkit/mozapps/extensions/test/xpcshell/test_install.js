/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://testing-common/httpd.js");

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
var gInstallDate;

// This verifies that add-ons can be installed from XPI files
// install.rdf size, icon.png, icon64.png size
const ADDON1_SIZE = 612;

const ADDONS = {
  test_install1: {
    "install.rdf": {
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      name: "Test 1",
      description: "Test Description",
      bootstrap: true,

      targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1"}],
    },
    "icon.png": "Fake icon image",
    "icon64.png": "Fake icon image",
  },
  test_install2_1: {
    "install.rdf": {
      id: "addon2@tests.mozilla.org",
      version: "2.0",
      name: "Real Test 2",
      description: "Test Description",
      bootstrap: true,

      targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1"}],

    },
    "icon.png": "Fake icon image",
  },
  test_install2_2: {
    "install.rdf": {
      id: "addon2@tests.mozilla.org",
      version: "3.0",
      name: "Real Test 3",
      description: "Test Description",
      bootstrap: true,

      targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1"}],
    },
  },
  test_install3: {
    "install.rdf": {
      id: "addon3@tests.mozilla.org",
      version: "1.0",
      name: "Real Test 4",
      description: "Test Description",
      bootstrap: true,

      updateURL: "http://example.com/data/test_install.rdf",

      targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
          minVersion: "0",
          maxVersion: "0"}],
    },
  },
  test_install6: {
    "install.rdf": {
      id: "addon6@tests.mozilla.org",
      version: "1.0",
      name: "Addon Test 6",
      description: "Test Description",
      bootstrap: true,

      targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1"}],
    },
  },
  test_install7: {
    "install.rdf": {
      type: "32",
    },
  },
};

const XPIS = {};

for (let [name, files] of Object.entries(ADDONS)) {
  XPIS[name] = AddonTestUtils.createTempXPIFile(files);
  testserver.registerFile(`/addons/${name}.xpi`, XPIS[name]);
}

const ZipReader = Components.Constructor(
  "@mozilla.org/libjar/zip-reader;1", "nsIZipReader",
  "open");

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function checkInstall(install, expected) {
  for (let [key, value] of Object.entries(expected)) {
    if (value instanceof Ci.nsIURI) {
      equal(install[key] && install[key].spec, value.spec, `Expected value of install.${key}`);
    } else {
      deepEqual(install[key], value, `Expected value of install.${key}`);
    }
  }
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  await promiseStartupManager();

  // Make sure we only register once despite multiple calls
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);

  // Create and configure the HTTP server.
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerPathHandler("/redirect", function(aRequest, aResponse) {
    aResponse.setStatusLine(null, 301, "Moved Permanently");
    let url = aRequest.host + ":" + aRequest.port + aRequest.queryString;
    aResponse.setHeader("Location", "http://" + url);
  });
  gPort = testserver.identity.primaryPort;
});

// Checks that an install from a local file proceeds as expected
add_task(async function test_1() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let install = await AddonManager.getInstallForFile(XPIS.test_install1);
  ensure_test_completed();

  let uri = Services.io.newFileURI(XPIS.test_install1);
  checkInstall(install, {
    type: "extension",
    version: "1.0",
    name: "Test 1",
    state: AddonManager.STATE_DOWNLOADED,
    iconURL: null,
    sourceURI: uri,
  });

  let {addon} = install;
  checkAddon("addon1@tests.mozilla.org", addon, {
    install,
    size: ADDON1_SIZE,
    iconURL: `jar:${uri.spec}!/icon.png`,
    icon64URL: `jar:${uri.spec}!/icon64.png`,
    sourceURI: uri,
  });
  notEqual(addon.syncGUID, null);
  ok(addon.hasResource("install.rdf"));
  equal(addon.getResourceURI("install.rdf").spec, `jar:${uri.spec}!/install.rdf`);

  let activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  let fooInstalls = await AddonManager.getInstallsByTypes(["foo"]);
  equal(fooInstalls.length, 0);

  let extensionInstalls = await AddonManager.getInstallsByTypes(["extension"]);
  equal(extensionInstalls.length, 1);
  equal(extensionInstalls[0], install);

  let installSyncGUID = await new Promise(resolve => {
    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled",
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], () => {
      resolve(install.addon.syncGUID);
    });
    install.install();
  });

  ensure_test_completed();

  addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  ok(addon);

  let pendingAddons = await AddonManager.getAddonsWithOperationsByTypes(null);
  equal(pendingAddons.length, 0);

  uri = NetUtil.newURI(addon.iconURL);
  if (uri instanceof Ci.nsIJARURI) {
    let {file} = uri.JARFile.QueryInterface(Ci.nsIFileURL);
    let zipReader = new ZipReader(file);
    try {
      ok(zipReader.hasEntry(uri.JAREntry));
    } finally {
      zipReader.close();
    }
  } else {
    let {file} = uri.QueryInterface(Ci.nsIFileURL);
    ok(file.exists());
  }

  let updateDate = Date.now();

  ok(!hasFlag(addon.permissions, AddonManager.PERM_CAN_ENABLE));
  ok(hasFlag(addon.permissions, AddonManager.PERM_CAN_DISABLE));

  await promiseRestartManager();

  activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls, 0);

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  let uri2 = do_get_addon_root_uri(profileDir, "addon1@tests.mozilla.org");

  checkAddon("addon1@tests.mozilla.org", a1, {
    syncGUID: installSyncGUID,
    type: "extension",
    version: "1.0",
    name: "Test 1",
    foreignInstall: false,
    size: ADDON1_SIZE,
    iconURL: uri2 + "icon.png",
    icon64URL: uri2 + "icon64.png",
    sourceURI: Services.io.newFileURI(XPIS.test_install1),
  });

  notEqual(a1.syncGUID, null);
  ok(a1.syncGUID.length >= 9);

  ok(isExtensionInBootstrappedList(profileDir, a1.id));
  ok(XPIS.test_install1.exists());
  do_check_in_crash_annotation(a1.id, a1.version);

  let difference = a1.installDate.getTime() - updateDate;
  if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
    do_throw("Add-on install time was out by " + difference + "ms");

  difference = a1.updateDate.getTime() - updateDate;
  if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
    do_throw("Add-on update time was out by " + difference + "ms");

  ok(a1.hasResource("install.rdf"));
  ok(!a1.hasResource("foo.bar"));

  equal(a1.getResourceURI("install.rdf").spec, uri2 + "install.rdf");

  // Ensure that extension bundle (or icon if unpacked) has updated
  // lastModifiedDate.
  let testURI = a1.getResourceURI(TEST_UNPACKED ? "icon.png" : "");
  let testFile = testURI.QueryInterface(Ci.nsIFileURL).file;
  ok(testFile.exists());
  difference = testFile.lastModifiedTime - Date.now();
  ok(Math.abs(difference) < MAX_TIME_DIFFERENCE);

  a1.uninstall();
  let { id, version } = a1;
  await promiseRestartManager();
  do_check_not_in_crash_annotation(id, version);
});

// Tests that an install from a url downloads.
add_task(async function test_2() {
  let url = "http://example.com/addons/test_install2_1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", null, "Test 2", null, "1.0");
  checkInstall(install, {
    version: "1.0",
    name: "Test 2",
    state: AddonManager.STATE_AVAILABLE,
    iconURL: null,
    sourceURI: Services.io.newURI(url),
  });

  let activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  install = await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], install1 => {
      resolve(install1);
      return false;
    });

    install.addListener({
      onDownloadProgress() {
        executeSoon(function() {
          Cu.forceGC();
        });
      }
    });

    install.install();
  });

  ensure_test_completed();
  checkInstall(install, {
    version: "2.0",
    name: "Real Test 2",
    state: AddonManager.STATE_DOWNLOADED,
    iconURL: null,
  });
  equal(install.addon.install, install);

  install = await new Promise(resolve => {
    prepare_test({
      "addon2@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);
    install.install();
  });

  let updateDate = Date.now();

  ensure_test_completed();

  await promiseRestartManager();

  let installs = await AddonManager.getAllInstalls();
  equal(installs, 0);

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2, {
    type: "extension",
    version: "2.0",
    name: "Real Test 2",
    sourceURI: Services.io.newURI(url),
  });
  notEqual(a2.syncGUID, null);

  ok(isExtensionInBootstrappedList(profileDir, a2.id));
  ok(XPIS.test_install2_1.exists());

  do_check_in_crash_annotation(a2.id, a2.version);

  let difference = a2.installDate.getTime() - updateDate;
  if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
    do_throw("Add-on install time was out by " + difference + "ms");

  difference = a2.updateDate.getTime() - updateDate;
  if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
    do_throw("Add-on update time was out by " + difference + "ms");

  gInstallDate = a2.installDate;
});

// Tests that installing a new version of an existing add-on works
add_task(async function test_4() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install2_2.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", null, "Test 3", null, "3.0");
  ensure_test_completed();

  checkInstall(install, {
    version: "3.0",
    name: "Test 3",
    state: AddonManager.STATE_AVAILABLE,
    existingAddon: null,
  });

  let activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  install = await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], install1 => {
      resolve(install1);
      return false;
    });
    install.install();
  });

  ensure_test_completed();

  checkInstall(install, {
    version: "3.0",
    name: "Real Test 3",
    state: AddonManager.STATE_DOWNLOADED,
    existingAddon: await AddonManager.getAddonByID("addon2@tests.mozilla.org"),
  });

  equal(install.addon.install, install);

  // Installation will continue when there is nothing returned.
  install = await new Promise(resolve => {
    prepare_test({
      "addon2@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled",
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);
    install.install();
  });

  ensure_test_completed();

  await promiseRestartManager();

  let installs2 = await AddonManager.getInstallsByTypes(null);
  equal(installs2.length, 0);

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2, {
    type: "extension",
    version: "3.0",
    name: "Real Test 3",
    isActive: true,
    foreignInstall: false,
    sourceURI: Services.io.newURI(url),
    installDate: gInstallDate,
  });

  ok(isExtensionInBootstrappedList(profileDir, a2.id));
  ok(XPIS.test_install2_2.exists());

  do_check_in_crash_annotation(a2.id, a2.version);

  // Update date should be later (or the same if this test is too fast)
  ok(a2.installDate <= a2.updateDate);

  a2.uninstall();
});

// Tests that an install that requires a compatibility update works
add_task(async function test_6() {
  await promiseRestartManager();

  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install3.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", null, "Real Test 4", null, "1.0");
  ensure_test_completed();

  checkInstall(install, {
    version: "1.0",
    name: "Real Test 4",
    state: AddonManager.STATE_AVAILABLE,
  });

  let activeInstalls = await AddonManager.getInstallsByTypes(null);
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  install = await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], install1 => {
      resolve(install1);
      return false;
    });
    install.install();
  });

  ensure_test_completed();
  checkInstall(install, {
    version: "1.0",
    name: "Real Test 4",
    state: AddonManager.STATE_DOWNLOADED,
    existingAddon: null,
  });
  checkAddon("addon3@tests.mozilla.org", install.addon, {
    appDisabled: false,
  });

  // Continue the install
  await new Promise(resolve => {
    prepare_test({
      "addon3@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled",
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);
    install.install();
  });

  ensure_test_completed();

  await promiseRestartManager();

  let installs = await AddonManager.getAllInstalls();
  equal(installs, 0);

  let a3 = await AddonManager.getAddonByID("addon3@tests.mozilla.org");
  checkAddon("addon3@tests.mozilla.org", a3, {
    type: "extension",
    version: "1.0",
    name: "Real Test 4",
    isActive: true,
    appDisabled: false,
  });
  notEqual(a3.syncGUID, null);

  ok(isExtensionInBootstrappedList(profileDir, a3.id));

  ok(XPIS.test_install3.exists());
  a3.uninstall();
});

add_task(async function test_8() {
  await promiseRestartManager();

  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);

  prepare_test({ }, [
    "onNewInstall"
  ]);

  let install = await AddonManager.getInstallForFile(XPIS.test_install3);
  ok(install.addon.isCompatible);

  await new Promise(resolve => {
    prepare_test({
      "addon3@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled",
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);
    install.install();
  });

  await promiseRestartManager();

  let a3 = await AddonManager.getAddonByID("addon3@tests.mozilla.org");
  checkAddon("addon3@tests.mozilla.org", a3, {
    type: "extension",
    version: "1.0",
    name: "Real Test 4",
    isActive: true,
    appDisabled: false,
  });
  notEqual(a3.syncGUID, null);

  ok(isExtensionInBootstrappedList(profileDir, a3.id));

  ok(XPIS.test_install3.exists());
  a3.uninstall();
});

// Test that after cancelling a download it is removed from the active installs
add_task(async function test_9() {
  await promiseRestartManager();

  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install3.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", null, "Real Test 4", null, "1.0");
  ensure_test_completed();

  checkInstall(install, {
    version: "1.0",
    name: "Real Test 4",
    state: AddonManager.STATE_AVAILABLE,
  });

  let activeInstalls = await AddonManager.getInstallsByTypes(null);
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  install = await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], () => {
      prepare_test({}, [
        "onDownloadCancelled"
      ], resolve);

      install.cancel();
    });

    install.install();
  });

  let file = install.file;

  // Allow the file removal to complete
  activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls.length, 0);
  ok(!file.exists());
});

// Check that cancelling the install from onDownloadStarted actually cancels it
add_task(async function test_14() {
  await promiseRestartManager();

  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install2_1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  ensure_test_completed();

  equal(install.file, null);

  install = await new Promise(resolve => {
    prepare_test({ }, [
      "onDownloadStarted"
    ], resolve);
    install.install();
  });

  // Wait for the channel to be ready to cancel
  executeSoon(function() {
    install.cancel();
  });

  await new Promise(resolve => {
    prepare_test({ }, [
      "onDownloadCancelled"
    ], resolve);
  });

  let file = install.file;

  install.addListener({
    onDownloadProgress() {
      do_throw("Download should not have continued");
    },
    onDownloadEnded() {
      do_throw("Download should not have continued");
    }
  });

  // Allow the listener to return to see if it continues downloading. The
  // The listener only really tests if we give it time to see progress, the
  // file check isn't ideal either
  ok(!file.exists());
});

// Checks that cancelling the install from onDownloadEnded actually cancels it
add_task(async function test_15() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install2_1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  ensure_test_completed();

  equal(install.file, null);

  await new Promise(resolve => {
    prepare_test({ }, [
      "onDownloadStarted",
      "onDownloadEnded"
    ], () => {
      prepare_test({ }, [
        "onDownloadCancelled"
      ]);

      install.cancel();
      resolve();
    });
    install.install();
  });

  ensure_test_completed();

  install.addListener({
    onInstallStarted() {
      do_throw("Install should not have continued");
    }
  });
});

// Verify that the userDisabled value carries over to the upgrade by default
add_task(async function test_16() {
  await promiseRestartManager();

  let url = "http://example.com/addons/test_install2_1.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await new Promise(resolve => {
    aInstall.addListener({
      onInstallStarted() {
        ok(!aInstall.addon.userDisabled);
        aInstall.addon.userDisabled = true;
      },

      onInstallEnded() {
        resolve();
      }
    });
    aInstall.install();
  });

  await promiseRestartManager();

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2, {
    userDisabled: true,
    isActive: false,
  });

  let url_2 = "http://example.com/addons/test_install2_2.xpi";
  let aInstall_2 = await AddonManager.getInstallForURL(url_2, "application/x-xpinstall");
  await new Promise(resolve => {
    aInstall_2.addListener({
      onInstallEnded() {
        resolve();
      }
    });
    aInstall_2.install();
  });

  checkAddon("addon2@tests.mozilla.org", aInstall_2.addon, {
    userDisabled: true,
    isActive: false,
  });

  await promiseRestartManager();

  let a2_2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2_2, {
    userDisabled: true,
    isActive: false,
  });

  a2_2.uninstall();
});

// Verify that changing the userDisabled value before onInstallEnded works
add_task(async function test_17() {
  await promiseRestartManager();

  let url = "http://example.com/addons/test_install2_1.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await new Promise(resolve => {
    aInstall.addListener({
      onInstallEnded() {
        resolve();
      }
    });
    aInstall.install();
  });

  ok(!aInstall.addon.userDisabled);

  await promiseRestartManager();

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2, {
    userDisabled: false,
    isActive: true,
  });

  let url_2 = "http://example.com/addons/test_install2_2.xpi";
  let aInstall_2 = await AddonManager.getInstallForURL(url_2, "application/x-xpinstall");

  await new Promise(resolve => {
    aInstall_2.addListener({
      onInstallStarted() {
        ok(!aInstall_2.addon.userDisabled);
        aInstall_2.addon.userDisabled = true;
      },

      onInstallEnded() {
        resolve();
      }
    });
    aInstall_2.install();
  });
  await promiseRestartManager();

  let a2_2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2_2, {
    userDisabled: true,
    isActive: false,
  });

  a2_2.uninstall();
});

// Verify that changing the userDisabled value before onInstallEnded works
add_task(async function test_18() {
  await promiseRestartManager();

  let url = "http://example.com/addons/test_install2_1.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await new Promise(resolve => {
    aInstall.addListener({
      onInstallStarted() {
        ok(!aInstall.addon.userDisabled);
        aInstall.addon.userDisabled = true;
      },

      onInstallEnded() {
        resolve();
      }
    });
    aInstall.install();
  });

  await promiseRestartManager();

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2, {
    userDisabled: true,
    isActive: false,
  });

  let url_2 = "http://example.com/addons/test_install2_2.xpi";
  let aInstall_2 = await AddonManager.getInstallForURL(url_2, "application/x-xpinstall");
  await new Promise(resolve => {
    aInstall_2.addListener({
      onInstallStarted() {
        ok(aInstall_2.addon.userDisabled);
        aInstall_2.addon.userDisabled = false;
      },

      onInstallEnded() {
        resolve();
      }
    });
    aInstall_2.install();
  });

  await promiseRestartManager();

  let a2_2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2_2, {
    isActive: true,
    userDisabled: false,
  });

  a2_2.uninstall();
});


// Checks that metadata is not stored if the pref is set to false
add_task(async function test_18_1() {
  await promiseRestartManager();

  Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", true);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS,
                             "http://example.com/data/test_install_addons.json");
  Services.prefs.setCharPref(PREF_COMPAT_OVERRIDES,
                             "http://example.com/data/test_install_compat.json");

  Services.prefs.setBoolPref("extensions.addon2@tests.mozilla.org.getAddons.cache.enabled", false);

  let url = "http://example.com/addons/test_install2_1.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall");

  let addon = await new Promise(resolve => {
    aInstall.addListener({
      onInstallEnded(unused, aAddon) {
        resolve(aAddon);
      }
    });
    aInstall.install();
  });

  notEqual(addon.fullDescription, "Repository description");

  await promiseRestartManager();

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  notEqual(a2.fullDescription, "Repository description");

  a2.uninstall();
});

// Checks that metadata is downloaded for new installs and is visible before and
// after restart
add_task(async function test_19() {
  await promiseRestartManager();
  Services.prefs.setBoolPref("extensions.addon2@tests.mozilla.org.getAddons.cache.enabled", true);

  let url = "http://example.com/addons/test_install2_1.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await new Promise(resolve => {
    aInstall.addListener({
      onInstallEnded(unused, aAddon) {
        resolve(aAddon);
      }
    });
    aInstall.install();
  });

  let a1 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  equal(a1.fullDescription, "Repository description");

  await promiseRestartManager();

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  equal(a2.fullDescription, "Repository description");

  a2.uninstall();
});

// Do the same again to make sure it works when the data is already in the cache
add_task(async function test_20() {
  await promiseRestartManager();

  let url = "http://example.com/addons/test_install2_1.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await new Promise(resolve => {
    aInstall.addListener({
      onInstallEnded(unused, aAddon) {
        resolve(aAddon);
      }
    });
    aInstall.install();
  });

  let a1 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  equal(a1.fullDescription, "Repository description");

  await promiseRestartManager();

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  equal(a2.fullDescription, "Repository description");

  a2.uninstall();
});

// Tests that an install can be restarted after being cancelled
add_task(async function test_22() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install3.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  ensure_test_completed();

  checkInstall(aInstall, {
    state: AddonManager.STATE_AVAILABLE,
  });

  let install = await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], install1 => {
      prepare_test({}, [
        "onDownloadCancelled"
      ]);
      aInstall.cancel();
      resolve(install1);
    });
    aInstall.install();
  });

  ensure_test_completed();

  await new Promise(resolve => {
    prepare_test({
      "addon3@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onDownloadStarted",
      "onDownloadEnded",
      "onInstallStarted",
      "onInstallEnded"
    ], resolve);

    install.install();
  });

  ensure_test_completed();

  AddonManager.removeAddonListener(AddonListener);
  install.addon.uninstall();
});

// Tests that an install can be restarted after being cancelled when a hash
// was provided
add_task(async function test_23() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install3.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", do_get_file_hash(XPIS.test_install3));

  ensure_test_completed();

  checkInstall(install, {
    state: AddonManager.STATE_AVAILABLE,
  });

  await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], () => {
      prepare_test({}, [
        "onDownloadCancelled"
      ]);

      install.cancel();
      resolve();
    });
    install.install();
  });

  ensure_test_completed();

  await new Promise(resolve => {
    prepare_test({
      "addon3@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled",
      ]
    }, [
      "onDownloadStarted",
      "onDownloadEnded",
      "onInstallStarted",
      "onInstallEnded"
    ], resolve);

    install.install();
  });

  ensure_test_completed();

  AddonManager.removeAddonListener(AddonListener);
  install.addon.uninstall();
});

// Tests that an install with a bad hash can be restarted after it fails, though
// it will only fail again
add_task(async function test_24() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install3.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall", "sha1:foo");
  ensure_test_completed();

  checkInstall(aInstall, {
    state: AddonManager.STATE_AVAILABLE,
  });

  let install = await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadFailed",
    ], resolve);
    aInstall.install();
  });

  await new Promise(resolve => {
    prepare_test({ }, [
      "onDownloadStarted",
      "onDownloadFailed"
    ], resolve);

    install.install();
  });
});

// Tests that installs with a hash for a local file work
add_task(async function test_25() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = Services.io.newFileURI(XPIS.test_install3).spec;
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall", do_get_file_hash(XPIS.test_install3));
  ensure_test_completed();

  checkInstall(aInstall, {
    state: AddonManager.STATE_DOWNLOADED,
    error: 0,
  });

  prepare_test({ }, [
    "onDownloadCancelled"
  ]);

  aInstall.cancel();

  ensure_test_completed();
});

add_task(async function test_26() {
  prepare_test({ }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadCancelled"
  ]);

  let url = "http://example.com/redirect?/addons/test_install1.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall");

  await new Promise(resolve => {
    let observerService = Cc["@mozilla.org/network/http-activity-distributor;1"].
                          getService(Ci.nsIHttpActivityDistributor);
    observerService.addObserver({
      observeActivity(aChannel, aType, aSubtype, aTimestamp, aSizeData,
                                aStringData) {
        aChannel.QueryInterface(Ci.nsIChannel);
        // Wait for the final event for the redirected URL
        if (aChannel.URI.spec != "http://example.com/addons/test_install1.xpi" ||
            aType != Ci.nsIHttpActivityObserver.ACTIVITY_TYPE_HTTP_TRANSACTION ||
            aSubtype != Ci.nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE)
          return;

        // Request should have been cancelled
        equal(aChannel.status, Cr.NS_BINDING_ABORTED);

        observerService.removeObserver(this);

        resolve();
      }
    });

    aInstall.addListener({
      onDownloadProgress(aDownloadProgressInstall) {
        aDownloadProgressInstall.cancel();
      }
    });

    aInstall.install();
  });
});


// Tests that an install can be restarted during onDownloadCancelled after being
// cancelled in mid-download
add_task(async function test_27() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install3.xpi";
  let aInstall = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  ensure_test_completed();

  checkInstall(aInstall, {
    state: AddonManager.STATE_AVAILABLE,
  });

  aInstall.addListener({
    onDownloadProgress() {
      aInstall.removeListener(this);
      aInstall.cancel();
    }
  });

  let install = await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadCancelled",
    ], resolve);
    aInstall.install();
  });

  install = await new Promise(resolve => {
    prepare_test({
      "addon3@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled",
      ]
    }, [
      "onDownloadStarted",
      "onDownloadEnded",
      "onInstallStarted",
      "onInstallEnded"
    ], resolve);

    let file = install.file;
    install.install();
    notEqual(file.path, install.file.path);
    ok(!file.exists());
  });

  ensure_test_completed();

  AddonManager.removeAddonListener(AddonListener);
  install.addon.uninstall();
});

// Tests that an install with a matching compatibility override has appDisabled
// set correctly.
add_task(async function test_29() {
  Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", true);

  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_install6.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", null, "Addon Test 6", null, "1.0");
  ensure_test_completed();

  checkInstall(install, {
    version: "1.0",
    name: "Addon Test 6",
    state: AddonManager.STATE_AVAILABLE,
  });

  let activeInstalls = await AddonManager.getInstallsByTypes(null);
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  install = await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded"
    ], install2 => {
      resolve(install2);
      return false;
    });
    install.install();
  });

  // ensure_test_completed();
  checkInstall(install, {
    state: AddonManager.STATE_DOWNLOADED,
  });
  checkAddon("addon6@tests.mozilla.org", install.addon, {
    isCompatible: false,
    appDisabled: true,
  });

  await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadCancelled"
    ], resolve);
    install.cancel();
  });
});

// Tests that a multi-package XPI with no add-ons inside shows up as a
// corrupt file
add_task(async function test_30() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let install = await AddonManager.getInstallForFile(XPIS.test_install7);
  ensure_test_completed();

  checkInstall(install, {
    state: AddonManager.STATE_DOWNLOAD_FAILED,
    error: AddonManager.ERROR_CORRUPT_FILE,
  });
});
