/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LightweightThemeManager"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

const MANDATORY = ["id", "name"];
const OPTIONAL = ["headerURL", "textcolor", "accentcolor",
                  "iconURL", "previewURL", "author", "description",
                  "homepageURL", "version"];

ChromeUtils.defineModuleGetter(this, "LightweightThemePersister",
  "resource://gre/modules/addons/LightweightThemePersister.jsm");
ChromeUtils.defineModuleGetter(this, "LightweightThemeImageOptimizer",
  "resource://gre/modules/addons/LightweightThemeImageOptimizer.jsm");
ChromeUtils.defineModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");


XPCOMUtils.defineLazyGetter(this, "_prefs", () => {
  return Services.prefs.getBranch("lightweightThemes.");
});


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

  get usedThemes() {
    let themes = [];
    try {
      themes = JSON.parse(_prefs.getStringPref("usedThemes"));
    } catch (e) { }

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
    if (!theme)
      return;

    var currentTheme = this.currentTheme;
    if (currentTheme && currentTheme.id == aId) {
      this.themeChanged(null);
    }

    _updateUsedThemes(_usedThemesExceptId(aId));
  },

  parseTheme(aString, aBaseURI) {
    try {
      return _sanitizeTheme(JSON.parse(aString), aBaseURI, false);
    } catch (e) {
      return null;
    }
  },

  /**
   * Switches to a new lightweight theme.
   *
   * @param  aData
   *         The lightweight theme to switch to
   */
  themeChanged(aData) {
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

function _makeURI(aURL, aBaseURI) {
  return Services.io.newURI(aURL, null, aBaseURI);
}

function _updateUsedThemes(aList) {
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

function _persistImages(aData, aCallback) {
  if (AppConstants.platform != "android") {
    // On Android, the LightweightThemeConsumer is responsible for doing this.
    LightweightThemePersister.persistImages(aData, aCallback);
  }
}
