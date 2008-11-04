/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Blocklist Service.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Robert Strong <robert.bugzilla@gmail.com>
#   Michael Wu <flamingice@sourmilk.net>
#   Dave Townsend <dtownsend@oxymoronical.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const TOOLKIT_ID                      = "toolkit@mozilla.org"
const KEY_PROFILEDIR                  = "ProfD";
const KEY_APPDIR                      = "XCurProcD";
const FILE_BLOCKLIST                  = "blocklist.xml";
const PREF_BLOCKLIST_URL              = "extensions.blocklist.url";
const PREF_BLOCKLIST_ENABLED          = "extensions.blocklist.enabled";
const PREF_BLOCKLIST_INTERVAL         = "extensions.blocklist.interval";
const PREF_BLOCKLIST_LEVEL            = "extensions.blocklist.level";
const PREF_GENERAL_USERAGENT_LOCALE   = "general.useragent.locale";
const PREF_PARTNER_BRANCH             = "app.partner.";
const PREF_APP_DISTRIBUTION           = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION   = "distribution.version";
const PREF_APP_UPDATE_CHANNEL         = "app.update.channel";
const PREF_EM_LOGGING_ENABLED         = "extensions.logging.enabled";
const XMLURI_BLOCKLIST                = "http://www.mozilla.org/2006/addons-blocklist";
const XMLURI_PARSE_ERROR              = "http://www.mozilla.org/newlayout/xml/parsererror.xml"
const UNKNOWN_XPCOM_ABI               = "unknownABI";
const URI_BLOCKLIST_DIALOG            = "chrome://mozapps/content/extensions/blocklist.xul"
const DEFAULT_SEVERITY                = 3;
const DEFAULT_LEVEL                   = 2;
const MAX_BLOCK_LEVEL                 = 3;

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

var gApp = null;
var gPref = null;
var gOS = null;
var gConsole = null;
var gVersionChecker = null;
var gLoggingEnabled = null;
var gABI = null;
var gOSVersion = null;
var gBlocklistEnabled = true;
var gBlocklistLevel = DEFAULT_LEVEL;

// shared code for suppressing bad cert dialogs
#include ../../shared/src/badCertHandler.js

/**
 * Logs a string to the error console.
 * @param   string
 *          The string to write to the error console..
 */
function LOG(string) {
  if (gLoggingEnabled) {
    dump("*** " + string + "\n");
    if (gConsole)
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
 * Gets the file at the specified hierarchy under a Directory Service key.
 * @param   key
 *          The Directory Service Key to start from
 * @param   pathArray
 *          An array of path components to locate beneath the directory
 *          specified by |key|. The last item in this array must be the
 *          leaf name of a file.
 * @return  nsIFile object for the file specified. The file is NOT created
 *          if it does not exist, however all required directories along
 *          the way are.
 */
function getFile(key, pathArray) {
  var fileLocator = Cc["@mozilla.org/file/directory_service;1"].
                    getService(Ci.nsIProperties);
  var file = fileLocator.get(key, Ci.nsILocalFile);
  for (var i = 0; i < pathArray.length - 1; ++i) {
    file.append(pathArray[i]);
    if (!file.exists())
      file.create(Ci.nsILocalFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
  }
  file.followLinks = false;
  file.append(pathArray[pathArray.length - 1]);
  return file;
}

/**
 * Opens a safe file output stream for writing.
 * @param   file
 *          The file to write to.
 * @param   modeFlags
 *          (optional) File open flags. Can be undefined.
 * @returns nsIFileOutputStream to write to.
 */
function openSafeFileOutputStream(file, modeFlags) {
  var fos = Cc["@mozilla.org/network/safe-file-output-stream;1"].
            createInstance(Ci.nsIFileOutputStream);
  if (modeFlags === undefined)
    modeFlags = MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE;
  if (!file.exists())
    file.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_FILE);
  fos.init(file, modeFlags, PERMS_FILE, 0);
  return fos;
}

/**
 * Closes a safe file output stream.
 * @param   stream
 *          The stream to close.
 */
function closeSafeFileOutputStream(stream) {
  if (stream instanceof Ci.nsISafeOutputStream)
    stream.finish();
  else
    stream.close();
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
      return defaultPrefs.getCharPref(PREF_GENERAL_USERAGENT_LOCALE);
  } catch (e) {}

  return gPref.getCharPref(PREF_GENERAL_USERAGENT_LOCALE);
}

/**
 * Read the update channel from defaults only.  We do this to ensure that
 * the channel is tightly coupled with the application and does not apply
 * to other installations of the application that may use the same profile.
 */
function getUpdateChannel() {
  var channel = "default";
  var prefName;
  var prefValue;

  var defaults = gPref.getDefaultBranch(null);
  try {
    channel = defaults.getCharPref(PREF_APP_UPDATE_CHANNEL);
  } catch (e) {
    // use default when pref not found
  }

  try {
    var partners = gPref.getChildList(PREF_PARTNER_BRANCH, { });
    if (partners.length) {
      channel += "-cck";
      partners.sort();

      for each (prefName in partners) {
        prefValue = gPref.getCharPref(prefName);
        channel += "-" + prefValue;
      }
    }
  }
  catch (e) {
    Components.utils.reportError(e);
  }

  return channel;
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
 * Manages the Blocklist. The Blocklist is a representation of the contents of
 * blocklist.xml and allows us to remotely disable / re-enable blocklisted
 * items managed by the Extension Manager with an item's appDisabled property.
 * It also blocklists plugins with data from blocklist.xml.
 */

function Blocklist() {
  gApp = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
  gApp.QueryInterface(Ci.nsIXULRuntime);
  gPref = Cc["@mozilla.org/preferences-service;1"].
          getService(Ci.nsIPrefService).
          QueryInterface(Ci.nsIPrefBranch2);
  gVersionChecker = Cc["@mozilla.org/xpcom/version-comparator;1"].
                    getService(Ci.nsIVersionComparator);
  gConsole = Cc["@mozilla.org/consoleservice;1"].
             getService(Ci.nsIConsoleService);

  gOS = Cc["@mozilla.org/observer-service;1"].
        getService(Ci.nsIObserverService);
  gOS.addObserver(this, "xpcom-shutdown", false);

  // Not all builds have a known ABI
  try {
    gABI = gApp.XPCOMABI;
  }
  catch (e) {
    LOG("Blocklist: XPCOM ABI unknown.");
    gABI = UNKNOWN_XPCOM_ABI;
  }

  var osVersion;
  var sysInfo = Components.classes["@mozilla.org/system-info;1"]
                          .getService(Components.interfaces.nsIPropertyBag2);
  try {
    osVersion = sysInfo.getProperty("name") + " " + sysInfo.getProperty("version");
  }
  catch (e) {
    LOG("Blocklist: OS Version unknown.");
  }

  if (osVersion) {
    try {
      osVersion += " (" + sysInfo.getProperty("secondaryLibrary") + ")";
    }
    catch (e) {
      // Not all platforms have a secondary widget library, so an error is nothing to worry about.
    }
    gOSVersion = encodeURIComponent(osVersion);
  }

#ifdef XP_MACOSX
  // Mac universal build should report a different ABI than either macppc
  // or mactel.
  var macutils = Components.classes["@mozilla.org/xpcom/mac-utils;1"]
                           .getService(Components.interfaces.nsIMacUtils);

  if (macutils.isUniversalBinary)
    gABI = "Universal-gcc3";
#endif
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

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
    case "app-startup":
      gOS.addObserver(this, "profile-after-change", false);
      gOS.addObserver(this, "quit-application", false);
      break;
    case "profile-after-change":
      gLoggingEnabled = getPref("getBoolPref", PREF_EM_LOGGING_ENABLED, false);
      gBlocklistEnabled = getPref("getBoolPref", PREF_BLOCKLIST_ENABLED, true);
      gBlocklistLevel = Math.min(getPref("getIntPref", PREF_BLOCKLIST_LEVEL, DEFAULT_LEVEL),
                                 MAX_BLOCK_LEVEL);
      gPref.addObserver("extensions.blocklist.", this, false);
      var tm = Cc["@mozilla.org/updates/timer-manager;1"].
               getService(Ci.nsIUpdateTimerManager);
      var interval = getPref("getIntPref", PREF_BLOCKLIST_INTERVAL, 86400);
      tm.registerTimer("blocklist-background-update-timer", this, interval);
      break;
    case "quit-application":
      gOS.removeObserver(this, "profile-after-change");
      gOS.removeObserver(this, "quit-application");
      gPref.removeObserver("extensions.blocklist.", this);
      break;
    case "xpcom-shutdown":
      gOS.removeObserver(this, "xpcom-shutdown");
      gOS = null;
      gPref = null;
      gConsole = null;
      gVersionChecker = null;
      gApp = null;
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
  isAddonBlocklisted: function(id, version, appVersion, toolkitVersion) {
    return this.getAddonBlocklistState(id, version, appVersion, toolkitVersion) ==
                   Ci.nsIBlocklistService.STATE_BLOCKED;
  },

  /* See nsIBlocklistService */
  getAddonBlocklistState: function(id, version, appVersion, toolkitVersion) {
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
  _getAddonBlocklistState: function(id, version, addonEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    if (!appVersion)
      appVersion = gApp.version;
    if (!toolkitVersion)
      toolkitVersion = gApp.platformVersion;

    var blItem = addonEntries[id];
    if (!blItem)
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;

    for (var i = 0; i < blItem.length; ++i) {
      if (blItem[i].includesItem(version, appVersion, toolkitVersion))
        return blItem[i].severity >= gBlocklistLevel ? Ci.nsIBlocklistService.STATE_BLOCKED :
                                                       Ci.nsIBlocklistService.STATE_SOFTBLOCKED;
    }
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  notify: function(aTimer) {
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

    dsURI = dsURI.replace(/%APP_ID%/g, gApp.ID);
    dsURI = dsURI.replace(/%APP_VERSION%/g, gApp.version);
    dsURI = dsURI.replace(/%PRODUCT%/g, gApp.name);
    dsURI = dsURI.replace(/%VERSION%/g, gApp.version);
    dsURI = dsURI.replace(/%BUILD_ID%/g, gApp.appBuildID);
    dsURI = dsURI.replace(/%BUILD_TARGET%/g, gApp.OS + "_" + gABI);
    dsURI = dsURI.replace(/%OS_VERSION%/g, gOSVersion);
    dsURI = dsURI.replace(/%LOCALE%/g, getLocale());
    dsURI = dsURI.replace(/%CHANNEL%/g, getUpdateChannel());
    dsURI = dsURI.replace(/%PLATFORM_VERSION%/g, gApp.platformVersion);
    dsURI = dsURI.replace(/%DISTRIBUTION%/g,
                      getDistributionPrefValue(PREF_APP_DISTRIBUTION));
    dsURI = dsURI.replace(/%DISTRIBUTION_VERSION%/g,
                      getDistributionPrefValue(PREF_APP_DISTRIBUTION_VERSION));
    dsURI = dsURI.replace(/\+/g, "%2B");

    // Verify that the URI is valid
    try {
      var uri = newURI(dsURI);
    }
    catch (e) {
      LOG("Blocklist::notify: There was an error creating the blocklist URI\r\n" +
          "for: " + dsURI + ", error: " + e);
      return;
    }

    var request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                  createInstance(Ci.nsIXMLHttpRequest);
    request.open("GET", uri.spec, true);
    request.channel.notificationCallbacks = new BadCertHandler();
    request.overrideMimeType("text/xml");
    request.setRequestHeader("Cache-Control", "no-cache");
    request.QueryInterface(Components.interfaces.nsIJSXMLHttpRequest);

    var self = this;
    request.onerror = function(event) { self.onXMLError(event); };
    request.onload  = function(event) { self.onXMLLoad(event);  };
    request.send(null);

    // When the blocklist loads we need to compare it to the current copy so
    // make sure we have loaded it.
    if (!this._addonEntries)
      this._loadBlocklist();
  },

  onXMLLoad: function(aEvent) {
    var request = aEvent.target;
    try {
      checkCert(request.channel);
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
    var blocklistFile = getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);
    if (blocklistFile.exists())
      blocklistFile.remove(false);
    var fos = openSafeFileOutputStream(blocklistFile);
    fos.write(request.responseText, request.responseText.length);
    closeSafeFileOutputStream(fos);

    var oldAddonEntries = this._addonEntries;
    var oldPluginEntries = this._pluginEntries;
    this._addonEntries = { };
    this._pluginEntries = { };
    this._loadBlocklistFromFile(getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]));

    this._blocklistUpdated(oldAddonEntries, oldPluginEntries);
  },

  onXMLError: function(aEvent) {
    try {
      var request = aEvent.target;
      // the following may throw (e.g. a local file or timeout)
      var status = request.status;
    }
    catch (e) {
      request = aEvent.target.channel.QueryInterface(Ci.nsIRequest);
      status = request.status;
    }
    var statusText = request.statusText;
    // When status is 0 we don't have a valid channel.
    if (status == 0)
      statusText = "nsIXMLHttpRequest channel unavailable";
    LOG("Blocklist:onError: There was an error loading the blocklist file\r\n" +
        statusText);
  },

  /**
   * Finds the newest blocklist file from the application and the profile and
   * load it or does nothing if neither exist.
   */
  _loadBlocklist: function() {
    this._addonEntries = { };
    this._pluginEntries = { };
    var profFile = getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);
    if (profFile.exists()) {
      this._loadBlocklistFromFile(profFile);
      return;
    }
    var appFile = getFile(KEY_APPDIR, [FILE_BLOCKLIST]);
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
#        <emItem id="item_1@domain">
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
#        <emItem id="item_2@domain">
#          <versionRange minVersion="3.1" maxVersion="4.*"/>
#        </emItem>
#        <emItem id="item_3@domain">
#          <versionRange>
#            <targetApplication id="{ec8030f7-c20a-464f-9b0e-13a3a9e97384}">
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#            </targetApplication>
#          </versionRange>
#        </emItem>
#        <emItem id="item_4@domain">
#          <versionRange>
#            <targetApplication>
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#            </targetApplication>
#          </versionRange>
#        <emItem id="item_5@domain"/>
#      </emItems>
#      <pluginItems>
#        <pluginItem>
#          <!-- All match tags must match a plugin to blocklist a plugin -->
#          <match name="name" exp="some plugin"/>
#          <match name="description" exp="1[.]2[.]3"/>
#        </pluginItem>
#      </pluginItems>
#    </blocklist>
   */

  _loadBlocklistFromFile: function(file) {
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
    fileStream.init(file, MODE_RDONLY, PERMS_FILE, 0);
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
      this._addonEntries = this._processItemNodes(childNodes, "em",
                                                  this._handleEmItemNode);
      this._pluginEntries = this._processItemNodes(childNodes, "plugin",
                                                   this._handlePluginItemNode);
    }
    catch (e) {
      LOG("Blocklist::_loadBlocklistFromFile: Error constructing blocklist " + e);
      return;
    }
    fileStream.close();
  },

  _processItemNodes: function(deChildNodes, prefix, handler) {
    var result = [];
    var itemNodes;
    var containerName = prefix + "Items";
    for (var i = 0; i < deChildNodes.length; ++i) {
      var emItemsElement = deChildNodes.item(i);
      if (emItemsElement instanceof Ci.nsIDOMElement &&
          emItemsElement.localName == containerName) {
        itemNodes = emItemsElement.childNodes;
        break;
      }
    }
    if (!itemNodes)
      return result;

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

  _handleEmItemNode: function(blocklistElement, result) {
    if (!matchesOSABI(blocklistElement))
      return;

    var versionNodes = blocklistElement.childNodes;
    var id = blocklistElement.getAttribute("id");
    result[id] = [];
    for (var x = 0; x < versionNodes.length; ++x) {
      var versionRangeElement = versionNodes.item(x);
      if (!(versionRangeElement instanceof Ci.nsIDOMElement) ||
          versionRangeElement.localName != "versionRange")
        continue;

      result[id].push(new BlocklistItemData(versionRangeElement));
    }
    // if only the extension ID is specified block all versions of the
    // extension for the current application.
    if (result[id].length == 0)
      result[id].push(new BlocklistItemData(null));
  },

  _handlePluginItemNode: function(blocklistElement, result) {
    if (!matchesOSABI(blocklistElement))
      return;

    var matchNodes = blocklistElement.childNodes;
    var blockEntry = {
      matches: {},
      versions: []
    };
    for (var x = 0; x < matchNodes.length; ++x) {
      var matchElement = matchNodes.item(x);
      if (!(matchElement instanceof Ci.nsIDOMElement))
        continue;
      if (matchElement.localName == "match") {
        var name = matchElement.getAttribute("name");
        var exp = matchElement.getAttribute("exp");
        blockEntry.matches[name] = new RegExp(exp, "m");
      }
      if (matchElement.localName == "versionRange")
        blockEntry.versions.push(new BlocklistItemData(matchElement));
    }
    // Add a default versionRange if there wasn't one specified
    if (blockEntry.versions.length == 0)
      blockEntry.versions.push(new BlocklistItemData(null));
    result.push(blockEntry);
  },

  /* See nsIBlocklistService */
  getPluginBlocklistState: function(plugin, appVersion, toolkitVersion) {
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
  _getPluginBlocklistState: function(plugin, pluginEntries, appVersion, toolkitVersion) {
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

      for (var i = 0; i < blockEntry.versions.length; i++) {
        if (blockEntry.versions[i].includesItem(plugin.version, appVersion,
                                                toolkitVersion))
          return blockEntry.versions[i].severity >= gBlocklistLevel ?
                                                    Ci.nsIBlocklistService.STATE_BLOCKED :
                                                    Ci.nsIBlocklistService.STATE_SOFTBLOCKED;
      }
    }

    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  _blocklistUpdated: function(oldAddonEntries, oldPluginEntries) {
    var addonList = [];

    var em = Cc["@mozilla.org/extensions/manager;1"].
             getService(Ci.nsIExtensionManager);
    var addons = em.updateAndGetNewBlocklistedItems({});

    for (let i = 0; i < addons.length; i++) {
      let oldState = -1;
      if (oldAddonEntries)
        oldState = this._getAddonBlocklistState(addons[i].id, addons[i].version,
                                                oldAddonEntries);
      let state = this.getAddonBlocklistState(addons[i].id, addons[i].version);
      // We don't want to re-warn about items
      if (state == oldState)
        continue;

      addonList.push({
        name: addons[i].name,
        version: addons[i].version,
        icon: addons[i].iconURL,
        disable: false,
        blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
        item: addons[i]
      });
    }

    var phs = Cc["@mozilla.org/plugin/host;1"].
              getService(Ci.nsIPluginHost);
    var plugins = phs.getPluginTags({});

    for (let i = 0; i < plugins.length; i++) {
      let oldState = -1;
      if (oldPluginEntries)
        oldState = this._getPluginBlocklistState(plugins[i], oldPluginEntries);
      let state = this.getPluginBlocklistState(plugins[i]);
      // We don't want to re-warn about items
      if (state == oldState)
        continue;

      if (plugins[i].blocklisted) {
        if (state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED)
          plugins[i].disabled = true;
      }
      else if (!plugins[i].disabled && state != Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
        addonList.push({
          name: plugins[i].name,
          version: plugins[i].version,
          icon: "chrome://mozapps/skin/plugins/pluginGeneric.png",
          disable: false,
          blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
          item: plugins[i]
        });
      }
      plugins[i].blocklisted = state == Ci.nsIBlocklistService.STATE_BLOCKED;
    }

    if (addonList.length == 0)
      return;

    var args = {
      restart: false,
      list: addonList
    };
    // This lets the dialog get the raw js object
    args.wrappedJSObject = args;

    var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
             getService(Ci.nsIWindowWatcher);
    ww.openWindow(null, URI_BLOCKLIST_DIALOG, "",
                  "chrome,centerscreen,dialog,modal,titlebar", args);

    for (let i = 0; i < addonList.length; i++) {
      if (!addonList[i].disable)
        continue;

      if (addonList[i].item instanceof Ci.nsIUpdateItem)
        em.disableItem(addonList[i].item.id);
      else if (addonList[i].item instanceof Ci.nsIPluginTag)
        addonList[i].item.disabled = true;
      else
        LOG("Unknown add-on type: " + addonList[i].item);
    }

    if (args.restart)
      restartApp();
  },

  classDescription: "Blocklist Service",
  contractID: "@mozilla.org/extensions/blocklist;1",
  classID: Components.ID("{66354bc9-7ed1-4692-ae1d-8da97d6b205e}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsIBlocklistService,
                                         Ci.nsITimerCallback]),
  _xpcom_categories: [{
    category: "app-startup",
    service: true
  }]
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
  includesItem: function(version, appVersion, toolkitVersion) {
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
  matchesRange: function(version, minVersion, maxVersion) {
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
  matchesTargetRange: function(appID, appVersion) {
    var blTargetApp = this.targetApps[appID];
    if (!blTargetApp)
      return false;

    for (var x = 0; x < blTargetApp.length; ++x) {
      if (this.matchesRange(appVersion, blTargetApp[x].minVersion, blTargetApp[x].maxVersion))
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
  getBlocklistAppVersions: function(targetAppElement) {
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
  getBlocklistVersionRange: function(versionRangeElement) {
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

function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([Blocklist]);
}
