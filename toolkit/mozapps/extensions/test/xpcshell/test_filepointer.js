/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that various operations with file pointers work and do not affect the
// source files

const ID1 = "addon1@tests.mozilla.org";
const ID2 = "addon2@tests.mozilla.org";

const profileDir = gProfD.clone();
profileDir.append("extensions");
profileDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

const sourceDir = gProfD.clone();
sourceDir.append("source");

function promiseWriteWebExtension(path, data) {
  let files = ExtensionTestCommon.generateFiles(data);
  return AddonTestUtils.promiseWriteFilesToDir(path, files);
}

function promiseWritePointer(aId, aName) {
  let path = OS.Path.join(profileDir.path, aName || aId);

  let target = OS.Path.join(sourceDir.path, do_get_expected_addon_name(aId));

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
  let target = OS.Path.join(sourceDir.path, ID1);
  await promiseWriteWebExtension(target, {
    manifest: {
      version: "1.0",
      applications: { gecko: { id: ID1 } },
    },
  });
  await promiseWritePointer(ID1);
  await promiseStartupManager();

  let addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  let file = getAddonFile(addon);
  equal(file.parent.path, sourceDir.path);

  let rootUri = do_get_addon_root_uri(sourceDir, ID1);
  let uri = addon.getResourceURI();
  equal(uri.spec, rootUri);

  // Check that upgrade is disabled for addons installed by file-pointers.
  equal(addon.permissions & AddonManager.PERM_CAN_UPGRADE, 0);
});

// Tests that installing the addon from some other source doesn't clobber
// the original sources
add_task(async function test_addon_over_pointer() {
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: { gecko: { id: ID1 } },
    },
  });

  let install = await AddonManager.getInstallForFile(
    xpi,
    "application/x-xpinstall"
  );
  await install.install();

  let addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "2.0");

  let url = addon.getResourceURI();
  if (url instanceof Ci.nsIJARURI) {
    url = url.JARFile;
  }
  let { file } = url.QueryInterface(Ci.nsIFileURL);
  equal(file.parent.path, profileDir.path);

  let rootUri = do_get_addon_root_uri(profileDir, ID1);
  let uri = addon.getResourceURI();
  equal(uri.spec, rootUri);

  let source = sourceDir.clone();
  source.append(ID1);
  ok(source.exists());

  await addon.uninstall();
});

// Tests that uninstalling doesn't clobber the original sources
add_task(async function test_uninstall_pointer() {
  await promiseWritePointer(ID1);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  await addon.uninstall();

  let source = sourceDir.clone();
  source.append(ID1);
  ok(source.exists());
});

// Tests that misnaming a pointer doesn't clobber the sources
add_task(async function test_bad_pointer() {
  await promiseWritePointer(ID2, ID1);

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);
  equal(a1, null);
  equal(a2, null);

  let source = sourceDir.clone();
  source.append(ID1);
  ok(source.exists());

  let pointer = profileDir.clone();
  pointer.append(ID2);
  ok(!pointer.exists());
});

// Tests that changing the ID of an existing add-on doesn't clobber the sources
add_task(async function test_bad_pointer_id() {
  let dir = sourceDir.clone();
  dir.append(ID1);

  // Make sure the modification time changes enough to be detected.
  setExtensionModifiedTime(dir, dir.lastModifiedTime - 5000);
  await promiseWritePointer(ID1);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  await promiseWriteWebExtension(dir.path, {
    manifest: {
      version: "1.0",
      applications: { gecko: { id: ID2 } },
    },
  });
  setExtensionModifiedTime(dir, dir.lastModifiedTime - 5000);

  await promiseRestartManager();

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);
  equal(a1, null);
  equal(a2, null);

  let source = sourceDir.clone();
  source.append(ID1);
  ok(source.exists());

  let pointer = profileDir.clone();
  pointer.append(ID1);
  ok(!pointer.exists());
});

// Removing the pointer file should uninstall the add-on
add_task(async function test_remove_pointer() {
  let dir = sourceDir.clone();
  dir.append(ID1);

  await promiseWriteWebExtension(dir.path, {
    manifest: {
      version: "1.0",
      applications: { gecko: { id: ID1 } },
    },
  });

  setExtensionModifiedTime(dir, dir.lastModifiedTime - 5000);
  await promiseWritePointer(ID1);

  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  let pointer = profileDir.clone();
  pointer.append(ID1);
  pointer.remove(false);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID(ID1);
  equal(addon, null);
});

// Removing the pointer file and replacing it with a directory should work
add_task(async function test_replace_pointer() {
  await promiseWritePointer(ID1);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  let pointer = profileDir.clone();
  pointer.append(ID1);
  pointer.remove(false);

  await promiseWriteWebExtension(OS.Path.join(profileDir.path, ID1), {
    manifest: {
      version: "2.0",
      applications: { gecko: { id: ID1 } },
    },
  });

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "2.0");

  await addon.uninstall();
});

// Changes to the source files should be detected
add_task(async function test_change_pointer_sources() {
  await promiseWritePointer(ID1);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  let dir = sourceDir.clone();
  dir.append(ID1);
  await promiseWriteWebExtension(dir.path, {
    manifest: {
      version: "2.0",
      applications: { gecko: { id: ID1 } },
    },
  });
  setExtensionModifiedTime(dir, dir.lastModifiedTime - 5000);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "2.0");

  await addon.uninstall();
});

// Removing the add-on the pointer file points at should uninstall the add-on
add_task(async function test_remove_pointer_target() {
  let target = OS.Path.join(sourceDir.path, ID1);
  await promiseWriteWebExtension(target, {
    manifest: {
      version: "1.0",
      applications: { gecko: { id: ID1 } },
    },
  });
  await promiseWritePointer(ID1);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(ID1);
  notEqual(addon, null);
  equal(addon.version, "1.0");

  await OS.File.removeDir(target);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID(ID1);
  equal(addon, null);

  let pointer = profileDir.clone();
  pointer.append(ID1);
  ok(!pointer.exists());
});

// Tests that installing a new add-on by pointer with a relative path works
add_task(async function test_new_relative_pointer() {
  let target = OS.Path.join(sourceDir.path, ID1);
  await promiseWriteWebExtension(target, {
    manifest: {
      version: "1.0",
      applications: { gecko: { id: ID1 } },
    },
  });
  await promiseWriteRelativePointer(ID1);
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID(ID1);
  equal(addon.version, "1.0");

  let { file } = addon.getResourceURI().QueryInterface(Ci.nsIFileURL);
  equal(file.parent.path, sourceDir.path);

  let rootUri = do_get_addon_root_uri(sourceDir, ID1);
  let uri = addon.getResourceURI();
  equal(uri.spec, rootUri);

  // Check that upgrade is disabled for addons installed by file-pointers.
  equal(addon.permissions & AddonManager.PERM_CAN_UPGRADE, 0);
});
