/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AppConstants.jsm");

try {
  // AddonManager.jsm doesn't allow itself to be imported in the child
  // process. We're used in the child process (for now), so guard against
  // this.
  Components.utils.import("resource://gre/modules/AddonManager.jsm");
  /* globals AddonManagerPrivate*/
} catch (e) {
}

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ServiceRequest",
                                  "resource://gre/modules/ServiceRequest.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
// The blocklist updater is the new system in charge of fetching remote data
// securely and efficiently. It will replace the current XML-based system.
// See Bug 1257565 and Bug 1252456.
const BlocklistUpdater = {};
XPCOMUtils.defineLazyModuleGetter(BlocklistUpdater, "checkVersions",
                                  "resource://services-common/blocklist-updater.js");

const TOOLKIT_ID                      = "toolkit@mozilla.org";
const KEY_PROFILEDIR                  = "ProfD";
const KEY_APPDIR                      = "XCurProcD";
const FILE_BLOCKLIST                  = "blocklist.xml";
const PREF_BLOCKLIST_LASTUPDATETIME   = "app.update.lastUpdateTime.blocklist-background-update-timer";
const PREF_BLOCKLIST_URL              = "extensions.blocklist.url";
const PREF_BLOCKLIST_ITEM_URL         = "extensions.blocklist.itemURL";
const PREF_BLOCKLIST_ENABLED          = "extensions.blocklist.enabled";
const PREF_BLOCKLIST_LEVEL            = "extensions.blocklist.level";
const PREF_BLOCKLIST_PINGCOUNTTOTAL   = "extensions.blocklist.pingCountTotal";
const PREF_BLOCKLIST_PINGCOUNTVERSION = "extensions.blocklist.pingCountVersion";
const PREF_BLOCKLIST_SUPPRESSUI       = "extensions.blocklist.suppressUI";
const PREF_ONECRL_VIA_AMO             = "security.onecrl.via.amo";
const PREF_BLOCKLIST_UPDATE_ENABLED   = "services.blocklist.update_enabled";
const PREF_APP_DISTRIBUTION           = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION   = "distribution.version";
const PREF_EM_LOGGING_ENABLED         = "extensions.logging.enabled";
const XMLURI_BLOCKLIST                = "http://www.mozilla.org/2006/addons-blocklist";
const XMLURI_PARSE_ERROR              = "http://www.mozilla.org/newlayout/xml/parsererror.xml"
const URI_BLOCKLIST_DIALOG            = "chrome://mozapps/content/extensions/blocklist.xul"
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

XPCOMUtils.defineLazyServiceGetter(this, "gConsole",
                                   "@mozilla.org/consoleservice;1",
                                   "nsIConsoleService");

XPCOMUtils.defineLazyServiceGetter(this, "gVersionChecker",
                                   "@mozilla.org/xpcom/version-comparator;1",
                                   "nsIVersionComparator");

XPCOMUtils.defineLazyServiceGetter(this, "gCertBlocklistService",
                                   "@mozilla.org/security/certblocklist;1",
                                   "nsICertBlocklist");

XPCOMUtils.defineLazyGetter(this, "gPref", function() {
  return Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService).
         QueryInterface(Ci.nsIPrefBranch);
});

// From appinfo in Services.jsm. It is not possible to use the one in
// Services.jsm since it will not successfully QueryInterface nsIXULAppInfo in
// xpcshell tests due to other code calling Services.appinfo before the
// nsIXULAppInfo is created by the tests.
XPCOMUtils.defineLazyGetter(this, "gApp", function() {
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
  let sysInfo = Cc["@mozilla.org/system-info;1"].
                getService(Ci.nsIPropertyBag2);
  try {
    osVersion = sysInfo.getProperty("name") + " " + sysInfo.getProperty("version");
  } catch (e) {
    LOG("BlockList Global gOSVersion: OS Version unknown.");
  }

  if (osVersion) {
    try {
      osVersion += " (" + sysInfo.getProperty("secondaryLibrary") + ")";
    } catch (e) {
      // Not all platforms have a secondary widget library, so an error is nothing to worry about.
    }
    osVersion = encodeURIComponent(osVersion);
  }
  return osVersion;
});

// shared code for suppressing bad cert dialogs
XPCOMUtils.defineLazyGetter(this, "gCertUtils", function() {
  let temp = { };
  Components.utils.import("resource://gre/modules/CertUtils.jsm", temp);
  return temp;
});

/**
 * Logs a string to the error console.
 * @param   string
 *          The string to write to the error console..
 */
function LOG(string) {
  if (gLoggingEnabled) {
    dump("*** " + string + "\n");
    gConsole.logStringMessage(string);
  }
}

/**
 * Gets a preference value, handling the case where there is no default.
 * @param   func
 *          The name of the preference function to call, on nsIPrefBranch
 * @param   preference
 *          The name of the preference
 * @param   defaultValue
 *          The default value to return in the event the preference has
 *          no setting
 * @returns The value of the preference, or undefined if there was no
 *          user or default value.
 */
function getPref(func, preference, defaultValue) {
  try {
    return gPref[func](preference);
  } catch (e) {
  }
  return defaultValue;
}

/**
 * Constructs a URI to a spec.
 * @param   spec
 *          The spec to construct a URI to
 * @returns The nsIURI constructed.
 */
function newURI(spec) {
  var ioServ = Cc["@mozilla.org/network/io-service;1"].
               getService(Ci.nsIIOService);
  return ioServ.newURI(spec);
}

// Restarts the application checking in with observers first
function restartApp() {
  // Notify all windows that an application quit has been requested.
  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                   createInstance(Ci.nsISupportsPRBool);
  os.notifyObservers(cancelQuit, "quit-application-requested");

  // Something aborted the quit process.
  if (cancelQuit.data)
    return;

  var as = Cc["@mozilla.org/toolkit/app-startup;1"].
           getService(Ci.nsIAppStartup);
  as.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
}

/**
 * Checks whether this blocklist element is valid for the current OS and ABI.
 * If the element has an "os" attribute then the current OS must appear in
 * its comma separated list for the element to be valid. Similarly for the
 * xpcomabi attribute.
 */
function matchesOSABI(blocklistElement) {
  if (blocklistElement.hasAttribute("os")) {
    var choices = blocklistElement.getAttribute("os").split(",");
    if (choices.length > 0 && choices.indexOf(gApp.OS) < 0)
      return false;
  }

  if (blocklistElement.hasAttribute("xpcomabi")) {
    choices = blocklistElement.getAttribute("xpcomabi").split(",");
    if (choices.length > 0 && choices.indexOf(gApp.XPCOMABI) < 0)
      return false;
  }

  return true;
}

/**
 * Gets the current value of the locale.  It's possible for this preference to
 * be localized, so we have to do a little extra work here.  Similar code
 * exists in nsHttpHandler.cpp when building the UA string.
 */
function getLocale() {
  return Services.locale.getRequestedLocales();
}

/* Get the distribution pref values, from defaults only */
function getDistributionPrefValue(aPrefName) {
  return gPref.getDefaultBranch(null).getCharPref(aPrefName, "default");
}

/**
 * Parse a string representation of a regular expression. Needed because we
 * use the /pattern/flags form (because it's detectable), which is only
 * supported as a literal in JS.
 *
 * @param  aStr
 *         String representation of regexp
 * @return RegExp instance
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

function Blocklist() {
  Services.obs.addObserver(this, "xpcom-shutdown");
  Services.obs.addObserver(this, "sessionstore-windows-restored");
  gLoggingEnabled = getPref("getBoolPref", PREF_EM_LOGGING_ENABLED, false);
  gBlocklistEnabled = getPref("getBoolPref", PREF_BLOCKLIST_ENABLED, true);
  gBlocklistLevel = Math.min(getPref("getIntPref", PREF_BLOCKLIST_LEVEL, DEFAULT_LEVEL),
                                     MAX_BLOCK_LEVEL);
  gPref.addObserver("extensions.blocklist.", this);
  gPref.addObserver(PREF_EM_LOGGING_ENABLED, this);
  this.wrappedJSObject = this;
  // requests from child processes come in here, see receiveMessage.
  Services.ppmm.addMessageListener("Blocklist:getPluginBlocklistState", this);
  Services.ppmm.addMessageListener("Blocklist:content-blocklist-updated", this);
}

Blocklist.prototype = {
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
    Services.ppmm.removeMessageListener("Blocklist:getPluginBlocklistState", this);
    Services.ppmm.removeMessageListener("Blocklist:content-blocklist-updated", this);
    gPref.removeObserver("extensions.blocklist.", this);
    gPref.removeObserver(PREF_EM_LOGGING_ENABLED, this);
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
    case "xpcom-shutdown":
      this.shutdown();
      break;
    case "nsPref:changed":
      switch (aData) {
        case PREF_EM_LOGGING_ENABLED:
          gLoggingEnabled = getPref("getBoolPref", PREF_EM_LOGGING_ENABLED, false);
          break;
        case PREF_BLOCKLIST_ENABLED:
          gBlocklistEnabled = getPref("getBoolPref", PREF_BLOCKLIST_ENABLED, true);
          this._loadBlocklist();
          this._blocklistUpdated(null, null);
          break;
        case PREF_BLOCKLIST_LEVEL:
          gBlocklistLevel = Math.min(getPref("getIntPref", PREF_BLOCKLIST_LEVEL, DEFAULT_LEVEL),
                                     MAX_BLOCK_LEVEL);
          this._blocklistUpdated(null, null);
          break;
      }
      break;
    case "sessionstore-windows-restored":
      Services.obs.removeObserver(this, "sessionstore-windows-restored");
      this._preloadBlocklist();
      break;
    }
  },

  // Message manager message handlers
  receiveMessage(aMsg) {
    switch (aMsg.name) {
      case "Blocklist:getPluginBlocklistState":
        return this.getPluginBlocklistState(aMsg.data.addonData,
                                            aMsg.data.appVersion,
                                            aMsg.data.toolkitVersion);
      case "Blocklist:content-blocklist-updated":
        Services.obs.notifyObservers(null, "content-blocklist-updated");
        break;
      default:
        throw new Error("Unknown blocklist message received from content: " + aMsg.name);
    }
    return undefined;
  },

  /* See nsIBlocklistService */
  isAddonBlocklisted(addon, appVersion, toolkitVersion) {
    return this.getAddonBlocklistState(addon, appVersion, toolkitVersion) ==
                   Ci.nsIBlocklistService.STATE_BLOCKED;
  },

  /* See nsIBlocklistService */
  getAddonBlocklistState(addon, appVersion, toolkitVersion) {
    if (!this._isBlocklistLoaded())
      this._loadBlocklist();
    return this._getAddonBlocklistState(addon, this._addonEntries,
                                        appVersion, toolkitVersion);
  },

  /**
   * Private version of getAddonBlocklistState that allows the caller to pass in
   * the add-on blocklist entries to compare against.
   *
   * @param   id
   *          The ID of the item to get the blocklist state for.
   * @param   version
   *          The version of the item to get the blocklist state for.
   * @param   addonEntries
   *          The add-on blocklist entries to compare against.
   * @param   appVersion
   *          The application version to compare to, will use the current
   *          version if null.
   * @param   toolkitVersion
   *          The toolkit version to compare to, will use the current version if
   *          null.
   * @returns The blocklist state for the item, one of the STATE constants as
   *          defined in nsIBlocklistService.
   */
  _getAddonBlocklistState(addon, addonEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (!appVersion && !gApp.version)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    if (!appVersion)
      appVersion = gApp.version;
    if (!toolkitVersion)
      toolkitVersion = gApp.platformVersion;

    var blItem = this._findMatchingAddonEntry(addonEntries, addon);
    if (!blItem)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    for (let currentblItem of blItem.versions) {
      if (currentblItem.includesItem(addon.version, appVersion, toolkitVersion))
        return currentblItem.severity >= gBlocklistLevel ? Ci.nsIBlocklistService.STATE_BLOCKED :
                                                       Ci.nsIBlocklistService.STATE_SOFTBLOCKED;
    }
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  /**
   * Returns the set of prefs of the add-on stored in the blocklist file
   * (probably to revert them on disabling).
   * @param addon
   *        The add-on whose to-be-reset prefs are to be found.
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

  /* See nsIBlocklistService */
  getAddonBlocklistURL(addon, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled)
      return "";

    if (!this._isBlocklistLoaded())
      this._loadBlocklist();

    let blItem = this._findMatchingAddonEntry(this._addonEntries, addon);
    if (!blItem || !blItem.blockID)
      return null;

    return this._createBlocklistURL(blItem.blockID);
  },

  _createBlocklistURL(id) {
    let url = Services.urlFormatter.formatURLPref(PREF_BLOCKLIST_ITEM_URL);
    url = url.replace(/%blockID%/g, id);

    return url;
  },

  notify(aTimer) {
    if (!gBlocklistEnabled)
      return;

    try {
      var dsURI = gPref.getCharPref(PREF_BLOCKLIST_URL);
    } catch (e) {
      LOG("Blocklist::notify: The " + PREF_BLOCKLIST_URL + " preference" +
          " is missing!");
      return;
    }

    var pingCountVersion = getPref("getIntPref", PREF_BLOCKLIST_PINGCOUNTVERSION, 0);
    var pingCountTotal = getPref("getIntPref", PREF_BLOCKLIST_PINGCOUNTTOTAL, 1);
    var daysSinceLastPing = 0;
    if (pingCountVersion == 0) {
      daysSinceLastPing = "new";
    } else {
      // Seconds in one day is used because nsIUpdateTimerManager stores the
      // last update time in seconds.
      let secondsInDay = 60 * 60 * 24;
      let lastUpdateTime = getPref("getIntPref", PREF_BLOCKLIST_LASTUPDATETIME, 0);
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
      gPref.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION, pingCountVersion);
    }

    if (pingCountTotal != "invalid") {
      pingCountTotal++;
      if (pingCountTotal > 2147483647) {
        // Rollover to 1 if the value is greater than what is support by an
        // integer preference.
        pingCountTotal = -1;
      }
      gPref.setIntPref(PREF_BLOCKLIST_PINGCOUNTTOTAL, pingCountTotal);
    }

    // Verify that the URI is valid
    try {
      var uri = newURI(dsURI);
    } catch (e) {
      LOG("Blocklist::notify: There was an error creating the blocklist URI\r\n" +
          "for: " + dsURI + ", error: " + e);
      return;
    }

    LOG("Blocklist::notify: Requesting " + uri.spec);
    let request = new ServiceRequest();
    request.open("GET", uri.spec, true);
    request.channel.notificationCallbacks = new gCertUtils.BadCertHandler();
    request.overrideMimeType("text/xml");
    request.setRequestHeader("Cache-Control", "no-cache");
    request.QueryInterface(Components.interfaces.nsIJSXMLHttpRequest);

    request.addEventListener("error", event => this.onXMLError(event));
    request.addEventListener("load", event => this.onXMLLoad(event));
    request.send(null);

    // When the blocklist loads we need to compare it to the current copy so
    // make sure we have loaded it.
    if (!this._isBlocklistLoaded())
      this._loadBlocklist();

    // If blocklist update via Kinto is enabled, poll for changes and sync.
    // Currently certificates blocklist relies on it by default.
    if (gPref.getBoolPref(PREF_BLOCKLIST_UPDATE_ENABLED)) {
      BlocklistUpdater.checkVersions().catch(() => {
        // Bug 1254099 - Telemetry (success or errors) will be collected during this process.
      });
    }
  },

  onXMLLoad: Task.async(function*(aEvent) {
    let request = aEvent.target;
    try {
      gCertUtils.checkCert(request.channel);
    } catch (e) {
      LOG("Blocklist::onXMLLoad: " + e);
      return;
    }
    let responseXML = request.responseXML;
    if (!responseXML || responseXML.documentElement.namespaceURI == XMLURI_PARSE_ERROR ||
        (request.status != 200 && request.status != 0)) {
      LOG("Blocklist::onXMLLoad: there was an error during load");
      return;
    }

    var oldAddonEntries = this._addonEntries;
    var oldPluginEntries = this._pluginEntries;
    this._addonEntries = [];
    this._gfxEntries = [];
    this._pluginEntries = [];

    this._loadBlocklistFromString(request.responseText);
    // We don't inform the users when the graphics blocklist changed at runtime.
    // However addons and plugins blocking status is refreshed.
    this._blocklistUpdated(oldAddonEntries, oldPluginEntries);

    try {
      let path = OS.Path.join(OS.Constants.Path.profileDir, FILE_BLOCKLIST);
      yield OS.File.writeAtomic(path, request.responseText, {tmpPath: path + ".tmp"});
    } catch (e) {
      LOG("Blocklist::onXMLLoad: " + e);
    }
  }),

  onXMLError(aEvent) {
    try {
      var request = aEvent.target;
      // the following may throw (e.g. a local file or timeout)
      var status = request.status;
    } catch (e) {
      request = aEvent.target.channel.QueryInterface(Ci.nsIRequest);
      status = request.status;
    }
    var statusText = "nsIXMLHttpRequest channel unavailable";
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
    var profFile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);
    if (profFile.exists()) {
      this._loadBlocklistFromFile(profFile);
      return;
    }
    var appFile = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);
    if (appFile.exists()) {
      this._loadBlocklistFromFile(appFile);
      return;
    }
    LOG("Blocklist::_loadBlocklist: no XML File found");
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
#      <certItems>
#        <!-- issuerName is the DER issuer name data base64 encoded... -->
#        <certItem issuerName="MA0xCzAJBgNVBAMMAmNh">
#          <!-- ... as is the serial number DER data -->
#          <serialNumber>AkHVNA==</serialNumber>
#        </certItem>
#        <!-- subject is the DER subject name data base64 encoded... -->
#        <certItem subject="MA0xCzAJBgNVBAMMAmNh" pubKeyHash="/xeHA5s+i9/z9d8qy6JEuE1xGoRYIwgJuTE/lmaGJ7M=">
#        </certItem>
#      </certItems>
#    </blocklist>
   */

  _loadBlocklistFromFile(file) {
    if (!gBlocklistEnabled) {
      LOG("Blocklist::_loadBlocklistFromFile: blocklist is disabled");
      return;
    }

    let telemetry = Services.telemetry;

    if (this._isBlocklistPreloaded()) {
      telemetry.getHistogramById("BLOCKLIST_SYNC_FILE_LOAD").add(false);
      this._loadBlocklistFromString(this._preloadedBlocklistContent);
      delete this._preloadedBlocklistContent;
      return;
    }

    if (!file.exists()) {
      LOG("Blocklist::_loadBlocklistFromFile: XML File does not exist " + file.path);
      return;
    }

    telemetry.getHistogramById("BLOCKLIST_SYNC_FILE_LOAD").add(true);

    let text = "";
    let fstream = null;
    let cstream = null;

    try {
      fstream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                          .createInstance(Components.interfaces.nsIFileInputStream);
      cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"]
                          .createInstance(Components.interfaces.nsIConverterInputStream);

      fstream.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
      cstream.init(fstream, "UTF-8", 0, 0);

      let str = {};
      let read = 0;

      do {
        read = cstream.readString(0xffffffff, str); // read as much as we can and put it in str.value
        text += str.value;
      } while (read != 0);
    } catch (e) {
      LOG("Blocklist::_loadBlocklistFromFile: Failed to load XML file " + e);
    } finally {
      if (cstream)
        cstream.close();
      if (fstream)
        fstream.close();
    }

    if (text)
        this._loadBlocklistFromString(text);
  },

  _isBlocklistLoaded() {
    return this._addonEntries != null && this._gfxEntries != null && this._pluginEntries != null;
  },

  _isBlocklistPreloaded() {
    return this._preloadedBlocklistContent != null;
  },

  /* Used for testing */
  _clear() {
    this._addonEntries = null;
    this._gfxEntries = null;
    this._pluginEntries = null;
    this._preloadedBlocklistContent = null;
  },

  _preloadBlocklist: Task.async(function*() {
    let profPath = OS.Path.join(OS.Constants.Path.profileDir, FILE_BLOCKLIST);
    try {
      yield this._preloadBlocklistFile(profPath);
      return;
    } catch (e) {
      LOG("Blocklist::_preloadBlocklist: Failed to load XML file " + e)
    }

    var appFile = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);
    try {
      yield this._preloadBlocklistFile(appFile.path);
      return;
    } catch (e) {
      LOG("Blocklist::_preloadBlocklist: Failed to load XML file " + e)
    }

    LOG("Blocklist::_preloadBlocklist: no XML File found");
  }),

  _preloadBlocklistFile: Task.async(function*(path) {
    if (this._addonEntries) {
      // The file has been already loaded.
      return;
    }

    if (!gBlocklistEnabled) {
      LOG("Blocklist::_preloadBlocklistFile: blocklist is disabled");
      return;
    }

    let text = yield OS.File.read(path, { encoding: "utf-8" });

    if (!this._addonEntries) {
      // Store the content only if a sync load has not been performed in the meantime.
      this._preloadedBlocklistContent = text;
    }
  }),

  _loadBlocklistFromString(text) {
    try {
      var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                   createInstance(Ci.nsIDOMParser);
      var doc = parser.parseFromString(text, "text/xml");
      if (doc.documentElement.namespaceURI != XMLURI_BLOCKLIST) {
        LOG("Blocklist::_loadBlocklistFromFile: aborting due to incorrect " +
            "XML Namespace.\r\nExpected: " + XMLURI_BLOCKLIST + "\r\n" +
            "Received: " + doc.documentElement.namespaceURI);
        return;
      }

      var populateCertBlocklist = getPref("getBoolPref", PREF_ONECRL_VIA_AMO, true);

      var childNodes = doc.documentElement.childNodes;
      for (let element of childNodes) {
        if (!(element instanceof Ci.nsIDOMElement))
          continue;
        switch (element.localName) {
        case "emItems":
          this._addonEntries = this._processItemNodes(element.childNodes, "emItem",
                                                      this._handleEmItemNode);
          break;
        case "pluginItems":
          // We don't support plugins on b2g.
          if (AppConstants.MOZ_B2G) {
            return;
          }
          this._pluginEntries = this._processItemNodes(element.childNodes, "pluginItem",
                                                       this._handlePluginItemNode);
          break;
        case "certItems":
          if (populateCertBlocklist) {
            this._processItemNodes(element.childNodes, "certItem",
                                   this._handleCertItemNode.bind(this));
          }
          break;
        case "gfxItems":
          // Parse as simple list of objects.
          this._gfxEntries = this._processItemNodes(element.childNodes, "gfxBlacklistEntry",
                                                    this._handleGfxBlacklistNode);
          break;
        default:
          LOG("Blocklist::_loadBlocklistFromString: ignored entries " + element.localName);
        }
      }
      if (populateCertBlocklist) {
        gCertBlocklistService.saveEntries();
      }
      if (this._gfxEntries.length > 0) {
        this._notifyObserversBlocklistGFX();
      }
    } catch (e) {
      LOG("Blocklist::_loadBlocklistFromFile: Error constructing blocklist " + e);
    }
  },

  _processItemNodes(itemNodes, itemName, handler) {
    var result = [];
    for (var i = 0; i < itemNodes.length; ++i) {
      var blocklistElement = itemNodes.item(i);
      if (!(blocklistElement instanceof Ci.nsIDOMElement) ||
          blocklistElement.localName != itemName)
        continue;

      handler(blocklistElement, result);
    }
    return result;
  },

  _handleCertItemNode(blocklistElement, result) {
    let issuer = blocklistElement.getAttribute("issuerName");
    if (issuer) {
      for (let snElement of blocklistElement.children) {
        try {
          gCertBlocklistService.revokeCertByIssuerAndSerial(issuer, snElement.textContent);
        } catch (e) {
          // we want to keep trying other elements since missing all items
          // is worse than missing one
          LOG("Blocklist::_handleCertItemNode: Error adding revoked cert by Issuer and Serial" + e);
        }
      }
      return;
    }

    let pubKeyHash = blocklistElement.getAttribute("pubKeyHash");
    let subject = blocklistElement.getAttribute("subject");

    if (pubKeyHash && subject) {
      try {
        gCertBlocklistService.revokeCertBySubjectAndPubKey(subject, pubKeyHash);
      } catch (e) {
        LOG("Blocklist::_handleCertItemNode: Error adding revoked cert by Subject and PubKey" + e);
      }
    }
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
      if (!(childElement instanceof Ci.nsIDOMElement))
        continue;
      if (childElement.localName === "prefs") {
        let prefElements = childElement.childNodes;
        for (let i = 0; i < prefElements.length; i++) {
          let prefElement = prefElements.item(i);
          if (!(prefElement instanceof Ci.nsIDOMElement) ||
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
      if (!(matchElement instanceof Ci.nsIDOMElement))
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
      if (!(matchElement instanceof Ci.nsIDOMElement))
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
              Components.utils.reportError(e);
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
  getPluginBlocklistState(plugin, appVersion, toolkitVersion) {
    if (AppConstants.platform == "android" ||
        AppConstants.MOZ_B2G) {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }
    if (!this._isBlocklistLoaded())
      this._loadBlocklist();
    return this._getPluginBlocklistState(plugin, this._pluginEntries,
                                         appVersion, toolkitVersion);
  },

  /**
   * Private helper to get the blocklist entry for a plugin given a set of
   * blocklist entries and versions.
   *
   * @param   plugin
   *          The nsIPluginTag to get the blocklist state for.
   * @param   pluginEntries
   *          The plugin blocklist entries to compare against.
   * @param   appVersion
   *          The application version to compare to, will use the current
   *          version if null.
   * @param   toolkitVersion
   *          The toolkit version to compare to, will use the current version if
   *          null.
   * @returns {entry: blocklistEntry, version: blocklistEntryVersion},
   *          or null if there is no matching entry.
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

    for (var blockEntry of pluginEntries) {
      var matchFailed = false;
      for (var name in blockEntry.matches) {
        if (!(name in plugin) ||
            typeof(plugin[name]) != "string" ||
            !blockEntry.matches[name].test(plugin[name])) {
          matchFailed = true;
          break;
        }
      }

      if (matchFailed)
        continue;

      for (let blockEntryVersion of blockEntry.versions) {
        if (blockEntryVersion.includesItem(plugin.version, appVersion,
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
   * @param   plugin
   *          The nsIPluginTag to get the blocklist state for.
   * @param   pluginEntries
   *          The plugin blocklist entries to compare against.
   * @param   appVersion
   *          The application version to compare to, will use the current
   *          version if null.
   * @param   toolkitVersion
   *          The toolkit version to compare to, will use the current version if
   *          null.
   * @returns The blocklist state for the item, one of the STATE constants as
   *          defined in nsIBlocklistService.
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

  /* See nsIBlocklistService */
  getPluginBlocklistURL(plugin) {
    if (!this._isBlocklistLoaded())
      this._loadBlocklist();

    let r = this._getPluginBlocklistEntry(plugin, this._pluginEntries);
    if (!r) {
      return null;
    }
    let {entry: blockEntry} = r;
    if (!blockEntry.blockID) {
      return null;
    }

    return this._createBlocklistURL(blockEntry.blockID);
  },

  /* See nsIBlocklistService */
  getPluginInfoURL(plugin) {
    if (!this._isBlocklistLoaded())
      this._loadBlocklist();

    let r = this._getPluginBlocklistEntry(plugin, this._pluginEntries);
    if (!r) {
      return null;
    }
    let {entry: blockEntry} = r;
    if (!blockEntry.blockID) {
      return null;
    }

    return blockEntry.infoURL;
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
    Services.ppmm.broadcastAsyncMessage("Blocklist:blocklistInvalidated", {});
  },

  _blocklistUpdated(oldAddonEntries, oldPluginEntries) {
    if (AppConstants.MOZ_B2G) {
      return;
    }

    var addonList = [];

    // A helper function that reverts the prefs passed to default values.
    function resetPrefs(prefs) {
      for (let pref of prefs)
        gPref.clearUserPref(pref);
    }
    const types = ["extension", "theme", "locale", "dictionary", "service"];
    AddonManager.getAddonsByTypes(types, addons => {
      for (let addon of addons) {
        let oldState = Ci.nsIBlocklistService.STATE_NOTBLOCKED;
        if (oldAddonEntries)
          oldState = this._getAddonBlocklistState(addon, oldAddonEntries);
        let state = this.getAddonBlocklistState(addon);

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

        addonList.push({
          name: addon.name,
          version: addon.version,
          icon: addon.iconURL,
          disable: false,
          blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
          item: addon,
          url: this.getAddonBlocklistURL(addon),
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
        let state = this.getPluginBlocklistState(plugin);
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
              icon: "chrome://mozapps/skin/plugins/pluginGeneric.png",
              disable: false,
              blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
              item: plugin,
              url: this.getPluginBlocklistURL(plugin),
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
                                 .getService(Ci.nsIBlocklistPrompt);
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
      }

      Services.obs.addObserver(applyBlocklistChanges, "addon-blocklist-closed");

      if (getPref("getBoolPref", PREF_BLOCKLIST_SUPPRESSUI, false)) {
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
    });
  },

  classID: Components.ID("{66354bc9-7ed1-4692-ae1d-8da97d6b205e}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsIBlocklistService,
                                         Ci.nsITimerCallback]),
};

/**
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
      if (!(targetAppElement instanceof Ci.nsIDOMElement) ||
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
   * @param   version
   *          The version of the item being tested.
   * @param   appVersion
   *          The application version to test with.
   * @param   toolkitVersion
   *          The toolkit version to test with.
   * @returns True if the version range covers the item version and application
   *          or toolkit version.
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
   * @param   version
   *          The version to test.
   * @param   minVersion
   *          The minimum version. If null it is assumed that version is always
   *          larger.
   * @param   maxVersion
   *          The maximum version. If null it is assumed that version is always
   *          smaller.
   */
  matchesRange(version, minVersion, maxVersion) {
    if (minVersion && gVersionChecker.compare(version, minVersion) < 0)
      return false;
    if (maxVersion && gVersionChecker.compare(version, maxVersion) > 0)
      return false;
    return true;
  },

  /**
   * Tests if there is a matching range for the given target application id and
   * version.
   * @param   appID
   *          The application ID to test for, may be for an application or toolkit
   * @param   appVersion
   *          The version of the application to test for.
   * @returns True if this version range covers the application version given.
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
   * @param   targetAppElement
   *          A targetApplication blocklist element.
   * @returns An array of JS objects with the following properties:
   *          "minVersion"  The minimum version in a version range (default = null).
   *          "maxVersion"  The maximum version in a version range (default = null).
   */
  getBlocklistAppVersions(targetAppElement) {
    var appVersions = [ ];

    if (targetAppElement) {
      for (var i = 0; i < targetAppElement.childNodes.length; ++i) {
        var versionRangeElement = targetAppElement.childNodes.item(i);
        if (!(versionRangeElement instanceof Ci.nsIDOMElement) ||
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
   * @param   versionRangeElement
   *          The versionRange blocklist element.
   * @returns A JS object with the following properties:
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

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Blocklist]);
