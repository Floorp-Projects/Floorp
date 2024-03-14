/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The AddonUpdateChecker is responsible for retrieving the update information
 * from an add-on's remote update manifest.
 */

const TIMEOUT = 60 * 1000;
const TOOLKIT_ID = "toolkit@mozilla.org";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.sys.mjs",
  AddonSettings: "resource://gre/modules/addons/AddonSettings.sys.mjs",
  Blocklist: "resource://gre/modules/Blocklist.sys.mjs",
  CertUtils: "resource://gre/modules/CertUtils.sys.mjs",
  ServiceRequest: "resource://gre/modules/ServiceRequest.sys.mjs",
});

import { Log } from "resource://gre/modules/Log.sys.mjs";

const LOGGER_ID = "addons.update-checker";

// Create a new logger for use by the Addons Update Checker
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

/**
 * Sanitizes the update URL in an update item, as returned by
 * parseRDFManifest and parseJSONManifest. Ensures that:
 *
 * - The URL is secure, or secured by a strong enough hash.
 * - The security principal of the update manifest has permission to
 *   load the URL.
 *
 * @param aUpdate
 *        The update item to sanitize.
 * @param aRequest
 *        The XMLHttpRequest used to load the manifest.
 * @param aHashPattern
 *        The regular expression used to validate the update hash.
 * @param aHashString
 *        The human-readable string specifying which hash functions
 *        are accepted.
 */
function sanitizeUpdateURL(aUpdate, aRequest, aHashPattern, aHashString) {
  if (aUpdate.updateURL) {
    let scriptSecurity = Services.scriptSecurityManager;
    let principal = scriptSecurity.getChannelURIPrincipal(aRequest.channel);
    try {
      // This logs an error on failure, so no need to log it a second time
      scriptSecurity.checkLoadURIStrWithPrincipal(
        principal,
        aUpdate.updateURL,
        scriptSecurity.DISALLOW_SCRIPT
      );
    } catch (e) {
      delete aUpdate.updateURL;
      return;
    }

    if (
      lazy.AddonManager.checkUpdateSecurity &&
      !aUpdate.updateURL.startsWith("https:") &&
      !aHashPattern.test(aUpdate.updateHash)
    ) {
      logger.warn(
        `Update link ${aUpdate.updateURL} is not secure and is not verified ` +
          `by a strong enough hash (needs to be ${aHashString}).`
      );
      delete aUpdate.updateURL;
      delete aUpdate.updateHash;
    }
  }
}

/**
 * Parses an JSON update manifest into an array of update objects.
 *
 * @param  aId
 *         The ID of the add-on being checked for updates
 * @param  aRequest
 *         The XMLHttpRequest that has retrieved the update manifest
 * @param  aManifestData
 *         The pre-parsed manifest, as a JSON object tree
 * @return an array of update objects
 * @throws if the update manifest is invalid in any way
 */
function parseJSONManifest(aId, aRequest, aManifestData) {
  let TYPE_CHECK = {
    array: val => Array.isArray(val),
    object: val => val && typeof val == "object" && !Array.isArray(val),
  };

  function getProperty(aObj, aProperty, aType, aDefault = undefined) {
    if (!(aProperty in aObj)) {
      return aDefault;
    }

    let value = aObj[aProperty];

    let matchesType =
      aType in TYPE_CHECK ? TYPE_CHECK[aType](value) : typeof value == aType;
    if (!matchesType) {
      throw Components.Exception(
        `Update manifest property '${aProperty}' has incorrect type (expected ${aType})`
      );
    }

    return value;
  }

  function getRequiredProperty(aObj, aProperty, aType) {
    let value = getProperty(aObj, aProperty, aType);
    if (value === undefined) {
      throw Components.Exception(
        `Update manifest is missing a required ${aProperty} property.`
      );
    }
    return value;
  }

  let manifest = aManifestData;

  if (!TYPE_CHECK.object(manifest)) {
    throw Components.Exception(
      "Root element of update manifest must be a JSON object literal"
    );
  }

  // The set of add-ons this manifest has updates for
  let addons = getProperty(manifest, "addons", "object");

  if (!addons) {
    let keys = Object.keys(manifest);
    if (keys.length) {
      // "addons" property is optional. The presence of other properties may be
      // a sign of a mistake, so print a warning to help with debugging.
      logger.warn(
        `Update manifest for ${aId} is missing the "addons" property, found ${keys} instead.`
      );
    } else {
      // If the add-on isn't listed, the update server may return an empty
      // response.
      logger.warn(`Received empty update manifest for ${aId}.`);
    }
    return [];
  }

  // The entry for this particular add-on
  let addon = getProperty(addons, aId, "object");

  // A missing entry doesn't count as a failure, just as no avialable update
  // information
  if (!addon) {
    logger.warn("Update manifest did not contain an entry for " + aId);
    return [];
  }

  // The list of available updates
  let updates = getProperty(addon, "updates", "array", []);

  let results = [];

  for (let update of updates) {
    let version = getRequiredProperty(update, "version", "string");

    logger.debug(`Found an update entry for ${aId} version ${version}`);

    let applications = getProperty(update, "applications", "object", {
      gecko: {},
    });

    // "gecko" is currently the only supported application entry. If
    // it's missing, skip this update.
    if (!("gecko" in applications)) {
      logger.debug(
        "gecko not in application entry, skipping update of ${addon}"
      );
      continue;
    }

    let app = getProperty(applications, "gecko", "object");

    let appEntry = {
      id: TOOLKIT_ID,
      minVersion: getProperty(
        app,
        "strict_min_version",
        "string",
        lazy.AddonManagerPrivate.webExtensionsMinPlatformVersion
      ),
      maxVersion: "*",
    };

    let result = {
      id: aId,
      version,
      updateURL: getProperty(update, "update_link", "string"),
      updateHash: getProperty(update, "update_hash", "string"),
      updateInfoURL: getProperty(update, "update_info_url", "string"),
      strictCompatibility: false,
      targetApplications: [appEntry],
    };

    if ("strict_max_version" in app) {
      if ("advisory_max_version" in app) {
        logger.warn(
          "Ignoring 'advisory_max_version' update manifest property for " +
            aId +
            " property since 'strict_max_version' also present"
        );
      }

      appEntry.maxVersion = getProperty(app, "strict_max_version", "string");
      result.strictCompatibility = appEntry.maxVersion != "*";
    } else if ("advisory_max_version" in app) {
      appEntry.maxVersion = getProperty(app, "advisory_max_version", "string");
    }

    // Add an app entry for the current API ID, too, so that it overrides any
    // existing app-specific entries, which would take priority over the toolkit
    // entry.
    //
    // Note: This currently only has any effect on legacy extensions (mainly
    // those used in tests), since WebExtensions cannot yet specify app-specific
    // compatibility ranges.
    result.targetApplications.push(
      Object.assign({}, appEntry, { id: Services.appinfo.ID })
    );

    // The JSON update protocol requires an SHA-2 hash. RDF still
    // supports SHA-1, for compatibility reasons.
    sanitizeUpdateURL(result, aRequest, /^sha(256|512):/, "sha256 or sha512");

    results.push(result);
  }
  return results;
}

/**
 * Starts downloading an update manifest and then passes it to an appropriate
 * parser to convert to an array of update objects
 *
 * @param  aId
 *         The ID of the add-on being checked for updates
 * @param  aUrl
 *         The URL of the update manifest
 * @param  aObserver
 *         An observer to pass results to
 */
function UpdateParser(aId, aUrl, aObserver) {
  this.id = aId;
  this.observer = aObserver;
  this.url = aUrl;

  logger.debug("Requesting " + aUrl);
  try {
    this.request = new lazy.ServiceRequest({ mozAnon: true });
    this.request.open("GET", this.url, true);
    this.request.channel.notificationCallbacks =
      new lazy.CertUtils.BadCertHandler(
        !lazy.AddonSettings.UPDATE_REQUIREBUILTINCERTS
      );
    this.request.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    // Prevent the request from writing to cache.
    this.request.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    this.request.overrideMimeType("text/plain");
    this.request.timeout = TIMEOUT;
    this.request.addEventListener("load", () => this.onLoad());
    this.request.addEventListener("error", () => this.onError());
    this.request.addEventListener("timeout", () => this.onTimeout());
    this.request.send(null);
  } catch (e) {
    logger.error(`Failed to request update manifest for ${this.id}`, e);
  }
}

UpdateParser.prototype = {
  id: null,
  observer: null,
  request: null,
  url: null,

  /**
   * Called when the manifest has been successfully loaded.
   */
  onLoad() {
    let request = this.request;
    this.request = null;
    this._doneAt = new Error("place holder");

    try {
      lazy.CertUtils.checkCert(
        request.channel,
        !lazy.AddonSettings.UPDATE_REQUIREBUILTINCERTS
      );
    } catch (e) {
      logger.warn("Request failed: " + this.url + " - " + e);
      this.notifyError(lazy.AddonManager.ERROR_DOWNLOAD_ERROR);
      return;
    }

    if (!Components.isSuccessCode(request.status)) {
      logger.warn("Request failed: " + this.url + " - " + request.status);
      this.notifyError(lazy.AddonManager.ERROR_DOWNLOAD_ERROR);
      return;
    }

    let channel = request.channel;
    if (channel instanceof Ci.nsIHttpChannel && !channel.requestSucceeded) {
      logger.warn(
        "Request failed: " +
          this.url +
          " - " +
          channel.responseStatus +
          ": " +
          channel.responseStatusText
      );
      this.notifyError(lazy.AddonManager.ERROR_DOWNLOAD_ERROR);
      return;
    }

    let results;
    try {
      let json = JSON.parse(request.responseText);
      results = parseJSONManifest(this.id, request, json);
    } catch (e) {
      logger.warn(
        `onUpdateCheckComplete failed to parse update manifest for ${this.id}`,
        e
      );
      this.notifyError(lazy.AddonManager.ERROR_PARSE_ERROR);
      return;
    }

    if ("onUpdateCheckComplete" in this.observer) {
      try {
        this.observer.onUpdateCheckComplete(results);
      } catch (e) {
        logger.warn(
          `onUpdateCheckComplete notification failed for ${this.id}`,
          e
        );
      }
    } else {
      logger.warn(
        "onUpdateCheckComplete may not properly cancel",
        new Error("stack marker")
      );
    }
  },

  /**
   * Called when the request times out
   */
  onTimeout() {
    this.request = null;
    this._doneAt = new Error("Timed out");
    logger.warn("Request for " + this.url + " timed out");
    this.notifyError(lazy.AddonManager.ERROR_TIMEOUT);
  },

  /**
   * Called when the manifest failed to load.
   */
  onError() {
    if (!Components.isSuccessCode(this.request.status)) {
      logger.warn("Request failed: " + this.url + " - " + this.request.status);
    } else if (this.request.channel instanceof Ci.nsIHttpChannel) {
      try {
        if (this.request.channel.requestSucceeded) {
          logger.warn(
            "Request failed: " +
              this.url +
              " - " +
              this.request.channel.responseStatus +
              ": " +
              this.request.channel.responseStatusText
          );
        }
      } catch (e) {
        logger.warn("HTTP Request failed for an unknown reason");
      }
    } else {
      logger.warn("Request failed for an unknown reason");
    }

    this.request = null;
    this._doneAt = new Error("UP_onError");

    this.notifyError(lazy.AddonManager.ERROR_DOWNLOAD_ERROR);
  },

  /**
   * Helper method to notify the observer that an error occurred.
   */
  notifyError(aStatus) {
    if ("onUpdateCheckError" in this.observer) {
      try {
        this.observer.onUpdateCheckError(aStatus);
      } catch (e) {
        logger.warn("onUpdateCheckError notification failed", e);
      }
    }
  },

  /**
   * Called to cancel an in-progress update check.
   */
  cancel() {
    if (!this.request) {
      logger.error("Trying to cancel already-complete request", this._doneAt);
      return;
    }
    this.request.abort();
    this.request = null;
    this._doneAt = new Error("UP_cancel");
    this.notifyError(lazy.AddonManager.ERROR_CANCELLED);
  },
};

/**
 * Tests if an update matches a version of the application or platform
 *
 * @param  aUpdate
 *         The available update
 * @param  aAppVersion
 *         The application version to use
 * @param  aPlatformVersion
 *         The platform version to use
 * @param  aIgnoreMaxVersion
 *         Ignore maxVersion when testing if an update matches. Optional.
 * @param  aIgnoreStrictCompat
 *         Ignore strictCompatibility when testing if an update matches. Optional.
 * @return true if the update is compatible with the application/platform
 */
function matchesVersions(
  aUpdate,
  aAppVersion,
  aPlatformVersion,
  aIgnoreMaxVersion,
  aIgnoreStrictCompat
) {
  if (aUpdate.strictCompatibility && !aIgnoreStrictCompat) {
    aIgnoreMaxVersion = false;
  }

  let result = false;
  for (let app of aUpdate.targetApplications) {
    if (app.id == Services.appinfo.ID) {
      return (
        Services.vc.compare(aAppVersion, app.minVersion) >= 0 &&
        (aIgnoreMaxVersion ||
          Services.vc.compare(aAppVersion, app.maxVersion) <= 0)
      );
    }
    if (app.id == TOOLKIT_ID) {
      result =
        Services.vc.compare(aPlatformVersion, app.minVersion) >= 0 &&
        (aIgnoreMaxVersion ||
          Services.vc.compare(aPlatformVersion, app.maxVersion) <= 0);
    }
  }
  return result;
}

export var AddonUpdateChecker = {
  /**
   * Retrieves the best matching compatibility update for the application from
   * a list of available update objects.
   *
   * @param  aUpdates
   *         An array of update objects
   * @param  aVersion
   *         The version of the add-on to get new compatibility information for
   * @param  aIgnoreCompatibility
   *         An optional parameter to get the first compatibility update that
   *         is compatible with any version of the application or toolkit
   * @param  aAppVersion
   *         The version of the application or null to use the current version
   * @param  aPlatformVersion
   *         The version of the platform or null to use the current version
   * @param  aIgnoreMaxVersion
   *         Ignore maxVersion when testing if an update matches. Optional.
   * @param  aIgnoreStrictCompat
   *         Ignore strictCompatibility when testing if an update matches. Optional.
   * @return an update object if one matches or null if not
   */
  getCompatibilityUpdate(
    aUpdates,
    aVersion,
    aIgnoreCompatibility,
    aAppVersion,
    aPlatformVersion,
    aIgnoreMaxVersion,
    aIgnoreStrictCompat
  ) {
    if (!aAppVersion) {
      aAppVersion = Services.appinfo.version;
    }
    if (!aPlatformVersion) {
      aPlatformVersion = Services.appinfo.platformVersion;
    }

    for (let update of aUpdates) {
      if (Services.vc.compare(update.version, aVersion) == 0) {
        if (aIgnoreCompatibility) {
          for (let targetApp of update.targetApplications) {
            let id = targetApp.id;
            if (id == Services.appinfo.ID || id == TOOLKIT_ID) {
              return update;
            }
          }
        } else if (
          matchesVersions(
            update,
            aAppVersion,
            aPlatformVersion,
            aIgnoreMaxVersion,
            aIgnoreStrictCompat
          )
        ) {
          return update;
        }
      }
    }
    return null;
  },

  /**
   * Asynchronously returns the newest available update from a list of update objects.
   *
   * @param  aUpdates
   *         An array of update objects
   * @param  aAddon
   *         The add-on that is being updated.
   * @param  aAppVersion
   *         The version of the application or null to use the current version
   * @param  aPlatformVersion
   *         The version of the platform or null to use the current version
   * @param  aIgnoreMaxVersion
   *         When determining compatible updates, ignore maxVersion. Optional.
   * @param  aIgnoreStrictCompat
   *         When determining compatible updates, ignore strictCompatibility. Optional.
   * @return an update object if one matches or null if not
   */
  async getNewestCompatibleUpdate(
    aUpdates,
    aAddon,
    aAppVersion,
    aPlatformVersion,
    aIgnoreMaxVersion,
    aIgnoreStrictCompat
  ) {
    if (!aAppVersion) {
      aAppVersion = Services.appinfo.version;
    }
    if (!aPlatformVersion) {
      aPlatformVersion = Services.appinfo.platformVersion;
    }

    let newestVersion = aAddon.version;
    let newest = null;
    let blocked = null;
    let blockedState;
    for (let update of aUpdates) {
      if (!update.updateURL) {
        continue;
      }
      if (Services.vc.compare(newestVersion, update.version) >= 0) {
        // Update older than add-on version or older than previous result.
        continue;
      }
      if (
        !matchesVersions(
          update,
          aAppVersion,
          aPlatformVersion,
          aIgnoreMaxVersion,
          aIgnoreStrictCompat
        )
      ) {
        continue;
      }
      let state = await lazy.Blocklist.getAddonBlocklistState(
        update,
        aAppVersion,
        aPlatformVersion
      );
      if (state != Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
        if (
          !blocked ||
          Services.vc.compare(blocked.version, update.version) < 0
        ) {
          blocked = update;
          blockedState = state;
        }
        continue;
      }
      newest = update;
      newestVersion = update.version;
    }
    if (
      blocked &&
      (!newest || Services.vc.compare(blocked.version, newestVersion) >= 0)
    ) {
      // If |newest| has a higher version than |blocked|, then the add-on would
      // not be considered for installation. But if |blocked| would otherwise
      // be eligible for installation, then report to telemetry that installation
      // has been blocked because of the blocklist.
      lazy.Blocklist.recordAddonBlockChangeTelemetry(
        {
          id: aAddon.id,
          version: blocked.version,
          blocklistState: blockedState,
        },
        "addon_update_check"
      );
    }
    return newest;
  },

  /**
   * Starts an update check.
   *
   * @param  aId
   *         The ID of the add-on being checked for updates
   * @param  aUrl
   *         The URL of the add-on's update manifest
   * @param  aObserver
   *         An observer to notify of results
   * @return UpdateParser so that the caller can use UpdateParser.cancel() to shut
   *         down in-progress update requests
   */
  checkForUpdates(aId, aUrl, aObserver) {
    return new UpdateParser(aId, aUrl, aObserver);
  },
};
