/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LightweightThemeManager"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {AddonManager} = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

const MAX_PREVIEW_SECONDS = 30;

const MANDATORY = ["id", "name"];
const OPTIONAL = ["headerURL", "textcolor", "accentcolor",
                  "iconURL", "previewURL", "author", "description",
                  "homepageURL", "updateURL", "version"];

ChromeUtils.defineModuleGetter(this, "LightweightThemePersister",
  "resource://gre/modules/addons/LightweightThemePersister.jsm");
ChromeUtils.defineModuleGetter(this, "LightweightThemeImageOptimizer",
  "resource://gre/modules/addons/LightweightThemeImageOptimizer.jsm");
ChromeUtils.defineModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "ServiceRequest",
  "resource://gre/modules/ServiceRequest.jsm");


XPCOMUtils.defineLazyGetter(this, "_prefs", () => {
  return Services.prefs.getBranch("lightweightThemes.");
});

XPCOMUtils.defineLazyPreferenceGetter(this, "requireSecureUpdates",
                                      "extensions.checkUpdateSecurity", true);


// Holds optional fallback theme data that will be returned when no data for an
// active theme can be found. This the case for WebExtension Themes, for example.
var _fallbackThemeData = null;

// Holds whether or not the default theme should display in dark mode. This is
// typically the case when the OS has a dark system appearance.
var _defaultThemeIsInDarkMode = false;
// Holds the dark theme to be used if the OS has a dark system appearance and
// the default theme is selected.
var _defaultDarkThemeID = null;

var LightweightThemeManager = {
  set fallbackThemeData(data) {
    if (data && Object.getOwnPropertyNames(data).length) {
      _fallbackThemeData = Object.assign({}, data);
      if (LightweightThemePersister.persistEnabled) {
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

  isBuiltIn(theme) {
    return this._builtInThemes.has(theme.id);
  },

  get selectedThemeID() {
    return _prefs.getStringPref("selectedThemeID") || DEFAULT_THEME_ID;
  },

  get defaultDarkThemeID() {
    return _defaultDarkThemeID;
  },

  get usedThemes() {
    let themes = [];
    try {
      themes = JSON.parse(_prefs.getStringPref("usedThemes"));
    } catch (e) { }

    themes.push(...this._builtInThemes.values());
    return themes;
  },

  /*
   * Returns the currently active theme, but doesn't take a potentially
   * available fallback theme into account.
   *
   * This will always return the original theme data and not make use of
   * locally persisted resources.
   */
  get currentTheme() {
    let selectedThemeID = _prefs.getStringPref("selectedThemeID", DEFAULT_THEME_ID);

    let data = null;
    if (selectedThemeID) {
      data = this.getUsedTheme(selectedThemeID);
    }
    return data;
  },

  /*
   * Returns the currently active theme, taking the fallback theme into account
   * if we'd be using the default theme otherwise.
   *
   * This will always return the original theme data and not make use of
   * locally persisted resources.
   */
  get currentThemeWithFallback() {
    return _substituteDefaultThemeIfNeeded(this.currentTheme);
  },

  /*
   * Returns the currently active theme, taking the fallback theme into account
   * if we'd be using the default theme otherwise.
   *
   * This will rewrite the theme data to use locally persisted resources if
   * available.
   *
   * Unless you have any special requirements, this is what you normally want
   * to use in order to retrieve the currently active theme for use in the UI.
   */
  get currentThemeWithPersistedData() {
    let data = this.currentThemeWithFallback;

    if (data && LightweightThemePersister.persistEnabled) {
      data = LightweightThemePersister.getPersistedData(data);
    }

    return data;
  },

  set currentTheme(aData) {
    _setCurrentTheme(aData, false);
  },

  setCurrentTheme(aData) {
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

    var currentTheme = this.currentTheme;
    if (currentTheme && currentTheme.id == aId) {
      this.themeChanged(null);
    }

    _updateUsedThemes(_usedThemesExceptId(aId));
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
    aData = _substituteDefaultThemeIfNeeded(aData);

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
      _notifyWindows(this.currentThemeWithPersistedData);
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
    if (!theme.updateURL) {
      return theme;
    }

    let req = new ServiceRequest({mozAnon: true});

    req.mozBackgroundRequest = true;
    req.overrideMimeType("text/plain");
    req.open("GET", theme.updateURL, true);
    // Prevent the request from reading from the cache.
    req.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    // Prevent the request from writing to the cache.
    req.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;

    await new Promise(resolve => {
      req.addEventListener("load", resolve, {once: true});
      req.addEventListener("error", resolve, {once: true});
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
      let install = await AddonManager.getInstallForURL(url, {
        hash,
        telemetryInfo: {source: "lwt-converted-theme"},
      });

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
      t => this._updateOneTheme(t, t.id == selectedID).catch(err => {})));
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
      if (LightweightThemePersister.persistEnabled) {
        LightweightThemeImageOptimizer.purge();
        _persistImages(themeToSwitchTo, () => {
          _notifyWindows(this.currentThemeWithPersistedData);
        });
      }
    }

    _notifyWindows(themeToSwitchTo);
    Services.obs.notifyObservers(null, "lightweight-theme-changed");
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
      if (LightweightThemePersister.persistEnabled) {
        LightweightThemeImageOptimizer.purge();
        _persistImages(themeToSwitchTo, () => {
          _notifyWindows(this.currentThemeWithPersistedData);
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
};

function _setCurrentTheme(aData, aLocal) {
  aData = _sanitizeTheme(aData, null, aLocal);

  return (async () => {
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
      }

      let current = LightweightThemeManager.currentTheme;
      let usedThemes = _usedThemesExceptId(aData.id);
      if (current && current.id != aData.id) {
        usedThemes.splice(1, 0, aData);
      } else {
        usedThemes.unshift(aData);
      }
      _updateUsedThemes(usedThemes);
    }

    if (cancel.data)
      return null;

    return LightweightThemeManager.currentTheme;
  })();
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
      throw Components.Exception("Invalid argument", Cr.NS_ERROR_INVALID_ARG);
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

  _prefs.setStringPref("usedThemes", JSON.stringify(aList));

  Services.obs.notifyObservers(null, "lightweight-theme-list-changed");
}

function _notifyWindows(aThemeData) {
  Services.obs.notifyObservers(null, "lightweight-theme-styling-update",
                               JSON.stringify({theme: aThemeData}));
}

function _substituteDefaultThemeIfNeeded(aThemeData) {
  if (!aThemeData || aThemeData.id == DEFAULT_THEME_ID) {
    if (_fallbackThemeData) {
      return _fallbackThemeData;
    }
    if (_defaultThemeIsInDarkMode && _defaultDarkThemeID) {
      return LightweightThemeManager.getUsedTheme(_defaultDarkThemeID);
    }
  }

  return aThemeData;
}

var _previewTimer;
var _previewTimerCallback = {
  notify() {
    LightweightThemeManager.resetPreview();
  },
};

function _persistImages(aData, aCallback) {
  if (AppConstants.platform != "android") {
    // On Android, the LightweightThemeConsumer is responsible for doing this.
    LightweightThemePersister.persistImages(aData, aCallback);
  }
}
