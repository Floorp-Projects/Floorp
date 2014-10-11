/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [];

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const URI_EXTENSION_STRINGS  = "chrome://mozapps/locale/extensions/extensions.properties";
const STRING_TYPE_NAME       = "type.%ID%.name";
const LIST_UPDATED_TOPIC     = "plugins-list-updated";

Cu.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.plugins";

// Create a new logger for use by the Addons Plugin Provider
// (Requires AddonManager.jsm)
let logger = Log.repository.getLogger(LOGGER_ID);

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
  get name() "PluginProvider",

  // A dictionary mapping IDs to names and descriptions
  plugins: null,

  startup: function PL_startup() {
    Services.obs.addObserver(this, LIST_UPDATED_TOPIC, false);
    Services.obs.addObserver(this, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED, false);
  },

  /**
   * Called when the application is shutting down. Only necessary for tests
   * to be able to simulate a shutdown.
   */
  shutdown: function PL_shutdown() {
    this.plugins = null;
    Services.obs.removeObserver(this, AddonManager.OPTIONS_NOTIFICATION_DISPLAYED);
    Services.obs.removeObserver(this, LIST_UPDATED_TOPIC);
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
    case AddonManager.OPTIONS_NOTIFICATION_DISPLAYED:
      this.getAddonByID(aData, function PL_displayPluginInfo(plugin) {
        if (!plugin)
          return;

        let libLabel = aSubject.getElementById("pluginLibraries");
        libLabel.textContent = plugin.pluginLibraries.join(", ");

        let typeLabel = aSubject.getElementById("pluginMimeTypes"), types = [];
        for (let type of plugin.pluginMimeTypes) {
          let extras = [type.description.trim(), type.suffixes].
                       filter(function(x) x).join(": ");
          types.push(type.type + (extras ? " (" + extras + ")" : ""));
        }
        typeLabel.textContent = types.join(",\n");
      });
      break;
    case LIST_UPDATED_TOPIC:
      if (this.plugins)
        this.updatePluginList();
      break;
    }
  },

  /**
   * Creates a PluginWrapper for a plugin object.
   */
  buildWrapper: function PL_buildWrapper(aPlugin) {
    return new PluginWrapper(aPlugin.id,
                             aPlugin.name,
                             aPlugin.description,
                             aPlugin.tags);
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

    if (aId in this.plugins)
      aCallback(this.buildWrapper(this.plugins[aId]));
    else
      aCallback(null);
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

  /**
   * Builds a list of the current plugins reported by the plugin host
   *
   * @return a dictionary of plugins indexed by our generated ID
   */
  getPluginList: function PL_getPluginList() {
    let tags = Cc["@mozilla.org/plugin/host;1"].
               getService(Ci.nsIPluginHost).
               getPluginTags({});

    let list = {};
    let seenPlugins = {};
    for (let tag of tags) {
      if (!(tag.name in seenPlugins))
        seenPlugins[tag.name] = {};
      if (!(tag.description in seenPlugins[tag.name])) {
        let plugin = {
          id: getIDHashForString(tag.name + tag.description),
          name: tag.name,
          description: tag.description,
          tags: [tag]
        };

        seenPlugins[tag.name][tag.description] = plugin;
        list[plugin.id] = plugin;
      }
      else {
        seenPlugins[tag.name][tag.description].tags.push(tag);
      }
    }

    return list;
  },

  /**
   * Builds the list of known plugins from the plugin host
   */
  buildPluginList: function PL_buildPluginList() {
    this.plugins = this.getPluginList();
  },

  /**
   * Updates the plugins from the plugin host by comparing the current plugins
   * to the last known list sending out any necessary API notifications for
   * changes.
   */
  updatePluginList: function PL_updatePluginList() {
    let newList = this.getPluginList();

    let lostPlugins = [this.buildWrapper(this.plugins[id])
                       for each (id in Object.keys(this.plugins)) if (!(id in newList))];
    let newPlugins = [this.buildWrapper(newList[id])
                      for each (id in Object.keys(newList)) if (!(id in this.plugins))];
    let matchedIDs = [id for each (id in Object.keys(newList)) if (id in this.plugins)];

    // The plugin host generates new tags for every plugin after a scan and
    // if the plugin's filename has changed then the disabled state won't have
    // been carried across, send out notifications for anything that has
    // changed (see bug 830267).
    let changedWrappers = [];
    for (let id of matchedIDs) {
      let oldWrapper = this.buildWrapper(this.plugins[id]);
      let newWrapper = this.buildWrapper(newList[id]);

      if (newWrapper.isActive != oldWrapper.isActive) {
        AddonManagerPrivate.callAddonListeners(newWrapper.isActive ?
                                               "onEnabling" : "onDisabling",
                                               newWrapper, false);
        changedWrappers.push(newWrapper);
      }
    }

    // Notify about new installs
    for (let plugin of newPlugins) {
      AddonManagerPrivate.callInstallListeners("onExternalInstall", null,
                                               plugin, null, false);
      AddonManagerPrivate.callAddonListeners("onInstalling", plugin, false);
    }

    // Notify for any plugins that have vanished.
    for (let plugin of lostPlugins)
      AddonManagerPrivate.callAddonListeners("onUninstalling", plugin, false);

    this.plugins = newList;

    // Signal that new installs are complete
    for (let plugin of newPlugins)
      AddonManagerPrivate.callAddonListeners("onInstalled", plugin);

    // Signal that enables/disables are complete
    for (let wrapper of changedWrappers) {
      AddonManagerPrivate.callAddonListeners(wrapper.isActive ?
                                             "onEnabled" : "onDisabled",
                                             wrapper);
    }

    // Signal that uninstalls are complete
    for (let plugin of lostPlugins)
      AddonManagerPrivate.callAddonListeners("onUninstalled", plugin);
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

  this.__defineGetter__("userDisabled", function() {
    if (aTags[0].disabled)
      return true;

    if ((Services.prefs.getBoolPref("plugins.click_to_play") && aTags[0].clicktoplay) ||
        this.blocklistState == Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE ||
        this.blocklistState == Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE)
      return AddonManager.STATE_ASK_TO_ACTIVATE;

    return false;
  });

  this.__defineSetter__("userDisabled", function(aVal) {
    let previousVal = this.userDisabled;
    if (aVal === previousVal)
      return aVal;

    for (let tag of aTags) {
      if (aVal === true)
        tag.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
      else if (aVal === false)
        tag.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
      else if (aVal == AddonManager.STATE_ASK_TO_ACTIVATE)
        tag.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
    }

    // If 'userDisabled' was 'true' and we're going to a state that's not
    // that, we're enabling, so call those listeners.
    if (previousVal === true && aVal !== true) {
      AddonManagerPrivate.callAddonListeners("onEnabling", this, false);
      AddonManagerPrivate.callAddonListeners("onEnabled", this);
    }

    // If 'userDisabled' was not 'true' and we're going to a state where
    // it is, we're disabling, so call those listeners.
    if (previousVal !== true && aVal === true) {
      AddonManagerPrivate.callAddonListeners("onDisabling", this, false);
      AddonManagerPrivate.callAddonListeners("onDisabled", this);
    }

    // If the 'userDisabled' value involved AddonManager.STATE_ASK_TO_ACTIVATE,
    // call the onPropertyChanged listeners.
    if (previousVal == AddonManager.STATE_ASK_TO_ACTIVATE ||
        aVal == AddonManager.STATE_ASK_TO_ACTIVATE) {
      AddonManagerPrivate.callAddonListeners("onPropertyChanged", this, ["userDisabled"]);
    }

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
      while ((entry = entries.nextFile)) {
        if (entry.isSymlink() || !entry.isDirectory())
          size += entry.fileSize;
        else
          size += getDirectorySize(entry);
      }
      entries.close();
      return size;
    }

    let size = 0;
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    for (let tag of aTags) {
      file.initWithPath(tag.fullpath);
      if (file.isDirectory())
        size += getDirectorySize(file);
      else
        size += file.fileSize;
    }
    return size;
  });

  this.__defineGetter__("pluginLibraries", function() {
    let libs = [];
    for (let tag of aTags)
      libs.push(tag.filename);
    return libs;
  });

  this.__defineGetter__("pluginFullpath", function() {
    let paths = [];
    for (let tag of aTags)
      paths.push(tag.fullpath);
    return paths;
  })

  this.__defineGetter__("pluginMimeTypes", function() {
    let types = [];
    for (let tag of aTags) {
      let mimeTypes = tag.getMimeTypes({});
      let mimeDescriptions = tag.getMimeDescriptions({});
      let extensions = tag.getExtensions({});
      for (let i = 0; i < mimeTypes.length; i++) {
        let type = {};
        type.type = mimeTypes[i];
        type.description = mimeDescriptions[i];
        type.suffixes = extensions[i];

        types.push(type);
      }
    }
    return types;
  });

  this.__defineGetter__("installDate", function() {
    let date = 0;
    for (let tag of aTags) {
      date = Math.max(date, tag.lastModifiedTime);
    }
    return new Date(date);
  });

  this.__defineGetter__("scope", function() {
    let path = aTags[0].fullpath;
    // Plugins inside the application directory are in the application scope
    let dir = Services.dirsvc.get("APlugns", Ci.nsIFile);
    if (path.startsWith(dir.path))
      return AddonManager.SCOPE_APPLICATION;

    // Plugins inside the profile directory are in the profile scope
    dir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    if (path.startsWith(dir.path))
      return AddonManager.SCOPE_PROFILE;

    // Plugins anywhere else in the user's home are in the user scope,
    // but not all platforms have a home directory.
    try {
      dir = Services.dirsvc.get("Home", Ci.nsIFile);
      if (path.startsWith(dir.path))
        return AddonManager.SCOPE_USER;
    } catch (e if (e.result && e.result == Components.results.NS_ERROR_FAILURE)) {
      // Do nothing: missing "Home".
    }

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

      if (this.userDisabled !== true)
        permissions |= AddonManager.PERM_CAN_DISABLE;

      let blocklistState = this.blocklistState;
      let isCTPBlocklisted =
        (blocklistState == Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE ||
         blocklistState == Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE);

      if (this.userDisabled !== AddonManager.STATE_ASK_TO_ACTIVATE &&
          (Services.prefs.getBoolPref("plugins.click_to_play") ||
           isCTPBlocklisted)) {
        permissions |= AddonManager.PERM_CAN_ASK_TO_ACTIVATE;
      }

      if (this.userDisabled !== false && !isCTPBlocklisted) {
        permissions |= AddonManager.PERM_CAN_ENABLE;
      }
    }
    return permissions;
  });
}

PluginWrapper.prototype = {
  optionsType: AddonManager.OPTIONS_TYPE_INLINE_INFO,
  optionsURL: "chrome://mozapps/content/extensions/pluginPrefs.xul",

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

  get foreignInstall() {
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
                                    AddonManager.VIEW_TYPE_LIST, 6000,
                                    AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE)
]);
