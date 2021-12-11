/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// 1 day default
const DEFAULT_SECONDS_BETWEEN_CHECKS = 60 * 60 * 24;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
// These symbols are, unfortunately, accessed via the module global from
// tests, and therefore cannot be lexical definitions.
var { GMPPrefs, GMPUtils, GMP_PLUGIN_IDS } = ChromeUtils.import(
  "resource://gre/modules/GMPUtils.jsm"
);
const { ProductAddonChecker } = ChromeUtils.import(
  "resource://gre/modules/addons/ProductAddonChecker.jsm"
);

var EXPORTED_SYMBOLS = [
  "GMPInstallManager",
  "GMPExtractor",
  "GMPDownloader",
  "GMPAddon",
];

ChromeUtils.defineModuleGetter(
  this,
  "CertUtils",
  "resource://gre/modules/CertUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ServiceRequest",
  "resource://gre/modules/ServiceRequest.jsm"
);

function getScopedLogger(prefix) {
  // `PARENT_LOGGER_ID.` being passed here effectively links this logger
  // to the parentLogger.
  return Log.repository.getLoggerWithMessagePrefix("Toolkit.GMP", prefix + " ");
}

const LOCAL_GMP_SOURCES = [
  {
    id: "gmp-gmpopenh264",
    src: "chrome://global/content/gmp-sources/openh264.json",
  },
  {
    id: "gmp-widevinecdm",
    src: "chrome://global/content/gmp-sources/widevinecdm.json",
  },
];

function downloadJSON(uri) {
  let log = getScopedLogger("GMPInstallManager.checkForAddons");
  log.info("fetching config from: " + uri);
  return new Promise((resolve, reject) => {
    let xmlHttp = new ServiceRequest({ mozAnon: true });

    xmlHttp.onload = function(aResponse) {
      resolve(JSON.parse(this.responseText));
    };

    xmlHttp.onerror = function(e) {
      reject("Fetching " + uri + " results in error code: " + e.target.status);
    };

    xmlHttp.open("GET", uri);
    xmlHttp.overrideMimeType("application/json");
    xmlHttp.send();
  });
}

/**
 * If downloading from the network fails (AUS server is down),
 * load the sources from local build configuration.
 */
function downloadLocalConfig() {
  let log = getScopedLogger("GMPInstallManager.downloadLocalConfig");
  return Promise.all(
    LOCAL_GMP_SOURCES.map(conf => {
      return downloadJSON(conf.src).then(addons => {
        let platforms = addons.vendors[conf.id].platforms;
        let target = Services.appinfo.OS + "_" + UpdateUtils.ABI;
        let details = null;

        while (!details) {
          if (!(target in platforms)) {
            // There was no matching platform so return false, this addon
            // will be filtered from the results below
            log.info("no details found for: " + target);
            return false;
          }
          // Field either has the details of the binary or is an alias
          // to another build target key that does
          if (platforms[target].alias) {
            target = platforms[target].alias;
          } else {
            details = platforms[target];
          }
        }

        log.info("found plugin: " + conf.id);
        return {
          id: conf.id,
          URL: details.fileUrl,
          hashFunction: addons.hashFunction,
          hashValue: details.hashValue,
          version: addons.vendors[conf.id].version,
          size: details.filesize,
        };
      });
    })
  ).then(addons => {
    // Some filters may not match this platform so
    // filter those out
    addons = addons.filter(x => x !== false);

    return {
      usedFallback: true,
      addons,
    };
  });
}

/**
 * Provides an easy API for downloading and installing GMP Addons
 */
function GMPInstallManager() {}
/**
 * Temp file name used for downloading
 */
GMPInstallManager.prototype = {
  /**
   * Obtains a URL with replacement of vars
   */
  async _getURL() {
    let log = getScopedLogger("GMPInstallManager._getURL");
    // Use the override URL if it is specified.  The override URL is just like
    // the normal URL but it does not check the cert.
    let url = GMPPrefs.getString(GMPPrefs.KEY_URL_OVERRIDE, "");
    if (url) {
      log.info("Using override url: " + url);
    } else {
      url = GMPPrefs.getString(GMPPrefs.KEY_URL);
      log.info("Using url: " + url);
    }

    url = await UpdateUtils.formatUpdateURL(url);

    log.info("Using url (with replacement): " + url);
    return url;
  },

  /**
   * Records telemetry results on if fetching update.xml from Balrog succeeded
   * and the method that was used to verify the response from Balrog.
   * @param didGetAddonList
   *        A boolean indicating if an update.xml containing the addon list was
   *        successfully fetched (true) or not (false).
   * @param checkContentSignature
   *        If the update.xml response was verified using a content signature.
   *        If this is false, then it's assumed that response was
   *        verified using cert pinning.
   */
  recordUpdateXmlTelemetry(didGetAddonList, checkContentSignature) {
    let log = getScopedLogger("GMPInstallManager.recordUpdateXmlTelemetry");

    try {
      let updateResultHistogram = Services.telemetry.getHistogramById(
        "MEDIA_GMP_UPDATE_XML_FETCH_RESULT"
      );

      if (didGetAddonList) {
        if (checkContentSignature) {
          updateResultHistogram.add("content_sig_ok");
        } else {
          updateResultHistogram.add("cert_pinning_ok");
        }
      } else if (checkContentSignature) {
        updateResultHistogram.add("content_sig_fail");
      } else {
        updateResultHistogram.add("cert_pinning_fail");
      }
    } catch (e) {
      // We don't expect this path to be hit, but we don't want telemetry
      // failures to break GMP updates, so catch any issues here and let the
      // update machinery continue.
      log.error(
        `Failed to record telemetry result of getProductAddonList, got error: ${e}`
      );
    }
  },

  /**
   * Performs an addon check.
   * @return a promise which will be resolved or rejected.
   *         The promise is resolved with an object with properties:
   *           addons: array of addons
   *           usedFallback: whether the data was collected from online or
   *                         from fallback data within the build
   *         The promise is rejected with an object with properties:
   *           target: The XHR request object
   *           status: The HTTP status code
   *           type: Sometimes specifies type of rejection
   */
  async checkForAddons() {
    let log = getScopedLogger("GMPInstallManager.checkForAddons");
    if (this._deferred) {
      log.error("checkForAddons already called");
      return Promise.reject({ type: "alreadycalled" });
    }

    if (!GMPPrefs.getBool(GMPPrefs.KEY_UPDATE_ENABLED, true)) {
      log.info("Updates are disabled via media.gmp-manager.updateEnabled");
      return { usedFallback: true, addons: [] };
    }

    this._deferred = PromiseUtils.defer();

    // Should content signature checking of Balrog replies be used? If so this
    // will be done instead of the older cert pinning method.
    let checkContentSignature = GMPPrefs.getBool(
      GMPPrefs.KEY_CHECK_CONTENT_SIGNATURE,
      false
    );

    let allowNonBuiltIn = true;
    let certs = null;
    // Only check certificates if we're not using a custom URL, and only if
    // we're not checking a content signature.
    if (
      !Services.prefs.prefHasUserValue(GMPPrefs.KEY_URL_OVERRIDE) &&
      !checkContentSignature
    ) {
      allowNonBuiltIn = !GMPPrefs.getString(
        GMPPrefs.KEY_CERT_REQUIREBUILTIN,
        true
      );
      if (GMPPrefs.getBool(GMPPrefs.KEY_CERT_CHECKATTRS, true)) {
        certs = CertUtils.readCertPrefs(GMPPrefs.KEY_CERTS_BRANCH);
      }
    }

    let url = await this._getURL();

    log.info(
      `Fetching product addon list url=${url}, allowNonBuiltIn=${allowNonBuiltIn}, certs=${certs}, checkContentSignature=${checkContentSignature}`
    );
    let addonPromise = ProductAddonChecker.getProductAddonList(
      url,
      allowNonBuiltIn,
      certs,
      checkContentSignature
    )
      .then(res => {
        this.recordUpdateXmlTelemetry(true, checkContentSignature);
        return res;
      })
      .catch(() => {
        this.recordUpdateXmlTelemetry(false, checkContentSignature);
        return downloadLocalConfig();
      });

    addonPromise.then(
      res => {
        if (!res || !res.addons) {
          this._deferred.resolve({ addons: [] });
        } else {
          res.addons = res.addons.map(a => new GMPAddon(a));
          this._deferred.resolve(res);
        }
        delete this._deferred;
      },
      ex => {
        this._deferred.reject(ex);
        delete this._deferred;
      }
    );
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
  installAddon(gmpAddon) {
    if (this._deferred) {
      let log = getScopedLogger("GMPInstallManager.installAddon");
      log.error("previous error encountered");
      return Promise.reject({ type: "previouserrorencountered" });
    }
    this.gmpDownloader = new GMPDownloader(gmpAddon);
    return this.gmpDownloader.start();
  },
  _getTimeSinceLastCheck() {
    let now = Math.round(Date.now() / 1000);
    // Default to 0 here because `now - 0` will be returned later if that case
    // is hit. We want a large value so a check will occur.
    let lastCheck = GMPPrefs.getInt(GMPPrefs.KEY_UPDATE_LAST_CHECK, 0);
    // Handle clock jumps, return now since we want it to represent
    // a lot of time has passed since the last check.
    if (now < lastCheck) {
      return now;
    }
    return now - lastCheck;
  },
  get _isEMEEnabled() {
    return GMPPrefs.getBool(GMPPrefs.KEY_EME_ENABLED, true);
  },
  _isAddonEnabled(aAddon) {
    return GMPPrefs.getBool(GMPPrefs.KEY_PLUGIN_ENABLED, true, aAddon);
  },
  _isAddonUpdateEnabled(aAddon) {
    return (
      this._isAddonEnabled(aAddon) &&
      GMPPrefs.getBool(GMPPrefs.KEY_PLUGIN_AUTOUPDATE, true, aAddon)
    );
  },
  _updateLastCheck() {
    let now = Math.round(Date.now() / 1000);
    GMPPrefs.setInt(GMPPrefs.KEY_UPDATE_LAST_CHECK, now);
  },
  _versionchangeOccurred() {
    let savedBuildID = GMPPrefs.getString(GMPPrefs.KEY_BUILDID, "");
    let buildID = Services.appinfo.platformBuildID || "";
    if (savedBuildID == buildID) {
      return false;
    }
    GMPPrefs.setString(GMPPrefs.KEY_BUILDID, buildID);
    return true;
  },
  /**
   * Wrapper for checkForAddons and installAddon.
   * Will only install if not already installed and will log the results.
   * This will only install/update the OpenH264 and EME plugins
   * @return a promise which will be resolved if all addons could be installed
   *         successfully, rejected otherwise.
   */
  async simpleCheckAndInstall() {
    let log = getScopedLogger("GMPInstallManager.simpleCheckAndInstall");

    if (this._versionchangeOccurred()) {
      log.info(
        "A version change occurred. Ignoring " +
          "media.gmp-manager.lastCheck to check immediately for " +
          "new or updated GMPs."
      );
    } else {
      let secondsBetweenChecks = GMPPrefs.getInt(
        GMPPrefs.KEY_SECONDS_BETWEEN_CHECKS,
        DEFAULT_SECONDS_BETWEEN_CHECKS
      );
      let secondsSinceLast = this._getTimeSinceLastCheck();
      log.info(
        "Last check was: " +
          secondsSinceLast +
          " seconds ago, minimum seconds: " +
          secondsBetweenChecks
      );
      if (secondsBetweenChecks > secondsSinceLast) {
        log.info("Will not check for updates.");
        return { status: "too-frequent-no-check" };
      }
    }

    try {
      let { usedFallback, addons } = await this.checkForAddons();
      this._updateLastCheck();
      log.info("Found " + addons.length + " addons advertised.");
      let addonsToInstall = addons.filter(function(gmpAddon) {
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
          log.info(
            "Addon |" +
              gmpAddon.id +
              "| not installing updates based " +
              "on fallback."
          );
          return false;
        }

        let addonUpdateEnabled = false;
        if (GMP_PLUGIN_IDS.includes(gmpAddon.id)) {
          if (!this._isAddonEnabled(gmpAddon.id)) {
            log.info(
              "GMP |" + gmpAddon.id + "| has been disabled; skipping check."
            );
          } else if (!this._isAddonUpdateEnabled(gmpAddon.id)) {
            log.info(
              "Auto-update is off for " + gmpAddon.id + ", skipping check."
            );
          } else {
            addonUpdateEnabled = true;
          }
        } else {
          // Currently, we only support installs of OpenH264 and EME plugins.
          log.info(
            "Auto-update is off for unknown plugin '" +
              gmpAddon.id +
              "', skipping check."
          );
        }

        return addonUpdateEnabled;
      }, this);

      if (!addonsToInstall.length) {
        log.info("No new addons to install, returning");
        return { status: "nothing-new-to-install" };
      }

      let installResults = [];
      let failureEncountered = false;
      for (let addon of addonsToInstall) {
        try {
          await this.installAddon(addon);
          installResults.push({
            id: addon.id,
            result: "succeeded",
          });
        } catch (e) {
          failureEncountered = true;
          installResults.push({
            id: addon.id,
            result: "failed",
          });
        }
      }
      if (failureEncountered) {
        // eslint-disable-next-line no-throw-literal
        throw { status: "failed", results: installResults };
      }
      return { status: "succeeded", results: installResults };
    } catch (e) {
      log.error("Could not check for addons", e);
      throw e;
    }
  },

  /**
   * Makes sure everything is cleaned up
   */
  uninit() {
    let log = getScopedLogger("GMPInstallManager.uninit");
    if (this._request) {
      log.info("Aborting request");
      this._request.abort();
    }
    if (this._deferred) {
      log.info("Rejecting deferred");
      this._deferred.reject({ type: "uninitialized" });
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
  log.info("Created new addon: " + this.toString());
}

GMPAddon.prototype = {
  /**
   * Returns a string representation of the addon
   */
  toString() {
    return (
      this.id +
      " (" +
      "isValid: " +
      this.isValid +
      ", isInstalled: " +
      this.isInstalled +
      ", hashFunction: " +
      this.hashFunction +
      ", hashValue: " +
      this.hashValue +
      (this.size !== undefined ? ", size: " + this.size : "") +
      ")"
    );
  },
  /**
   * If all the fields aren't specified don't consider this addon valid
   * @return true if the addon is parsed and valid
   */
  get isValid() {
    return (
      this.id &&
      this.URL &&
      this.version &&
      this.hashFunction &&
      !!this.hashValue
    );
  },
  get isInstalled() {
    return (
      this.version &&
      GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_VERSION, "", this.id) ===
        this.version
    );
  },
  get isEME() {
    return this.id == "gmp-widevinecdm" || this.id.indexOf("gmp-eme-") == 0;
  },
  get isOpenH264() {
    return this.id == "gmp-gmpopenh264";
  },
  /**
   * @return true if the addon has been previously installed and this is
   * a new version, if this is a fresh install return false
   */
  get isUpdate() {
    return (
      this.version &&
      GMPPrefs.getBool(GMPPrefs.KEY_PLUGIN_VERSION, false, this.id)
    );
  },
};
/**
 * Constructs a GMPExtractor object which is used to extract a GMP zip
 * into the specified location.
 * @param zipPath The path on disk of the zip file to extract
 * @param relativePath The relative path inside the profile directory to
 * extract the zip to.
 */
function GMPExtractor(zipPath, relativeInstallPath) {
  this.zipPath = zipPath;
  this.relativeInstallPath = relativeInstallPath;
}
GMPExtractor.prototype = {
  /**
   * Installs the this.zipPath contents into the directory used to store GMP
   * addons for the current platform.
   *
   * @return a promise which will be resolved or rejected
   *         See GMPInstallManager.installAddon for resolve/rejected info
   */
  install() {
    this._deferred = PromiseUtils.defer();
    let deferredPromise = this._deferred;
    let { zipPath, relativeInstallPath } = this;
    // Escape the zip path since the worker will use it as a URI
    let zipFile = new FileUtils.File(zipPath);
    let zipURI = Services.io.newFileURI(zipFile).spec;
    let worker = new ChromeWorker(
      "resource://gre/modules/GMPExtractorWorker.js"
    );
    worker.onmessage = function(msg) {
      let log = getScopedLogger("GMPExtractor");
      worker.terminate();
      if (msg.data.result != "success") {
        log.error("Failed to extract zip file: " + zipURI);
        return deferredPromise.reject({
          target: this,
          status: msg.data.exception,
          type: "exception",
        });
      }
      log.info("Successfully extracted zip file: " + zipURI);
      return deferredPromise.resolve(msg.data.extractedPaths);
    };
    worker.postMessage({ zipURI, relativeInstallPath });
    return this._deferred.promise;
  },
};

/**
 * Constructs an object which downloads and initiates an install of
 * the specified GMPAddon object.
 * @param gmpAddon The addon to install.
 */
function GMPDownloader(gmpAddon) {
  this._gmpAddon = gmpAddon;
}

GMPDownloader.prototype = {
  /**
   * Starts the download process for an addon.
   * @return a promise which will be resolved or rejected
   *         See GMPInstallManager.installAddon for resolve/rejected info
   */
  start() {
    let log = getScopedLogger("GMPDownloader");
    let gmpAddon = this._gmpAddon;

    if (!gmpAddon.isValid) {
      log.info("gmpAddon is not valid, will not continue");
      return Promise.reject({
        target: this,
        type: "downloaderr",
      });
    }
    // If the HTTPS-Only Mode is enabled, every insecure request gets upgraded
    // by default. This upgrade has to be prevented for openh264 downloads since
    // the server doesn't support https://
    const downloadOptions = {
      httpsOnlyNoUpgrade: gmpAddon.isOpenH264,
    };
    return ProductAddonChecker.downloadAddon(gmpAddon, downloadOptions).then(
      zipPath => {
        let relativePath = OS.Path.join(gmpAddon.id, gmpAddon.version);
        log.info("install to directory path: " + relativePath);
        let gmpInstaller = new GMPExtractor(zipPath, relativePath);
        let installPromise = gmpInstaller.install();
        return installPromise.then(extractedPaths => {
          // Success, set the prefs
          let now = Math.round(Date.now() / 1000);
          GMPPrefs.setInt(GMPPrefs.KEY_PLUGIN_LAST_UPDATE, now, gmpAddon.id);
          // Remember our ABI, so that if the profile is migrated to another
          // platform or from 32 -> 64 bit, we notice and don't try to load the
          // unexecutable plugin library.
          let abi = GMPUtils._expectedABI(gmpAddon);
          log.info("Setting ABI to '" + abi + "' for " + gmpAddon.id);
          GMPPrefs.setString(GMPPrefs.KEY_PLUGIN_ABI, abi, gmpAddon.id);
          // Setting the version pref signals installation completion to consumers,
          // if you need to set other prefs etc. do it before this.
          GMPPrefs.setString(
            GMPPrefs.KEY_PLUGIN_VERSION,
            gmpAddon.version,
            gmpAddon.id
          );
          return extractedPaths;
        });
      }
    );
  },
};
