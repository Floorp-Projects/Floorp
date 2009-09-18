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
        return this.usedThemes[0];
    } catch (e) {}
    return null;
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
      _notifyWindows(this.currentTheme);
    }
  }
};

function _usedThemesExceptId(aId)
  LightweightThemeManager.usedThemes.filter(function (t) t.id != aId);

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
}

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
