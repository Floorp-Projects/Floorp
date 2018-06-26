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
XPCOMUtils.defineLazyGlobalGetters(this, ["XMLHttpRequest"]);

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

XPCOMUtils.defineLazyGetter(this, "gAppID", function() {
  return gApp.ID;
});

XPCOMUtils.defineLazyGetter(this, "gAppVersion", function() {
  return gApp.version;
});

XPCOMUtils.defineLazyGetter(this, "gABI", function() {
  let abi = null;
  try {
    abi = gApp.XPCOMABI;
  } catch (e) {
    LOG("BlockList Global gABI: XPCOM ABI unknown.");
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
  let os = blocklistElement.getAttribute("os");
  if (os) {
    let choices = os.split(",");
    if (choices.length > 0 && !choices.includes(gApp.OS))
      return false;
  }

  let xpcomabi = blocklistElement.getAttribute("xpcomabi");
  if (xpcomabi) {
    let choices = xpcomabi.split(",");
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
          // This is a bit messy. Especially in tests, but in principle also by users toggling
          // this preference repeatedly, plugin loads could race with each other if we don't
          // enforce that they are applied sequentially.
          // So we only update once the previous `_blocklistUpdated` call finishes running.
          let lastUpdate = this._lastUpdate || undefined;
          let newUpdate = this._lastUpdate = (async () => {
            await lastUpdate;
            this._clear();
            await this.loadBlocklistAsync();
            await this._blocklistUpdated(null, null);
            if (newUpdate == this._lastUpdate) {
              delete this._lastUpdate;
            }
          })().catch(Cu.reportError);
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
    if (!appVersion && !gAppVersion)
      return null;

    if (!appVersion)
      appVersion = gAppVersion;
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
      for (let [key, value] of Object.entries(entry)) {
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

    let replacements = {
      APP_ID: gAppID,
      PRODUCT: gApp.name,
      BUILD_ID: gApp.appBuildID,
      BUILD_TARGET: gApp.OS + "_" + gABI,
      OS_VERSION: gOSVersion,
      LOCALE: getLocale(),
      CHANNEL: UpdateUtils.UpdateChannel,
      PLATFORM_VERSION: gApp.platformVersion,
      DISTRIBUTION: getDistributionPrefValue(PREF_APP_DISTRIBUTION),
      DISTRIBUTION_VERSION: getDistributionPrefValue(PREF_APP_DISTRIBUTION_VERSION),
      PING_COUNT: pingCountVersion,
      TOTAL_PING_COUNT: pingCountTotal,
      DAYS_SINCE_LAST_PING: daysSinceLastPing,
    };
    dsURI = dsURI.replace(/%([A-Z_]+)%/g, function(fullMatch, name) {
      // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
      if (gAppVersion && (name == "APP_VERSION" || name == "VERSION")) {
        return gAppVersion;
      }
      // Some items, like DAYS_SINCE_LAST_PING, can be undefined, so we can't just
      // `return replacements[name] || fullMatch` or something like that.
      if (!replacements.hasOwnProperty(name)) {
        return fullMatch;
      }
      return replacements[name];
    });
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

    if (!this.isLoaded) {
      await this.loadBlocklistAsync();
    }

    var oldAddonEntries = this._addonEntries;
    var oldPluginEntries = this._pluginEntries;

    await this._loadBlocklistFromXML(responseXML);
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
    delete this._loadPromise;
  },

  /**
   * Trigger loading the blocklist content asynchronously.
   */
  async loadBlocklistAsync() {
    if (this.isLoaded) {
      return;
    }
    if (!this._loadPromise) {
      this._loadPromise = this._loadBlocklistAsyncInternal();
    }
    await this._loadPromise;
  },

  async _loadBlocklistAsyncInternal() {
    try {
      // Get the path inside the try...catch because there's no profileDir in e.g. xpcshell tests.
      let profFile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);
      await this._loadFileInternal(profFile);
      return;
    } catch (e) {
      LOG("Blocklist::loadBlocklistAsync: Failed to load XML file " + e);
    }

    var appFile = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);
    try {
      await this._loadFileInternal(appFile);
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

  async _loadFileInternal(file) {
    if (this.isLoaded) {
      return;
    }

    if (!gBlocklistEnabled) {
      LOG("Blocklist::_loadFileInternal: blocklist is disabled");
      return;
    }

    let xmlDoc = await new Promise((resolve, reject) => {
      let request = new XMLHttpRequest();
      request.open("GET", Services.io.newFileURI(file).spec, true);
      request.overrideMimeType("text/xml");
      request.addEventListener("error", reject);
      request.addEventListener("load", function() {
        let {status} = request;
        if (status != 200 && status != 0) {
          LOG("_loadFileInternal: there was an error during load, got status: " + status);
          reject(new Error("Couldn't load blocklist file"));
          return;
        }
        let doc = request.responseXML;
        if (doc.documentElement.namespaceURI != XMLURI_BLOCKLIST) {
          LOG("Blocklist::_loadBlocklistFromString: aborting due to incorrect " +
              "XML Namespace.\nExpected: " + XMLURI_BLOCKLIST + "\n" +
              "Received: " + doc.documentElement.namespaceURI);
          reject(new Error("Local blocklist file has the wrong namespace!"));
          return;
        }
        resolve(doc);
      });
      request.send(null);
    });

    await new Promise(resolve => {
      ChromeUtils.idleDispatch(async () => {
        if (!this.isLoaded) {
          await this._loadBlocklistFromXML(xmlDoc);
        }
        resolve();
      });
    });
  },

  async _loadBlocklistFromXML(doc) {
    this._addonEntries = [];
    this._gfxEntries = [];
    this._pluginEntries = [];
    try {
      var children = doc.documentElement.children;
      for (let element of children) {
        switch (element.localName) {
        case "emItems":
          this._addonEntries = await this._processItemNodes(element.children, "emItem",
                                                            this._handleEmItemNode);
          break;
        case "pluginItems":
          this._pluginEntries = await this._processItemNodes(element.children, "pluginItem",
                                                             this._handlePluginItemNode);
          break;
        case "gfxItems":
          // Parse as simple list of objects.
          this._gfxEntries = await this._processItemNodes(element.children, "gfxBlacklistEntry",
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

  async _processItemNodes(items, itemName, handler) {
    var result = [];
    let deadline = await new Promise(ChromeUtils.idleDispatch);
    for (let item of items) {
      if (item.localName == itemName) {
        handler(item, result);
      }
      if (!deadline || deadline.didTimeout || deadline.timeRemaining() < 1) {
        deadline = await new Promise(ChromeUtils.idleDispatch);
      }
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
      attributes: {},
      // Atleast one of EXTENSION_BLOCK_FILTERS must get added to attributes
    };

    for (let filter of EXTENSION_BLOCK_FILTERS) {
      let attr = blocklistElement.getAttribute(filter);
      if (attr) {
        // Any filter starting with '/' is interpreted as a regex. So if an attribute
        // starts with a '/' it must be checked via a regex.
        if (attr.startsWith("/")) {
          let lastSlash = attr.lastIndexOf("/");
          let pattern = attr.slice(1, lastSlash);
          let flags = attr.slice(lastSlash + 1);
          blockEntry.attributes[filter] = new RegExp(pattern, flags);
        } else {
          blockEntry.attributes[filter] = attr;
        }
      }
    }

    var children = blocklistElement.children;

    for (let childElement of children) {
      let localName = childElement.localName;
      if (localName == "prefs" && childElement.hasChildNodes) {
        let prefElements = childElement.children;
        for (let prefElement of prefElements) {
          if (prefElement.localName == "pref") {
            blockEntry.prefs.push(prefElement.textContent);
          }
        }
      } else if (localName == "versionRange") {
        blockEntry.versions.push(new BlocklistItemData(childElement));
      }
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

    let children = blocklistElement.children;
    var blockEntry = {
      matches: {},
      versions: [],
      blockID: null,
      infoURL: null,
    };
    var hasMatch = false;
    for (let childElement of children) {
      switch (childElement.localName) {
        case "match":
          var name = childElement.getAttribute("name");
          var exp = childElement.getAttribute("exp");
          try {
            blockEntry.matches[name] = new RegExp(exp, "m");
            hasMatch = true;
          } catch (e) {
            // Ignore invalid regular expressions
          }
          break;
        case "versionRange":
          blockEntry.versions.push(new BlocklistItemData(childElement));
          break;
        case "infoURL":
          blockEntry.infoURL = childElement.textContent;
          break;
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

    for (let matchElement of blocklistElement.children) {
      let value;
      if (matchElement.localName == "devices") {
        value = [];
        for (let childElement of matchElement.children) {
          const childValue = (childElement.textContent || "").trim();
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
        value = {minVersion: (matchElement.getAttribute("minVersion") || "").trim() || "0",
                 maxVersion: (matchElement.getAttribute("maxVersion") || "").trim() || "*"};
      } else {
        value = (matchElement.textContent || "").trim();
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
    if (!appVersion && !gAppVersion)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    if (!appVersion)
      appVersion = gAppVersion;
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
    let sortedProps = [
      "blockID", "devices", "driverVersion", "driverVersionComparator", "driverVersionMax",
      "feature", "featureStatus", "hardware", "manufacturer", "model", "os", "osversion",
      "product", "vendor", "versionRange",
    ];
    // Notify `GfxInfoBase`, by passing a string serialization.
    // This way we avoid spreading XML structure logics there.
    let payload = [];
    for (let gfxEntry of this._gfxEntries) {
      let entryLines = [];
      for (let key of sortedProps) {
        if (gfxEntry[key]) {
          let value = gfxEntry[key];
          if (Array.isArray(value)) {
            value = value.join(",");
          } else if (value.maxVersion) {
            // When XML is parsed, both minVersion and maxVersion are set.
            value = value.minVersion + "," + value.maxVersion;
          }
          entryLines.push(key + ":" + value);
        }
      }
      payload.push(entryLines.join("\t"));
    }
    Services.obs.notifyObservers(null, "blocklist-data-gfxItems", payload.join("\n"));
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
  this.targetApps = {};
  let foundTarget = false;
  this.severity = DEFAULT_SEVERITY;
  this.vulnerabilityStatus = VULNERABILITYSTATUS_NONE;
  if (versionRangeElement) {
    let versionRange = this.getBlocklistVersionRange(versionRangeElement);
    this.minVersion = versionRange.minVersion;
    this.maxVersion = versionRange.maxVersion;
    if (versionRangeElement.hasAttribute("severity"))
      this.severity = versionRangeElement.getAttribute("severity");
    if (versionRangeElement.hasAttribute("vulnerabilitystatus")) {
      this.vulnerabilityStatus = versionRangeElement.getAttribute("vulnerabilitystatus");
    }
    for (let targetAppElement of versionRangeElement.children) {
      if (targetAppElement.localName == "targetApplication") {
        foundTarget = true;
        // default to the current application if id is not provided.
        let appID = targetAppElement.id || gAppID;
        this.targetApps[appID] = this.getBlocklistAppVersions(targetAppElement);
      }
    }
  } else {
    this.minVersion = this.maxVersion = null;
  }

  // Default to all versions of the current application when no targetApplication
  // elements were found
  if (!foundTarget)
    this.targetApps[gAppID] = [{minVersion: null, maxVersion: null}];
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
    if (this.matchesTargetRange(gAppID, appVersion))
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
      for (let versionRangeElement of targetAppElement.children) {
        if (versionRangeElement.localName == "versionRange") {
          appVersions.push(this.getBlocklistVersionRange(versionRangeElement));
        }
      }
    }
    // return minVersion = null and maxVersion = null if no specific versionRange
    // elements were found
    if (appVersions.length == 0)
      appVersions.push({minVersion: null, maxVersion: null});
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
    // getAttribute returns null if the attribute is not present.
    let minVersion = versionRangeElement.getAttribute("minVersion");
    let maxVersion = versionRangeElement.getAttribute("maxVersion");

    return { minVersion, maxVersion };
  }
};

Blocklist._init();
