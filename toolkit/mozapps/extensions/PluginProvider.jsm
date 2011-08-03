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
# The Original Code is the Extension Manager.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;

var EXPORTED_SYMBOLS = [];

Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const URI_EXTENSION_STRINGS  = "chrome://mozapps/locale/extensions/extensions.properties";
const STRING_TYPE_NAME       = "type.%ID%.name";

["LOG", "WARN", "ERROR"].forEach(function(aName) {
  this.__defineGetter__(aName, function() {
    Components.utils.import("resource://gre/modules/AddonLogging.jsm");

    LogManager.getLogger("addons.plugins", this);
    return this[aName];
  });
}, this);

function getIDHashForString(aStr) {
  // return the two-digit hexadecimal code for a byte
  function toHexString(charCode)
    ("0" + charCode.toString(16)).slice(-2);

  let hasher = Cc["@mozilla.org/security/hash;1"].
               createInstance(Ci.nsICryptoHash);
  hasher.init(Ci.nsICryptoHash.MD5);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].
                     createInstance(Ci.nsIStringInputStream);
                     stringStream.data = aStr ? aStr : "null";
  hasher.updateFromStream(stringStream, -1);

  // convert the binary hash data to a hex string.
  let binary = hasher.finish(false);
  let hash = [toHexString(binary.charCodeAt(i)) for (i in binary)].join("").toLowerCase();
  return "{" + hash.substr(0, 8) + "-" +
               hash.substr(8, 4) + "-" +
               hash.substr(12, 4) + "-" +
               hash.substr(16, 4) + "-" +
               hash.substr(20) + "}";
}

var PluginProvider = {
  // A dictionary mapping IDs to names and descriptions
  plugins: null,

  /**
   * Called when the application is shutting down. Only necessary for tests
   * to be able to simulate a shutdown.
   */
  shutdown: function PL_shutdown() {
    this.plugins = null;
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         A callback to pass the Addon to
   */
  getAddonByID: function PL_getAddon(aId, aCallback) {
    if (!this.plugins)
      this.buildPluginList();

    if (aId in this.plugins) {
      let name = this.plugins[aId].name;
      let description = this.plugins[aId].description;
      let tags = this.plugins[aId].tags;

      aCallback(new PluginWrapper(aId, name, description, tags));
    }
    else {
      aCallback(null);
    }
  },

  /**
   * Called to get Addons of a particular type.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types.
   * @param  callback
   *         A callback to pass an array of Addons to
   */
  getAddonsByTypes: function PL_getAddonsByTypes(aTypes, aCallback) {
    if (aTypes && aTypes.indexOf("plugin") < 0) {
      aCallback([]);
      return;
    }

    if (!this.plugins)
      this.buildPluginList();

    let results = [];

    for (let id in this.plugins) {
      this.getAddonByID(id, function(aAddon) {
        results.push(aAddon);
      });
    }

    aCallback(results);
  },

  /**
   * Called to get Addons that have pending operations.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types
   * @param  aCallback
   *         A callback to pass an array of Addons to
   */
  getAddonsWithOperationsByTypes: function PL_getAddonsWithOperationsByTypes(aTypes, aCallback) {
    aCallback([]);
  },

  /**
   * Called to get the current AddonInstalls, optionally restricting by type.
   *
   * @param  aTypes
   *         An array of types or null to get all types
   * @param  aCallback
   *         A callback to pass the array of AddonInstalls to
   */
  getInstallsByTypes: function PL_getInstallsByTypes(aTypes, aCallback) {
    aCallback([]);
  },

  buildPluginList: function PL_buildPluginList() {
    let tags = Cc["@mozilla.org/plugin/host;1"].
               getService(Ci.nsIPluginHost).
               getPluginTags({});

    this.plugins = {};
    let plugins = {};
    tags.forEach(function(aTag) {
      if (!(aTag.name in plugins))
        plugins[aTag.name] = {};
      if (!(aTag.description in plugins[aTag.name])) {
        let plugin = {
          name: aTag.name,
          description: aTag.description,
          tags: [aTag]
        };

        let id = getIDHashForString(aTag.name + aTag.description);

        plugins[aTag.name][aTag.description] = plugin;
        this.plugins[id] = plugin;
      }
      else {
        plugins[aTag.name][aTag.description].tags.push(aTag);
      }
    }, this);
  }
};

/**
 * The PluginWrapper wraps a set of nsIPluginTags to provide the data visible to
 * public callers through the API.
 */
function PluginWrapper(aId, aName, aDescription, aTags) {
  let safedesc = aDescription.replace(/<\/?[a-z][^>]*>/gi, " ");
  let homepageURL = null;
  if (/<A\s+HREF=[^>]*>/i.test(aDescription))
    homepageURL = /<A\s+HREF=["']?([^>"'\s]*)/i.exec(aDescription)[1];

  this.__defineGetter__("id", function() aId);
  this.__defineGetter__("type", function() "plugin");
  this.__defineGetter__("name", function() aName);
  this.__defineGetter__("creator", function() null);
  this.__defineGetter__("description", function() safedesc);
  this.__defineGetter__("version", function() aTags[0].version);
  this.__defineGetter__("homepageURL", function() homepageURL);

  this.__defineGetter__("isActive", function() !aTags[0].blocklisted && !aTags[0].disabled);
  this.__defineGetter__("appDisabled", function() aTags[0].blocklisted);
  this.__defineGetter__("userDisabled", function() aTags[0].disabled);
  this.__defineSetter__("userDisabled", function(aVal) {
    if (aTags[0].disabled == aVal)
      return;

    aTags.forEach(function(aTag) {
      aTag.disabled = aVal;
    });
    AddonManagerPrivate.callAddonListeners(aVal ? "onDisabling" : "onEnabling", this, false);
    AddonManagerPrivate.callAddonListeners(aVal ? "onDisabled" : "onEnabled", this);
    return aVal;
  });

  this.__defineGetter__("blocklistState", function() {
    let bs = Cc["@mozilla.org/extensions/blocklist;1"].
             getService(Ci.nsIBlocklistService);
    return bs.getPluginBlocklistState(aTags[0]);
  });

  this.__defineGetter__("blocklistURL", function() {
    let bs = Cc["@mozilla.org/extensions/blocklist;1"].
             getService(Ci.nsIBlocklistService);
    return bs.getPluginBlocklistURL(aTags[0]);
  });

  this.__defineGetter__("size", function() {
    function getDirectorySize(aFile) {
      let size = 0;
      let entries = aFile.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
      let entry;
      while (entry = entries.nextFile) {
        if (entry.isSymlink() || !entry.isDirectory())
          size += entry.fileSize;
        else
          size += getDirectorySize(entry);
      }
      entries.close();
      return size;
    }

    let size = 0;
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    aTags.forEach(function(aTag) {
      file.initWithPath(aTag.fullpath);
      if (file.isDirectory())
        size += getDirectorySize(file);
      else
        size += file.fileSize;
    });
    return size;
  });

  this.__defineGetter__("installDate", function() {
    let date = 0;
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    aTags.forEach(function(aTag) {
      file.initWithPath(aTag.fullpath);
      date = Math.max(date, file.lastModifiedTime);
    });
    return new Date(date);
  });

  this.__defineGetter__("scope", function() {
    let path = aTags[0].fullpath;
    // Plugins inside the application directory are in the application scope
    let dir = Services.dirsvc.get("APlugns", Ci.nsILocalFile);
    if (path.substring(0, dir.path.length) == dir.path)
      return AddonManager.SCOPE_APPLICATION;

    // Plugins inside the profile directory are in the profile scope
    dir = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    if (path.substring(0, dir.path.length) == dir.path)
      return AddonManager.SCOPE_PROFILE;

    // Plugins anywhere else in the user's home are in the user scope
    dir = Services.dirsvc.get("Home", Ci.nsILocalFile);
    if (path.substring(0, dir.path.length) == dir.path)
      return AddonManager.SCOPE_USER;

    // Any other locations are system scope
    return AddonManager.SCOPE_SYSTEM;
  });

  this.__defineGetter__("pendingOperations", function() {
    return AddonManager.PENDING_NONE;
  });

  this.__defineGetter__("operationsRequiringRestart", function() {
    return AddonManager.OP_NEEDS_RESTART_NONE;
  });

  this.__defineGetter__("permissions", function() {
    let permissions = 0;
    if (!this.appDisabled) {
      if (this.userDisabled)
        permissions |= AddonManager.PERM_CAN_ENABLE;
      else
        permissions |= AddonManager.PERM_CAN_DISABLE;
    }
    return permissions;
  });
}

PluginWrapper.prototype = {
  get updateDate() {
    return this.installDate;
  },

  get isCompatible() {
    return true;
  },

  get isPlatformCompatible() {
    return true;
  },

  get providesUpdatesSecurely() {
    return true;
  },

  isCompatibleWith: function(aAppVerison, aPlatformVersion) {
    return true;
  },

  findUpdates: function(aListener, aReason, aAppVersion, aPlatformVersion) {
    if ("onNoCompatibilityUpdateAvailable" in aListener)
      aListener.onNoCompatibilityUpdateAvailable(this);
    if ("onNoUpdateAvailable" in aListener)
      aListener.onNoUpdateAvailable(this);
    if ("onUpdateFinished" in aListener)
      aListener.onUpdateFinished(this);
  }
};

AddonManagerPrivate.registerProvider(PluginProvider, [
  new AddonManagerPrivate.AddonType("plugin", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 6000)
]);
