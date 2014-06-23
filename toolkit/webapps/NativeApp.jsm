/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["NativeApp"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/WebappOSUtils.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

const DEFAULT_ICON_URL = "chrome://global/skin/icons/webapps-64.png";

const ERR_NOT_INSTALLED = "The application isn't installed";
const ERR_UPDATES_UNSUPPORTED_OLD_NAMING_SCHEME =
  "Updates for apps installed with the old naming scheme unsupported";

// 0755
const PERMS_DIRECTORY = OS.Constants.libc.S_IRWXU |
                        OS.Constants.libc.S_IRGRP | OS.Constants.libc.S_IXGRP |
                        OS.Constants.libc.S_IROTH | OS.Constants.libc.S_IXOTH;

// 0644
const PERMS_FILE = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR |
                   OS.Constants.libc.S_IRGRP |
                   OS.Constants.libc.S_IROTH;

const DESKTOP_DIR = OS.Constants.Path.desktopDir;
const HOME_DIR = OS.Constants.Path.homeDir;
const TMP_DIR = OS.Constants.Path.tmpDir;

/**
 * This function implements the common constructor for
 * the Windows, Mac and Linux native app shells. It sets
 * the app unique name. It's meant to be called as
 * CommonNativeApp.call(this, ...) from the platform-specific
 * constructor.
 *
 * @param aApp {Object} the app object provided to the install function
 * @param aManifest {Object} the manifest data provided by the web app
 * @param aCategories {Array} array of app categories
 * @param aRegistryDir {String} (optional) path to the registry
 *
 */
function CommonNativeApp(aApp, aManifest, aCategories, aRegistryDir) {
  // Set the name property of the app object, otherwise
  // WebappOSUtils::getUniqueName won't work.
  aApp.name = aManifest.name;
  this.uniqueName = WebappOSUtils.getUniqueName(aApp);

  let localeManifest = new ManifestHelper(aManifest, aApp.origin);

  this.appLocalizedName = localeManifest.name;
  this.appNameAsFilename = stripStringForFilename(aApp.name);

  if (aApp.updateManifest) {
    this.isPackaged = true;
  }

  this.categories = aCategories.slice(0);

  this.registryDir = aRegistryDir || OS.Constants.Path.profileDir;

  this.app = aApp;

  this._dryRun = false;
  try {
    if (Services.prefs.getBoolPref("browser.mozApps.installer.dry_run")) {
      this._dryRun = true;
    }
  } catch (ex) {}
}

CommonNativeApp.prototype = {
  uniqueName: null,
  appName: null,
  appNameAsFilename: null,
  iconURI: null,
  developerName: null,
  shortDescription: null,
  categories: null,
  webappJson: null,
  runtimeFolder: null,
  manifest: null,
  registryDir: null,

  /**
   * This function reads and parses the data from the app
   * manifest and stores it in the NativeApp object.
   *
   * @param aManifest {Object} the manifest data provided by the web app
   *
   */
  _setData: function(aManifest) {
    let manifest = new ManifestHelper(aManifest, this.app.origin);
    let origin = Services.io.newURI(this.app.origin, null, null);

    this.iconURI = Services.io.newURI(manifest.biggestIconURL || DEFAULT_ICON_URL,
                                      null, null);

    if (manifest.developer) {
      if (manifest.developer.name) {
        let devName = manifest.developer.name.substr(0, 128);
        if (devName) {
          this.developerName = devName;
        }
      }

      if (manifest.developer.url) {
        this.developerUrl = manifest.developer.url;
      }
    }

    if (manifest.description) {
      let firstLine = manifest.description.split("\n")[0];
      let shortDesc = firstLine.length <= 256
                      ? firstLine
                      : firstLine.substr(0, 253) + "…";
      this.shortDescription = shortDesc;
    } else {
      this.shortDescription = this.appName;
    }

    if (manifest.version) {
      this.version = manifest.version;
    }

    this.webappJson = {
      // The app registry is the Firefox profile from which the app
      // was installed.
      "registryDir": this.registryDir,
      "app": {
        "manifest": aManifest,
        "origin": this.app.origin,
        "manifestURL": this.app.manifestURL,
        "installOrigin": this.app.installOrigin,
        "categories": this.categories,
        "receipts": this.app.receipts,
        "installTime": this.app.installTime,
      }
    };

    if (this.app.etag) {
      this.webappJson.app.etag = this.app.etag;
    }

    if (this.app.packageEtag) {
      this.webappJson.app.packageEtag = this.app.packageEtag;
    }

    if (this.app.updateManifest) {
      this.webappJson.app.updateManifest = this.app.updateManifest;
    }

    this.runtimeFolder = OS.Constants.Path.libDir;
  },

  /**
   * This function retrieves the icon for an app.
   * If the retrieving fails, it uses the default chrome icon.
   */
  _getIcon: function(aTmpDir) {
    try {
      // If the icon is in the zip package, we should modify the url
      // to point to the zip file (we can't use the app protocol yet
      // because the app isn't installed yet).
      if (this.iconURI.scheme == "app") {
        let zipUrl = OS.Path.toFileURI(OS.Path.join(aTmpDir,
                                                    this.zipFile));

        let filePath = this.iconURI.QueryInterface(Ci.nsIURL).filePath;

        this.iconURI = Services.io.newURI("jar:" + zipUrl + "!" + filePath,
                                          null, null);
      }


      let [ mimeType, icon ] = yield downloadIcon(this.iconURI);
      yield this._processIcon(mimeType, icon, aTmpDir);
    }
    catch(e) {
      Cu.reportError("Failure retrieving icon: " + e);

      let iconURI = Services.io.newURI(DEFAULT_ICON_URL, null, null);

      let [ mimeType, icon ] = yield downloadIcon(iconURI);
      yield this._processIcon(mimeType, icon, aTmpDir);

      // Set the iconURI property so that the user notification will have the
      // correct icon.
      this.iconURI = iconURI;
    }
  },

  /**
   * Creates the profile to be used for this app.
   */
  createProfile: function() {
    if (this._dryRun) {
      return null;
    }

    let profSvc = Cc["@mozilla.org/toolkit/profile-service;1"].
                  getService(Ci.nsIToolkitProfileService);

    try {
      let appProfile = profSvc.createDefaultProfileForApp(this.uniqueName,
                                                          null, null);
      return appProfile.localDir;
    } catch (ex if ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {
      return null;
    }
  },
};

#ifdef XP_WIN

#include WinNativeApp.js

#elifdef XP_MACOSX

#include MacNativeApp.js

#elifdef XP_UNIX

#include LinuxNativeApp.js

#endif

/* Helper Functions */

/**
 * Async write a data string into a file
 *
 * @param aPath     the path to the file to write to
 * @param aData     a string with the data to be written
 */
function writeToFile(aPath, aData) {
  return Task.spawn(function() {
    let data = new TextEncoder().encode(aData);

    let file;
    try {
      file = yield OS.File.open(aPath, { truncate: true, write: true },
                                { unixMode: PERMS_FILE });
      yield file.write(data);
    } finally {
      yield file.close();
    }
  });
}

/**
 * Strips all non-word characters from the beginning and end of a string.
 * Strips invalid characters from the string.
 *
 */
function stripStringForFilename(aPossiblyBadFilenameString) {
  // Strip everything from the front up to the first [0-9a-zA-Z]
  let stripFrontRE = new RegExp("^\\W*", "gi");

  // Strip white space characters starting from the last [0-9a-zA-Z]
  let stripBackRE = new RegExp("\\s*$", "gi");

  // Strip invalid characters from the filename
  let filenameRE = new RegExp("[<>:\"/\\\\|\\?\\*]", "gi");

  let stripped = aPossiblyBadFilenameString.replace(stripFrontRE, "");
  stripped = stripped.replace(stripBackRE, "");
  stripped = stripped.replace(filenameRE, "");

  // If the filename ends up empty, let's call it "webapp".
  if (stripped == "") {
    stripped = "webapp";
  }

  return stripped;
}

/**
 * Finds a unique name available in a folder (i.e., non-existent file)
 *
 * @param aPathSet a set of paths that represents the set of
 * directories where we want to write
 * @param aName   string with the filename (minus the extension) desired
 * @param aExtension string with the file extension, including the dot

 * @return file name or null if folder is unwritable or unique name
 *         was not available
 */
function getAvailableFileName(aPathSet, aName, aExtension) {
  return Task.spawn(function*() {
    let name = aName + aExtension;

    function checkUnique(aName) {
      return Task.spawn(function*() {
        for (let path of aPathSet) {
          if (yield OS.File.exists(OS.Path.join(path, aName))) {
            return false;
          }
        }

        return true;
      });
    }

    if (yield checkUnique(name)) {
      return name;
    }

    // If we're here, the plain name wasn't enough. Let's try modifying the name
    // by adding "(" + num + ")".
    for (let i = 2; i < 100; i++) {
      name = aName + " (" + i + ")" + aExtension;

      if (yield checkUnique(name)) {
        return name;
      }
    }

    throw "No available filename";
  });
}

/**
 * Attempts to remove files or directories.
 *
 * @param aPaths An array with paths to files to remove
 */
function removeFiles(aPaths) {
  for (let path of aPaths) {
    let file = getFile(path);

    try {
      if (file.exists()) {
        file.followLinks = false;
        file.remove(true);
      }
    } catch(ex) {}
  }
}

/**
 * Move (overwriting) the contents of one directory into another.
 *
 * @param srcPath A path to the source directory
 * @param destPath A path to the destination directory
 */
function moveDirectory(srcPath, destPath) {
  let srcDir = getFile(srcPath);
  let destDir = getFile(destPath);

  let entries = srcDir.directoryEntries;
  let array = [];
  while (entries.hasMoreElements()) {
    let entry = entries.getNext().QueryInterface(Ci.nsIFile);
    if (entry.isDirectory()) {
      yield moveDirectory(entry.path, OS.Path.join(destPath, entry.leafName));
    } else {
      entry.moveTo(destDir, entry.leafName);
    }
  }

  // The source directory is now empty, remove it.
  yield OS.File.removeEmptyDir(srcPath);
}

function escapeXML(aStr) {
  return aStr.toString()
             .replace(/&/g, "&amp;")
             .replace(/"/g, "&quot;")
             .replace(/'/g, "&apos;")
             .replace(/</g, "&lt;")
             .replace(/>/g, "&gt;");
}

// Helper to create a nsIFile from a set of path components
function getFile() {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(OS.Path.join.apply(OS.Path, arguments));
  return file;
}

// Download an icon using either a temp file or a pipe.
function downloadIcon(aIconURI) {
  let deferred = Promise.defer();

  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let mimeType;
  try {
    let tIndex = aIconURI.path.indexOf(";");
    if("data" == aIconURI.scheme && tIndex != -1) {
      mimeType = aIconURI.path.substring(0, tIndex);
    } else {
      mimeType = mimeService.getTypeFromURI(aIconURI);
     }
  } catch(e) {
    deferred.reject("Failed to determine icon MIME type: " + e);
    return deferred.promise;
  }

  function onIconDownloaded(aStatusCode, aIcon) {
    if (Components.isSuccessCode(aStatusCode)) {
      deferred.resolve([ mimeType, aIcon ]);
    } else {
      deferred.reject("Failure downloading icon: " + aStatusCode);
    }
  }

  try {
#ifdef XP_MACOSX
    let downloadObserver = {
      onDownloadComplete: function(downloader, request, cx, aStatus, file) {
        onIconDownloaded(aStatus, file);
      }
    };

    let tmpIcon = Services.dirsvc.get("TmpD", Ci.nsIFile);
    tmpIcon.append("tmpicon." + mimeService.getPrimaryExtension(mimeType, ""));
    tmpIcon.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

    let listener = Cc["@mozilla.org/network/downloader;1"]
                     .createInstance(Ci.nsIDownloader);
    listener.init(downloadObserver, tmpIcon);
#else
    let pipe = Cc["@mozilla.org/pipe;1"]
                 .createInstance(Ci.nsIPipe);
    pipe.init(true, true, 0, 0xffffffff, null);

    let listener = Cc["@mozilla.org/network/simple-stream-listener;1"]
                     .createInstance(Ci.nsISimpleStreamListener);
    listener.init(pipe.outputStream, {
        onStartRequest: function() {},
        onStopRequest: function(aRequest, aContext, aStatusCode) {
          pipe.outputStream.close();
          onIconDownloaded(aStatusCode, pipe.inputStream);
       }
    });
#endif

    let channel = NetUtil.newChannel(aIconURI);
    let { BadCertHandler } = Cu.import("resource://gre/modules/CertUtils.jsm", {});
    // Pass true to avoid optional redirect-cert-checking behavior.
    channel.notificationCallbacks = new BadCertHandler(true);

    channel.asyncOpen(listener, null);
  } catch(e) {
    deferred.reject("Failure initiating download of icon: " + e);
  }

  return deferred.promise;
}
