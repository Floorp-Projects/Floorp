// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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

var PreferencesView = {
  _currentLocale: null,
  _list: null,
  _msg: null,
  _restartCount: 0,

  _messageActions: function ev__messageActions(aData) {
    if (aData == "prefs-restart-app") {
      // Notify all windows that an application quit has been requested
      var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
      var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      os.notifyObservers(cancelQuit, "quit-application-requested", "restart");

      // If nothing aborted, quit the app
      if (cancelQuit.data == false) {
        let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
        appStartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
      }
    }
  },

  showMessage: function ev_showMessage(aMsg, aValue, aButtonLabel, aShowCloseButton, aNotifyData) {
    let notification = this._msg.getNotificationWithValue(aValue);
    if (notification)
      return;

    let self = this;
    let buttons = null;
    if (aButtonLabel) {
      buttons = [ {
        label: aButtonLabel,
        accessKey: "",
        data: aNotifyData,
        callback: function(aNotification, aButton) {
          self._messageActions(aButton.data);
          return true;
        }
      } ];
    }

    this._msg.appendNotification(aMsg, aValue, "", this._msg.PRIORITY_WARNING_LOW, buttons).hideclose = !aShowCloseButton;
  },

  showRestart: function ev_showRestart() {
    // Increment the count in case the view is not completely initialized
    this._restartCount++;

    if (this._msg) {
      let strings = document.getElementById("bundle_browser");
      this.showMessage(strings.getString("notificationRestart.label"), "restart-app",
                       strings.getString("notificationRestart.button"), false, "prefs-restart-app");
    }
  },

  hideRestart: function ev_hideRestart() {
    this._restartCount--;
    if (this._restartCount == 0 && this._msg) {
      let notification = this._msg.getNotificationWithValue("restart-app");
      if (notification)
        notification.close();
    }
  },

  init: function ev_init() {
    if (this._msg)
      return;

    this._msg = document.getElementById("prefs-messages");

    let self = this;
    let panels = document.getElementById("panel-items");
    panels.addEventListener("select",
                            function(aEvent) {
                              if (panels.selectedPanel.id == "prefs-container")
                                self._delayedInit();
                            },
                            false);
  },

  _delayedInit: function ev__delayedInit() {
    if (this._list)
      return;

    this._list = document.getElementById("prefs-languages");
    this._loadLocales();
  },

  _loadLocales: function _loadLocales() {
    // Query available and selected locales
    let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
    chrome.QueryInterface(Ci.nsIToolkitChromeRegistry);
    
    let selectedLocale = chrome.getSelectedLocale("browser");
    let availableLocales = chrome.getLocalesForPackage("browser");
    
    let bundles = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
    let strings = bundles.createBundle("chrome://browser/content/languages.properties");

    // Render locale menulist by iterating through the query result from getLocalesForPackage()
    let selectedItem = null;
    let localeCount = 0;
    while (availableLocales.hasMore()) {
      let locale = availableLocales.getNext();
      try {
        var label = strings.GetStringFromName(locale);
      } catch (e) {
        label = locale;
      }
      let item = this._list.appendItem(label, locale);
      if (locale == selectedLocale) {
        this._currentLocale = locale;
        selectedItem = item;
      }
      localeCount++;
    }

    // Are we using auto-detection?
    let autoDetect = false;
    try {
      autoDetect = gPrefService.getBoolPref("intl.locale.matchOS");
    }
    catch (e) {}
    
    // Highlight current locale (or auto-detect entry)
    if (autoDetect)
      this._list.selectedItem = document.getElementById("prefs-languages-auto");
    else
      this._list.selectedItem = selectedItem;
    
    // Hide the setting if we only have one locale
    if (localeCount == 1)
      document.getElementById("prefs-uilanguage").hidden = true;
  },
  
  updateLocale: function updateLocale() {
    // Which locale did the user select?
    let newLocale = this._list.selectedItem.value;
    
    if (newLocale == "auto") {
      if (gPrefService.prefHasUserValue("general.useragent.locale"))
        gPrefService.clearUserPref("general.useragent.locale");
      gPrefService.setBoolPref("intl.locale.matchOS", true);
    } else {
      gPrefService.setBoolPref("intl.locale.matchOS", false);
      gPrefService.setCharPref("general.useragent.locale", newLocale);
    }

    // Show the restart notification, if needed
    if (this._currentLocale == newLocale)
      this.hideRestart();
    else
      this.showRestart();
  }  
};
