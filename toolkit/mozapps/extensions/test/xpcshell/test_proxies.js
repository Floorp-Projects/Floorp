/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the semantics of extension proxy files and symlinks

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");

var ADDONS = [
  {
    id: "proxy1@tests.mozilla.org",
    dirId: "proxy1@tests.mozilla.com",
    type: "proxy"
  },
  {
    id: "proxy2@tests.mozilla.org",
    type: "proxy"
  },
  {
    id: "symlink1@tests.mozilla.org",
    dirId: "symlink1@tests.mozilla.com",
    type: "symlink"
  },
  {
    id: "symlink2@tests.mozilla.org",
    type: "symlink"
  }
];

var METADATA = {
  version: "2.0",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

const gHaveSymlinks = AppConstants.platform != "win";


function createSymlink(aSource, aDest) {
  if (aSource instanceof Ci.nsIFile)
    aSource = aSource.path;
  if (aDest instanceof Ci.nsIFile)
    aDest = aDest.path;

  return OS.File.unixSymLink(aSource, aDest);
}

function promiseWriteFile(aFile, aData) {
  if (!aFile.parent.exists())
    aFile.parent.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

  return OS.File.writeAtomic(aFile.path, new TextEncoder().encode(aData));
}

function checkAddonsExist() {
  for (let addon of ADDONS) {
    let file = addon.directory.clone();
    file.append("install.rdf");
    Assert.ok(file.exists());
  }
}


const profileDir = gProfD.clone();
profileDir.append("extensions");


function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  add_task(run_proxy_tests);

  if (gHaveSymlinks)
    add_task(run_symlink_tests);

  run_next_test();
}

async function run_proxy_tests() {
  if (!gHaveSymlinks) {
    ADDONS = ADDONS.filter(a => a.type != "symlink");
  }

  for (let addon of ADDONS) {
    addon.directory = gTmpD.clone();
    addon.directory.append(addon.id);

    addon.proxyFile = profileDir.clone();
    addon.proxyFile.append(addon.dirId || addon.id);

    METADATA.id = addon.id;
    METADATA.name = addon.id;
    await promiseWriteInstallRDFToDir(METADATA, gTmpD);

    if (addon.type == "proxy") {
      await promiseWriteFile(addon.proxyFile, addon.directory.path);
    } else if (addon.type == "symlink") {
      await createSymlink(addon.directory, addon.proxyFile);
    }
  }

  await promiseStartupManager();

  // Check that all add-ons original sources still exist after invalid
  // add-ons have been removed at startup.
  checkAddonsExist();

  let addons = await AddonManager.getAddonsByIDs(ADDONS.map(addon => addon.id));
  try {
    for (let [i, addon] of addons.entries()) {
      // Ensure that valid proxied add-ons were installed properly on
      // platforms that support the installation method.
      print(ADDONS[i].id,
            ADDONS[i].dirId,
            ADDONS[i].dirId != null,
            ADDONS[i].type == "symlink");
      Assert.equal(addon == null,
                   ADDONS[i].dirId != null);

      if (addon != null) {
        let fixURL = url => {
          if (AppConstants.platform == "macosx")
            return url.replace(RegExp(`^file:///private/`), "file:///");
          return url;
        };

        // Check that proxied add-ons do not have upgrade permissions.
        Assert.equal(addon.permissions & AddonManager.PERM_CAN_UPGRADE, 0);

        // Check that getResourceURI points to the right place.
        Assert.equal(Services.io.newFileURI(ADDONS[i].directory).spec,
                     fixURL(addon.getResourceURI().spec),
                     `Base resource URL resolves as expected`);

        let file = ADDONS[i].directory.clone();
        file.append("install.rdf");

        Assert.equal(Services.io.newFileURI(file).spec,
                     fixURL(addon.getResourceURI("install.rdf").spec),
                     `Resource URLs resolve as expected`);

        addon.uninstall();
      }
    }

    // Check that original sources still exist after explicit uninstall.
    await promiseRestartManager();
    checkAddonsExist();

    await promiseShutdownManager();

    // Check that all of the proxy files have been removed and remove
    // the original targets.
    for (let addon of ADDONS) {
      equal(addon.proxyFile.exists(), addon.dirId != null,
            `Proxy file ${addon.proxyFile.path} should exist?`);
      addon.directory.remove(true);
      try {
        addon.proxyFile.remove(false);
      } catch (e) {}
    }
  } catch (e) {
    do_throw(e);
  }
}

async function run_symlink_tests() {
  // Check that symlinks are not followed out of a directory tree
  // when deleting an add-on.

  METADATA.id = "unpacked@test.mozilla.org";
  METADATA.name = METADATA.id;
  METADATA.unpack = "true";

  let tempDirectory = gTmpD.clone();
  tempDirectory.append(METADATA.id);

  let tempFile = tempDirectory.clone();
  tempFile.append("test.txt");
  tempFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);

  let addonDirectory = profileDir.clone();
  addonDirectory.append(METADATA.id);

  await promiseWriteInstallRDFToDir(METADATA, profileDir);

  let symlink = addonDirectory.clone();
  symlink.append(tempDirectory.leafName);
  await createSymlink(tempDirectory, symlink);

  // Make sure that the symlink was created properly.
  let file = symlink.clone();
  file.append(tempFile.leafName);
  file.normalize();
  Assert.equal(file.path.replace(/^\/private\//, "/"), tempFile.path);

  await promiseStartupManager();

  let addon = await AddonManager.getAddonByID(METADATA.id);
  Assert.notEqual(addon, null);

  addon.uninstall();

  await promiseRestartManager();
  await promiseShutdownManager();

  // Check that the install directory is gone.
  Assert.ok(!addonDirectory.exists());

  // Check that the temp file is not gone.
  Assert.ok(tempFile.exists());

  tempDirectory.remove(true);
}
