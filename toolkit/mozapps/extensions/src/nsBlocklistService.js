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
const FILE_BLOCKLIST                  = "blocklist.xml";
const PREF_BLOCKLIST_URL              = "extensions.blocklist.url";
const PREF_BLOCKLIST_ENABLED          = "extensions.blocklist.enabled";
const PREF_BLOCKLIST_INTERVAL         = "extensions.blocklist.interval";
const PREF_EM_LOGGING_ENABLED         = "extensions.logging.enabled";
const XMLURI_BLOCKLIST                = "http://www.mozilla.org/2006/addons-blocklist";
const XMLURI_PARSE_ERROR              = "http://www.mozilla.org/newlayout/xml/parsererror.xml"

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

/**
 * Checks whether this blocklist element is valid for the current OS and ABI.
 * If the element has an "os" attribute then the current OS must appear in
 * it's comma separated list for the element to be valid. Similarly for the
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
 * Manages the Blocklist. The Blocklist is a representation of the contents of
 * blocklist.xml and allows us to remotely disable / re-enable blocklisted
 * items managed by the Extension Manager with an item's appDisabled property.
 * It also blocklists plugins with data from blocklist.xml.
 */

function Blocklist() {
  gApp = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
  gApp.QueryInterface(Ci.nsIXULRuntime);
  gPref = Cc["@mozilla.org/preferences-service;1"].
          getService(Ci.nsIPrefBranch2);
  gVersionChecker = Cc["@mozilla.org/xpcom/version-comparator;1"].
                    getService(Ci.nsIVersionComparator);
  gConsole = Cc["@mozilla.org/consoleservice;1"].
             getService(Ci.nsIConsoleService);

  gOS = Cc["@mozilla.org/observer-service;1"].
        getService(Ci.nsIObserverService);
  gOS.addObserver(this, "xpcom-shutdown", false);
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
      gOS.addObserver(this, "plugins-list-updated", false);
      gOS.addObserver(this, "profile-after-change", false);
      gOS.addObserver(this, "quit-application", false);
      break;
    case "profile-after-change":
      gLoggingEnabled = getPref("getBoolPref", PREF_EM_LOGGING_ENABLED, false);
      var tm = Cc["@mozilla.org/updates/timer-manager;1"].
               getService(Ci.nsIUpdateTimerManager);
      var interval = getPref("getIntPref", PREF_BLOCKLIST_INTERVAL, 86400);
      tm.registerTimer("blocklist-background-update-timer", this, interval);
      break;
    case "plugins-list-updated":
      this._checkPluginsList();
      break;
    case "quit-application":
      gOS.removeObserver(this, "plugins-list-updated");
      gOS.removeObserver(this, "profile-after-change");
      gOS.removeObserver(this, "quit-application");
      break;
    case "xpcom-shutdown":
      gOS.removeObserver(this, "xpcom-shutdown");
      gOS = null;
      gPref = null;
      gConsole = null;
      gVersionChecker = null;
      gApp = null;
      break;
    }
  },

  isAddonBlocklisted: function(id, version, appVersion, toolkitVersion) {
    if (!this._addonEntries)
      this._loadBlocklistFromFile(getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]));
    if (appVersion === undefined)
      appVersion = gApp.version;
    if (toolkitVersion === undefined)
      toolkitVersion = gApp.platformVersion;

    var blItem = this._addonEntries[id];
    if (!blItem)
      return false;

    for (var i = 0; i < blItem.length; ++i) {
      if (gVersionChecker.compare(version, blItem[i].minVersion) < 0  ||
          gVersionChecker.compare(version, blItem[i].maxVersion) > 0)
        continue;

      var blTargetApp = blItem[i].targetApps[gApp.ID];
      if (blTargetApp) {
        for (var x = 0; x < blTargetApp.length; ++x) {
          if (gVersionChecker.compare(appVersion, blTargetApp[x].minVersion) < 0 ||
              gVersionChecker.compare(appVersion, blTargetApp[x].maxVersion) > 0)
            continue;
          return true;
        }
      }

      blTargetApp = blItem[i].targetApps[TOOLKIT_ID];
      if (!blTargetApp)
        return false;
      for (x = 0; x < blTargetApp.length; ++x) {
        if (gVersionChecker.compare(toolkitVersion, blTargetApp[x].minVersion) < 0 ||
            gVersionChecker.compare(toolkitVersion, blTargetApp[x].maxVersion) > 0)
          continue;
        return true;
      }
    }
    return false;
  },

  notify: function(aTimer) {
    if (getPref("getBoolPref", PREF_BLOCKLIST_ENABLED, true) == false)
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
    this._loadBlocklistFromFile(getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]));
    var em = Cc["@mozilla.org/extensions/manager;1"].
             getService(Ci.nsIExtensionManager);
    em.checkForBlocklistChanges();
    this._checkPluginsList();
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
    this._addonEntries = { };
    this._pluginEntries = { };
    if (getPref("getBoolPref", PREF_BLOCKLIST_ENABLED, true) == false) {
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
    var matchList;
    for (var x = 0; x < matchNodes.length; ++x) {
      var matchElement = matchNodes.item(x);
      if (!(matchElement instanceof Ci.nsIDOMElement) ||
          matchElement.localName != "match")
        continue;

      var name = matchElement.getAttribute("name");
      var exp = matchElement.getAttribute("exp");
      if (!matchList)
        matchList = { };
      matchList[name] = new RegExp(exp, "m");
    }
    if (matchList)
      result.push(matchList);
  },

  _checkPlugin: function(plugin) {
    for each (var matchList in this._pluginEntries) {
      var matchFailed = false;
      for (var name in matchList) {
        if (typeof(plugin[name]) != "string" ||
            !matchList[name].test(plugin[name])) {
          matchFailed = true;
          break;
        }
      }

      if (!matchFailed) {
        plugin.blocklisted = true;
        return;
      }
    }
    plugin.blocklisted = false;
  },

  _checkPluginsList: function() {
    if (!this._addonEntries)
      this._loadBlocklistFromFile(getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]));
    var phs = Cc["@mozilla.org/plugin/host;1"].
              getService(Ci.nsIPluginHost);
    phs.getPluginTags({ }).forEach(this._checkPlugin, this);
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
  // Default to all versions of the extension and the current application when
  // versionRange is not defined.
  if (!found)
    this.targetApps[gApp.ID] = this.getBlocklistAppVersions(null);
}

BlocklistItemData.prototype = {
/**
 * Retrieves a version range (e.g. minVersion and maxVersion) for a
 * blocklist item's targetApplication element.
 * @param   targetAppElement
 *          A targetApplication blocklist element.
 * @returns An array of JS objects with the following properties:
 *          "minVersion"  The minimum version in a version range (default = 0).
 *          "maxVersion"  The maximum version in a version range (default = *).
 */
  getBlocklistAppVersions: function(targetAppElement) {
    var appVersions = [ ];
    var found = false;

    if (targetAppElement) {
      for (var i = 0; i < targetAppElement.childNodes.length; ++i) {
        var versionRangeElement = targetAppElement.childNodes.item(i);
        if (!(versionRangeElement instanceof Ci.nsIDOMElement) ||
            versionRangeElement.localName != "versionRange")
          continue;
        found = true;
        appVersions.push(this.getBlocklistVersionRange(versionRangeElement));
      }
    }
    // return minVersion = 0 and maxVersion = * if not available
    if (!found)
      return [ this.getBlocklistVersionRange(null) ];
    return appVersions;
  },

/**
 * Retrieves a version range (e.g. minVersion and maxVersion) for a blocklist
 * versionRange element.
 * @param   versionRangeElement
 *          The versionRange blocklist element.
 * @returns A JS object with the following properties:
 *          "minVersion"  The minimum version in a version range (default = 0).
 *          "maxVersion"  The maximum version in a version range (default = *).
 */
  getBlocklistVersionRange: function(versionRangeElement) {
    var minVersion = "0";
    var maxVersion = "*";
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
