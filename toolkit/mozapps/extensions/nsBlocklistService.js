/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");

const TOOLKIT_ID                      = "toolkit@mozilla.org"
const KEY_PROFILEDIR                  = "ProfD";
const KEY_APPDIR                      = "XCurProcD";
const FILE_BLOCKLIST                  = "blocklist.xml";
const PREF_BLOCKLIST_LASTUPDATETIME   = "app.update.lastUpdateTime.blocklist-background-update-timer";
const PREF_BLOCKLIST_URL              = "extensions.blocklist.url";
const PREF_BLOCKLIST_ITEM_URL         = "extensions.blocklist.itemURL";
const PREF_BLOCKLIST_ENABLED          = "extensions.blocklist.enabled";
const PREF_BLOCKLIST_INTERVAL         = "extensions.blocklist.interval";
const PREF_BLOCKLIST_LEVEL            = "extensions.blocklist.level";
const PREF_BLOCKLIST_PINGCOUNTTOTAL   = "extensions.blocklist.pingCountTotal";
const PREF_BLOCKLIST_PINGCOUNTVERSION = "extensions.blocklist.pingCountVersion";
const PREF_PLUGINS_NOTIFYUSER         = "plugins.update.notifyUser";
const PREF_GENERAL_USERAGENT_LOCALE   = "general.useragent.locale";
const PREF_APP_DISTRIBUTION           = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION   = "distribution.version";
const PREF_EM_LOGGING_ENABLED         = "extensions.logging.enabled";
const XMLURI_BLOCKLIST                = "http://www.mozilla.org/2006/addons-blocklist";
const XMLURI_PARSE_ERROR              = "http://www.mozilla.org/newlayout/xml/parsererror.xml"
const UNKNOWN_XPCOM_ABI               = "unknownABI";
const URI_BLOCKLIST_DIALOG            = "chrome://mozapps/content/extensions/blocklist.xul"
const DEFAULT_SEVERITY                = 3;
const DEFAULT_LEVEL                   = 2;
const MAX_BLOCK_LEVEL                 = 3;
const SEVERITY_OUTDATED               = 0;
const VULNERABILITYSTATUS_NONE             = 0;
const VULNERABILITYSTATUS_UPDATE_AVAILABLE = 1;
const VULNERABILITYSTATUS_NO_UPDATE        = 2;

var gLoggingEnabled = null;
var gBlocklistEnabled = true;
var gBlocklistLevel = DEFAULT_LEVEL;

XPCOMUtils.defineLazyServiceGetter(this, "gConsole",
                                   "@mozilla.org/consoleservice;1",
                                   "nsIConsoleService");

XPCOMUtils.defineLazyServiceGetter(this, "gVersionChecker",
                                   "@mozilla.org/xpcom/version-comparator;1",
                                   "nsIVersionComparator");

XPCOMUtils.defineLazyGetter(this, "gPref", function bls_gPref() {
  return Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService).
         QueryInterface(Ci.nsIPrefBranch);
});

XPCOMUtils.defineLazyGetter(this, "gApp", function bls_gApp() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo).
         QueryInterface(Ci.nsIXULRuntime);
});

XPCOMUtils.defineLazyGetter(this, "gABI", function bls_gABI() {
  let abi = null;
  try {
    abi = gApp.XPCOMABI;
  }
  catch (e) {
    LOG("BlockList Global gABI: XPCOM ABI unknown.");
  }
#ifdef XP_MACOSX
  // Mac universal build should report a different ABI than either macppc
  // or mactel.
  let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].
                 getService(Ci.nsIMacUtils);

  if (macutils.isUniversalBinary)
    abi += "-u-" + macutils.architecturesInBinary;
#endif
  return abi;
});

XPCOMUtils.defineLazyGetter(this, "gOSVersion", function bls_gOSVersion() {
  let osVersion;
  let sysInfo = Cc["@mozilla.org/system-info;1"].
                getService(Ci.nsIPropertyBag2);
  try {
    osVersion = sysInfo.getProperty("name") + " " + sysInfo.getProperty("version");
  }
  catch (e) {
    LOG("BlockList Global gOSVersion: OS Version unknown.");
  }

  if (osVersion) {
    try {
      osVersion += " (" + sysInfo.getProperty("secondaryLibrary") + ")";
    }
    catch (e) {
      // Not all platforms have a secondary widget library, so an error is nothing to worry about.
    }
    osVersion = encodeURIComponent(osVersion);
  }
  return osVersion;
});

// shared code for suppressing bad cert dialogs
XPCOMUtils.defineLazyGetter(this, "gCertUtils", function bls_gCertUtils() {
  let temp = { };
  Components.utils.import("resource://gre/modules/CertUtils.jsm", temp);
  return temp;
});

function getObserverService() {
  return Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
}

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
  }
  catch (e) {
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
  return ioServ.newURI(spec, null, null);
}

// Restarts the application checking in with observers first
function restartApp() {
  // Notify all windows that an application quit has been requested.
  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                   createInstance(Ci.nsISupportsPRBool);
  os.notifyObservers(cancelQuit, "quit-application-requested", null);

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
  try {
      // Get the default branch
      var defaultPrefs = gPref.getDefaultBranch(null);
      return defaultPrefs.getComplexValue(PREF_GENERAL_USERAGENT_LOCALE,
                                          Ci.nsIPrefLocalizedString).data;
  } catch (e) {}

  return gPref.getCharPref(PREF_GENERAL_USERAGENT_LOCALE);
}

/* Get the distribution pref values, from defaults only */
function getDistributionPrefValue(aPrefName) {
  var prefValue = "default";

  var defaults = gPref.getDefaultBranch(null);
  try {
    prefValue = defaults.getCharPref(aPrefName);
  } catch (e) {
    // use default when pref not found
  }

  return prefValue;
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
  let os = getObserverService();
  os.addObserver(this, "xpcom-shutdown", false);
  gLoggingEnabled = getPref("getBoolPref", PREF_EM_LOGGING_ENABLED, false);
  gBlocklistEnabled = getPref("getBoolPref", PREF_BLOCKLIST_ENABLED, true);
  gBlocklistLevel = Math.min(getPref("getIntPref", PREF_BLOCKLIST_LEVEL, DEFAULT_LEVEL),
                                     MAX_BLOCK_LEVEL);
  gPref.addObserver("extensions.blocklist.", this, false);
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
  _pluginEntries: null,

  observe: function Blocklist_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
    case "xpcom-shutdown":
      let os = getObserverService();
      os.removeObserver(this, "xpcom-shutdown");
      gPref.removeObserver("extensions.blocklist.", this);
      break;
    case "nsPref:changed":
      switch (aData) {
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
    }
  },

  /* See nsIBlocklistService */
  isAddonBlocklisted: function Blocklist_isAddonBlocklisted(id, version, appVersion, toolkitVersion) {
    return this.getAddonBlocklistState(id, version, appVersion, toolkitVersion) ==
                   Ci.nsIBlocklistService.STATE_BLOCKED;
  },

  /* See nsIBlocklistService */
  getAddonBlocklistState: function Blocklist_getAddonBlocklistState(id, version, appVersion, toolkitVersion) {
    if (!this._addonEntries)
      this._loadBlocklist();
    return this._getAddonBlocklistState(id, version, this._addonEntries,
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
  _getAddonBlocklistState: function Blocklist_getAddonBlocklistStateCall(id,
                           version, addonEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    if (!appVersion)
      appVersion = gApp.version;
    if (!toolkitVersion)
      toolkitVersion = gApp.platformVersion;

    var blItem = this._findMatchingAddonEntry(addonEntries, id);
    if (!blItem)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    for (let currentblItem of blItem.versions) {
      if (currentblItem.includesItem(version, appVersion, toolkitVersion))
        return currentblItem.severity >= gBlocklistLevel ? Ci.nsIBlocklistService.STATE_BLOCKED :
                                                       Ci.nsIBlocklistService.STATE_SOFTBLOCKED;
    }
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  _findMatchingAddonEntry: function Blocklist_findMatchingAddonEntry(aAddonEntries,
                                                                     aId) {
    for (let entry of aAddonEntries) {
      if (entry.id instanceof RegExp) {
        if (entry.id.test(aId))
          return entry;
      } else if (entry.id == aId) {
        return entry;
      }
    }
    return null;
  },

  /* See nsIBlocklistService */
  getAddonBlocklistURL: function Blocklist_getAddonBlocklistURL(id, version, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled)
      return "";

    if (!this._addonEntries)
      this._loadBlocklist();

    let blItem = this._findMatchingAddonEntry(this._addonEntries, id);
    if (!blItem || !blItem.blockID)
      return null;

    return this._createBlocklistURL(blItem.blockID);
  },

  _createBlocklistURL: function Blocklist_createBlocklistURL(id) {
    let url = Services.urlFormatter.formatURLPref(PREF_BLOCKLIST_ITEM_URL);
    url = url.replace(/%blockID%/g, id);

    return url;
  },

  notify: function Blocklist_notify(aTimer) {
    if (!gBlocklistEnabled)
      return;

    try {
      var dsURI = gPref.getCharPref(PREF_BLOCKLIST_URL);
    }
    catch (e) {
      LOG("Blocklist::notify: The " + PREF_BLOCKLIST_URL + " preference" +
          " is missing!");
      return;
    }

    var pingCountVersion = getPref("getIntPref", PREF_BLOCKLIST_PINGCOUNTVERSION, 0);
    var pingCountTotal = getPref("getIntPref", PREF_BLOCKLIST_PINGCOUNTTOTAL, 1);
    var daysSinceLastPing = 0;
    if (pingCountVersion == 0) {
      daysSinceLastPing = "new";
    }
    else {
      // Seconds in one day is used because nsIUpdateTimerManager stores the
      // last update time in seconds.
      let secondsInDay = 60 * 60 * 24;
      let lastUpdateTime = getPref("getIntPref", PREF_BLOCKLIST_LASTUPDATETIME, 0);
      if (lastUpdateTime == 0) {
        daysSinceLastPing = "invalid";
      }
      else {
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
    dsURI = dsURI.replace(/%APP_VERSION%/g, gApp.version);
    dsURI = dsURI.replace(/%PRODUCT%/g, gApp.name);
    dsURI = dsURI.replace(/%VERSION%/g, gApp.version);
    dsURI = dsURI.replace(/%BUILD_ID%/g, gApp.appBuildID);
    dsURI = dsURI.replace(/%BUILD_TARGET%/g, gApp.OS + "_" + gABI);
    dsURI = dsURI.replace(/%OS_VERSION%/g, gOSVersion);
    dsURI = dsURI.replace(/%LOCALE%/g, getLocale());
    dsURI = dsURI.replace(/%CHANNEL%/g, UpdateChannel.get());
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
    }
    catch (e) {
      LOG("Blocklist::notify: There was an error creating the blocklist URI\r\n" +
          "for: " + dsURI + ", error: " + e);
      return;
    }

    LOG("Blocklist::notify: Requesting " + uri.spec);
    var request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                  createInstance(Ci.nsIXMLHttpRequest);
    request.open("GET", uri.spec, true);
    request.channel.notificationCallbacks = new gCertUtils.BadCertHandler();
    request.overrideMimeType("text/xml");
    request.setRequestHeader("Cache-Control", "no-cache");
    request.QueryInterface(Components.interfaces.nsIJSXMLHttpRequest);

    var self = this;
    request.addEventListener("error", function errorEventListener(event) {
                                      self.onXMLError(event); }, false);
    request.addEventListener("load", function loadEventListener(event) {
                                     self.onXMLLoad(event);  }, false);
    request.send(null);

    // When the blocklist loads we need to compare it to the current copy so
    // make sure we have loaded it.
    if (!this._addonEntries)
      this._loadBlocklist();
  },

  onXMLLoad: function Blocklist_onXMLLoad(aEvent) {
    var request = aEvent.target;
    try {
      gCertUtils.checkCert(request.channel);
    }
    catch (e) {
      LOG("Blocklist::onXMLLoad: " + e);
      return;
    }
    var responseXML = request.responseXML;
    if (!responseXML || responseXML.documentElement.namespaceURI == XMLURI_PARSE_ERROR ||
        (request.status != 200 && request.status != 0)) {
      LOG("Blocklist::onXMLLoad: there was an error during load");
      return;
    }
    var blocklistFile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);
    if (blocklistFile.exists())
      blocklistFile.remove(false);
    var fos = FileUtils.openSafeFileOutputStream(blocklistFile);
    fos.write(request.responseText, request.responseText.length);
    FileUtils.closeSafeFileOutputStream(fos);

    var oldAddonEntries = this._addonEntries;
    var oldPluginEntries = this._pluginEntries;
    this._addonEntries = [];
    this._pluginEntries = [];
    this._loadBlocklistFromFile(FileUtils.getFile(KEY_PROFILEDIR,
                                                  [FILE_BLOCKLIST]));

    this._blocklistUpdated(oldAddonEntries, oldPluginEntries);
  },

  onXMLError: function Blocklist_onXMLError(aEvent) {
    try {
      var request = aEvent.target;
      // the following may throw (e.g. a local file or timeout)
      var status = request.status;
    }
    catch (e) {
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
  _loadBlocklist: function Blocklist_loadBlocklist() {
    this._addonEntries = [];
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
#    </blocklist>
   */

  _loadBlocklistFromFile: function Blocklist_loadBlocklistFromFile(file) {
    if (!gBlocklistEnabled) {
      LOG("Blocklist::_loadBlocklistFromFile: blocklist is disabled");
      return;
    }

    if (!file.exists()) {
      LOG("Blocklist::_loadBlocklistFromFile: XML File does not exist");
      return;
    }

    var fileStream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                               .createInstance(Components.interfaces.nsIFileInputStream);
    fileStream.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
    try {
      var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                   createInstance(Ci.nsIDOMParser);
      var doc = parser.parseFromStream(fileStream, "UTF-8", file.fileSize, "text/xml");
      if (doc.documentElement.namespaceURI != XMLURI_BLOCKLIST) {
        LOG("Blocklist::_loadBlocklistFromFile: aborting due to incorrect " +
            "XML Namespace.\r\nExpected: " + XMLURI_BLOCKLIST + "\r\n" +
            "Received: " + doc.documentElement.namespaceURI);
        return;
      }

      var childNodes = doc.documentElement.childNodes;
      for (let element of childNodes) {
        if (!(element instanceof Ci.nsIDOMElement))
          continue;
        switch (element.localName) {
        case "emItems":
          this._addonEntries = this._processItemNodes(element.childNodes, "em",
                                                      this._handleEmItemNode);
          break;
        case "pluginItems":
          this._pluginEntries = this._processItemNodes(element.childNodes, "plugin",
                                                       this._handlePluginItemNode);
          break;
        default:
          Services.obs.notifyObservers(element,
                                       "blocklist-data-" + element.localName,
                                       null);
        }
      }
    }
    catch (e) {
      LOG("Blocklist::_loadBlocklistFromFile: Error constructing blocklist " + e);
      return;
    }
    fileStream.close();
  },

  _processItemNodes: function Blocklist_processItemNodes(itemNodes, prefix, handler) {
    var result = [];
    var itemName = prefix + "Item";
    for (var i = 0; i < itemNodes.length; ++i) {
      var blocklistElement = itemNodes.item(i);
      if (!(blocklistElement instanceof Ci.nsIDOMElement) ||
          blocklistElement.localName != itemName)
        continue;

      handler(blocklistElement, result);
    }
    return result;
  },

  _handleEmItemNode: function Blocklist_handleEmItemNode(blocklistElement, result) {
    if (!matchesOSABI(blocklistElement))
      return;

    let blockEntry = {
      id: null,
      versions: [],
      blockID: null
    };

    var versionNodes = blocklistElement.childNodes;
    var id = blocklistElement.getAttribute("id");
    // Add-on IDs cannot contain '/', so an ID starting with '/' must be a regex
    if (id.startsWith("/"))
      id = parseRegExp(id);
    blockEntry.id = id;

    for (var x = 0; x < versionNodes.length; ++x) {
      var versionRangeElement = versionNodes.item(x);
      if (!(versionRangeElement instanceof Ci.nsIDOMElement) ||
          versionRangeElement.localName != "versionRange")
        continue;

      blockEntry.versions.push(new BlocklistItemData(versionRangeElement));
    }
    // if only the extension ID is specified block all versions of the
    // extension for the current application.
    if (blockEntry.versions.length == 0)
      blockEntry.versions.push(new BlocklistItemData(null));

    blockEntry.blockID = blocklistElement.getAttribute("blockID");

    result.push(blockEntry);
  },

  _handlePluginItemNode: function Blocklist_handlePluginItemNode(blocklistElement, result) {
    if (!matchesOSABI(blocklistElement))
      return;

    var matchNodes = blocklistElement.childNodes;
    var blockEntry = {
      matches: {},
      versions: [],
      blockID: null,
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
      if (matchElement.localName == "versionRange")
        blockEntry.versions.push(new BlocklistItemData(matchElement));
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

  /* See nsIBlocklistService */
  getPluginBlocklistState: function Blocklist_getPluginBlocklistState(plugin,
                           appVersion, toolkitVersion) {
    if (!this._pluginEntries)
      this._loadBlocklist();
    return this._getPluginBlocklistState(plugin, this._pluginEntries,
                                         appVersion, toolkitVersion);
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
  _getPluginBlocklistState: function Blocklist_getPluginBlocklistState(plugin,
                            pluginEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    if (!appVersion)
      appVersion = gApp.version;
    if (!toolkitVersion)
      toolkitVersion = gApp.platformVersion;

    for each (var blockEntry in pluginEntries) {
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
        }
      }
    }

    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  /* See nsIBlocklistService */
  getPluginBlocklistURL: function Blocklist_getPluginBlocklistURL(plugin) {
    if (!gBlocklistEnabled)
      return "";

    if (!this._pluginEntries)
      this._loadBlocklist();

    for each (let blockEntry in this._pluginEntries) {
      let matchFailed = false;
      for (let name in blockEntry.matches) {
        if (!(name in plugin) ||
            typeof(plugin[name]) != "string" ||
            !blockEntry.matches[name].test(plugin[name])) {
          matchFailed = true;
          break;
        }
      }

      if (!matchFailed) {
        if(!blockEntry.blockID)
          return null;
        else
          return this._createBlocklistURL(blockEntry.blockID);
      }
    }
  },

  _blocklistUpdated: function Blocklist_blocklistUpdated(oldAddonEntries, oldPluginEntries) {
    var addonList = [];

    var self = this;
    const types = ["extension", "theme", "locale", "dictionary"]
    AddonManager.getAddonsByTypes(types, function blocklistUpdated_getAddonsByTypes(addons) {

      for (let addon of addons) {
        let oldState = Ci.nsIBlocklistService.STATE_NOTBLOCKED;
        if (oldAddonEntries)
          oldState = self._getAddonBlocklistState(addon.id, addon.version,
                                                  oldAddonEntries);
        let state = self.getAddonBlocklistState(addon.id, addon.version);

        LOG("Blocklist state for " + addon.id + " changed from " +
            oldState + " to " + state);

        // We don't want to re-warn about add-ons
        if (state == oldState)
          continue;

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
        if (!addon.isActive)
          continue;

        addonList.push({
          name: addon.name,
          version: addon.version,
          icon: addon.iconURL,
          disable: false,
          blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
          item: addon,
          url: self.getAddonBlocklistURL(addon.id),
        });
      }

      AddonManagerPrivate.updateAddonAppDisabledStates();

      var phs = Cc["@mozilla.org/plugin/host;1"].
                getService(Ci.nsIPluginHost);
      var plugins = phs.getPluginTags();

      for (let plugin of plugins) {
        let oldState = -1;
        if (oldPluginEntries)
          oldState = self._getPluginBlocklistState(plugin, oldPluginEntries);
        let state = self.getPluginBlocklistState(plugin);
        LOG("Blocklist state for " + plugin.name + " changed from " +
            oldState + " to " + state);
        // We don't want to re-warn about items
        if (state == oldState)
          continue;

        if (plugin.blocklisted) {
          if (state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED)
            plugin.disabled = true;
        }
        else if (!plugin.disabled && state != Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
          if (state == Ci.nsIBlocklistService.STATE_OUTDATED) {
            gPref.setBoolPref(PREF_PLUGINS_NOTIFYUSER, true);
          }
          else if (state != Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE &&
                   state != Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE) {
            addonList.push({
              name: plugin.name,
              version: plugin.version,
              icon: "chrome://mozapps/skin/plugins/pluginGeneric.png",
              disable: false,
              blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
              item: plugin,
              url: self.getPluginBlocklistURL(plugin),
            });
          }
        }
        plugin.blocklisted = state == Ci.nsIBlocklistService.STATE_BLOCKED;
      }

      if (addonList.length == 0) {
        Services.obs.notifyObservers(self, "blocklist-updated", "");
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
        Services.obs.notifyObservers(self, "blocklist-updated", "");
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
      let applyBlocklistChanges = function blocklistUpdated_applyBlocklistChanges() {
        for (let addon of addonList) {
          if (!addon.disable)
            continue;

          if (addon.item instanceof Ci.nsIPluginTag)
            addon.item.disabled = true;
          else
            addon.item.softDisabled = true;
        }

        if (args.restart)
          restartApp();

        Services.obs.notifyObservers(self, "blocklist-updated", "");
        Services.obs.removeObserver(applyBlocklistChanges, "addon-blocklist-closed");
      }

      Services.obs.addObserver(applyBlocklistChanges, "addon-blocklist-closed", false)

      function blocklistUnloadHandler(event) {
        if (event.target.location == URI_BLOCKLIST_DIALOG) {
          applyBlocklistChanges();
          blocklistWindow.removeEventListener("unload", blocklistUnloadHandler);
        }
      }

      let blocklistWindow = Services.ww.openWindow(null, URI_BLOCKLIST_DIALOG, "",
                              "chrome,centerscreen,dialog,titlebar", args);
      if (blocklistWindow)
        blocklistWindow.addEventListener("unload", blocklistUnloadHandler, false);
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
  includesItem: function BlocklistItemData_includesItem(version, appVersion, toolkitVersion) {
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
  matchesRange: function BlocklistItemData_matchesRange(version, minVersion, maxVersion) {
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
  matchesTargetRange: function BlocklistItemData_matchesTargetRange(appID, appVersion) {
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
  getBlocklistAppVersions: function BlocklistItemData_getBlocklistAppVersions(targetAppElement) {
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
  getBlocklistVersionRange: function BlocklistItemData_getBlocklistVersionRange(versionRangeElement) {
    var minVersion = null;
    var maxVersion = null;
    if (!versionRangeElement)
      return { minVersion: minVersion, maxVersion: maxVersion };

    if (versionRangeElement.hasAttribute("minVersion"))
      minVersion = versionRangeElement.getAttribute("minVersion");
    if (versionRangeElement.hasAttribute("maxVersion"))
      maxVersion = versionRangeElement.getAttribute("maxVersion");

    return { minVersion: minVersion, maxVersion: maxVersion };
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Blocklist]);
