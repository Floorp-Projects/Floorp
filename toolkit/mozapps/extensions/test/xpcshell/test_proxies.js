/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the semantics of extension proxy files and symlinks

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

const LocalFile = Components.Constructor("@mozilla.org/file/local;1",
                                         "nsILocalFile", "initWithPath");
const Process = Components.Constructor("@mozilla.org/process/util;1",
                                       "nsIProcess", "init");

const gHaveSymlinks = !("nsIWindowsRegKey" in AM_Ci);
if (gHaveSymlinks)
  var gLink = findExecutable("ln");


// Unfortunately, GRE provides no way to create or manipulate symlinks
// directly. Fallback to the ln(1) executable.
function createSymlink(aSource, aDest) {
  if (aSource instanceof AM_Ci.nsIFile)
    aSource = aSource.path;

  let process = Process(gLink);
  process.run(true, ["-s", aSource, aDest.path], 3);
  do_check_eq(process.exitValue, 0, Components.stack.caller);
  return process.exitValue;
}

function findExecutable(aName) {
  if (environment.PATH) {
    for each (let path in environment.PATH.split(":")) {
      try {
        let file = LocalFile(path);
        file.append(aName);

        if (file.exists() && file.isFile() && file.isExecutable())
          return file;
      }
      catch (e) {}
    }
  }
  return null;
}

function writeFile(aData, aFile) {
  if (!aFile.parent.exists())
    aFile.parent.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);

  var fos = AM_Cc["@mozilla.org/network/file-output-stream;1"].
            createInstance(AM_Ci.nsIFileOutputStream);
  fos.init(aFile,
           FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_TRUNCATE,
           FileUtils.PERMS_FILE, 0);
  fos.write(aData, aData.length);
  fos.close();
}

function checkAddonsExist() {
  for each (let addon in ADDONS) {
    let file = addon.directory.clone();
    file.append("install.rdf");
    do_check_true(file.exists(), Components.stack.caller);
  }
}


const profileDir = gProfD.clone();
profileDir.append("extensions");


function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  // Make sure we can create symlinks on platforms that support them.
  do_check_true(!gHaveSymlinks || gLink != null);

  add_test(run_proxy_tests);

  if (gHaveSymlinks)
    add_test(run_symlink_tests);

  run_next_test();
}

function run_proxy_tests() {

  for each (let addon in ADDONS) {
    addon.directory = gTmpD.clone();
    addon.directory.append(addon.id);

    addon.proxyFile = profileDir.clone();
    addon.proxyFile.append(addon.dirId || addon.id);

    METADATA.id = addon.id;
    METADATA.name = addon.id;
    writeInstallRDFToDir(METADATA, addon.directory);

    if (addon.type == "proxy") {
      writeFile(addon.directory.path, addon.proxyFile)
    }
    else if (addon.type == "symlink" && gHaveSymlinks) {
      createSymlink(addon.directory, addon.proxyFile)
    }
  }

  startupManager();

  // Check that all add-ons original sources still exist after invalid
  // add-ons have been removed at startup.
  checkAddonsExist();

  AddonManager.getAddonsByIDs(ADDONS.map(function(addon) addon.id),
                              function(addons) {
    try {
      for (let [i, addon] in Iterator(addons)) {
        // Ensure that valid proxied add-ons were installed properly on
        // platforms that support the installation method.
        print(ADDONS[i].id, 
              ADDONS[i].dirId,
              ADDONS[i].dirId != null,
              ADDONS[i].type == "symlink" && !gHaveSymlinks);
        do_check_eq(addon == null,
                    ADDONS[i].dirId != null
                        || ADDONS[i].type == "symlink" && !gHaveSymlinks);

        if (addon != null) {
          // Check that proxied add-ons do not have upgrade permissions.
          do_check_eq(addon.permissions & AddonManager.PERM_CAN_UPGRADE, 0);

          // Check that getResourceURI points to the right place.
          do_check_eq(ios.newFileURI(ADDONS[i].directory).spec,
                      addon.getResourceURI().spec);

          let file = ADDONS[i].directory.clone();
          file.append("install.rdf");
          do_check_eq(ios.newFileURI(file).spec,
                      addon.getResourceURI("install.rdf").spec);

          addon.uninstall();
        }
      }

      // Check that original sources still exist after explicit uninstall.
      restartManager();
      checkAddonsExist();

      shutdownManager();

      // Check that all of the proxy files have been removed and remove
      // the original targets.
      for each (let addon in ADDONS) {
        do_check_false(addon.proxyFile.exists());
        addon.directory.remove(true);
      }
    }
    catch (e) {
      do_throw(e);
    }

    run_next_test();
  });
}

function run_symlink_tests() {
  // Check that symlinks are not followed out of a directory tree
  // when deleting an add-on.

  METADATA.id = "unpacked@test.mozilla.org";
  METADATA.name = METADATA.id;
  METADATA.unpack = "true";

  let tempDirectory = gTmpD.clone();
  tempDirectory.append(METADATA.id);

  let tempFile = tempDirectory.clone();
  tempFile.append("test.txt");
  tempFile.create(AM_Ci.nsIFile.NORMAL_FILE_TYPE, 0644);

  let addonDirectory = profileDir.clone();
  addonDirectory.append(METADATA.id);

  let symlink = addonDirectory.clone();
  symlink.append(tempDirectory.leafname);
  createSymlink(tempDirectory, symlink);

  // Make sure that the symlink was created properly.
  let file = symlink.clone();
  file.append(tempFile.leafName);
  file.normalize();
  do_check_true(file.equals(tempFile));

  writeInstallRDFToDir(METADATA, addonDirectory);

  startupManager();

  AddonManager.getAddonByID(METADATA.id,
                            function(addon) {
    do_check_neq(addon, null);

    addon.uninstall();

    restartManager();
    shutdownManager();

    // Check that the install directory is gone.
    do_check_false(addonDirectory.exists());

    // Check that the temp file is not gone.
    do_check_true(tempFile.exists());

    tempDirectory.remove(true);

    run_next_test();
  });
}

