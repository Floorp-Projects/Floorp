/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["LightweightThemeManager"];

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const ID_SUFFIX              = "@personas.mozilla.org";
const PREF_LWTHEME_TO_SELECT = "extensions.lwThemeToSelect";
const PREF_GENERAL_SKINS_SELECTEDSKIN = "general.skins.selectedSkin";
const PREF_EM_DSS_ENABLED    = "extensions.dss.enabled";
const ADDON_TYPE             = "theme";

const URI_EXTENSION_STRINGS  = "chrome://mozapps/locale/extensions/extensions.properties";

const STRING_TYPE_NAME       = "type.%ID%.name";

const DEFAULT_MAX_USED_THEMES_COUNT = 30;

const MAX_PREVIEW_SECONDS = 30;

const MANDATORY = ["id", "name", "headerURL"];
const OPTIONAL = ["footerURL", "textcolor", "accentcolor", "iconURL",
                  "previewURL", "author", "description", "homepageURL",
                  "updateURL", "version"];

const PERSIST_ENABLED = true;
const PERSIST_BYPASS_CACHE = false;
const PERSIST_FILES = {
  headerURL: "lightweighttheme-header",
  footerURL: "lightweighttheme-footer"
};

XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeImageOptimizer",
  "resource://gre/modules/addons/LightweightThemeImageOptimizer.jsm");

this.__defineGetter__("_prefs", function prefsGetter() {
  delete this._prefs;
  return this._prefs = Services.prefs.getBranch("lightweightThemes.");
});

this.__defineGetter__("_maxUsedThemes", function maxUsedThemesGetter() {
  delete this._maxUsedThemes;
  try {
    this._maxUsedThemes = _prefs.getIntPref("maxUsedThemes");
  }
  catch (e) {
    this._maxUsedThemes = DEFAULT_MAX_USED_THEMES_COUNT;
  }
  return this._maxUsedThemes;
});

this.__defineSetter__("_maxUsedThemes", function maxUsedThemesSetter(aVal) {
  delete this._maxUsedThemes;
  return this._maxUsedThemes = aVal;
});

// Holds the ID of the theme being enabled or disabled while sending out the
// events so cached AddonWrapper instances can return correct values for
// permissions and pendingOperations
var _themeIDBeingEnabled = null;
var _themeIDBeingDisabled = null;

// Convert from the old storage format (in which the order of usedThemes
// was combined with isThemeSelected to determine which theme was selected)
// to the new one (where a selectedThemeID determines which theme is selected).
(function migrateToNewStorageFormat() {
  let wasThemeSelected = false;
  try {
    wasThemeSelected = _prefs.getBoolPref("isThemeSelected");
  } catch(e) { }

  if (wasThemeSelected) {
    _prefs.clearUserPref("isThemeSelected");
    let themes = [];
    try {
      themes = JSON.parse(_prefs.getComplexValue("usedThemes",
                                                 Ci.nsISupportsString).data);
    } catch (e) { }

    if (Array.isArray(themes) && themes[0]) {
      _prefs.setCharPref("selectedThemeID", themes[0].id);
    }
  }
})();

this.LightweightThemeManager = {
  get name() {
    return "LightweightThemeManager";
  },

  // Themes that can be added for an application.  They can't be removed, and
  // will always show up at the top of the list.
  _builtInThemes: new Map(),

  get usedThemes () {
    let themes = [];
    try {
      themes = JSON.parse(_prefs.getComplexValue("usedThemes",
                                                 Ci.nsISupportsString).data);
    } catch (e) { }

    themes.push(...this._builtInThemes.values());
    return themes;
  },

  get currentTheme () {
    let selectedThemeID = null;
    try {
      selectedThemeID = _prefs.getCharPref("selectedThemeID");
    } catch (e) {}

    let data = null;
    if (selectedThemeID) {
      data = this.getUsedTheme(selectedThemeID);
    }
    return data;
  },

  get currentThemeForDisplay () {
    var data = this.currentTheme;

    if (data && PERSIST_ENABLED) {
      for (let key in PERSIST_FILES) {
        try {
          if (data[key] && _prefs.getBoolPref("persisted." + key))
            data[key] = _getLocalImageURI(PERSIST_FILES[key]).spec
                        + "?" + data.id + ";" + _version(data);
        } catch (e) {}
      }
    }

    return data;
  },

  set currentTheme (aData) {
    return _setCurrentTheme(aData, false);
  },

  setLocalTheme: function LightweightThemeManager_setLocalTheme(aData) {
    _setCurrentTheme(aData, true);
  },

  getUsedTheme: function LightweightThemeManager_getUsedTheme(aId) {
    var usedThemes = this.usedThemes;
    for (let usedTheme of usedThemes) {
      if (usedTheme.id == aId)
        return usedTheme;
    }
    return null;
  },

  forgetUsedTheme: function LightweightThemeManager_forgetUsedTheme(aId) {
    let theme = this.getUsedTheme(aId);
    if (!theme || LightweightThemeManager._builtInThemes.has(theme.id))
      return;

    let wrapper = new AddonWrapper(theme);
    AddonManagerPrivate.callAddonListeners("onUninstalling", wrapper, false);

    var currentTheme = this.currentTheme;
    if (currentTheme && currentTheme.id == aId) {
      this.themeChanged(null);
      AddonManagerPrivate.notifyAddonChanged(null, ADDON_TYPE, false);
    }

    _updateUsedThemes(_usedThemesExceptId(aId));
    AddonManagerPrivate.callAddonListeners("onUninstalled", wrapper);
  },

  addBuiltInTheme: function LightweightThemeManager_addBuiltInTheme(theme) {
    if (!theme || !theme.id || this.usedThemes.some(t => t.id == theme.id)) {
      throw new Error("Trying to add invalid builtIn theme");
    }

    this._builtInThemes.set(theme.id, theme);
  },

  forgetBuiltInTheme: function LightweightThemeManager_forgetBuiltInTheme(id) {
    if (!this._builtInThemes.has(id)) {
      let currentTheme = this.currentTheme;
      if (currentTheme && currentTheme.id == id) {
        this.currentTheme = null;
      }
    }
    return this._builtInThemes.delete(id);
  },

  clearBuiltInThemes: function LightweightThemeManager_clearBuiltInThemes() {
    for (let id of this._builtInThemes.keys()) {
      this.forgetBuiltInTheme(id);
    }
  },

  previewTheme: function LightweightThemeManager_previewTheme(aData) {
    let cancel = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
    cancel.data = false;
    Services.obs.notifyObservers(cancel, "lightweight-theme-preview-requested",
                                 JSON.stringify(aData));
    if (cancel.data)
      return;

    if (_previewTimer)
      _previewTimer.cancel();
    else
      _previewTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    _previewTimer.initWithCallback(_previewTimerCallback,
                                   MAX_PREVIEW_SECONDS * 1000,
                                   _previewTimer.TYPE_ONE_SHOT);

    _notifyWindows(aData);
  },

  resetPreview: function LightweightThemeManager_resetPreview() {
    if (_previewTimer) {
      _previewTimer.cancel();
      _previewTimer = null;
      _notifyWindows(this.currentThemeForDisplay);
    }
  },

  parseTheme: function LightweightThemeManager_parseTheme(aString, aBaseURI) {
    try {
      return _sanitizeTheme(JSON.parse(aString), aBaseURI, false);
    } catch (e) {
      return null;
    }
  },

  updateCurrentTheme: function LightweightThemeManager_updateCurrentTheme() {
    try {
      if (!_prefs.getBoolPref("update.enabled"))
        return;
    } catch (e) {
      return;
    }

    var theme = this.currentTheme;
    if (!theme || !theme.updateURL)
      return;

    var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);

    req.mozBackgroundRequest = true;
    req.overrideMimeType("text/plain");
    req.open("GET", theme.updateURL, true);
    // Prevent the request from reading from the cache.
    req.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    // Prevent the request from writing to the cache.
    req.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;

    var self = this;
    req.addEventListener("load", function loadEventListener() {
      if (req.status != 200)
        return;

      let newData = self.parseTheme(req.responseText, theme.updateURL);
      if (!newData ||
          newData.id != theme.id ||
          _version(newData) == _version(theme))
        return;

      var currentTheme = self.currentTheme;
      if (currentTheme && currentTheme.id == theme.id)
        self.currentTheme = newData;
    }, false);

    req.send(null);
  },

  /**
   * Switches to a new lightweight theme.
   *
   * @param  aData
   *         The lightweight theme to switch to
   */
  themeChanged: function LightweightThemeManager_themeChanged(aData) {
    if (_previewTimer) {
      _previewTimer.cancel();
      _previewTimer = null;
    }

    if (aData) {
      let usedThemes = _usedThemesExceptId(aData.id);
      usedThemes.unshift(aData);
      _updateUsedThemes(usedThemes);
      if (PERSIST_ENABLED) {
        LightweightThemeImageOptimizer.purge();
        _persistImages(aData, function themeChanged_persistImages() {
          _notifyWindows(this.currentThemeForDisplay);
        }.bind(this));
      }
    }

    if (aData)
      _prefs.setCharPref("selectedThemeID", aData.id);
    else
      _prefs.setCharPref("selectedThemeID", "");

    _notifyWindows(aData);
    Services.obs.notifyObservers(null, "lightweight-theme-changed", null);
  },

  /**
   * Starts the Addons provider and enables the new lightweight theme if
   * necessary.
   */
  startup: function LightweightThemeManager_startup() {
    if (Services.prefs.prefHasUserValue(PREF_LWTHEME_TO_SELECT)) {
      let id = Services.prefs.getCharPref(PREF_LWTHEME_TO_SELECT);
      if (id)
        this.themeChanged(this.getUsedTheme(id));
      else
        this.themeChanged(null);
      Services.prefs.clearUserPref(PREF_LWTHEME_TO_SELECT);
    }

    _prefs.addObserver("", _prefObserver, false);
  },

  /**
   * Shuts down the provider.
   */
  shutdown: function LightweightThemeManager_shutdown() {
    _prefs.removeObserver("", _prefObserver);
  },

  /**
   * Called when a new add-on has been enabled when only one add-on of that type
   * can be enabled.
   *
   * @param  aId
   *         The ID of the newly enabled add-on
   * @param  aType
   *         The type of the newly enabled add-on
   * @param  aPendingRestart
   *         true if the newly enabled add-on will only become enabled after a
   *         restart
   */
  addonChanged: function LightweightThemeManager_addonChanged(aId, aType, aPendingRestart) {
    if (aType != ADDON_TYPE)
      return;

    let id = _getInternalID(aId);
    let current = this.currentTheme;

    try {
      let next = Services.prefs.getCharPref(PREF_LWTHEME_TO_SELECT);
      if (id == next && aPendingRestart)
        return;

      Services.prefs.clearUserPref(PREF_LWTHEME_TO_SELECT);
      if (next) {
        AddonManagerPrivate.callAddonListeners("onOperationCancelled",
                                               new AddonWrapper(this.getUsedTheme(next)));
      }
      else {
        if (id == current.id) {
          AddonManagerPrivate.callAddonListeners("onOperationCancelled",
                                                 new AddonWrapper(current));
          return;
        }
      }
    }
    catch (e) {
    }

    if (current) {
      if (current.id == id)
        return;
      _themeIDBeingDisabled = current.id;
      let wrapper = new AddonWrapper(current);
      if (aPendingRestart) {
        Services.prefs.setCharPref(PREF_LWTHEME_TO_SELECT, "");
        AddonManagerPrivate.callAddonListeners("onDisabling", wrapper, true);
      }
      else {
        AddonManagerPrivate.callAddonListeners("onDisabling", wrapper, false);
        this.themeChanged(null);
        AddonManagerPrivate.callAddonListeners("onDisabled", wrapper);
      }
      _themeIDBeingDisabled = null;
    }

    if (id) {
      let theme = this.getUsedTheme(id);
      _themeIDBeingEnabled = id;
      let wrapper = new AddonWrapper(theme);
      if (aPendingRestart) {
        AddonManagerPrivate.callAddonListeners("onEnabling", wrapper, true);
        Services.prefs.setCharPref(PREF_LWTHEME_TO_SELECT, id);

        // Flush the preferences to disk so they survive any crash
        Services.prefs.savePrefFile(null);
      }
      else {
        AddonManagerPrivate.callAddonListeners("onEnabling", wrapper, false);
        this.themeChanged(theme);
        AddonManagerPrivate.callAddonListeners("onEnabled", wrapper);
      }
      _themeIDBeingEnabled = null;
    }
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         A callback to pass the Addon to
   */
  getAddonByID: function LightweightThemeManager_getAddonByID(aId, aCallback) {
    let id = _getInternalID(aId);
    if (!id) {
      aCallback(null);
      return;
     }

    let theme = this.getUsedTheme(id);
    if (!theme) {
      aCallback(null);
      return;
    }

    aCallback(new AddonWrapper(theme));
  },

  /**
   * Called to get Addons of a particular type.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types.
   * @param  aCallback
   *         A callback to pass an array of Addons to
   */
  getAddonsByTypes: function LightweightThemeManager_getAddonsByTypes(aTypes, aCallback) {
    if (aTypes && aTypes.indexOf(ADDON_TYPE) == -1) {
      aCallback([]);
      return;
    }

    aCallback(this.usedThemes.map(a => new AddonWrapper(a)));
  },
};

/**
 * The AddonWrapper wraps lightweight theme to provide the data visible to
 * consumers of the AddonManager API.
 */
function AddonWrapper(aTheme) {
  this.__defineGetter__("id", function AddonWrapper_idGetter() {
    return aTheme.id + ID_SUFFIX;
  });
  this.__defineGetter__("type", function AddonWrapper_typeGetter() {
    return ADDON_TYPE;
  });
  this.__defineGetter__("isActive", function AddonWrapper_isActiveGetter() {
    let current = LightweightThemeManager.currentTheme;
    if (current)
      return aTheme.id == current.id;
    return false;
  });

  this.__defineGetter__("name", function AddonWrapper_nameGetter() {
    return aTheme.name;
  });
  this.__defineGetter__("version", function AddonWrapper_versionGetter() {
    return "version" in aTheme ? aTheme.version : "";
  });

  ["description", "homepageURL", "iconURL"].forEach(function(prop) {
    this.__defineGetter__(prop, function AddonWrapper_optionalPropGetter() {
      return prop in aTheme ? aTheme[prop] : null;
    });
  }, this);

  ["installDate", "updateDate"].forEach(function(prop) {
    this.__defineGetter__(prop, function AddonWrapper_datePropGetter() {
      return prop in aTheme ? new Date(aTheme[prop]) : null;
    });
  }, this);

  this.__defineGetter__("creator", function AddonWrapper_creatorGetter() {
    return "author" in aTheme ? new AddonManagerPrivate.AddonAuthor(aTheme.author) : null;
  });

  this.__defineGetter__("screenshots", function AddonWrapper_screenshotsGetter() {
    let url = aTheme.previewURL;
    return [new AddonManagerPrivate.AddonScreenshot(url)];
  });

  this.__defineGetter__("pendingOperations",
                       function AddonWrapper_pendingOperationsGetter() {
    let pending = AddonManager.PENDING_NONE;
    if (this.isActive == this.userDisabled)
      pending |= this.isActive ? AddonManager.PENDING_DISABLE : AddonManager.PENDING_ENABLE;
    return pending;
  });

  this.__defineGetter__("operationsRequiringRestart", 
               function AddonWrapper_operationsRequiringRestartGetter() {
    // If a non-default theme is in use then a restart will be required to
    // enable lightweight themes unless dynamic theme switching is enabled
    if (Services.prefs.prefHasUserValue(PREF_GENERAL_SKINS_SELECTEDSKIN)) {
      try {
        if (Services.prefs.getBoolPref(PREF_EM_DSS_ENABLED))
          return AddonManager.OP_NEEDS_RESTART_NONE;
      }
      catch (e) {
      }
      return AddonManager.OP_NEEDS_RESTART_ENABLE;
    }

    return AddonManager.OP_NEEDS_RESTART_NONE;
  });

  this.__defineGetter__("size", function AddonWrapper_sizeGetter() {
    // The size changes depending on whether the theme is in use or not, this is
    // probably not worth exposing.
    return null;
  });

  this.__defineGetter__("permissions", function AddonWrapper_permissionsGetter() {
    let permissions = 0;

    // Do not allow uninstall of builtIn themes.
    if (!LightweightThemeManager._builtInThemes.has(aTheme.id))
      permissions = AddonManager.PERM_CAN_UNINSTALL;
    if (this.userDisabled)
      permissions |= AddonManager.PERM_CAN_ENABLE;
    else
      permissions |= AddonManager.PERM_CAN_DISABLE;
    return permissions;
  });

  this.__defineGetter__("userDisabled", function AddonWrapper_userDisabledGetter() {
    if (_themeIDBeingEnabled == aTheme.id)
      return false;
    if (_themeIDBeingDisabled == aTheme.id)
      return true;

    try {
      let toSelect = Services.prefs.getCharPref(PREF_LWTHEME_TO_SELECT);
      return aTheme.id != toSelect;
    }
    catch (e) {
      let current = LightweightThemeManager.currentTheme;
      return !current || current.id != aTheme.id;
    }
  });

  this.__defineSetter__("userDisabled", function AddonWrapper_userDisabledSetter(val) {
    if (val == this.userDisabled)
      return val;

    if (val)
      LightweightThemeManager.currentTheme = null;
    else
      LightweightThemeManager.currentTheme = aTheme;

    return val;
  });

  this.uninstall = function AddonWrapper_uninstall() {
    LightweightThemeManager.forgetUsedTheme(aTheme.id);
  };

  this.cancelUninstall = function AddonWrapper_cancelUninstall() {
    throw new Error("Theme is not marked to be uninstalled");
  };

  this.findUpdates = function AddonWrapper_findUpdates(listener, reason, appVersion, platformVersion) {
    AddonManagerPrivate.callNoUpdateListeners(this, listener, reason, appVersion, platformVersion);
  };
}

AddonWrapper.prototype = {
  // Lightweight themes are never disabled by the application
  get appDisabled() {
    return false;
  },

  // Lightweight themes are always compatible
  get isCompatible() {
    return true;
  },

  get isPlatformCompatible() {
    return true;
  },

  get scope() {
    return AddonManager.SCOPE_PROFILE;
  },

  get foreignInstall() {
    return false;
  },

  // Lightweight themes are always compatible
  isCompatibleWith: function AddonWrapper_isCompatibleWith(appVersion, platformVersion) {
    return true;
  },

  // Lightweight themes are always securely updated
  get providesUpdatesSecurely() {
    return true;
  },

  // Lightweight themes are never blocklisted
  get blocklistState() {
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  }
};

/**
 * Converts the ID used by the public AddonManager API to an lightweight theme
 * ID.
 *
 * @param   id
 *          The ID to be converted
 *
 * @return  the lightweight theme ID or null if the ID was not for a lightweight
 *          theme.
 */
function _getInternalID(id) {
  if (!id)
    return null;
  let len = id.length - ID_SUFFIX.length;
  if (len > 0 && id.substring(len) == ID_SUFFIX)
    return id.substring(0, len);
  return null;
}

function _setCurrentTheme(aData, aLocal) {
  aData = _sanitizeTheme(aData, null, aLocal);

  let needsRestart = (ADDON_TYPE == "theme") &&
                     Services.prefs.prefHasUserValue(PREF_GENERAL_SKINS_SELECTEDSKIN);

  let cancel = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
  cancel.data = false;
  Services.obs.notifyObservers(cancel, "lightweight-theme-change-requested",
                               JSON.stringify(aData));

  if (aData) {
    let theme = LightweightThemeManager.getUsedTheme(aData.id);
    let isInstall = !theme || theme.version != aData.version;
    if (isInstall) {
      aData.updateDate = Date.now();
      if (theme && "installDate" in theme)
        aData.installDate = theme.installDate;
      else
        aData.installDate = aData.updateDate;

      var oldWrapper = theme ? new AddonWrapper(theme) : null;
      var wrapper = new AddonWrapper(aData);
      AddonManagerPrivate.callInstallListeners("onExternalInstall", null,
                                               wrapper, oldWrapper, false);
      AddonManagerPrivate.callAddonListeners("onInstalling", wrapper, false);
    }

    let current = LightweightThemeManager.currentTheme;
    let usedThemes = _usedThemesExceptId(aData.id);
    if (current && current.id != aData.id)
      usedThemes.splice(1, 0, aData);
    else
      usedThemes.unshift(aData);
    _updateUsedThemes(usedThemes);

    if (isInstall)
       AddonManagerPrivate.callAddonListeners("onInstalled", wrapper);
  }

  if (cancel.data)
    return null;

  AddonManagerPrivate.notifyAddonChanged(aData ? aData.id + ID_SUFFIX : null,
                                         ADDON_TYPE, needsRestart);

  return LightweightThemeManager.currentTheme;
}

function _sanitizeTheme(aData, aBaseURI, aLocal) {
  if (!aData || typeof aData != "object")
    return null;

  var resourceProtocols = ["http", "https", "resource"];
  if (aLocal)
    resourceProtocols.push("file");
  var resourceProtocolExp = new RegExp("^(" + resourceProtocols.join("|") + "):");

  function sanitizeProperty(prop) {
    if (!(prop in aData))
      return null;
    if (typeof aData[prop] != "string")
      return null;
    let val = aData[prop].trim();
    if (!val)
      return null;

    if (!/URL$/.test(prop))
      return val;

    try {
      val = _makeURI(val, aBaseURI ? _makeURI(aBaseURI) : null).spec;
      if ((prop == "updateURL" ? /^https:/ : resourceProtocolExp).test(val))
        return val;
      return null;
    }
    catch (e) {
      return null;
    }
  }

  let result = {};
  for (let mandatoryProperty of MANDATORY) {
    let val = sanitizeProperty(mandatoryProperty);
    if (!val)
      throw Components.results.NS_ERROR_INVALID_ARG;
    result[mandatoryProperty] = val;
  }

  for (let optionalProperty of OPTIONAL) {
    let val = sanitizeProperty(optionalProperty);
    if (!val)
      continue;
    result[optionalProperty] = val;
  }

  return result;
}

function _usedThemesExceptId(aId) {
  return LightweightThemeManager.usedThemes.filter(
    function usedThemesExceptId_filterID(t) {
      return "id" in t && t.id != aId;
    });
}

function _version(aThemeData) {
  return aThemeData.version || "";
}

function _makeURI(aURL, aBaseURI) {
  return Services.io.newURI(aURL, null, aBaseURI);
}

function _updateUsedThemes(aList) {
  // Remove app-specific themes before saving them to the usedThemes pref.
  aList = aList.filter(theme => !LightweightThemeManager._builtInThemes.has(theme.id));

  // Send uninstall events for all themes that need to be removed.
  while (aList.length > _maxUsedThemes) {
    let wrapper = new AddonWrapper(aList[aList.length - 1]);
    AddonManagerPrivate.callAddonListeners("onUninstalling", wrapper, false);
    aList.pop();
    AddonManagerPrivate.callAddonListeners("onUninstalled", wrapper);
  }

  var str = Cc["@mozilla.org/supports-string;1"]
              .createInstance(Ci.nsISupportsString);
  str.data = JSON.stringify(aList);
  _prefs.setComplexValue("usedThemes", Ci.nsISupportsString, str);

  Services.obs.notifyObservers(null, "lightweight-theme-list-changed", null);
}

function _notifyWindows(aThemeData) {
  Services.obs.notifyObservers(null, "lightweight-theme-styling-update",
                               JSON.stringify(aThemeData));
}

var _previewTimer;
var _previewTimerCallback = {
  notify: function _previewTimerCallback_notify() {
    LightweightThemeManager.resetPreview();
  }
};

/**
 * Called when any of the lightweightThemes preferences are changed.
 */
function _prefObserver(aSubject, aTopic, aData) {
  switch (aData) {
    case "maxUsedThemes":
      try {
        _maxUsedThemes = _prefs.getIntPref(aData);
      }
      catch (e) {
        _maxUsedThemes = DEFAULT_MAX_USED_THEMES_COUNT;
      }
      // Update the theme list to remove any themes over the number we keep
      _updateUsedThemes(LightweightThemeManager.usedThemes);
      break;
  }
}

function _persistImages(aData, aCallback) {
  function onSuccess(key) {
    return function () {
      let current = LightweightThemeManager.currentTheme;
      if (current && current.id == aData.id) {
        _prefs.setBoolPref("persisted." + key, true);
      }
      if (--numFilesToPersist == 0 && aCallback) {
        aCallback();
      }
    };
  }

  let numFilesToPersist = 0;
  for (let key in PERSIST_FILES) {
    _prefs.setBoolPref("persisted." + key, false);
    if (aData[key]) {
      numFilesToPersist++;
      _persistImage(aData[key], PERSIST_FILES[key], onSuccess(key));
    }
  }
}

function _getLocalImageURI(localFileName) {
  var localFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  localFile.append(localFileName);
  return Services.io.newFileURI(localFile);
}

function _persistImage(sourceURL, localFileName, successCallback) {
  if (/^(file|resource):/.test(sourceURL))
    return;

  var targetURI = _getLocalImageURI(localFileName);
  var sourceURI = _makeURI(sourceURL);

  var persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                  .createInstance(Ci.nsIWebBrowserPersist);

  persist.persistFlags =
    Ci.nsIWebBrowserPersist.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
    Ci.nsIWebBrowserPersist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION |
    (PERSIST_BYPASS_CACHE ?
       Ci.nsIWebBrowserPersist.PERSIST_FLAGS_BYPASS_CACHE :
       Ci.nsIWebBrowserPersist.PERSIST_FLAGS_FROM_CACHE);

  persist.progressListener = new _persistProgressListener(successCallback);

  persist.saveURI(sourceURI, null,
                  null, Ci.nsIHttpChannel.REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE,
                  null, null, targetURI, null);
}

function _persistProgressListener(successCallback) {
  this.onLocationChange = function persistProgressListener_onLocationChange() {};
  this.onProgressChange = function persistProgressListener_onProgressChange() {};
  this.onStatusChange   = function persistProgressListener_onStatusChange() {};
  this.onSecurityChange = function persistProgressListener_onSecurityChange() {};
  this.onStateChange    = function persistProgressListener_onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aRequest &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      try {
        if (aRequest.QueryInterface(Ci.nsIHttpChannel).requestSucceeded) {
          // success
          successCallback();
          return;
        }
      } catch (e) { }
      // failure
    }
  };
}

AddonManagerPrivate.registerProvider(LightweightThemeManager, [
  new AddonManagerPrivate.AddonType("theme", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 5000)
]);
