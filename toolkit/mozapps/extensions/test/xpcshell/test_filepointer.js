/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that various operations with file pointers work and do not affect the
// source files

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon1_2 = {
  id: "addon1@tests.mozilla.org",
  version: "2.0",
  name: "Test 1",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "1.0",
  name: "Test 2",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");
profileDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

const sourceDir = gProfD.clone();
sourceDir.append("source");

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
testserver.registerDirectory("/data/", do_get_file("data"));
testserver.registerDirectory("/addons/", do_get_file("addons"));
gPort = testserver.identity.primaryPort;

function promiseWritePointer(aId, aName) {
  let path = OS.Path.join(profileDir.path, aName || aId);

  let target = OS.Path.join(sourceDir.path,
                            do_get_expected_addon_name(aId));

  return OS.File.writeAtomic(path, new TextEncoder().encode(target));
}

function promiseWriteRelativePointer(aId, aName) {
  let path = OS.Path.join(profileDir.path, aName || aId);

  let absTarget = sourceDir.clone();
  absTarget.append(do_get_expected_addon_name(aId));

  let relTarget = absTarget.getRelativeDescriptor(profileDir);

  return OS.File.writeAtomic(path, new TextEncoder().encode(relTarget));
}

add_task(async function setup() {
  ok(TEST_UNPACKED, "Pointer files only work with unpacked directories");

  // Unpacked extensions are never signed, so this can only run with
  // signature checks disabled.
  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, false);

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
});

// Tests that installing a new add-on by pointer works
add_task(async function test_new_pointer_install() {
  await promiseWriteInstallRDFForExtension(addon1, sourceDir);
  await promiseWritePointer(addon1.id);
  await promiseStartupManager();

  let addon = await AddonManager.getAddonByID(addon1.id);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  let file = addon.getResourceURI().QueryInterface(Ci.nsIFileURL).file;
  equal(file.parent.path, sourceDir.path);

  let rootUri = do_get_addon_root_uri(sourceDir, addon1.id);
  let uri = addon.getResourceURI("/");
  equal(uri.spec, rootUri);
  uri = addon.getResourceURI("install.rdf");
  equal(uri.spec, rootUri + "install.rdf");

  // Check that upgrade is disabled for addons installed by file-pointers.
  equal(addon.permissions & AddonManager.PERM_CAN_UPGRADE, 0);
});

// Tests that installing the addon from some other source doesn't clobber
// the original sources
add_task(async function test_addon_over_pointer() {
  prepare_test({}, [
    "onNewInstall",
  ]);

  let xpi = AddonTestUtils.createTempXPIFile({
    "install.rdf": {
      id: "addon1@tests.mozilla.org",
      version: "2.0",
      name: "File Pointer Test",
      bootstrap: true,

      targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1"}],
    },
  });

  testserver.registerFile("/addons/test_filepointer.xpi", xpi);

  let url = "http://example.com/addons/test_filepointer.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await new Promise(resolve => {
    ensure_test_completed();

    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onInstalling", false],
        ["onInstalled", false],
      ]
    }, [
      "onDownloadStarted",
      "onDownloadEnded",
      "onInstallStarted",
      "onInstallEnded"
    ], callback_soon(resolve));

    install.install();
  });

  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(addon1.id);
  notEqual(addon, null);
  equal(addon.version, "2.0");

  let file = addon.getResourceURI().QueryInterface(Ci.nsIFileURL).file;
  equal(file.parent.path, profileDir.path);

  let rootUri = do_get_addon_root_uri(profileDir, addon1.id);
  let uri = addon.getResourceURI("/");
  equal(uri.spec, rootUri);
  uri = addon.getResourceURI("install.rdf");
  equal(uri.spec, rootUri + "install.rdf");

  let source = sourceDir.clone();
  source.append(addon1.id);
  ok(source.exists());

  await addon.uninstall();
});

// Tests that uninstalling doesn't clobber the original sources
add_task(async function test_uninstall_pointer() {
  await promiseRestartManager();
  await promiseWritePointer(addon1.id);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  notEqual(addon, null);
  equal(addon.version, "1.0");

  await addon.uninstall();

  await promiseRestartManager();

  let source = sourceDir.clone();
  source.append(addon1.id);
  ok(source.exists());
});

// Tests that misnaming a pointer doesn't clobber the sources
add_task(async function test_bad_pointer() {
  await promiseWritePointer("addon2@tests.mozilla.org", addon1.id);

  await promiseRestartManager();

  let [a1, a2] = await AddonManager.getAddonsByIDs(
    ["addon1@tests.mozilla.org", "addon2@tests.mozilla.org"]);

  equal(a1, null);
  equal(a2, null);

  let source = sourceDir.clone();
  source.append(addon1.id);
  ok(source.exists());

  let pointer = profileDir.clone();
  pointer.append("addon2@tests.mozilla.org");
  ok(!pointer.exists());
});

// Tests that changing the ID of an existing add-on doesn't clobber the sources
add_task(async function test_bad_pointer_id() {
  var dest = await promiseWriteInstallRDFForExtension(addon1, sourceDir);
  // Make sure the modification time changes enough to be detected.
  setExtensionModifiedTime(dest, dest.lastModifiedTime - 5000);
  await promiseWritePointer(addon1.id);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(addon1.id);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  await promiseWriteInstallRDFForExtension(addon2, sourceDir, addon1.id);
  setExtensionModifiedTime(dest, dest.lastModifiedTime - 5000);

  await promiseRestartManager();

  let [a1, a2] = await AddonManager.getAddonsByIDs(
    ["addon1@tests.mozilla.org", "addon2@tests.mozilla.org"]);

  equal(a1, null);
  equal(a2, null);

  let source = sourceDir.clone();
  source.append(addon1.id);
  ok(source.exists());

  let pointer = profileDir.clone();
  pointer.append(addon1.id);
  ok(!pointer.exists());
});

// Removing the pointer file should uninstall the add-on
add_task(async function test_remove_pointer() {
  var dest = await promiseWriteInstallRDFForExtension(addon1, sourceDir);
  // Make sure the modification time changes enough to be detected in run_test_8.
  setExtensionModifiedTime(dest, dest.lastModifiedTime - 5000);
  await promiseWritePointer(addon1.id);

  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(addon1.id);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  let pointer = profileDir.clone();
  pointer.append(addon1.id);
  pointer.remove(false);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  equal(addon, null);
});

// Removing the pointer file and replacing it with a directory should work
add_task(async function test_replace_pointer() {
  await promiseWritePointer(addon1.id);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  notEqual(addon, null);
  equal(addon.version, "1.0");

  let pointer = profileDir.clone();
  pointer.append(addon1.id);
  pointer.remove(false);

  await promiseWriteInstallRDFForExtension(addon1_2, profileDir);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  notEqual(addon, null);
  equal(addon.version, "2.0");

  await addon.uninstall();
});

// Changes to the source files should be detected
add_task(async function test_change_pointer_sources() {
  await promiseRestartManager();
  await promiseWritePointer(addon1.id);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  notEqual(addon, null);
  equal(addon.version, "1.0");

  let dest = await promiseWriteInstallRDFForExtension(addon1_2, sourceDir);
  setExtensionModifiedTime(dest, dest.lastModifiedTime - 5000);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  notEqual(addon, null);
  equal(addon.version, "2.0");

  await addon.uninstall();
});

// Removing the add-on the pointer file points at should uninstall the add-on
add_task(async function test_remove_pointer_target() {
  await promiseRestartManager();
  var dest = await promiseWriteInstallRDFForExtension(addon1, sourceDir);
  await promiseWritePointer(addon1.id);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(addon1.id);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  dest.remove(true);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID(addon1.id);
  equal(addon, null);

  let pointer = profileDir.clone();
  pointer.append(addon1.id);
  ok(!pointer.exists());
});

// Tests that installing a new add-on by pointer with a relative path works
add_task(async function test_new_relative_pointer() {
  await promiseWriteInstallRDFForExtension(addon1, sourceDir);
  await promiseWriteRelativePointer(addon1.id);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(addon1.id);
  equal(addon.version, "1.0");

  let file = addon.getResourceURI().QueryInterface(Ci.nsIFileURL).file;
  equal(file.parent.path, sourceDir.path);

  let rootUri = do_get_addon_root_uri(sourceDir, addon1.id);
  let uri = addon.getResourceURI("/");
  equal(uri.spec, rootUri);
  uri = addon.getResourceURI("install.rdf");
  equal(uri.spec, rootUri + "install.rdf");

  // Check that upgrade is disabled for addons installed by file-pointers.
  equal(addon.permissions & AddonManager.PERM_CAN_UPGRADE, 0);
});
