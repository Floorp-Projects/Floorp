/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LightweightThemeManager"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
/* globals AddonManagerPrivate*/
ChromeUtils.import("resource://gre/modules/Services.jsm");

const ID_SUFFIX              = "@personas.mozilla.org";
const ADDON_TYPE             = "theme";
const ADDON_TYPE_WEBEXT      = "webextension-theme";

const URI_EXTENSION_STRINGS  = "chrome://mozapps/locale/extensions/extensions.properties";

const DEFAULT_THEME_ID = "default-theme@mozilla.org";
const DEFAULT_MAX_USED_THEMES_COUNT = 30;

const MAX_PREVIEW_SECONDS = 30;

const MANDATORY = ["id", "name"];
const OPTIONAL = ["headerURL", "footerURL", "textcolor", "accentcolor",
                  "iconURL", "previewURL", "author", "description",
                  "homepageURL", "updateURL", "version"];

const PERSIST_ENABLED = true;
const PERSIST_BYPASS_CACHE = false;
const PERSIST_FILES = {
  headerURL: "lightweighttheme-header",
  footerURL: "lightweighttheme-footer"
};

ChromeUtils.defineModuleGetter(this, "LightweightThemeImageOptimizer",
  "resource://gre/modules/addons/LightweightThemeImageOptimizer.jsm");
ChromeUtils.defineModuleGetter(this, "ServiceRequest",
  "resource://gre/modules/ServiceRequest.jsm");


XPCOMUtils.defineLazyGetter(this, "_prefs", () => {
  return Services.prefs.getBranch("lightweightThemes.");
});

Object.defineProperty(this, "_maxUsedThemes", {
  get() {
    delete this._maxUsedThemes;
    this._maxUsedThemes = _prefs.getIntPref("maxUsedThemes", DEFAULT_MAX_USED_THEMES_COUNT);
    return this._maxUsedThemes;
  },

  set(val) {
    delete this._maxUsedThemes;
    return this._maxUsedThemes = val;
  },
  configurable: true,
});

XPCOMUtils.defineLazyPreferenceGetter(this, "requireSecureUpdates",
                                      "extensions.checkUpdateSecurity", true);


// Holds the ID of the theme being enabled or disabled while sending out the
// events so cached AddonWrapper instances can return correct values for
// permissions and pendingOperations
var _themeIDBeingEnabled = null;
var _themeIDBeingDisabled = null;

// Holds optional fallback theme data that will be returned when no data for an
// active theme can be found. This the case for WebExtension Themes, for example.
var _fallbackThemeData = null;

// Holds whether or not the default theme should display in dark mode. This is
// typically the case when the OS has a dark system appearance.
var _defaultThemeIsInDarkMode = false;
// Holds the dark theme to be used if the OS has a dark system appearance and
// the default theme is selected.
var _defaultDarkThemeID = null;

// Convert from the old storage format (in which the order of usedThemes
// was combined with isThemeSelected to determine which theme was selected)
// to the new one (where a selectedThemeID determines which theme is selected).
(function() {
  let wasThemeSelected = _prefs.getBoolPref("isThemeSelected", false);

  if (wasThemeSelected) {
    _prefs.clearUserPref("isThemeSelected");
    let themes = [];
    try {
      themes = JSON.parse(_prefs.getStringPref("usedThemes"));
    } catch (e) { }

    if (Array.isArray(themes) && themes[0]) {
      _prefs.setCharPref("selectedThemeID", themes[0].id);
    }
  }
})();

var LightweightThemeManager = {
  get name() {
    return "LightweightThemeManager";
  },

  set fallbackThemeData(data) {
    if (data && Object.getOwnPropertyNames(data).length) {
      _fallbackThemeData = Object.assign({}, data);
      if (PERSIST_ENABLED) {
        LightweightThemeImageOptimizer.purge();
        _persistImages(_fallbackThemeData, () => {});
      }
    } else {
      _fallbackThemeData = null;
    }
    return _fallbackThemeData;
  },

  // Themes that can be added for an application.  They can't be removed, and
  // will always show up at the top of the list.
  _builtInThemes: new Map(),

  get usedThemes() {
    let themes = [];
    try {
      themes = JSON.parse(_prefs.getStringPref("usedThemes"));
    } catch (e) { }

    themes.push(...this._builtInThemes.values());
    return themes;
  },

  get currentTheme() {
    let selectedThemeID = _prefs.getStringPref("selectedThemeID", DEFAULT_THEME_ID);

    let data = null;
    if (selectedThemeID) {
      data = this.getUsedTheme(selectedThemeID);
    }
    return data;
  },

  get currentThemeForDisplay() {
    var data = this.currentTheme;

    if (!data || data.id == DEFAULT_THEME_ID) {
      if (_fallbackThemeData) {
        return _fallbackThemeData;
      }
      if (_defaultThemeIsInDarkMode && _defaultDarkThemeID) {
        return this.getUsedTheme(_defaultDarkThemeID);
      }
    }

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

  set currentTheme(aData) {
    return _setCurrentTheme(aData, false);
  },

  setLocalTheme(aData) {
    _setCurrentTheme(aData, true);
  },

  getUsedTheme(aId) {
    var usedThemes = this.usedThemes;
    for (let usedTheme of usedThemes) {
      if (usedTheme.id == aId)
        return usedTheme;
    }
    return null;
  },

  forgetUsedTheme(aId) {
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

  addBuiltInTheme(theme, { useInDarkMode } = {}) {
    if (!theme || !theme.id || this.usedThemes.some(t => t.id == theme.id)) {
      throw new Error("Trying to add invalid builtIn theme");
    }

    this._builtInThemes.set(theme.id, theme);

    if (_prefs.getStringPref("selectedThemeID", DEFAULT_THEME_ID) == theme.id) {
      this.currentTheme = theme;
    }

    if (useInDarkMode) {
      _defaultDarkThemeID = theme.id;
    }
  },

  forgetBuiltInTheme(id) {
    if (!this._builtInThemes.has(id)) {
      let currentTheme = this.currentTheme;
      if (currentTheme && currentTheme.id == id) {
        this.currentTheme = null;
      }
    }
    return this._builtInThemes.delete(id);
  },

  clearBuiltInThemes() {
    for (let id of this._builtInThemes.keys()) {
      this.forgetBuiltInTheme(id);
    }
  },

  previewTheme(aData) {
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

  resetPreview() {
    if (_previewTimer) {
      _previewTimer.cancel();
      _previewTimer = null;
      _notifyWindows(this.currentThemeForDisplay);
    }
  },

  parseTheme(aString, aBaseURI) {
    try {
      return _sanitizeTheme(JSON.parse(aString), aBaseURI, false);
    } catch (e) {
      return null;
    }
  },

  /*
   * Try to update a single LWT.  If there is an XPI update, apply it
   * immediately.  If there is a regular LWT update, only apply it if
   * this is the current theme.
   *
   * Returns the LWT object (which could be the old one or a new one)
   * if this theme is still an LWT, or null if this theme should be
   * removed from the usedThemes list (ie, because it was updated to an
   * xpi packaged theme).
   */
  async _updateOneTheme(theme, isCurrent) {
    let req = new ServiceRequest();

    req.mozBackgroundRequest = true;
    req.overrideMimeType("text/plain");
    req.open("GET", theme.updateURL, true);
    // Prevent the request from reading from the cache.
    req.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    // Prevent the request from writing to the cache.
    req.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;

    await new Promise(resolve => {
      req.addEventListener("load", resolve, {once: true});
      req.send(null);
    });

    if (req.status != 200)
      return theme;

    let parsed;
    try {
      parsed = JSON.parse(req.responseText);
    } catch (e) {
      return theme;
    }

    if ("converted_theme" in parsed) {
      const {url, hash} = parsed.converted_theme;
      let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", hash);

      install.addListener({
        onDownloadEnded() {
          if (install.addon && install.type !== "theme") {
            Cu.reportError(`Refusing to update lightweight theme to a ${install.type} (from ${url})`);
            install.cancel();
            return false;
          }
          return true;
        },
      });

      let updated = null;
      try {
        updated = await install.install();
      } catch (e) { }

      if (updated) {
        if (isCurrent) {
          await updated.enable();
        }
        return null;
      }
    } else if (isCurrent) {
      // Double-parse of the response.  meh.
      let newData = this.parseTheme(req.responseText, theme.updateURL);
      if (!newData ||
          newData.id != theme.id ||
          _version(newData) == _version(theme))
        return theme;

      var currentTheme = this.currentTheme;
      if (currentTheme && currentTheme.id == theme.id) {
        this.currentTheme = newData;
        // Careful: the currentTheme setter has the side effect of
        // copying installDate which is not present in newData.
        return this.currentTheme;
      }
    }

    return theme;
  },

  async updateThemes() {
    if (!_prefs.getBoolPref("update.enabled", false))
      return;

    let allThemes;
    try {
      allThemes = JSON.parse(_prefs.getStringPref("usedThemes"));
    } catch (e) {
      return;
    }

    let selectedID = _prefs.getStringPref("selectedThemeID", DEFAULT_THEME_ID);
    let newThemes = await Promise.all(allThemes.map(
      t => this._updateOneTheme(t, t.id == selectedID)));
    newThemes = newThemes.filter(t => t);
    _prefs.setStringPref("usedThemes", JSON.stringify(newThemes));
  },

  /**
   * Switches to a new lightweight theme.
   *
   * @param  aData
   *         The lightweight theme to switch to
   */
  themeChanged(aData) {
    if (_previewTimer) {
      _previewTimer.cancel();
      _previewTimer = null;
    }

    if (aData) {
      _prefs.setCharPref("selectedThemeID", aData.id);
    } else {
      _prefs.setCharPref("selectedThemeID", "");
    }

    let themeToSwitchTo = aData;
    if (aData && aData.id == DEFAULT_THEME_ID && _defaultThemeIsInDarkMode &&
        _defaultDarkThemeID) {
      themeToSwitchTo =
        LightweightThemeManager.getUsedTheme(_defaultDarkThemeID);
    }

    if (themeToSwitchTo) {
      let usedThemes = _usedThemesExceptId(themeToSwitchTo.id);
      usedThemes.unshift(themeToSwitchTo);
      _updateUsedThemes(usedThemes);
      if (PERSIST_ENABLED) {
        LightweightThemeImageOptimizer.purge();
        _persistImages(themeToSwitchTo, () => {
          _notifyWindows(this.currentThemeForDisplay);
        });
      }
    }

    _notifyWindows(themeToSwitchTo);
    Services.obs.notifyObservers(null, "lightweight-theme-changed");
  },

  /**
   * Starts the Addons provider and enables the new lightweight theme if
   * necessary.
   */
  startup() {
    _prefs.addObserver("", _prefObserver);
  },

  /**
   * Shuts down the provider.
   */
  shutdown() {
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
   */
  addonChanged(aId, aType) {
    if (aType != ADDON_TYPE && aType != ADDON_TYPE_WEBEXT)
      return;

    let id = _getInternalID(aId);
    let current = this.currentTheme;

    if (current && id == current.id) {
      AddonManagerPrivate.callAddonListeners("onOperationCancelled",
                                             new AddonWrapper(current));
      return;
    }

    if (current) {
      if (current.id == id || (!aId && current.id == DEFAULT_THEME_ID))
        return;
      _themeIDBeingDisabled = current.id;
      let wrapper = new AddonWrapper(current);

      AddonManagerPrivate.callAddonListeners("onDisabling", wrapper, false);
      _prefs.setCharPref("selectedThemeID", "");
      AddonManagerPrivate.callAddonListeners("onDisabled", wrapper);
      _themeIDBeingDisabled = null;
    }

    if (id) {
      let theme = this.getUsedTheme(id);
      // WebExtension themes have an ID, but no LWT wrapper, so bail out here.
      if (!theme)
        return;
      _themeIDBeingEnabled = id;
      let wrapper = new AddonWrapper(theme);

      AddonManagerPrivate.callAddonListeners("onEnabling", wrapper, false);
      this.themeChanged(theme);
      AddonManagerPrivate.callAddonListeners("onEnabled", wrapper);

      _themeIDBeingEnabled = null;
    }
  },

  /**
   * Called when the system has either switched to, or switched away from a dark
   * theme.
   *
   * @param  aEvent
   *         The MediaQueryListEvent associated with the system theme change.
   */
  systemThemeChanged(aEvent) {
    let themeToSwitchTo = null;
    if (aEvent.matches && !_defaultThemeIsInDarkMode && _defaultDarkThemeID) {
      themeToSwitchTo = this.getUsedTheme(_defaultDarkThemeID);
      _defaultThemeIsInDarkMode = true;
    } else if (!aEvent.matches && _defaultThemeIsInDarkMode) {
      themeToSwitchTo = this.getUsedTheme(DEFAULT_THEME_ID);
      _defaultThemeIsInDarkMode = false;
    } else {
      // We are already set to the correct mode. Bail out early.
      return;
    }

    if (_prefs.getStringPref("selectedThemeID", "") != DEFAULT_THEME_ID) {
      return;
    }

    if (themeToSwitchTo) {
      let usedThemes = _usedThemesExceptId(themeToSwitchTo.id);
      usedThemes.unshift(themeToSwitchTo);
      _updateUsedThemes(usedThemes);
      if (PERSIST_ENABLED) {
        LightweightThemeImageOptimizer.purge();
        _persistImages(themeToSwitchTo, () => {
          _notifyWindows(this.currentThemeForDisplay);
        });
      }
    }

    _notifyWindows(themeToSwitchTo);
    Services.obs.notifyObservers(null, "lightweight-theme-changed");
  },

  /**
   * Handles system theme changes.
   *
   * @param  aEvent
   *         The MediaQueryListEvent associated with the system theme change.
   */
  handleEvent(aEvent) {
    if (aEvent.media == "(-moz-system-dark-theme)") {
      this.systemThemeChanged(aEvent);
    }
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   */
  async getAddonByID(aId) {
    let id = _getInternalID(aId);
    if (!id) {
      return null;
     }

    let theme = this.getUsedTheme(id);
    if (!theme) {
      return null;
    }

    return new AddonWrapper(theme);
  },

  /**
   * Called to get Addons of a particular type.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types.
   */
  getAddonsByTypes(aTypes) {
    if (aTypes && !aTypes.includes(ADDON_TYPE)) {
      return [];
    }

    return this.usedThemes.map(a => new AddonWrapper(a));
  },
};

const wrapperMap = new WeakMap();
let themeFor = wrapper => wrapperMap.get(wrapper);

/**
 * The AddonWrapper wraps lightweight theme to provide the data visible to
 * consumers of the AddonManager API.
 */
function AddonWrapper(aTheme) {
  wrapperMap.set(this, aTheme);
}

AddonWrapper.prototype = {
  get id() {
    return _getExternalID(themeFor(this).id);
  },

  get type() {
    return ADDON_TYPE;
  },

  get isActive() {
    let current = LightweightThemeManager.currentTheme;
    if (current)
      return themeFor(this).id == current.id;
    return false;
  },

  get name() {
    return themeFor(this).name;
  },

  get version() {
    let theme = themeFor(this);
    return "version" in theme ? theme.version : "";
  },

  get creator() {
    let theme = themeFor(this);
    return "author" in theme ? new AddonManagerPrivate.AddonAuthor(theme.author) : null;
  },

  get screenshots() {
    let url = themeFor(this).previewURL;
    return [new AddonManagerPrivate.AddonScreenshot(url)];
  },

  get pendingOperations() {
    let pending = AddonManager.PENDING_NONE;
    if (this.isActive == this.userDisabled)
      pending |= this.isActive ? AddonManager.PENDING_DISABLE : AddonManager.PENDING_ENABLE;
    return pending;
  },

  get operationsRequiringRestart() {
    return AddonManager.OP_NEEDS_RESTART_NONE;
  },

  get permissions() {
    let permissions = 0;

    // Do not allow uninstall of builtIn themes.
    if (!LightweightThemeManager._builtInThemes.has(themeFor(this).id))
      permissions = AddonManager.PERM_CAN_UNINSTALL;
    if (this.userDisabled)
      permissions |= AddonManager.PERM_CAN_ENABLE;
    else if (themeFor(this).id != DEFAULT_THEME_ID)
      permissions |= AddonManager.PERM_CAN_DISABLE;
    return permissions;
  },

  get userDisabled() {
    let id = themeFor(this).id;
    if (_themeIDBeingEnabled == id)
      return false;
    if (_themeIDBeingDisabled == id)
      return true;

    let current = LightweightThemeManager.currentTheme;
    return !current || current.id != id;
  },

  set userDisabled(val) {
    if (val == this.userDisabled)
      return val;

    if (val)
      LightweightThemeManager.currentTheme = null;
    else
      LightweightThemeManager.currentTheme = themeFor(this);

    return val;
  },

  async enable() {
    this.userDisabled = false;
  },
  async disable() {
    this.userDisabled = true;
  },

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

  uninstall() {
    LightweightThemeManager.forgetUsedTheme(themeFor(this).id);
  },

  cancelUninstall() {
    throw new Error("Theme is not marked to be uninstalled");
  },

  findUpdates(listener, reason, appVersion, platformVersion) {
    AddonManagerPrivate.callNoUpdateListeners(this, listener, reason, appVersion, platformVersion);
  },

  // Lightweight themes are always compatible
  isCompatibleWith(appVersion, platformVersion) {
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

["description", "homepageURL", "iconURL"].forEach(function(prop) {
  Object.defineProperty(AddonWrapper.prototype, prop, {
    get() {
      let theme = themeFor(this);
      return prop in theme ? theme[prop] : null;
    },
    enumarable: true,
  });
});

["installDate", "updateDate"].forEach(function(prop) {
  Object.defineProperty(AddonWrapper.prototype, prop, {
    get() {
      let theme = themeFor(this);
      return prop in theme ? new Date(theme[prop]) : null;
    },
    enumarable: true,
  });
});

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
  if (LightweightThemeManager._builtInThemes.has(id))
    return id;
  let len = id.length - ID_SUFFIX.length;
  if (len > 0 && id.substring(len) == ID_SUFFIX)
    return id.substring(0, len);
  return null;
}

function _getExternalID(id) {
  if (LightweightThemeManager._builtInThemes.has(id))
    return id;
  return id + ID_SUFFIX;
}

function _setCurrentTheme(aData, aLocal) {
  aData = _sanitizeTheme(aData, null, aLocal);

  let cancel = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
  cancel.data = false;
  Services.obs.notifyObservers(cancel, "lightweight-theme-change-requested",
                               JSON.stringify(aData));

  let notify = true;

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
    if (current && current.id != aData.id) {
      usedThemes.splice(1, 0, aData);
    } else {
      if (current && current.id == aData.id) {
        notify = false;
      }
      usedThemes.unshift(aData);
    }
    _updateUsedThemes(usedThemes);

    if (isInstall)
      AddonManagerPrivate.callAddonListeners("onInstalled", wrapper);
  }

  if (cancel.data)
    return null;

  if (notify) {
    AddonManagerPrivate.notifyAddonChanged(aData ? _getExternalID(aData.id) : null,
                                           ADDON_TYPE, false);
  }

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

      if (!resourceProtocolExp.test(val)) {
        return null;
      }
      if (prop == "updateURL" && requireSecureUpdates &&
          !val.startsWith("https:")) {
         return null;
      }
      return val;
    } catch (e) {
      return null;
    }
  }

  let result = {};
  for (let mandatoryProperty of MANDATORY) {
    let val = sanitizeProperty(mandatoryProperty);
    if (!val)
      throw Cr.NS_ERROR_INVALID_ARG;
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
  return LightweightThemeManager.usedThemes.filter(function(t) {
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

  _prefs.setStringPref("usedThemes", JSON.stringify(aList));

  Services.obs.notifyObservers(null, "lightweight-theme-list-changed");
}

function _notifyWindows(aThemeData) {
  Services.obs.notifyObservers(null, "lightweight-theme-styling-update",
                               JSON.stringify(aThemeData));
}

var _previewTimer;
var _previewTimerCallback = {
  notify() {
    LightweightThemeManager.resetPreview();
  }
};

/**
 * Called when any of the lightweightThemes preferences are changed.
 */
function _prefObserver(aSubject, aTopic, aData) {
  switch (aData) {
    case "maxUsedThemes":
      _maxUsedThemes = _prefs.getIntPref(aData, DEFAULT_MAX_USED_THEMES_COUNT);

      // Update the theme list to remove any themes over the number we keep
      _updateUsedThemes(LightweightThemeManager.usedThemes);
      break;
  }
}

function _persistImages(aData, aCallback) {
  function onSuccess(key) {
    return function() {
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

  persist.saveURI(sourceURI, 0,
                  null, Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
                  null, null, targetURI, null);
}

function _persistProgressListener(successCallback) {
  this.onLocationChange = function() {};
  this.onProgressChange = function() {};
  this.onStatusChange   = function() {};
  this.onSecurityChange = function() {};
  this.onStateChange    = function(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aRequest &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      try {
        if (aRequest.QueryInterface(Ci.nsIHttpChannel).requestSucceeded) {
          // success
          successCallback();
        }
      } catch (e) { }
      // failure
    }
  };
}

AddonManagerPrivate.registerProvider(LightweightThemeManager, [
  new AddonManagerPrivate.AddonType("theme", URI_EXTENSION_STRINGS,
                                    "type.themes.name",
                                    AddonManager.VIEW_TYPE_LIST, 5000)
]);
