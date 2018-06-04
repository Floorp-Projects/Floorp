/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint "valid-jsdoc": [2, {requireReturn: false}] */

var EXPORTED_SYMBOLS = ["Blocklist"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
Cu.importGlobalProperties(["DOMParser"]);

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "AddonManagerPrivate",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "CertUtils",
                               "resource://gre/modules/CertUtils.jsm");
ChromeUtils.defineModuleGetter(this, "FileUtils",
                               "resource://gre/modules/FileUtils.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateUtils",
                               "resource://gre/modules/UpdateUtils.jsm");
ChromeUtils.defineModuleGetter(this, "OS",
                               "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(this, "ServiceRequest",
                               "resource://gre/modules/ServiceRequest.jsm");

// The remote settings updater is the new system in charge of fetching remote data
// securely and efficiently. It will replace the current XML-based system.
// See Bug 1257565 and Bug 1252456.
const BlocklistClients = {};
ChromeUtils.defineModuleGetter(BlocklistClients, "initialize",
                               "resource://services-common/blocklist-clients.js");
XPCOMUtils.defineLazyGetter(this, "RemoteSettings", function() {
  // Instantiate blocklist clients.
  BlocklistClients.initialize();
  // Import RemoteSettings for ``pollChanges()``
  const { RemoteSettings } = ChromeUtils.import("resource://services-settings/remote-settings.js", {});
  return RemoteSettings;
});

const TOOLKIT_ID                      = "toolkit@mozilla.org";
const KEY_PROFILEDIR                  = "ProfD";
const KEY_APPDIR                      = "XCurProcD";
const FILE_BLOCKLIST                  = "blocklist.xml";
const PREF_BLOCKLIST_LASTUPDATETIME   = "app.update.lastUpdateTime.blocklist-background-update-timer";
const PREF_BLOCKLIST_URL              = "extensions.blocklist.url";
const PREF_BLOCKLIST_ITEM_URL         = "extensions.blocklist.itemURL";
const PREF_BLOCKLIST_ENABLED          = "extensions.blocklist.enabled";
const PREF_BLOCKLIST_LAST_MODIFIED    = "extensions.blocklist.lastModified";
const PREF_BLOCKLIST_LEVEL            = "extensions.blocklist.level";
const PREF_BLOCKLIST_PINGCOUNTTOTAL   = "extensions.blocklist.pingCountTotal";
const PREF_BLOCKLIST_PINGCOUNTVERSION = "extensions.blocklist.pingCountVersion";
const PREF_BLOCKLIST_SUPPRESSUI       = "extensions.blocklist.suppressUI";
const PREF_BLOCKLIST_UPDATE_ENABLED   = "services.blocklist.update_enabled";
const PREF_APP_DISTRIBUTION           = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION   = "distribution.version";
const PREF_EM_LOGGING_ENABLED         = "extensions.logging.enabled";
const XMLURI_BLOCKLIST                = "http://www.mozilla.org/2006/addons-blocklist";
const XMLURI_PARSE_ERROR              = "http://www.mozilla.org/newlayout/xml/parsererror.xml";
const URI_BLOCKLIST_DIALOG            = "chrome://mozapps/content/extensions/blocklist.xul";
const DEFAULT_SEVERITY                = 3;
const DEFAULT_LEVEL                   = 2;
const MAX_BLOCK_LEVEL                 = 3;
const SEVERITY_OUTDATED               = 0;
const VULNERABILITYSTATUS_NONE             = 0;
const VULNERABILITYSTATUS_UPDATE_AVAILABLE = 1;
const VULNERABILITYSTATUS_NO_UPDATE        = 2;

const EXTENSION_BLOCK_FILTERS = ["id", "name", "creator", "homepageURL", "updateURL"];

var gLoggingEnabled = null;
var gBlocklistEnabled = true;
var gBlocklistLevel = DEFAULT_LEVEL;

/**
 * @class nsIBlocklistPrompt
 *
 * nsIBlocklistPrompt is used, if available, by the default implementation of
 * nsIBlocklistService to display a confirmation UI to the user before blocking
 * extensions/plugins.
 */
/**
 * @method prompt
 *
 * Prompt the user about newly blocked addons. The prompt is then resposible
 * for soft-blocking any addons that need to be afterwards
 *
 * @param {object[]} aAddons
 *         An array of addons and plugins that are blocked. These are javascript
 *         objects with properties:
 *          name    - the plugin or extension name,
 *          version - the version of the extension or plugin,
 *          icon    - the plugin or extension icon,
 *          disable - can be used by the nsIBlocklistPrompt to allows users to decide
 *                    whether a soft-blocked add-on should be disabled,
 *          blocked - true if the item is hard-blocked, false otherwise,
 *          item    - the nsIPluginTag or Addon object
 */

// From appinfo in Services.jsm. It is not possible to use the one in
// Services.jsm since it will not successfully QueryInterface nsIXULAppInfo in
// xpcshell tests due to other code calling Services.appinfo before the
// nsIXULAppInfo is created by the tests.
XPCOMUtils.defineLazyGetter(this, "gApp", function() {
  // eslint-disable-next-line mozilla/use-services
  let appinfo = Cc["@mozilla.org/xre/app-info;1"]
                  .getService(Ci.nsIXULRuntime);
  try {
    appinfo.QueryInterface(Ci.nsIXULAppInfo);
  } catch (ex) {
    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (!(ex instanceof Components.Exception) ||
        ex.result != Cr.NS_NOINTERFACE)
      throw ex;
  }
  return appinfo;
});

XPCOMUtils.defineLazyGetter(this, "gABI", function() {
  let abi = null;
  try {
    abi = gApp.XPCOMABI;
  } catch (e) {
    LOG("BlockList Global gABI: XPCOM ABI unknown.");
  }

  if (AppConstants.platform == "macosx") {
    // Mac universal build should report a different ABI than either macppc
    // or mactel.
    let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].
                   getService(Ci.nsIMacUtils);

    if (macutils.isUniversalBinary)
      abi += "-u-" + macutils.architecturesInBinary;
  }
  return abi;
});

XPCOMUtils.defineLazyGetter(this, "gOSVersion", function() {
  let osVersion;
  try {
    osVersion = Services.sysinfo.getProperty("name") + " " + Services.sysinfo.getProperty("version");
  } catch (e) {
    LOG("BlockList Global gOSVersion: OS Version unknown.");
  }

  if (osVersion) {
    try {
      osVersion += " (" + Services.sysinfo.getProperty("secondaryLibrary") + ")";
    } catch (e) {
      // Not all platforms have a secondary widget library, so an error is nothing to worry about.
    }
    osVersion = encodeURIComponent(osVersion);
  }
  return osVersion;
});

/**
 * Logs a string to the error console.
 * @param {string} string
 *        The string to write to the error console..
 */
function LOG(string) {
  if (gLoggingEnabled) {
    dump("*** " + string + "\n");
    Services.console.logStringMessage(string);
  }
}

// Restarts the application checking in with observers first
function restartApp() {
  // Notify all windows that an application quit has been requested.
  var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                   createInstance(Ci.nsISupportsPRBool);
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested");

  // Something aborted the quit process.
  if (cancelQuit.data)
    return;

  Services.startup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
}

/**
 * Checks whether this blocklist element is valid for the current OS and ABI.
 * If the element has an "os" attribute then the current OS must appear in
 * its comma separated list for the element to be valid. Similarly for the
 * xpcomabi attribute.
 *
 * @param {Element} blocklistElement
 *        The blocklist element from an XML blocklist.
 * @returns {bool}
 *        Whether the entry matches the current OS.
 */
function matchesOSABI(blocklistElement) {
  if (blocklistElement.hasAttribute("os")) {
    var choices = blocklistElement.getAttribute("os").split(",");
    if (choices.length > 0 && !choices.includes(gApp.OS))
      return false;
  }

  if (blocklistElement.hasAttribute("xpcomabi")) {
    choices = blocklistElement.getAttribute("xpcomabi").split(",");
    if (choices.length > 0 && !choices.includes(gApp.XPCOMABI))
      return false;
  }

  return true;
}

/**
 * Gets the current value of the locale.  It's possible for this preference to
 * be localized, so we have to do a little extra work here.  Similar code
 * exists in nsHttpHandler.cpp when building the UA string.
 *
 * @returns {string} The current requested locale.
 */
function getLocale() {
  return Services.locale.getRequestedLocale();
}

/* Get the distribution pref values, from defaults only */
function getDistributionPrefValue(aPrefName) {
  return Services.prefs.getDefaultBranch(null).getCharPref(aPrefName, "default");
}

/**
 * Parse a string representation of a regular expression. Needed because we
 * use the /pattern/flags form (because it's detectable), which is only
 * supported as a literal in JS.
 *
 * @param {string} aStr
 *         String representation of regexp
 * @return {RegExp} instance
 */
function parseRegExp(aStr) {
  let lastSlash = aStr.lastIndexOf("/");
  let pattern = aStr.slice(1, lastSlash);
  let flags = aStr.slice(lastSlash + 1);
  return new RegExp(pattern, flags);
}

/**
 * Manages the Blocklist. The Blocklist is a representation of the contents of
 * blocklist.xml and allows us to remotely disable / re-enable blocklisted
 * items managed by the Extension Manager with an item's appDisabled property.
 * It also blocklists plugins with data from blocklist.xml.
 */
var Blocklist = {
  _init() {
    Services.obs.addObserver(this, "xpcom-shutdown");

    gLoggingEnabled = Services.prefs.getBoolPref(PREF_EM_LOGGING_ENABLED, false);
    gBlocklistEnabled = Services.prefs.getBoolPref(PREF_BLOCKLIST_ENABLED, true);
    gBlocklistLevel = Math.min(Services.prefs.getIntPref(PREF_BLOCKLIST_LEVEL, DEFAULT_LEVEL),
                               MAX_BLOCK_LEVEL);
    Services.prefs.addObserver("extensions.blocklist.", this);
    Services.prefs.addObserver(PREF_EM_LOGGING_ENABLED, this);

    // If the stub blocklist service deferred any queries because we
    // weren't loaded yet, execute them now.
    for (let entry of Services.blocklist.pluginQueries.splice(0)) {
      entry.resolve(this.getPluginBlocklistState(entry.plugin,
                                                 entry.appVersion,
                                                 entry.toolkitVersion));
    }
  },

  STATE_NOT_BLOCKED: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
  STATE_SOFTBLOCKED: Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
  STATE_BLOCKED: Ci.nsIBlocklistService.STATE_BLOCKED,
  STATE_OUTDATED: Ci.nsIBlocklistService.STATE_OUTDATED,
  STATE_VULNERABLE_UPDATE_AVAILABLE: Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE,
  STATE_VULNERABLE_NO_UPDATE: Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE,


  /**
   * Extension ID -> array of Version Ranges
   * Each value in the version range array is a JS Object that has the
   * following properties:
   *   "minVersion"  The minimum version in a version range (default = 0)
   *   "maxVersion"  The maximum version in a version range (default = *)
   *   "targetApps"  Application ID -> array of Version Ranges
   *                 (default = current application ID)
   *                 Each value in the version range array is a JS Object that
   *                 has the following properties:
   *                   "minVersion"  The minimum version in a version range
   *                                 (default = 0)
   *                   "maxVersion"  The maximum version in a version range
   *                                 (default = *)
   */
  _addonEntries: null,
  _gfxEntries: null,
  _pluginEntries: null,

  shutdown() {
    Services.obs.removeObserver(this, "xpcom-shutdown");
    Services.prefs.removeObserver("extensions.blocklist.", this);
    Services.prefs.removeObserver(PREF_EM_LOGGING_ENABLED, this);
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
    case "xpcom-shutdown":
      this.shutdown();
      break;
    case "profile-after-change":
      // We're only called here on non-Desktop-Firefox, and use this opportunity to try to
      // load the blocklist asynchronously. On desktop Firefox, we load the list from
      // nsBrowserGlue after sessionstore-windows-restored.
      this.loadBlocklistAsync();
      break;
    case "nsPref:changed":
      switch (aData) {
        case PREF_EM_LOGGING_ENABLED:
          gLoggingEnabled = Services.prefs.getBoolPref(PREF_EM_LOGGING_ENABLED, false);
          break;
        case PREF_BLOCKLIST_ENABLED:
          gBlocklistEnabled = Services.prefs.getBoolPref(PREF_BLOCKLIST_ENABLED, true);
          this._loadBlocklist();
          this._blocklistUpdated(null, null);
          break;
        case PREF_BLOCKLIST_LEVEL:
          gBlocklistLevel = Math.min(Services.prefs.getIntPref(PREF_BLOCKLIST_LEVEL, DEFAULT_LEVEL),
                                     MAX_BLOCK_LEVEL);
          this._blocklistUpdated(null, null);
          break;
      }
      break;
    }
  },

  /**
   * Determine the blocklist state of an add-on
   * @param {Addon} addon
   *        The addon item to be checked.
   * @param {string?} appVersion
   *        The version of the application we are checking in the blocklist.
   *        If this parameter is null, the version of the running application
   *        is used.
   * @param {string?} toolkitVersion
   *        The version of the toolkit we are checking in the blocklist.
   *        If this parameter is null, the version of the running toolkit
   *        is used.
   * @returns {integer} The STATE constant.
   */
  async getAddonBlocklistState(addon, appVersion, toolkitVersion) {
    await this.loadBlocklistAsync();
    return this._getAddonBlocklistState(addon, this._addonEntries,
                                        appVersion, toolkitVersion);
  },

  /**
   * Returns a matching blocklist entry for the given add-on, if one
   * exists.
   *
   * @param {Addon} addon
   *        The add-on object of the item to get the blocklist state for.
   * @param {object[]} addonEntries
   *        The add-on blocklist entries to compare against.
   * @param {string?} appVersion
   *        The application version to compare to, will use the current
   *        version if null.
   * @param {string?} toolkitVersion
   *        The toolkit version to compare to, will use the current version if
   *        null.
   * @returns {object?}
   *          A blocklist entry for this item, with `state` and `url`
   *          properties indicating the block state and URL, if there is
   *          a matching blocklist entry, or null otherwise.
   */
  _getAddonBlocklistEntry(addon, addonEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled)
      return null;

    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (!appVersion && !gApp.version)
      return null;

    if (!appVersion)
      appVersion = gApp.version;
    if (!toolkitVersion)
      toolkitVersion = gApp.platformVersion;

    var blItem = this._findMatchingAddonEntry(addonEntries, addon);
    if (!blItem)
      return null;

    for (let currentblItem of blItem.versions) {
      if (currentblItem.includesItem(addon.version, appVersion, toolkitVersion)) {
        return {
          state: (currentblItem.severity >= gBlocklistLevel ?
                  Ci.nsIBlocklistService.STATE_BLOCKED : Ci.nsIBlocklistService.STATE_SOFTBLOCKED),
          url: blItem.blockID && this._createBlocklistURL(blItem.blockID),
        };
      }
    }
    return null;
  },

  /**
   * Returns a promise that resolves to the blocklist entry.
   * The blocklist entry is an object with `state` and `url`
   * properties, if a blocklist entry for the add-on exists, or null
   * otherwise.

   * @param {Addon} addon
   *          The addon object to match.
   * @param {string?} appVersion
   *        The version of the application we are checking in the blocklist.
   *        If this parameter is null, the version of the running application
   *        is used.
   * @param {string?} toolkitVersion
   *        The version of the toolkit we are checking in the blocklist.
   *        If this parameter is null, the version of the running toolkit
   *        is used.
   * @returns {Promise<object?>}
   *        The blocklist entry for the add-on, if one exists, or null
   *        otherwise.
   */
  async getAddonBlocklistEntry(addon, appVersion, toolkitVersion) {
    await this.loadBlocklistAsync();
    return this._getAddonBlocklistEntry(addon, this._addonEntries,
                                        appVersion, toolkitVersion);
  },

  /**
   * Private version of getAddonBlocklistState that allows the caller to pass in
   * the add-on blocklist entries to compare against.
   *
   * @param {Addon} addon
   *        The add-on object of the item to get the blocklist state for.
   * @param {object[]} addonEntries
   *        The add-on blocklist entries to compare against.
   * @param {string?} appVersion
   *        The application version to compare to, will use the current
   *        version if null.
   * @param {string?} toolkitVersion
   *        The toolkit version to compare to, will use the current version if
   *        null.
   * @returns {integer}
   *        The blocklist state for the item, one of the STATE constants as
   *        defined in nsIBlocklistService.
   */
  _getAddonBlocklistState(addon, addonEntries, appVersion, toolkitVersion) {
    let entry = this._getAddonBlocklistEntry(addon, addonEntries, appVersion, toolkitVersion);
    if (entry)
      return entry.state;
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  /**
   * Returns the set of prefs of the add-on stored in the blocklist file
   * (probably to revert them on disabling).
   * @param {Addon} addon
   *        The add-on whose to-be-reset prefs are to be found.
   * @returns {string[]}
   *        An array of preference names.
   */
  _getAddonPrefs(addon) {
    let entry = this._findMatchingAddonEntry(this._addonEntries, addon);
    return entry.prefs.slice(0);
  },

  _findMatchingAddonEntry(aAddonEntries, aAddon) {
    if (!aAddon)
      return null;
    // Returns true if the params object passes the constraints set by entry.
    // (For every non-null property in entry, the same key must exist in
    // params and value must be the same)
    function checkEntry(entry, params) {
      for (let [key, value] of entry) {
        if (value === null || value === undefined)
          continue;
        if (params[key]) {
          if (value instanceof RegExp) {
            if (!value.test(params[key])) {
              return false;
            }
          } else if (value !== params[key]) {
            return false;
          }
        } else {
          return false;
        }
      }
      return true;
    }

    let params = {};
    for (let filter of EXTENSION_BLOCK_FILTERS) {
      params[filter] = aAddon[filter];
    }
    if (params.creator)
      params.creator = params.creator.name;
    for (let entry of aAddonEntries) {
      if (checkEntry(entry.attributes, params)) {
         return entry;
       }
     }
     return null;
  },

  _createBlocklistURL(id) {
    let url = Services.urlFormatter.formatURLPref(PREF_BLOCKLIST_ITEM_URL);
    return url.replace(/%blockID%/g, id);
  },

  notify(aTimer) {
    if (!gBlocklistEnabled)
      return;

    try {
      var dsURI = Services.prefs.getCharPref(PREF_BLOCKLIST_URL);
    } catch (e) {
      LOG("Blocklist::notify: The " + PREF_BLOCKLIST_URL + " preference" +
          " is missing!");
      return;
    }

    var pingCountVersion = Services.prefs.getIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION, 0);
    var pingCountTotal = Services.prefs.getIntPref(PREF_BLOCKLIST_PINGCOUNTTOTAL, 1);
    var daysSinceLastPing = 0;
    if (pingCountVersion == 0) {
      daysSinceLastPing = "new";
    } else {
      // Seconds in one day is used because nsIUpdateTimerManager stores the
      // last update time in seconds.
      let secondsInDay = 60 * 60 * 24;
      let lastUpdateTime = Services.prefs.getIntPref(PREF_BLOCKLIST_LASTUPDATETIME, 0);
      if (lastUpdateTime == 0) {
        daysSinceLastPing = "invalid";
      } else {
        let now = Math.round(Date.now() / 1000);
        daysSinceLastPing = Math.floor((now - lastUpdateTime) / secondsInDay);
      }

      if (daysSinceLastPing == 0 || daysSinceLastPing == "invalid") {
        pingCountVersion = pingCountTotal = "invalid";
      }
    }

    if (pingCountVersion < 1)
      pingCountVersion = 1;
    if (pingCountTotal < 1)
      pingCountTotal = 1;

    dsURI = dsURI.replace(/%APP_ID%/g, gApp.ID);
    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (gApp.version)
      dsURI = dsURI.replace(/%APP_VERSION%/g, gApp.version);
    dsURI = dsURI.replace(/%PRODUCT%/g, gApp.name);
    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (gApp.version)
      dsURI = dsURI.replace(/%VERSION%/g, gApp.version);
    dsURI = dsURI.replace(/%BUILD_ID%/g, gApp.appBuildID);
    dsURI = dsURI.replace(/%BUILD_TARGET%/g, gApp.OS + "_" + gABI);
    dsURI = dsURI.replace(/%OS_VERSION%/g, gOSVersion);
    dsURI = dsURI.replace(/%LOCALE%/g, getLocale());
    dsURI = dsURI.replace(/%CHANNEL%/g, UpdateUtils.UpdateChannel);
    dsURI = dsURI.replace(/%PLATFORM_VERSION%/g, gApp.platformVersion);
    dsURI = dsURI.replace(/%DISTRIBUTION%/g,
                      getDistributionPrefValue(PREF_APP_DISTRIBUTION));
    dsURI = dsURI.replace(/%DISTRIBUTION_VERSION%/g,
                      getDistributionPrefValue(PREF_APP_DISTRIBUTION_VERSION));
    dsURI = dsURI.replace(/%PING_COUNT%/g, pingCountVersion);
    dsURI = dsURI.replace(/%TOTAL_PING_COUNT%/g, pingCountTotal);
    dsURI = dsURI.replace(/%DAYS_SINCE_LAST_PING%/g, daysSinceLastPing);
    dsURI = dsURI.replace(/\+/g, "%2B");

    // Under normal operations it will take around 5,883,516 years before the
    // preferences used to store pingCountVersion and pingCountTotal will rollover
    // so this code doesn't bother trying to do the "right thing" here.
    if (pingCountVersion != "invalid") {
      pingCountVersion++;
      if (pingCountVersion > 2147483647) {
        // Rollover to -1 if the value is greater than what is support by an
        // integer preference. The -1 indicates that the counter has been reset.
        pingCountVersion = -1;
      }
      Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION, pingCountVersion);
    }

    if (pingCountTotal != "invalid") {
      pingCountTotal++;
      if (pingCountTotal > 2147483647) {
        // Rollover to 1 if the value is greater than what is support by an
        // integer preference.
        pingCountTotal = -1;
      }
      Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTTOTAL, pingCountTotal);
    }

    // Verify that the URI is valid
    try {
      var uri = Services.io.newURI(dsURI);
    } catch (e) {
      LOG("Blocklist::notify: There was an error creating the blocklist URI\r\n" +
          "for: " + dsURI + ", error: " + e);
      return;
    }

    LOG("Blocklist::notify: Requesting " + uri.spec);
    let request = new ServiceRequest();
    request.open("GET", uri.spec, true);
    request.channel.notificationCallbacks = new CertUtils.BadCertHandler();
    request.overrideMimeType("text/xml");

    // The server will return a `304 Not Modified` response if the blocklist was
    // not changed since last check.
    const lastModified = Services.prefs.getCharPref(PREF_BLOCKLIST_LAST_MODIFIED, "");
    if (lastModified) {
      request.setRequestHeader("If-Modified-Since", lastModified);
    } else {
      request.setRequestHeader("Cache-Control", "no-cache");
    }

    request.addEventListener("error", event => this.onXMLError(event));
    request.addEventListener("load", event => this.onXMLLoad(event));
    request.send(null);

    // When the blocklist loads we need to compare it to the current copy so
    // make sure we have loaded it.
    if (!this.isLoaded)
      this._loadBlocklist();

    // If blocklist update via Kinto is enabled, poll for changes and sync.
    // Currently certificates blocklist relies on it by default.
    if (Services.prefs.getBoolPref(PREF_BLOCKLIST_UPDATE_ENABLED)) {
      RemoteSettings.pollChanges().catch(() => {
        // Bug 1254099 - Telemetry (success or errors) will be collected during this process.
      });
    }
  },

  async onXMLLoad(aEvent) {
    let request = aEvent.target;
    try {
      CertUtils.checkCert(request.channel);
    } catch (e) {
      LOG("Blocklist::onXMLLoad: " + e);
      return;
    }

    let {status} = request;
    if (status == 304) {
      LOG("Blocklist::onXMLLoad: up to date.");
      return;
    }

    if (status != 200 && status != 0) {
      LOG("Blocklist::onXMLLoad: there was an error during load, got status: " + status);
      return;
    }

    let {responseXML} = request;
    if (!responseXML || responseXML.documentElement.namespaceURI == XMLURI_PARSE_ERROR) {
      LOG("Blocklist::onXMLLoad: there was an error during load, we got invalid XML");
      return;
    }

    // Save current blocklist timestamp to pref.
    const lastModified = request.getResponseHeader("Last-Modified") || "";
    Services.prefs.setCharPref(PREF_BLOCKLIST_LAST_MODIFIED, lastModified);

    var oldAddonEntries = this._addonEntries;
    var oldPluginEntries = this._pluginEntries;

    this._loadBlocklistFromXML(responseXML);
    // We don't inform the users when the graphics blocklist changed at runtime.
    // However addons and plugins blocking status is refreshed.
    this._blocklistUpdated(oldAddonEntries, oldPluginEntries);

    try {
      let path = OS.Path.join(OS.Constants.Path.profileDir, FILE_BLOCKLIST);
      await OS.File.writeAtomic(path, request.responseText, {tmpPath: path + ".tmp"});
    } catch (e) {
      LOG("Blocklist::onXMLLoad: " + e);
    }
  },

  onXMLError(aEvent) {
    try {
      var request = aEvent.target;
      // the following may throw (e.g. a local file or timeout)
      var status = request.status;
    } catch (e) {
      request = aEvent.target.channel.QueryInterface(Ci.nsIRequest);
      status = request.status;
    }
    var statusText = "XMLHttpRequest channel unavailable";
    // When status is 0 we don't have a valid channel.
    if (status != 0) {
      try {
        statusText = request.statusText;
      } catch (e) {
      }
    }
    LOG("Blocklist:onError: There was an error loading the blocklist file\r\n" +
        statusText);
  },

  /**
   * Finds the newest blocklist file from the application and the profile and
   * load it or does nothing if neither exist.
   */
  _loadBlocklist() {
    this._addonEntries = [];
    this._gfxEntries = [];
    this._pluginEntries = [];

    Services.telemetry.getHistogramById("BLOCKLIST_SYNC_FILE_LOAD").add(true);

    var profFile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);
    try {
      this._loadBlocklistFromFile(profFile);
    } catch (ex) {
      LOG("Blocklist::_loadBlocklist: couldn't load file from profile, trying app dir");
      try {
        var appFile = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);
        this._loadBlocklistFromFile(appFile);
      } catch (ex) {
        LOG("Blocklist::_loadBlocklist: no XML File found");
      }
    }
  },

  /**
#    The blocklist XML file looks something like this:
#
#    <blocklist xmlns="http://www.mozilla.org/2006/addons-blocklist">
#      <emItems>
#        <emItem id="item_1@domain" blockID="i1">
#          <prefs>
#            <pref>accessibility.accesskeycausesactivation</pref>
#            <pref>accessibility.blockautorefresh</pref>
#          </prefs>
#          <versionRange minVersion="1.0" maxVersion="2.0.*">
#            <targetApplication id="{ec8030f7-c20a-464f-9b0e-13a3a9e97384}">
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#              <versionRange minVersion="1.7" maxVersion="1.7.*"/>
#            </targetApplication>
#            <targetApplication id="toolkit@mozilla.org">
#              <versionRange minVersion="1.9" maxVersion="1.9.*"/>
#            </targetApplication>
#          </versionRange>
#          <versionRange minVersion="3.0" maxVersion="3.0.*">
#            <targetApplication id="{ec8030f7-c20a-464f-9b0e-13a3a9e97384}">
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#            </targetApplication>
#            <targetApplication id="toolkit@mozilla.org">
#              <versionRange minVersion="1.9" maxVersion="1.9.*"/>
#            </targetApplication>
#          </versionRange>
#        </emItem>
#        <emItem id="item_2@domain" blockID="i2">
#          <versionRange minVersion="3.1" maxVersion="4.*"/>
#        </emItem>
#        <emItem id="item_3@domain">
#          <versionRange>
#            <targetApplication id="{ec8030f7-c20a-464f-9b0e-13a3a9e97384}">
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#            </targetApplication>
#          </versionRange>
#        </emItem>
#        <emItem id="item_4@domain" blockID="i3">
#          <versionRange>
#            <targetApplication>
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#            </targetApplication>
#          </versionRange>
#        <emItem id="/@badperson\.com$/"/>
#      </emItems>
#      <pluginItems>
#        <pluginItem blockID="i4">
#          <!-- All match tags must match a plugin to blocklist a plugin -->
#          <match name="name" exp="some plugin"/>
#          <match name="description" exp="1[.]2[.]3"/>
#        </pluginItem>
#      </pluginItems>
#      <gfxItems>
#        <gfxItem ... />
#      </gfxItems>
#    </blocklist>
   */

  _loadBlocklistFromFile(file) {
    if (!gBlocklistEnabled) {
      LOG("Blocklist::_loadBlocklistFromFile: blocklist is disabled");
      return;
    }

    let text = "";
    let fstream = null;
    let cstream = null;

    try {
      fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
      cstream = Cc["@mozilla.org/intl/converter-input-stream;1"]
                  .createInstance(Ci.nsIConverterInputStream);

      fstream.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
      cstream.init(fstream, "UTF-8", 0, 0);

      let str = {};
      let read = 0;

      do {
        read = cstream.readString(0xffffffff, str); // read as much as we can and put it in str.value
        text += str.value;
      } while (read != 0);
    } finally {
      // There's no catch block because the callers will catch exceptions,
      // and may try to read another file if reading this file failed.
      if (cstream) {
        try {
          cstream.close();
        } catch (ex) {}
      }
      if (fstream) {
        try {
          fstream.close();
        } catch (ex) {}
      }
    }

    if (text)
      this._loadBlocklistFromString(text);
  },

  /**
   * Whether or not we've finished loading the blocklist.
   */
  get isLoaded() {
    return this._addonEntries != null && this._gfxEntries != null && this._pluginEntries != null;
  },

  /* Used for testing */
  _clear() {
    this._addonEntries = null;
    this._gfxEntries = null;
    this._pluginEntries = null;
    delete this._preloadPromise;
  },

  /**
   * Trigger loading the blocklist content asynchronously.
   */
  async loadBlocklistAsync() {
    if (this.isLoaded) {
      return;
    }
    if (!this._preloadPromise) {
      this._preloadPromise = this._loadBlocklistAsyncInternal();
    }
    await this._preloadPromise;
  },

  async _loadBlocklistAsyncInternal() {
    try {
      // Get the path inside the try...catch because there's no profileDir in e.g. xpcshell tests.
      let profPath = OS.Path.join(OS.Constants.Path.profileDir, FILE_BLOCKLIST);
      await this._preloadBlocklistFile(profPath);
      return;
    } catch (e) {
      LOG("Blocklist::loadBlocklistAsync: Failed to load XML file " + e);
    }

    var appFile = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);
    try {
      await this._preloadBlocklistFile(appFile.path);
      return;
    } catch (e) {
      LOG("Blocklist::loadBlocklistAsync: Failed to load XML file " + e);
    }

    LOG("Blocklist::loadBlocklistAsync: no XML File found");
    // Neither file is present, so we just add empty lists, to avoid JS errors fetching
    // blocklist information otherwise.
    this._addonEntries = [];
    this._gfxEntries = [];
    this._pluginEntries = [];
  },

  async _preloadBlocklistFile(path) {
    if (this.isLoaded) {
      return;
    }

    if (!gBlocklistEnabled) {
      LOG("Blocklist::_preloadBlocklistFile: blocklist is disabled");
      return;
    }

    let text = await OS.File.read(path, { encoding: "utf-8" });

    await new Promise((resolve, reject) => {
      ChromeUtils.idleDispatch(() => {
        if (!this.isLoaded) {
          Services.telemetry.getHistogramById("BLOCKLIST_SYNC_FILE_LOAD").add(false);
          try {
            this._loadBlocklistFromString(text);
          } catch (ex) {
            // Loading the blocklist failed. Ensure the caller knows.
            reject(ex);
          }
        }
        resolve();
      });
    });
  },

  _loadBlocklistFromString(text) {
    var parser = new DOMParser();
    var doc = parser.parseFromString(text, "text/xml");
    if (doc.documentElement.namespaceURI != XMLURI_BLOCKLIST) {
      LOG("Blocklist::_loadBlocklistFromString: aborting due to incorrect " +
          "XML Namespace.\r\nExpected: " + XMLURI_BLOCKLIST + "\r\n" +
          "Received: " + doc.documentElement.namespaceURI);
      throw new Error("Couldn't find an XML doc with the right namespace!");
    }
    this._loadBlocklistFromXML(doc);
  },

  _loadBlocklistFromXML(doc) {
    this._addonEntries = [];
    this._gfxEntries = [];
    this._pluginEntries = [];
    try {
      var childNodes = doc.documentElement.childNodes;
      for (let element of childNodes) {
        if (element.nodeType != doc.ELEMENT_NODE)
          continue;
        switch (element.localName) {
        case "emItems":
          this._addonEntries = this._processItemNodes(element.childNodes, "emItem",
                                                      this._handleEmItemNode);
          break;
        case "pluginItems":
          this._pluginEntries = this._processItemNodes(element.childNodes, "pluginItem",
                                                       this._handlePluginItemNode);
          break;
        case "gfxItems":
          // Parse as simple list of objects.
          this._gfxEntries = this._processItemNodes(element.childNodes, "gfxBlacklistEntry",
                                                    this._handleGfxBlacklistNode);
          break;
        default:
          LOG("Blocklist::_loadBlocklistFromXML: ignored entries " + element.localName);
        }
      }
      if (this._gfxEntries.length > 0) {
        this._notifyObserversBlocklistGFX();
      }
    } catch (e) {
      LOG("Blocklist::_loadBlocklistFromXML: Error constructing blocklist " + e);
    }
    // Dispatch to mainthread because consumers may try to construct nsIPluginHost
    // again based on this notification, while we were called from nsIPluginHost
    // anyway, leading to re-entrancy.
    Services.tm.dispatchToMainThread(function() {
      Services.obs.notifyObservers(null, "blocklist-loaded");
    });
  },

  _processItemNodes(itemNodes, itemName, handler) {
    var result = [];
    for (var i = 0; i < itemNodes.length; ++i) {
      var blocklistElement = itemNodes.item(i);
      if (blocklistElement.nodeType != blocklistElement.ELEMENT_NODE ||
          blocklistElement.localName != itemName)
        continue;

      handler(blocklistElement, result);
    }
    return result;
  },

  _handleEmItemNode(blocklistElement, result) {
    if (!matchesOSABI(blocklistElement))
      return;

    let blockEntry = {
      versions: [],
      prefs: [],
      blockID: null,
      attributes: new Map()
      // Atleast one of EXTENSION_BLOCK_FILTERS must get added to attributes
    };

    // Any filter starting with '/' is interpreted as a regex. So if an attribute
    // starts with a '/' it must be checked via a regex.
    function regExpCheck(attr) {
      return attr.startsWith("/") ? parseRegExp(attr) : attr;
    }

    for (let filter of EXTENSION_BLOCK_FILTERS) {
      let attr = blocklistElement.getAttribute(filter);
      if (attr)
        blockEntry.attributes.set(filter, regExpCheck(attr));
    }

    var childNodes = blocklistElement.childNodes;

    for (let x = 0; x < childNodes.length; x++) {
      var childElement = childNodes.item(x);
      if (childElement.nodeType != childElement.ELEMENT_NODE)
        continue;
      if (childElement.localName === "prefs") {
        let prefElements = childElement.childNodes;
        for (let i = 0; i < prefElements.length; i++) {
          let prefElement = prefElements.item(i);
          if (prefElement.nodeType != prefElement.ELEMENT_NODE ||
              prefElement.localName !== "pref")
            continue;
          blockEntry.prefs.push(prefElement.textContent);
        }
      } else if (childElement.localName === "versionRange")
        blockEntry.versions.push(new BlocklistItemData(childElement));
    }
    // if only the extension ID is specified block all versions of the
    // extension for the current application.
    if (blockEntry.versions.length == 0)
      blockEntry.versions.push(new BlocklistItemData(null));

    blockEntry.blockID = blocklistElement.getAttribute("blockID");

    result.push(blockEntry);
  },

  _handlePluginItemNode(blocklistElement, result) {
    if (!matchesOSABI(blocklistElement))
      return;

    var matchNodes = blocklistElement.childNodes;
    var blockEntry = {
      matches: {},
      versions: [],
      blockID: null,
      infoURL: null,
    };
    var hasMatch = false;
    for (var x = 0; x < matchNodes.length; ++x) {
      var matchElement = matchNodes.item(x);
      if (matchElement.nodeType != matchElement.ELEMENT_NODE)
        continue;
      if (matchElement.localName == "match") {
        var name = matchElement.getAttribute("name");
        var exp = matchElement.getAttribute("exp");
        try {
          blockEntry.matches[name] = new RegExp(exp, "m");
          hasMatch = true;
        } catch (e) {
          // Ignore invalid regular expressions
        }
      }
      if (matchElement.localName == "versionRange") {
        blockEntry.versions.push(new BlocklistItemData(matchElement));
      } else if (matchElement.localName == "infoURL") {
        blockEntry.infoURL = matchElement.textContent;
      }
    }
    // Plugin entries require *something* to match to an actual plugin
    if (!hasMatch)
      return;
    // Add a default versionRange if there wasn't one specified
    if (blockEntry.versions.length == 0)
      blockEntry.versions.push(new BlocklistItemData(null));

    blockEntry.blockID = blocklistElement.getAttribute("blockID");

    result.push(blockEntry);
  },

  // <gfxBlacklistEntry blockID="g60">
  //   <os>WINNT 6.0</os>
  //   <osversion>14</osversion> currently only used for Android
  //   <versionRange minVersion="42.0" maxVersion="13.0b2"/>
  //   <vendor>0x8086</vendor>
  //   <devices>
  //     <device>0x2582</device>
  //     <device>0x2782</device>
  //   </devices>
  //   <feature> DIRECT3D_10_LAYERS </feature>
  //   <featureStatus> BLOCKED_DRIVER_VERSION </featureStatus>
  //   <driverVersion> 8.52.322.2202 </driverVersion>
  //   <driverVersionMax> 8.52.322.2202 </driverVersionMax>
  //   <driverVersionComparator> LESS_THAN_OR_EQUAL </driverVersionComparator>
  //   <model>foo</model>
  //   <product>foo</product>
  //   <manufacturer>foo</manufacturer>
  //   <hardware>foo</hardware>
  // </gfxBlacklistEntry>
  _handleGfxBlacklistNode(blocklistElement, result) {
    const blockEntry = {};

    // The blockID attribute is always present in the actual data produced on server
    // (see https://github.com/mozilla/addons-server/blob/2016.05.05/src/olympia/blocklist/templates/blocklist/blocklist.xml#L74)
    // But it is sometimes missing in test fixtures.
    if (blocklistElement.hasAttribute("blockID")) {
      blockEntry.blockID = blocklistElement.getAttribute("blockID");
    }

    // Trim helper (spaces, tabs, no-break spaces..)
    const trim = (s) => (s || "").replace(/(^[\s\uFEFF\xA0]+)|([\s\uFEFF\xA0]+$)/g, "");

    for (let i = 0; i < blocklistElement.childNodes.length; ++i) {
      var matchElement = blocklistElement.childNodes.item(i);
      if (matchElement.nodeType != matchElement.ELEMENT_NODE)
        continue;

      let value;
      if (matchElement.localName == "devices") {
        value = [];
        for (let j = 0; j < matchElement.childNodes.length; j++) {
          const childElement = matchElement.childNodes.item(j);
          const childValue = trim(childElement.textContent);
          // Make sure no empty value is added.
          if (childValue) {
            if (/,/.test(childValue)) {
              // Devices can't contain comma.
              // (c.f serialization in _notifyObserversBlocklistGFX)
              const e = new Error(`Unsupported device name ${childValue}`);
              Cu.reportError(e);
            } else {
              value.push(childValue);
            }
          }
        }
      } else if (matchElement.localName == "versionRange") {
        value = {minVersion: trim(matchElement.getAttribute("minVersion")) || "0",
                 maxVersion: trim(matchElement.getAttribute("maxVersion")) || "*"};
      } else {
        value = trim(matchElement.textContent);
      }
      if (value) {
        blockEntry[matchElement.localName] = value;
      }
    }
    result.push(blockEntry);
  },

  /* See nsIBlocklistService */
  async getPluginBlocklistState(plugin, appVersion, toolkitVersion) {
    if (AppConstants.platform == "android") {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }
    await this.loadBlocklistAsync();
    return this._getPluginBlocklistState(plugin, this._pluginEntries,
                                         appVersion, toolkitVersion);
  },

  /**
   * Private helper to get the blocklist entry for a plugin given a set of
   * blocklist entries and versions.
   *
   * @param {nsIPluginTag} plugin
   *        The nsIPluginTag to get the blocklist state for.
   * @param {object[]} pluginEntries
   *        The plugin blocklist entries to compare against.
   * @param {string?} appVersion
   *        The application version to compare to, will use the current
   *        version if null.
   * @param {string?} toolkitVersion
   *        The toolkit version to compare to, will use the current version if
   *        null.
   * @returns {object?}
   *        {entry: blocklistEntry, version: blocklistEntryVersion},
   *        or null if there is no matching entry.
   */
  _getPluginBlocklistEntry(plugin, pluginEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled)
      return null;

    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (!appVersion && !gApp.version)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    if (!appVersion)
      appVersion = gApp.version;
    if (!toolkitVersion)
      toolkitVersion = gApp.platformVersion;

    const pluginProperties = {
      description: plugin.description,
      filename: plugin.filename,
      name: plugin.name,
      version: plugin.version,
    };
    for (var blockEntry of pluginEntries) {
      var matchFailed = false;
      for (var name in blockEntry.matches) {
        let pluginProperty = pluginProperties[name];
        if (typeof(pluginProperty) !== "string" ||
            !blockEntry.matches[name].test(pluginProperty)) {
          matchFailed = true;
          break;
        }
      }

      if (matchFailed)
        continue;

      for (let blockEntryVersion of blockEntry.versions) {
        if (blockEntryVersion.includesItem(pluginProperties.version, appVersion,
                                           toolkitVersion)) {
          return {entry: blockEntry, version: blockEntryVersion};
        }
      }
    }

    return null;
  },

  /**
   * Private version of getPluginBlocklistState that allows the caller to pass in
   * the plugin blocklist entries.
   *
   * @param {nsIPluginTag} plugin
   *        The nsIPluginTag to get the blocklist state for.
   * @param {object[]} pluginEntries
   *        The plugin blocklist entries to compare against.
   * @param {string?} appVersion
   *        The application version to compare to, will use the current
   *        version if null.
   * @param {string?} toolkitVersion
   *        The toolkit version to compare to, will use the current version if
   *        null.
   * @returns {integer}
   *        The blocklist state for the item, one of the STATE constants as
   *        defined in nsIBlocklistService.
   */
  _getPluginBlocklistState(plugin, pluginEntries, appVersion, toolkitVersion) {

    let r = this._getPluginBlocklistEntry(plugin, pluginEntries,
                                          appVersion, toolkitVersion);
    if (!r) {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }

    let {version: blockEntryVersion} = r;

    if (blockEntryVersion.severity >= gBlocklistLevel)
      return Ci.nsIBlocklistService.STATE_BLOCKED;
    if (blockEntryVersion.severity == SEVERITY_OUTDATED) {
      let vulnerabilityStatus = blockEntryVersion.vulnerabilityStatus;
      if (vulnerabilityStatus == VULNERABILITYSTATUS_UPDATE_AVAILABLE)
        return Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE;
      if (vulnerabilityStatus == VULNERABILITYSTATUS_NO_UPDATE)
        return Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE;
      return Ci.nsIBlocklistService.STATE_OUTDATED;
    }
    return Ci.nsIBlocklistService.STATE_SOFTBLOCKED;
  },

  async getPluginBlockURL(plugin) {
    await this.loadBlocklistAsync();

    let r = this._getPluginBlocklistEntry(plugin, this._pluginEntries);
    if (!r) {
      return null;
    }
    let blockEntry = r.entry;
    if (!blockEntry.blockID) {
      return null;
    }

    return blockEntry.infoURL || this._createBlocklistURL(blockEntry.blockID);
  },

  _notifyObserversBlocklistGFX() {
    // Notify `GfxInfoBase`, by passing a string serialization.
    // This way we avoid spreading XML structure logics there.
    const payload = this._gfxEntries.map((r) => {
      return Object.keys(r).sort().filter((k) => !/id|last_modified/.test(k)).map((key) => {
        let value = r[key];
        if (Array.isArray(value)) {
          value = value.join(",");
        } else if (value.hasOwnProperty("minVersion")) {
          // When XML is parsed, both minVersion and maxVersion are set.
          value = `${value.minVersion},${value.maxVersion}`;
        }
        return `${key}:${value}`;
      }).join("\t");
    }).join("\n");
    Services.obs.notifyObservers(null, "blocklist-data-gfxItems", payload);
  },

  _notifyObserversBlocklistUpdated() {
    Services.obs.notifyObservers(this, "blocklist-updated");
  },

  async _blocklistUpdated(oldAddonEntries, oldPluginEntries) {
    var addonList = [];

    // A helper function that reverts the prefs passed to default values.
    function resetPrefs(prefs) {
      for (let pref of prefs)
        Services.prefs.clearUserPref(pref);
    }
    const types = ["extension", "theme", "locale", "dictionary", "service"];
    let addons = await AddonManager.getAddonsByTypes(types);
    for (let addon of addons) {
      let oldState = addon.blocklistState;
      if (addon.updateBlocklistState) {
        await addon.updateBlocklistState(false);
      } else if (oldAddonEntries) {
        oldState = this._getAddonBlocklistState(addon, oldAddonEntries);
      } else {
        oldState = Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
      }
      let state = addon.blocklistState;

      LOG("Blocklist state for " + addon.id + " changed from " +
          oldState + " to " + state);

      // We don't want to re-warn about add-ons
      if (state == oldState)
        continue;

      if (state === Ci.nsIBlocklistService.STATE_BLOCKED) {
        // It's a hard block. We must reset certain preferences.
        let prefs = this._getAddonPrefs(addon);
        resetPrefs(prefs);
      }

      // Ensure that softDisabled is false if the add-on is not soft blocked
      if (state != Ci.nsIBlocklistService.STATE_SOFTBLOCKED)
        addon.softDisabled = false;

      // Don't warn about add-ons becoming unblocked.
      if (state == Ci.nsIBlocklistService.STATE_NOT_BLOCKED)
        continue;

      // If an add-on has dropped from hard to soft blocked just mark it as
      // soft disabled and don't warn about it.
      if (state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED &&
          oldState == Ci.nsIBlocklistService.STATE_BLOCKED) {
        addon.softDisabled = true;
        continue;
      }

      // If the add-on is already disabled for some reason then don't warn
      // about it
      if (!addon.isActive) {
        // But mark it as softblocked if necessary. Note that we avoid setting
        // softDisabled at the same time as userDisabled to make it clear
        // which was the original cause of the add-on becoming disabled in a
        // way that the user can change.
        if (state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED && !addon.userDisabled)
          addon.softDisabled = true;
        continue;
      }

      let entry = this._getAddonBlocklistEntry(addon, this._addonEntries);
      addonList.push({
        name: addon.name,
        version: addon.version,
        icon: addon.iconURL,
        disable: false,
        blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
        item: addon,
        url: entry && entry.url,
      });
    }

    AddonManagerPrivate.updateAddonAppDisabledStates();

    var phs = Cc["@mozilla.org/plugin/host;1"].
              getService(Ci.nsIPluginHost);
    var plugins = phs.getPluginTags();

    for (let plugin of plugins) {
      let oldState = -1;
      if (oldPluginEntries)
        oldState = this._getPluginBlocklistState(plugin, oldPluginEntries);
      let state = this._getPluginBlocklistState(plugin, this._pluginEntries);
      LOG("Blocklist state for " + plugin.name + " changed from " +
          oldState + " to " + state);
      // We don't want to re-warn about items
      if (state == oldState)
        continue;

      if (oldState == Ci.nsIBlocklistService.STATE_BLOCKED) {
        if (state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED)
          plugin.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
      } else if (!plugin.disabled && state != Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
        if (state != Ci.nsIBlocklistService.STATE_OUTDATED &&
            state != Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE &&
            state != Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE) {
          addonList.push({
            name: plugin.name,
            version: plugin.version,
            icon: "chrome://mozapps/skin/plugins/pluginGeneric.svg",
            disable: false,
            blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
            item: plugin,
            url: await this.getPluginBlockURL(plugin),
          });
        }
      }
    }

    if (addonList.length == 0) {
      this._notifyObserversBlocklistUpdated();
      return;
    }

    if ("@mozilla.org/addons/blocklist-prompt;1" in Cc) {
      try {
        let blockedPrompter = Cc["@mozilla.org/addons/blocklist-prompt;1"]
                               .getService().wrappedJSObject;
        blockedPrompter.prompt(addonList);
      } catch (e) {
        LOG(e);
      }
      this._notifyObserversBlocklistUpdated();
      return;
    }

    var args = {
      restart: false,
      list: addonList
    };
    // This lets the dialog get the raw js object
    args.wrappedJSObject = args;

    /*
      Some tests run without UI, so the async code listens to a message
      that can be sent programatically
    */
    let applyBlocklistChanges = () => {
      for (let addon of addonList) {
        if (!addon.disable)
          continue;

        if (addon.item instanceof Ci.nsIPluginTag)
          addon.item.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
        else {
          // This add-on is softblocked.
          addon.item.softDisabled = true;
          // We must revert certain prefs.
          let prefs = this._getAddonPrefs(addon.item);
          resetPrefs(prefs);
        }
      }

      if (args.restart)
        restartApp();

      this._notifyObserversBlocklistUpdated();
      Services.obs.removeObserver(applyBlocklistChanges, "addon-blocklist-closed");
    };

    Services.obs.addObserver(applyBlocklistChanges, "addon-blocklist-closed");

    if (Services.prefs.getBoolPref(PREF_BLOCKLIST_SUPPRESSUI, false)) {
      applyBlocklistChanges();
      return;
    }

    function blocklistUnloadHandler(event) {
      if (event.target.location == URI_BLOCKLIST_DIALOG) {
        applyBlocklistChanges();
        blocklistWindow.removeEventListener("unload", blocklistUnloadHandler);
      }
    }

    let blocklistWindow = Services.ww.openWindow(null, URI_BLOCKLIST_DIALOG, "",
                            "chrome,centerscreen,dialog,titlebar", args);
    if (blocklistWindow)
      blocklistWindow.addEventListener("unload", blocklistUnloadHandler);
  },
};

/*
 * Helper for constructing a blocklist.
 */
function BlocklistItemData(versionRangeElement) {
  var versionRange = this.getBlocklistVersionRange(versionRangeElement);
  this.minVersion = versionRange.minVersion;
  this.maxVersion = versionRange.maxVersion;
  if (versionRangeElement && versionRangeElement.hasAttribute("severity"))
    this.severity = versionRangeElement.getAttribute("severity");
  else
    this.severity = DEFAULT_SEVERITY;
  if (versionRangeElement && versionRangeElement.hasAttribute("vulnerabilitystatus")) {
    this.vulnerabilityStatus = versionRangeElement.getAttribute("vulnerabilitystatus");
  } else {
    this.vulnerabilityStatus = VULNERABILITYSTATUS_NONE;
  }
  this.targetApps = { };
  var found = false;

  if (versionRangeElement) {
    for (var i = 0; i < versionRangeElement.childNodes.length; ++i) {
      var targetAppElement = versionRangeElement.childNodes.item(i);
      if (targetAppElement.nodeType != targetAppElement.ELEMENT_NODE ||
          targetAppElement.localName != "targetApplication")
        continue;
      found = true;
      // default to the current application if id is not provided.
      var appID = targetAppElement.hasAttribute("id") ? targetAppElement.getAttribute("id") : gApp.ID;
      this.targetApps[appID] = this.getBlocklistAppVersions(targetAppElement);
    }
  }
  // Default to all versions of the current application when no targetApplication
  // elements were found
  if (!found)
    this.targetApps[gApp.ID] = this.getBlocklistAppVersions(null);
}

BlocklistItemData.prototype = {
  /**
   * Tests if a version of an item is included in the version range and target
   * application information represented by this BlocklistItemData using the
   * provided application and toolkit versions.
   * @param {string} version
   *        The version of the item being tested.
   * @param {string} appVersion
   *        The application version to test with.
   * @param {string} toolkitVersion
   *        The toolkit version to test with.
   * @returns {boolean}
   *        True if the version range covers the item version and application
   *        or toolkit version.
   */
  includesItem(version, appVersion, toolkitVersion) {
    // Some platforms have no version for plugins, these don't match if there
    // was a min/maxVersion provided
    if (!version && (this.minVersion || this.maxVersion))
      return false;

    // Check if the item version matches
    if (!this.matchesRange(version, this.minVersion, this.maxVersion))
      return false;

    // Check if the application version matches
    if (this.matchesTargetRange(gApp.ID, appVersion))
      return true;

    // Check if the toolkit version matches
    return this.matchesTargetRange(TOOLKIT_ID, toolkitVersion);
  },

  /**
   * Checks if a version is higher than or equal to the minVersion (if provided)
   * and lower than or equal to the maxVersion (if provided).
   * @param {string} version
   *        The version to test.
   * @param {string?} minVersion
   *        The minimum version. If null it is assumed that version is always
   *        larger.
   * @param {string?} maxVersion
   *        The maximum version. If null it is assumed that version is always
   *        smaller.
   * @returns {boolean}
   *        Whether the item matches the range.
   */
  matchesRange(version, minVersion, maxVersion) {
    if (minVersion && Services.vc.compare(version, minVersion) < 0)
      return false;
    if (maxVersion && Services.vc.compare(version, maxVersion) > 0)
      return false;
    return true;
  },

  /**
   * Tests if there is a matching range for the given target application id and
   * version.
   * @param {string} appID
   *        The application ID to test for, may be for an application or toolkit
   * @param {string} appVersion
   *        The version of the application to test for.
   * @returns {boolean}
   *        True if this version range covers the application version given.
   */
  matchesTargetRange(appID, appVersion) {
    var blTargetApp = this.targetApps[appID];
    if (!blTargetApp)
      return false;

    for (let app of blTargetApp) {
      if (this.matchesRange(appVersion, app.minVersion, app.maxVersion))
        return true;
    }

    return false;
  },

  /**
   * Retrieves a version range (e.g. minVersion and maxVersion) for a
   * blocklist item's targetApplication element.
   * @param {Element} targetAppElement
   *        A targetApplication blocklist element.
   * @returns {object[]}
   *        An array of JS objects with the following properties:
   *          "minVersion"  The minimum version in a version range (default = null).
   *          "maxVersion"  The maximum version in a version range (default = null).
   */
  getBlocklistAppVersions(targetAppElement) {
    var appVersions = [ ];

    if (targetAppElement) {
      for (var i = 0; i < targetAppElement.childNodes.length; ++i) {
        var versionRangeElement = targetAppElement.childNodes.item(i);
        if (versionRangeElement.nodeType != versionRangeElement.ELEMENT_NODE ||
            versionRangeElement.localName != "versionRange")
          continue;
        appVersions.push(this.getBlocklistVersionRange(versionRangeElement));
      }
    }
    // return minVersion = null and maxVersion = null if no specific versionRange
    // elements were found
    if (appVersions.length == 0)
      appVersions.push(this.getBlocklistVersionRange(null));
    return appVersions;
  },

  /**
   * Retrieves a version range (e.g. minVersion and maxVersion) for a blocklist
   * versionRange element.
   *
   * @param {Element} versionRangeElement
   *        The versionRange blocklist element.
   *
   * @returns {Object}
   *        A JS object with the following properties:
   *          "minVersion"  The minimum version in a version range (default = null).
   *          "maxVersion"  The maximum version in a version range (default = null).
   */
  getBlocklistVersionRange(versionRangeElement) {
    var minVersion = null;
    var maxVersion = null;
    if (!versionRangeElement)
      return { minVersion, maxVersion };

    if (versionRangeElement.hasAttribute("minVersion"))
      minVersion = versionRangeElement.getAttribute("minVersion");
    if (versionRangeElement.hasAttribute("maxVersion"))
      maxVersion = versionRangeElement.getAttribute("maxVersion");

    return { minVersion, maxVersion };
  }
};

Blocklist._init();
