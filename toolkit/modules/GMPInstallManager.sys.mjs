/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// 1 day default
const DEFAULT_SECONDS_BETWEEN_CHECKS = 60 * 60 * 24;

import { Log } from "resource://gre/modules/Log.sys.mjs";
import {
  GMPPrefs,
  GMPUtils,
  GMP_PLUGIN_IDS,
  WIDEVINE_L1_ID,
  WIDEVINE_L3_ID,
} from "resource://gre/modules/GMPUtils.sys.mjs";

import { ProductAddonChecker } from "resource://gre/modules/addons/ProductAddonChecker.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CertUtils: "resource://gre/modules/CertUtils.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  ServiceRequest: "resource://gre/modules/ServiceRequest.sys.mjs",
  UpdateUtils: "resource://gre/modules/UpdateUtils.sys.mjs",
});

function getScopedLogger(prefix) {
  // `PARENT_LOGGER_ID.` being passed here effectively links this logger
  // to the parentLogger.
  return Log.repository.getLoggerWithMessagePrefix("Toolkit.GMP", prefix + " ");
}

const LOCAL_GMP_SOURCES = [
  {
    id: "gmp-gmpopenh264",
    src: "chrome://global/content/gmp-sources/openh264.json",
    installByDefault: true,
  },
  {
    id: "gmp-widevinecdm",
    src: "chrome://global/content/gmp-sources/widevinecdm.json",
    installByDefault: true,
  },
  {
    id: "gmp-widevinecdm-l1",
    src: "chrome://global/content/gmp-sources/widevinecdm_l1.json",
    installByDefault: false,
  },
];

function getLocalSources() {
  if (GMPPrefs.getBool(GMPPrefs.KEY_ALLOW_LOCAL_SOURCES, true)) {
    return LOCAL_GMP_SOURCES;
  }

  let log = getScopedLogger("GMPInstallManager.checkForAddons");
  log.info("ignoring local sources");
  return [];
}

function redirectChromiumUpdateService(uri) {
  let log = getScopedLogger("GMPInstallManager.checkForAddons");
  log.info("fetching redirect from: " + uri);
  return new Promise((resolve, reject) => {
    let xmlHttp = new lazy.ServiceRequest({ mozAnon: true });

    xmlHttp.onload = function () {
      resolve(this.responseURL);
    };

    xmlHttp.onerror = function (e) {
      reject("Fetching " + uri + " results in error code: " + e.target.status);
    };

    xmlHttp.open("GET", uri);
    xmlHttp.overrideMimeType("*/*");
    xmlHttp.setRequestHeader("Range", "bytes=0-0");
    xmlHttp.send();
  });
}

function downloadJSON(uri) {
  let log = getScopedLogger("GMPInstallManager.checkForAddons");
  log.info("fetching config from: " + uri);
  return new Promise((resolve, reject) => {
    let xmlHttp = new lazy.ServiceRequest({ mozAnon: true });

    xmlHttp.onload = function () {
      resolve(JSON.parse(this.responseText));
    };

    xmlHttp.onerror = function (e) {
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
function downloadLocalConfig(sources) {
  if (!sources.length) {
    return Promise.resolve({ addons: [] });
  }

  let log = getScopedLogger("GMPInstallManager.downloadLocalConfig");
  return Promise.all(
    sources.map(conf => {
      return downloadJSON(conf.src).then(addons => {
        let platforms = addons.vendors[conf.id].platforms;
        let target = Services.appinfo.OS + "_" + lazy.UpdateUtils.ABI;
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
          mirrorURLs: details.mirrorUrls,
          hashFunction: addons.hashFunction,
          hashValue: details.hashValue,
          version: addons.vendors[conf.id].version,
          size: details.filesize,
          usedFallback: true,
        };
      });
    })
  ).then(addons => {
    // Some filters may not match this platform so
    // filter those out
    return { addons: addons.filter(x => x !== false) };
  });
}

/**
 * Provides an easy API for downloading and installing GMP Addons
 */
export function GMPInstallManager() {}

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

    url = await lazy.UpdateUtils.formatUpdateURL(url);

    log.info("Using url (with replacement): " + url);
    return url;
  },

  /**
   * Determines the root to use for verifying content signatures.
   * @param url
   *        The Balrog URL, i.e. the return value of _getURL().
   */
  _getContentSignatureRootForURL(url) {
    // The prod and stage URLs of Balrog are documented at:
    // https://mozilla-balrog.readthedocs.io/en/latest/infrastructure.html
    // Note: we are matching by prefix without the full domain nor slash, to
    // enable us to move to a different host name in the future if desired.
    if (url.startsWith("https://aus")) {
      return Ci.nsIContentSignatureVerifier.ContentSignatureProdRoot;
    }
    if (url.startsWith("https://stage.")) {
      return Ci.nsIContentSignatureVerifier.ContentSignatureStageRoot;
    }
    if (Services.env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
      return Ci.nsIX509CertDB.AppXPCShellRoot;
    }
    // When content signature verification for GMP was added (bug 1714621), a
    // pref existed to configure an arbitrary root, which enabled local testing.
    // This pref was removed later in bug 1769669, and replaced with hard-coded
    // roots (prod and tests only). Support for testing against the stage server
    // was restored in bug 1771992.
    // Note: other verifiers ultimately fall back to ContentSignatureLocalRoot,
    // to support local development. Here we use ContentSignatureProdRoot to
    // minimize risk (and the unclear demand for "local" development).
    return Ci.nsIContentSignatureVerifier.ContentSignatureProdRoot;
  },

  /**
   * Records telemetry results on if fetching update.xml from Balrog succeeded
   * when content signature was used to verify the response from Balrog.
   * @param didGetAddonList
   *        A boolean indicating if an update.xml containing the addon list was
   *        successfully fetched (true) or not (false).
   * @param err
   *        The error that was thrown (if it exists) for the failure case. This
   *        is expected to have a addonCheckerErr member which provides further
   *        information on why the addon checker failed.
   */
  recordUpdateXmlTelemetryForContentSignature(didGetAddonList, err = null) {
    let log = getScopedLogger(
      "GMPInstallManager.recordUpdateXmlTelemetryForContentSignature"
    );
    try {
      let updateResultHistogram = Services.telemetry.getHistogramById(
        "MEDIA_GMP_UPDATE_XML_FETCH_RESULT"
      );

      // The non-glean telemetry used here will be removed in future and just
      // the glean data will be gathered.
      if (didGetAddonList) {
        updateResultHistogram.add("content_sig_ok");
        Glean.gmp.updateXmlFetchResult.content_sig_success.add(1);
        return;
      }
      // All remaining cases are failure cases.
      updateResultHistogram.add("content_sig_fail");
      if (!err?.addonCheckerErr) {
        // Unknown error case. If this is happening we should audit error paths
        // to identify why we're not getting an error, or not getting it
        // labelled.
        Glean.gmp.updateXmlFetchResult.content_sig_unknown_error.add(1);
        return;
      }
      const errorToHistogramMap = {
        [ProductAddonChecker.NETWORK_REQUEST_ERR]:
          "content_sig_net_request_error",
        [ProductAddonChecker.NETWORK_TIMEOUT_ERR]: "content_sig_net_timeout",
        [ProductAddonChecker.ABORT_ERR]: "content_sig_abort",
        [ProductAddonChecker.VERIFICATION_MISSING_DATA_ERR]:
          "content_sig_missing_data",
        [ProductAddonChecker.VERIFICATION_FAILED_ERR]: "content_sig_failed",
        [ProductAddonChecker.VERIFICATION_INVALID_ERR]: "content_sig_invalid",
        [ProductAddonChecker.XML_PARSE_ERR]: "content_sig_xml_parse_error",
      };
      let metricID =
        errorToHistogramMap[err.addonCheckerErr] ?? "content_sig_unknown_error";
      let metric = Glean.gmp.updateXmlFetchResult[metricID];
      metric.add(1);
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
   * Records telemetry results on if fetching update.xml from Balrog succeeded
   * when cert pinning was used to verify the response from Balrog. This
   * should be removed once we're no longer using cert pinning.
   * @param didGetAddonList
   *        A boolean indicating if an update.xml containing the addon list was
   *        successfully fetched (true) or not (false).
   * @param err
   *        The error that was thrown (if it exists) for the failure case. This
   *        is expected to have a addonCheckerErr member which provides further
   *        information on why the addon checker failed.
   */
  recordUpdateXmlTelemetryForCertPinning(didGetAddonList, err = null) {
    let log = getScopedLogger(
      "GMPInstallManager.recordUpdateXmlTelemetryForCertPinning"
    );
    try {
      let updateResultHistogram = Services.telemetry.getHistogramById(
        "MEDIA_GMP_UPDATE_XML_FETCH_RESULT"
      );

      // The non-glean telemetry used here will be removed in future and just
      // the glean data will be gathered.
      if (didGetAddonList) {
        updateResultHistogram.add("cert_pinning_ok");
        Glean.gmp.updateXmlFetchResult.cert_pin_success.add(1);
        return;
      }
      // All remaining cases are failure cases.
      updateResultHistogram.add("cert_pinning_fail");
      if (!err?.addonCheckerErr) {
        // Unknown error case. If this is happening we should audit error paths
        // to identify why we're not getting an error, or not getting it
        // labelled.
        Glean.gmp.updateXmlFetchResult.cert_pin_unknown_error.add(1);
        return;
      }
      const errorToHistogramMap = {
        [ProductAddonChecker.NETWORK_REQUEST_ERR]: "cert_pin_net_request_error",
        [ProductAddonChecker.NETWORK_TIMEOUT_ERR]: "cert_pin_net_timeout",
        [ProductAddonChecker.ABORT_ERR]: "cert_pin_abort",
        [ProductAddonChecker.VERIFICATION_MISSING_DATA_ERR]:
          "cert_pin_missing_data",
        [ProductAddonChecker.VERIFICATION_FAILED_ERR]: "cert_pin_failed",
        [ProductAddonChecker.VERIFICATION_INVALID_ERR]: "cert_pin_invalid",
        [ProductAddonChecker.XML_PARSE_ERR]: "cert_pin_xml_parse_error",
      };
      let metricID =
        errorToHistogramMap[err.addonCheckerErr] ?? "cert_pin_unknown_error";
      let metric = Glean.gmp.updateXmlFetchResult[metricID];
      metric.add(1);
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

    this._deferred = Promise.withResolvers();
    let deferredPromise = this._deferred.promise;

    // Should content signature checking of Balrog replies be used? If so this
    // will be done instead of the older cert pinning method.
    let checkContentSignature = GMPPrefs.getBool(
      GMPPrefs.KEY_CHECK_CONTENT_SIGNATURE,
      true
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
        certs = lazy.CertUtils.readCertPrefs(GMPPrefs.KEY_CERTS_BRANCH);
      }
    }

    let url = await this._getURL();
    let trustedContentSignatureRoot = this._getContentSignatureRootForURL(url);

    log.info(
      `Fetching product addon list url=${url}, allowNonBuiltIn=${allowNonBuiltIn}, certs=${certs}, checkContentSignature=${checkContentSignature}, trustedContentSignatureRoot=${trustedContentSignatureRoot}`
    );

    let success = true;
    let res;
    try {
      res = await ProductAddonChecker.getProductAddonList(
        url,
        allowNonBuiltIn,
        certs,
        checkContentSignature,
        trustedContentSignatureRoot
      );

      if (checkContentSignature) {
        this.recordUpdateXmlTelemetryForContentSignature(true);
      } else {
        this.recordUpdateXmlTelemetryForCertPinning(true);
      }
    } catch (err) {
      success = false;
      if (checkContentSignature) {
        this.recordUpdateXmlTelemetryForContentSignature(false, err);
      } else {
        this.recordUpdateXmlTelemetryForCertPinning(false, err);
      }
    }

    let localSources = getLocalSources();

    try {
      if (!success) {
        log.info("Falling back to local config");
        let fallbackSources = localSources.filter(function (gmpSource) {
          return gmpSource.installByDefault;
        });
        res = await downloadLocalConfig(fallbackSources);
      }
    } catch (err) {
      this._deferred.reject(err);
      delete this._deferred;
      return deferredPromise;
    }

    let addons;
    if (res && res.addons) {
      addons = res.addons.map(a => new GMPAddon(a));
    } else {
      addons = [];
    }

    // We need to merge in the addons that are only available via fallback that
    // the user has requested be forced installed regardless of our update
    // server configuration.
    try {
      let forcedSources = localSources.filter(function (gmpSource) {
        return GMPPrefs.getBool(
          GMPPrefs.KEY_PLUGIN_FORCE_INSTALL,
          false,
          gmpSource.id
        );
      });

      let forcedConfigs = await downloadLocalConfig(
        forcedSources.filter(function (gmpSource) {
          return !addons.find(gmpAddon => gmpAddon.id == gmpSource.id);
        })
      );

      let forcedAddons = forcedConfigs.addons.map(
        config => new GMPAddon(config)
      );

      log.info("Forced " + forcedAddons.length + " addons.");
      addons = addons.concat(forcedAddons);
    } catch (err) {
      log.info("Failed to force addons: " + err);
    }

    // Now let's check the addons that we are configured to override to go
    // directly to the Chromium component update service.
    try {
      for (let gmpAddon of addons) {
        if (
          !GMPPrefs.getBool(
            GMPPrefs.KEY_PLUGIN_FORCE_CHROMIUM_UPDATE,
            false,
            gmpAddon.id
          )
        ) {
          continue;
        }

        const guid = GMPPrefs.getString(
          GMPPrefs.KEY_PLUGIN_CHROMIUM_GUID,
          "",
          gmpAddon.id
        );
        if (guid === "") {
          log.warn("Skipping chromium update, missing GUID for ", gmpAddon.id);
          continue;
        }

        const params = GMPUtils._getChromiumUpdateParameters(gmpAddon);
        const serviceUrl = GMPPrefs.getString(
          GMPPrefs.KEY_CHROMIUM_UPDATE_URL,
          ""
        );
        const redirectUrl = await redirectChromiumUpdateService(
          serviceUrl.replace("%GUID%", guid) + params
        );
        const versionMatch = redirectUrl.match(/_(\d+\.\d+\.\d+\.\d+)\//);
        if (!versionMatch || versionMatch.length !== 2) {
          log.warn(
            "Skipping chromium update, no version from URL: ",
            redirectUrl
          );
          continue;
        }

        const version = versionMatch[1];
        log.info(
          "Forcing " +
            gmpAddon.id +
            " to version " +
            version +
            " from chromium update " +
            redirectUrl
        );

        // Update the addon with the final URL and the extracted version.
        gmpAddon.URL = redirectUrl;
        gmpAddon.mirrorURLs = [];
        gmpAddon.version = version;
        gmpAddon.usedChromiumUpdate = true;

        // Delete these properties to avoid verifying the addon against our
        // balrog configuration, which may or may not match.
        delete gmpAddon.size;
        delete gmpAddon.hash;
        delete gmpAddon.hashFunction;
      }
    } catch (err) {
      log.info("Failed to switch addons to Chromium update service: " + err);
    }

    this._deferred.resolve({ addons });
    delete this._deferred;
    return deferredPromise;
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
      let { addons } = await this.checkForAddons();
      this._updateLastCheck();
      log.info("Found " + addons.length + " addons advertised.");
      let addonsToInstall = addons.filter(function (gmpAddon) {
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
        if (gmpAddon.usedFallback && gmpAddon.isUpdate) {
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
        let now = Math.round(Date.now() / 1000);
        GMPPrefs.setInt(GMPPrefs.KEY_UPDATE_LAST_EMPTY_CHECK, now);
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
export function GMPAddon(addon) {
  let log = getScopedLogger("GMPAddon.constructor");
  this.usedFallback = false;
  this.usedChromiumUpdate = false;
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
      (this.usedChromiumUpdate || (this.hashFunction && !!this.hashValue))
    );
  },
  get isInstalled() {
    return (
      this.version &&
      GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_VERSION, "", this.id) ===
        this.version &&
      (this.usedChromiumUpdate ||
        (!!this.hashValue &&
          GMPPrefs.getString(GMPPrefs.KEY_PLUGIN_HASHVALUE, "", this.id) ===
            this.hashValue))
    );
  },
  get isEME() {
    return this.id == WIDEVINE_L1_ID || this.id == WIDEVINE_L3_ID;
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
 * @param relativePath The relative path components inside the profile directory
 *                     to extract the zip to.
 */
export function GMPExtractor(zipPath, relativeInstallPath) {
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
    this._deferred = Promise.withResolvers();
    let deferredPromise = this._deferred;
    let { zipPath, relativeInstallPath } = this;
    // Escape the zip path since the worker will use it as a URI
    let zipFile = new lazy.FileUtils.File(zipPath);
    let zipURI = Services.io.newFileURI(zipFile).spec;
    let worker = new ChromeWorker(
      "resource://gre/modules/GMPExtractor.worker.js"
    );
    worker.onmessage = function (msg) {
      let log = getScopedLogger("GMPExtractor");
      worker.terminate();
      if (msg.data.result != "success") {
        log.error("Failed to extract zip file: " + zipURI);
        log.error("Exception: " + msg.data.exception);
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
export function GMPDownloader(gmpAddon) {
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
    let now = Math.round(Date.now() / 1000);
    GMPPrefs.setInt(GMPPrefs.KEY_PLUGIN_LAST_INSTALL_START, now, gmpAddon.id);

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
        let now = Math.round(Date.now() / 1000);
        GMPPrefs.setInt(GMPPrefs.KEY_PLUGIN_LAST_DOWNLOAD, now, gmpAddon.id);
        log.info(
          `install to directory path: ${gmpAddon.id}/${gmpAddon.version}`
        );
        let gmpInstaller = new GMPExtractor(zipPath, [
          gmpAddon.id,
          gmpAddon.version,
        ]);
        let installPromise = gmpInstaller.install();
        return installPromise
          .then(
            extractedPaths => {
              // Success, set the prefs
              let now = Math.round(Date.now() / 1000);
              GMPPrefs.setInt(
                GMPPrefs.KEY_PLUGIN_LAST_UPDATE,
                now,
                gmpAddon.id
              );
              // Remember our ABI, so that if the profile is migrated to another
              // platform or from 32 -> 64 bit, we notice and don't try to load the
              // unexecutable plugin library.
              let abi = GMPUtils._expectedABI(gmpAddon);
              log.info("Setting ABI to '" + abi + "' for " + gmpAddon.id);
              GMPPrefs.setString(GMPPrefs.KEY_PLUGIN_ABI, abi, gmpAddon.id);
              // We use the combination of the hash and version to ensure we are
              // up to date. Ignored if we used the Chromium update service directly.
              if (!gmpAddon.usedChromiumUpdate) {
                GMPPrefs.setString(
                  GMPPrefs.KEY_PLUGIN_HASHVALUE,
                  gmpAddon.hashValue,
                  gmpAddon.id
                );
              } else {
                GMPPrefs.reset(GMPPrefs.KEY_PLUGIN_HASHVALUE, gmpAddon.id);
              }
              // Setting the version pref signals installation completion to consumers,
              // if you need to set other prefs etc. do it before this.
              GMPPrefs.setString(
                GMPPrefs.KEY_PLUGIN_VERSION,
                gmpAddon.version,
                gmpAddon.id
              );
              return extractedPaths;
            },
            reason => {
              GMPPrefs.setString(
                GMPPrefs.KEY_PLUGIN_LAST_INSTALL_FAIL_REASON,
                reason,
                gmpAddon.id
              );
              let now = Math.round(Date.now() / 1000);
              GMPPrefs.setInt(
                GMPPrefs.KEY_PLUGIN_LAST_INSTALL_FAILED,
                now,
                gmpAddon.id
              );
              throw reason;
            }
          )
          .finally(() => {
            log.info(`Deleting ${gmpAddon.id} temporary zip file ${zipPath}`);
            // We need to send out an observer event to ensure the nsZipReaderCache
            // clears its cache entries associated with our temporary file. Otherwise
            // if the addons downloader reuses the temporary file path, then we may hit
            // the cache and get different contents than expected.
            Services.obs.notifyObservers(null, "flush-cache-entry", zipPath);
            IOUtils.remove(zipPath);
          });
      },
      reason => {
        GMPPrefs.setString(
          GMPPrefs.KEY_PLUGIN_LAST_DOWNLOAD_FAIL_REASON,
          reason,
          gmpAddon.id
        );
        let now = Math.round(Date.now() / 1000);
        GMPPrefs.setInt(
          GMPPrefs.KEY_PLUGIN_LAST_DOWNLOAD_FAILED,
          now,
          gmpAddon.id
        );
        throw reason;
      }
    );
  },
};
