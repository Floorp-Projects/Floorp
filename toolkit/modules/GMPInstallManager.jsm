/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu, manager: Cm} =
  Components;
// 1 day default
const DEFAULT_SECONDS_BETWEEN_CHECKS = 60 * 60 * 24;

var GMPInstallFailureReason = {
  GMP_INVALID: 1,
  GMP_HIDDEN: 2,
  GMP_DISABLED: 3,
  GMP_UPDATE_DISABLED: 4,
};

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/GMPUtils.jsm");
Cu.import("resource://gre/modules/addons/ProductAddonChecker.jsm");

this.EXPORTED_SYMBOLS = ["GMPInstallManager", "GMPExtractor", "GMPDownloader",
                         "GMPAddon"];

// Shared code for suppressing bad cert dialogs
XPCOMUtils.defineLazyGetter(this, "gCertUtils", function() {
  let temp = { };
  Cu.import("resource://gre/modules/CertUtils.jsm", temp);
  return temp;
});

XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");

function getScopedLogger(prefix) {
  // `PARENT_LOGGER_ID.` being passed here effectively links this logger
  // to the parentLogger.
  return Log.repository.getLoggerWithMessagePrefix("Toolkit.GMP", prefix + " ");
}

/**
 * Provides an easy API for downloading and installing GMP Addons
 */
function GMPInstallManager() {
}
/**
 * Temp file name used for downloading
 */
GMPInstallManager.prototype = {
  /**
   * Obtains a URL with replacement of vars
   */
  _getURL: function() {
    let log = getScopedLogger("GMPInstallManager._getURL");
    // Use the override URL if it is specified.  The override URL is just like
    // the normal URL but it does not check the cert.
    let url = GMPPrefs.get(GMPPrefs.KEY_URL_OVERRIDE);
    if (url) {
      log.info("Using override url: " + url);
    } else {
      url = GMPPrefs.get(GMPPrefs.KEY_URL);
      log.info("Using url: " + url);
    }

    url = UpdateUtils.formatUpdateURL(url);

    log.info("Using url (with replacement): " + url);
    return url;
  },
  /**
   * Performs an addon check.
   * @return a promise which will be resolved or rejected.
   *         The promise is resolved with an object with properties:
   *           gmpAddons: array of GMPAddons
   *           usedFallback: whether the data was collected from online or
   *                         from fallback data within the build
   *         The promise is rejected with an object with properties:
   *           target: The XHR request object
   *           status: The HTTP status code
   *           type: Sometimes specifies type of rejection
   */
  checkForAddons: function() {
    let log = getScopedLogger("GMPInstallManager.checkForAddons");
    if (this._deferred) {
        log.error("checkForAddons already called");
        return Promise.reject({type: "alreadycalled"});
    }
    this._deferred = Promise.defer();
    let url = this._getURL();

    let allowNonBuiltIn = true;
    let certs = null;
    if (!Services.prefs.prefHasUserValue(GMPPrefs.KEY_URL_OVERRIDE)) {
      allowNonBuiltIn = !GMPPrefs.get(GMPPrefs.KEY_CERT_REQUIREBUILTIN, true);
      if (GMPPrefs.get(GMPPrefs.KEY_CERT_CHECKATTRS, true)) {
        certs = gCertUtils.readCertPrefs(GMPPrefs.KEY_CERTS_BRANCH);
      }
    }

    let addonPromise = ProductAddonChecker
      .getProductAddonList(url, allowNonBuiltIn, certs);

    addonPromise.then(res => {
      if (!res || !res.gmpAddons) {
        this._deferred.resolve({gmpAddons: []});
      }
      else {
        res.gmpAddons = res.gmpAddons.map(a => new GMPAddon(a));
        this._deferred.resolve(res);
      }
      delete this._deferred;
    }, (ex) => {
      this._deferred.reject(ex);
      delete this._deferred;
    });

    return this._deferred.promise;
  },
  /**
   * Installs the specified addon and calls a callback when done.
   * @param gmpAddon The GMPAddon object to install
   * @return a promise which will be resolved or rejected
   *         The promise will resolve with an array of paths that were extracted
   *         The promise will reject with an error object:
   *           target: The XHR request object
   *           status: The HTTP status code
   *           type: A string to represent the type of error
   *                 downloaderr, verifyerr or previouserrorencountered
   */
  installAddon: function(gmpAddon) {
    if (this._deferred) {
        log.error("previous error encountered");
        return Promise.reject({type: "previouserrorencountered"});
    }
    this.gmpDownloader = new GMPDownloader(gmpAddon);
    return this.gmpDownloader.start();
  },
  _getTimeSinceLastCheck: function() {
    let now = Math.round(Date.now() / 1000);
    // Default to 0 here because `now - 0` will be returned later if that case
    // is hit. We want a large value so a check will occur.
    let lastCheck = GMPPrefs.get(GMPPrefs.KEY_UPDATE_LAST_CHECK, 0);
    // Handle clock jumps, return now since we want it to represent
    // a lot of time has passed since the last check.
    if (now < lastCheck) {
      return now;
    }
    return now - lastCheck;
  },
  get _isEMEEnabled() {
    return GMPPrefs.get(GMPPrefs.KEY_EME_ENABLED, true);
  },
  _isAddonEnabled: function(aAddon) {
    return GMPPrefs.get(GMPPrefs.KEY_PLUGIN_ENABLED, true, aAddon);
  },
  _isAddonUpdateEnabled: function(aAddon) {
    return this._isAddonEnabled(aAddon) &&
           GMPPrefs.get(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, true, aAddon);
  },
  _updateLastCheck: function() {
    let now = Math.round(Date.now() / 1000);
    GMPPrefs.set(GMPPrefs.KEY_UPDATE_LAST_CHECK, now);
  },
  _versionchangeOccurred: function() {
    let savedBuildID = GMPPrefs.get(GMPPrefs.KEY_BUILDID, null);
    let buildID = Services.appinfo.platformBuildID;
    if (savedBuildID == buildID) {
      return false;
    }
    GMPPrefs.set(GMPPrefs.KEY_BUILDID, buildID);
    return true;
  },
  /**
   * Wrapper for checkForAddons and installAddon.
   * Will only install if not already installed and will log the results.
   * This will only install/update the OpenH264 and EME plugins
   * @return a promise which will be resolved if all addons could be installed
   *         successfully, rejected otherwise.
   */
  simpleCheckAndInstall: Task.async(function*() {
    let log = getScopedLogger("GMPInstallManager.simpleCheckAndInstall");

    if (this._versionchangeOccurred()) {
      log.info("A version change occurred. Ignoring " +
               "media.gmp-manager.lastCheck to check immediately for " +
               "new or updated GMPs.");
    } else {
      let secondsBetweenChecks =
        GMPPrefs.get(GMPPrefs.KEY_SECONDS_BETWEEN_CHECKS,
                     DEFAULT_SECONDS_BETWEEN_CHECKS)
      let secondsSinceLast = this._getTimeSinceLastCheck();
      log.info("Last check was: " + secondsSinceLast +
               " seconds ago, minimum seconds: " + secondsBetweenChecks);
      if (secondsBetweenChecks > secondsSinceLast) {
        log.info("Will not check for updates.");
        return {status: "too-frequent-no-check"};
      }
    }

    try {
      let {usedFallback, gmpAddons} = yield this.checkForAddons();
      this._updateLastCheck();
      log.info("Found " + gmpAddons.length + " addons advertised.");
      let addonsToInstall = gmpAddons.filter(function(gmpAddon) {
        log.info("Found addon: " + gmpAddon.toString());

        if (!gmpAddon.isValid) {
          log.info("Addon |" + gmpAddon.id + "| is invalid.");
          return false;
        }

        if (GMPUtils.isPluginHidden(gmpAddon)) {
          log.info("Addon |" + gmpAddon.id + "| has been hidden.");
          return false;
        }

        if (gmpAddon.isInstalled) {
          log.info("Addon |" + gmpAddon.id + "| already installed.");
          return false;
        }

        // Do not install from fallback if already installed as it
        // may be a downgrade
        if (usedFallback && gmpAddon.isUpdate) {
         log.info("Addon |" + gmpAddon.id + "| not installing updates based " +
                  "on fallback.");
         return false;
        }

        let addonUpdateEnabled = false;
        if (GMP_PLUGIN_IDS.indexOf(gmpAddon.id) >= 0) {
          if (!this._isAddonEnabled(gmpAddon.id)) {
            log.info("GMP |" + gmpAddon.id + "| has been disabled; skipping check.");
          } else if (!this._isAddonUpdateEnabled(gmpAddon.id)) {
            log.info("Auto-update is off for " + gmpAddon.id +
                     ", skipping check.");
          } else {
            addonUpdateEnabled = true;
          }
        } else {
          // Currently, we only support installs of OpenH264 and EME plugins.
          log.info("Auto-update is off for unknown plugin '" + gmpAddon.id +
                   "', skipping check.");
        }

        return addonUpdateEnabled;
      }, this);

      if (!addonsToInstall.length) {
        log.info("No new addons to install, returning");
        return {status: "nothing-new-to-install"};
      }

      let installResults = [];
      let failureEncountered = false;
      for (let addon of addonsToInstall) {
        try {
          yield this.installAddon(addon);
          installResults.push({
            id:     addon.id,
            result: "succeeded",
          });
        } catch (e) {
          failureEncountered = true;
          installResults.push({
            id:     addon.id,
            result: "failed",
          });
        }
      }
      if (failureEncountered) {
        throw {status:  "failed",
               results: installResults};
      }
      return {status:  "succeeded",
              results: installResults};
    } catch (e) {
      log.error("Could not check for addons", e);
      throw e;
    }
  }),

  /**
   * Makes sure everything is cleaned up
   */
  uninit: function() {
    let log = getScopedLogger("GMPInstallManager.uninit");
    if (this._request) {
      log.info("Aborting request");
      this._request.abort();
    }
    if (this._deferred) {
        log.info("Rejecting deferred");
        this._deferred.reject({type: "uninitialized"});
    }
    log.info("Done cleanup");
  },

  /**
   * If set to true, specifies to leave the temporary downloaded zip file.
   * This is useful for tests.
   */
  overrideLeaveDownloadedZip: false,
};

/**
 * Used to construct a single GMP addon
 * GMPAddon objects are returns from GMPInstallManager.checkForAddons
 * GMPAddon objects can also be used in calls to GMPInstallManager.installAddon
 *
 * @param addon The ProductAddonChecker `addon` object
 */
function GMPAddon(addon) {
  let log = getScopedLogger("GMPAddon.constructor");
  for (let name of Object.keys(addon)) {
    this[name] = addon[name];
  }
  log.info ("Created new addon: " + this.toString());
}

GMPAddon.prototype = {
  /**
   * Returns a string representation of the addon
   */
  toString: function() {
    return this.id + " (" +
           "isValid: " + this.isValid +
           ", isInstalled: " + this.isInstalled +
           ", hashFunction: " + this.hashFunction +
           ", hashValue: " + this.hashValue +
           (this.size !== undefined ? ", size: " + this.size : "" ) +
           ")";
  },
  /**
   * If all the fields aren't specified don't consider this addon valid
   * @return true if the addon is parsed and valid
   */
  get isValid() {
    return this.id && this.URL && this.version &&
      this.hashFunction && !!this.hashValue;
  },
  get isInstalled() {
    return this.version &&
      GMPPrefs.get(GMPPrefs.KEY_PLUGIN_VERSION, "", this.id) === this.version;
  },
  get isEME() {
    return this.id == "gmp-widevinecdm" || this.id.indexOf("gmp-eme-") == 0;
  },
  /**
   * @return true if the addon has been previously installed and this is
   * a new version, if this is a fresh install return false
   */
  get isUpdate() {
    return this.version &&
      GMPPrefs.get(GMPPrefs.KEY_PLUGIN_VERSION, false, this.id);
  },
};
/**
 * Constructs a GMPExtractor object which is used to extract a GMP zip
 * into the specified location. (Which typically leties per platform)
 * @param zipPath The path on disk of the zip file to extract
 */
function GMPExtractor(zipPath, installToDirPath) {
    this.zipPath = zipPath;
    this.installToDirPath = installToDirPath;
}
GMPExtractor.prototype = {
  /**
   * Obtains a list of all the entries in a zipfile in the format of *.*.
   * This also includes files inside directories.
   *
   * @param zipReader the nsIZipReader to check
   * @return An array of string name entries which can be used
   *         in nsIZipReader.extract
   */
  _getZipEntries: function(zipReader) {
    let entries = [];
    let enumerator = zipReader.findEntries("*.*");
    while (enumerator.hasMore()) {
      entries.push(enumerator.getNext());
    }
    return entries;
  },
  /**
   * Installs the this.zipPath contents into the directory used to store GMP
   * addons for the current platform.
   *
   * @return a promise which will be resolved or rejected
   *         See GMPInstallManager.installAddon for resolve/rejected info
   */
  install: function() {
    try {
      let log = getScopedLogger("GMPExtractor.install");
      this._deferred = Promise.defer();
      log.info("Installing " + this.zipPath + "...");
      // Get the input zip file
      let zipFile = Cc["@mozilla.org/file/local;1"].
                    createInstance(Ci.nsIFile);
      zipFile.initWithPath(this.zipPath);

      // Initialize a zipReader and obtain the entries
      var zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                      createInstance(Ci.nsIZipReader);
      zipReader.open(zipFile)
      let entries = this._getZipEntries(zipReader);
      let extractedPaths = [];

      let destDir = Cc["@mozilla.org/file/local;1"].
                    createInstance(Ci.nsILocalFile);
      destDir.initWithPath(this.installToDirPath);
      // Make sure the destination exists
      if (!destDir.exists()) {
        destDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
      }

      // Extract each of the entries
      entries.forEach(entry => {
        // We don't need these types of files
        if (entry.includes("__MACOSX") ||
            entry == "_metadata/verified_contents.json" ||
            entry == "imgs/icon-128x128.png") {
          return;
        }
        let outFile = destDir.clone();
        // Do not extract into directories. Extract all files to the same
        // directory. DO NOT use |OS.Path.basename()| here, as in Windows it
        // does not work properly with forward slashes (which we must use here).
        let outBaseName = entry.slice(entry.lastIndexOf("/") + 1);
        outFile.appendRelativePath(outBaseName);

        zipReader.extract(entry, outFile);
        extractedPaths.push(outFile.path);
        // Ensure files are writable and executable. Otherwise we may be unable to
        // execute or uninstall them.
        outFile.permissions |= parseInt("0700", 8);
        log.info(entry + " was successfully extracted to: " +
            outFile.path);
      });
      zipReader.close();
      if (!GMPInstallManager.overrideLeaveDownloadedZip) {
        zipFile.remove(false);
      }

      log.info(this.zipPath + " was installed successfully");
      this._deferred.resolve(extractedPaths);
    } catch (e) {
      if (zipReader) {
        zipReader.close();
      }
      this._deferred.reject({
        target: this,
        status: e,
        type: "exception"
      });
    }
    return this._deferred.promise;
  }
};


/**
 * Constructs an object which downloads and initiates an install of
 * the specified GMPAddon object.
 * @param gmpAddon The addon to install.
 */
function GMPDownloader(gmpAddon)
{
  this._gmpAddon = gmpAddon;
}

GMPDownloader.prototype = {
  /**
   * Starts the download process for an addon.
   * @return a promise which will be resolved or rejected
   *         See GMPInstallManager.installAddon for resolve/rejected info
   */
  start: function() {
    let log = getScopedLogger("GMPDownloader");
    let gmpAddon = this._gmpAddon;

    if (!gmpAddon.isValid) {
      log.info("gmpAddon is not valid, will not continue");
      return Promise.reject({
        target: this,
        status: status,
        type: "downloaderr"
      });
    }

    return ProductAddonChecker.downloadAddon(gmpAddon).then((zipPath) => {
      let path = OS.Path.join(OS.Constants.Path.profileDir,
                              gmpAddon.id,
                              gmpAddon.version);
      log.info("install to directory path: " + path);
      let gmpInstaller = new GMPExtractor(zipPath, path);
      let installPromise = gmpInstaller.install();
      return installPromise.then(extractedPaths => {
        // Success, set the prefs
        let now = Math.round(Date.now() / 1000);
        GMPPrefs.set(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, now, gmpAddon.id);
        // Remember our ABI, so that if the profile is migrated to another
        // platform or from 32 -> 64 bit, we notice and don't try to load the
        // unexecutable plugin library.
        GMPPrefs.set(GMPPrefs.KEY_PLUGIN_ABI, UpdateUtils.ABI, gmpAddon.id);
        // Setting the version pref signals installation completion to consumers,
        // if you need to set other prefs etc. do it before this.
        GMPPrefs.set(GMPPrefs.KEY_PLUGIN_VERSION, gmpAddon.version,
                     gmpAddon.id);
        return extractedPaths;
      });
    });
  },
};
