/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the semantics of extension proxy files and symlinks

Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource://gre/modules/osfile.jsm");

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
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
}

const ios = AM_Cc["@mozilla.org/network/io-service;1"].getService(AM_Ci.nsIIOService);

const gHaveSymlinks = AppConstants.platform != "win";


function createSymlink(aSource, aDest) {
  if (aSource instanceof AM_Ci.nsIFile)
    aSource = aSource.path;
  if (aDest instanceof AM_Ci.nsIFile)
    aDest = aDest.path;

  return OS.File.unixSymLink(aSource, aDest);
}

function writeFile(aData, aFile) {
  if (!aFile.parent.exists())
    aFile.parent.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0o755);

  var fos = AM_Cc["@mozilla.org/network/file-output-stream;1"].
            createInstance(AM_Ci.nsIFileOutputStream);
  fos.init(aFile,
           FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_TRUNCATE,
           FileUtils.PERMS_FILE, 0);
  fos.write(aData, aData.length);
  fos.close();
}

function checkAddonsExist() {
  for (let addon of ADDONS) {
    let file = addon.directory.clone();
    file.append("install.rdf");
    do_check_true(file.exists(), Components.stack.caller);
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

function* run_proxy_tests() {
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
    writeInstallRDFToDir(METADATA, gTmpD);

    if (addon.type == "proxy") {
      writeFile(addon.directory.path, addon.proxyFile)
    }
    else if (addon.type == "symlink") {
      yield createSymlink(addon.directory, addon.proxyFile)
    }
  }

  startupManager();

  // Check that all add-ons original sources still exist after invalid
  // add-ons have been removed at startup.
  checkAddonsExist();

  return new Promise(resolve => {
    AddonManager.getAddonsByIDs(ADDONS.map(addon => addon.id), resolve);
  }).then(addons => {
    try {
      for (let [i, addon] of addons.entries()) {
        // Ensure that valid proxied add-ons were installed properly on
        // platforms that support the installation method.
        print(ADDONS[i].id,
              ADDONS[i].dirId,
              ADDONS[i].dirId != null,
              ADDONS[i].type == "symlink");
        do_check_eq(addon == null,
                    ADDONS[i].dirId != null);

        if (addon != null) {
          let fixURL = url => {
            if (AppConstants.platform == "macosx")
              return url.replace(RegExp(`^file:///private/`), "file:///");
            return url;
          };

          // Check that proxied add-ons do not have upgrade permissions.
          do_check_eq(addon.permissions & AddonManager.PERM_CAN_UPGRADE, 0);

          // Check that getResourceURI points to the right place.
          do_check_eq(ios.newFileURI(ADDONS[i].directory).spec,
                      fixURL(addon.getResourceURI().spec),
                      `Base resource URL resolves as expected`);

          let file = ADDONS[i].directory.clone();
          file.append("install.rdf");

          do_check_eq(ios.newFileURI(file).spec,
                      fixURL(addon.getResourceURI("install.rdf").spec),
                      `Resource URLs resolve as expected`);

          addon.uninstall();
        }
      }

      // Check that original sources still exist after explicit uninstall.
      restartManager();
      checkAddonsExist();

      shutdownManager();

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
    }
    catch (e) {
      do_throw(e);
    }
  });
}

function* run_symlink_tests() {
  // Check that symlinks are not followed out of a directory tree
  // when deleting an add-on.

  METADATA.id = "unpacked@test.mozilla.org";
  METADATA.name = METADATA.id;
  METADATA.unpack = "true";

  let tempDirectory = gTmpD.clone();
  tempDirectory.append(METADATA.id);

  let tempFile = tempDirectory.clone();
  tempFile.append("test.txt");
  tempFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);

  let addonDirectory = profileDir.clone();
  addonDirectory.append(METADATA.id);

  writeInstallRDFToDir(METADATA, profileDir);

  let symlink = addonDirectory.clone();
  symlink.append(tempDirectory.leafName);
  yield createSymlink(tempDirectory, symlink);

  // Make sure that the symlink was created properly.
  let file = symlink.clone();
  file.append(tempFile.leafName);
  file.normalize();
  do_check_eq(file.path.replace(/^\/private\//, "/"), tempFile.path);

  startupManager();

  return new Promise(resolve => {
    AddonManager.getAddonByID(METADATA.id, resolve);
  }).then(addon => {
    do_check_neq(addon, null);

    addon.uninstall();

    restartManager();
    shutdownManager();

    // Check that the install directory is gone.
    do_check_false(addonDirectory.exists());

    // Check that the temp file is not gone.
    do_check_true(tempFile.exists());

    tempDirectory.remove(true);
  });
}

