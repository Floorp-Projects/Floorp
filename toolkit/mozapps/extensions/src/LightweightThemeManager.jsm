/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Dao Gottwald <dao@mozilla.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var EXPORTED_SYMBOLS = ["LightweightThemeManager"];

const Cc = Components.classes;
const Ci = Components.interfaces;

const MAX_USED_THEMES_COUNT = 8;

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

__defineGetter__("_prefs", function () {
  delete this._prefs;
  return this._prefs =
         Cc["@mozilla.org/preferences-service;1"]
           .getService(Ci.nsIPrefService).getBranch("lightweightThemes.");
});

__defineGetter__("_observerService", function () {
  delete this._observerService;
  return this._observerService =
         Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
});

__defineGetter__("_ioService", function () {
  delete this._ioService;
  return this._ioService =
         Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
});

var LightweightThemeManager = {
  get usedThemes () {
    try {
      return JSON.parse(_prefs.getCharPref("usedThemes"));
    } catch (e) {
      return [];
    }
  },

  get currentTheme () {
    try {
      if (_prefs.getBoolPref("isThemeSelected"))
        var data = this.usedThemes[0];
    } catch (e) {}

    return data || null;
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
    let cancel = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
    cancel.data = false;
    _observerService.notifyObservers(cancel, "lightweight-theme-change-requested",
                                     JSON.stringify(aData));

    if (aData) {
      let usedThemes = _usedThemesExceptId(aData.id);
      if (cancel.data && _prefs.getBoolPref("isThemeSelected"))
        usedThemes.splice(1, 0, aData);
      else
        usedThemes.unshift(aData);
      _updateUsedThemes(usedThemes);
    }

    if (cancel.data)
      return null;

    if (_previewTimer) {
      _previewTimer.cancel();
      _previewTimer = null;
    }

    _prefs.setBoolPref("isThemeSelected", aData != null);
    _notifyWindows(aData);

    if (PERSIST_ENABLED && aData)
      _persistImages(aData);

    return aData;
  },

  getUsedTheme: function (aId) {
    var usedThemes = this.usedThemes;
    for (let i = 0; i < usedThemes.length; i++) {
      if (usedThemes[i].id == aId)
        return usedThemes[i];
    }
    return null;
  },

  forgetUsedTheme: function (aId) {
    var currentTheme = this.currentTheme;
    if (currentTheme && currentTheme.id == aId)
      this.currentTheme = null;

    _updateUsedThemes(_usedThemesExceptId(aId));
  },

  previewTheme: function (aData) {
    if (!aData)
      return;

    let cancel = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
    cancel.data = false;
    _observerService.notifyObservers(cancel, "lightweight-theme-preview-requested",
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

  resetPreview: function () {
    if (_previewTimer) {
      _previewTimer.cancel();
      _previewTimer = null;
      _notifyWindows(this.currentThemeForDisplay);
    }
  },

  parseTheme: function (aString, aBaseURI) {
    try {
      var data = JSON.parse(aString);
    } catch (e) {
      return null;
    }

    if (!data || typeof data != "object")
      return null;

    for (let prop in data) {
      if (typeof data[prop] == "string" &&
          (data[prop] = data[prop].trim()) &&
          (MANDATORY.indexOf(prop) > -1 || OPTIONAL.indexOf(prop) > -1)) {
        if (!/URL$/.test(prop))
          continue;

        try {
          data[prop] = _makeURI(data[prop], _makeURI(aBaseURI)).spec;
          if (/^https:/.test(data[prop]))
            continue;
          if (prop != "updateURL" && /^http:/.test(data[prop]))
            continue;
        } catch (e) {}
      }

      delete data[prop];
    }

    for (let i = 0; i < MANDATORY.length; i++) {
      if (!(MANDATORY[i] in data))
        return null;
    }

    return data;
  },

  updateCurrentTheme: function () {
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

    var self = this;
    req.onload = function () {
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
    };

    req.send(null);
  }
};

function _usedThemesExceptId(aId)
  LightweightThemeManager.usedThemes.filter(function (t) t.id != aId);

function _version(aThemeData)
  aThemeData.version || "";

function _makeURI(aURL, aBaseURI)
  _ioService.newURI(aURL, null, aBaseURI);

function _updateUsedThemes(aList) {
  if (aList.length > MAX_USED_THEMES_COUNT)
    aList.length = MAX_USED_THEMES_COUNT;

  _prefs.setCharPref("usedThemes", JSON.stringify(aList));

  _observerService.notifyObservers(null, "lightweight-theme-list-changed", null);
}

function _notifyWindows(aThemeData) {
  _observerService.notifyObservers(null, "lightweight-theme-changed",
                                   JSON.stringify(aThemeData));
}

var _previewTimer;
var _previewTimerCallback = {
  notify: function () {
    LightweightThemeManager.resetPreview();
  }
};

function _persistImages(aData) {
  function onSuccess(key) function () {
    let current = LightweightThemeManager.currentTheme;
    if (current && current.id == aData.id)
      _prefs.setBoolPref("persisted." + key, true);
  };

  for (let key in PERSIST_FILES) {
    _prefs.setBoolPref("persisted." + key, false);
    if (aData[key])
      _persistImage(aData[key], PERSIST_FILES[key], onSuccess(key));
  }
}

function _getLocalImageURI(localFileName) {
  var localFile = Cc["@mozilla.org/file/directory_service;1"]
                     .getService(Ci.nsIProperties)
                     .get("ProfD", Ci.nsILocalFile);
  localFile.append(localFileName);
  return _ioService.newFileURI(localFile);
}

function _persistImage(sourceURL, localFileName, callback) {
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

  persist.progressListener = new _persistProgressListener(callback);

  persist.saveURI(sourceURI, null, null, null, null, targetURI);
}

function _persistProgressListener(callback) {
  this.onLocationChange = function () {};
  this.onProgressChange = function () {};
  this.onStatusChange   = function () {};
  this.onSecurityChange = function () {};
  this.onStateChange    = function (aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aRequest &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      try {
        if (aRequest.QueryInterface(Ci.nsIHttpChannel).requestSucceeded) {
          // success
          callback();
          return;
        }
      } catch (e) { }
      // failure
    }
  };
}
