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
  _languages: null,
  _msg: null,

  _messageActions: function pv__messageActions(aData) {
    if (aData == "prefs-restart-app") {
      // Notify all windows that an application quit has been requested
      var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

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
    if (this._msg) {
      let strings = Strings.browser;
      this.showMessage(strings.GetStringFromName("notificationRestart.normal"), "restart-app",
                       strings.GetStringFromName("notificationRestart.button"), false, "prefs-restart-app");
    }
  },

  hideRestart: function ev_hideRestart() {
    if (this._msg) {
      let notification = this._msg.getNotificationWithValue("restart-app");
      if (notification)
        notification.close();
    }
  },

  delayedInit: function pv__delayedInit() {
    if (this._languages)
      return;

    this._msg = document.getElementById("prefs-messages");
    this._languages = document.getElementById("prefs-languages");
    this._loadLocales();

    this._loadHomePage();
  },

  _loadLocales: function _loadLocales() {
    // Query available and selected locales
    let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
    chrome.QueryInterface(Ci.nsIToolkitChromeRegistry);

    let selectedLocale = chrome.getSelectedLocale("browser");
    let availableLocales = chrome.getLocalesForPackage("browser");

    let strings = Services.strings.createBundle("chrome://browser/content/languages.properties");

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
      let item = this._languages.appendItem(label, locale);
      if (locale == selectedLocale) {
        this._currentLocale = locale;
        selectedItem = item;
      }
      localeCount++;
    }

    // Are we using auto-detection?
    let autoDetect = false;
    try {
      autoDetect = Services.prefs.getBoolPref("intl.locale.matchOS");
    }
    catch (e) {}

    // Highlight current locale (or auto-detect entry)
    if (autoDetect) {
      this._languages.selectedItem = document.getElementById("prefs-languages-auto");
      this._currentLocale = "auto";
    } else {
      this._languages.selectedItem = selectedItem;
    }

    // Hide the setting if we only have one locale
    if (localeCount == 1)
      document.getElementById("prefs-uilanguage").hidden = true;
  },

  updateLocale: function updateLocale() {
    // Which locale did the user select?
    let newLocale = this._languages.selectedItem.value;
    let prefs = Services.prefs;

    if (newLocale == "auto") {
      if (prefs.prefHasUserValue("general.useragent.locale"))
        prefs.clearUserPref("general.useragent.locale");
      prefs.setBoolPref("intl.locale.matchOS", true);
    } else {
      prefs.setBoolPref("intl.locale.matchOS", false);
      prefs.setCharPref("general.useragent.locale", newLocale);
    }

    // Show the restart notification, if needed
    if (this._currentLocale == newLocale)
      this.hideRestart();
    else
      this.showRestart();
  },

  _showHomePageHint: function _showHomePageHint(aHint) {
    if (aHint)
      document.getElementById("prefs-homepage").setAttribute("desc", aHint);
    else
      document.getElementById("prefs-homepage").removeAttribute("desc");
  },

  _loadHomePage: function _loadHomePage() {
    let url = Browser.getHomePage();
    let value = "default";
    let display = url;
    try {
      display = Services.prefs.getComplexValue("browser.startup.homepage.title", Ci.nsIPrefLocalizedString).data;
    } catch (e) { }

    switch (url) {
      case "about:empty":
        value = "none";
        display = null;
        break;
      case "about:home":
        value = "default";
        display = null;
        break;
      default:
        value = "custom";
        break;
    }

    // Show or hide the title or URL of the custom homepage
    this._showHomePageHint(display);

    // Add the helper "Custom Page" item in the menulist, if needed
    let options = document.getElementById("prefs-homepage-options");
    if (value == "custom") {
      // Make sure nothing is selected and just use a label to show the state
      options.appendItem(Strings.browser.GetStringFromName("homepage.custom2"), "custom");
    }

    // Select the right menulist item
    options.value = value;
  },

  updateHomePageList: function updateHomePageMenuList() {
    // Update the "Use Current Page" item in the menulist by disabling it if
    // the current page is already the user homepage
    let currentUrl = Browser.selectedBrowser.currentURI.spec;
    let currentHomepage = Browser.getHomePage();
    let isHomepage = (currentHomepage == currentUrl);
    let itemRow = document.getElementById("prefs-homepage-currentpage");
    if (currentHomepage == "about:home") {
      itemRow.disabled = isHomepage;
      itemRow.hidden = false;
    } else {
      itemRow.hidden = isHomepage;
      itemRow.disabled = false;
    }
  },

  updateHomePage: function updateHomePage() {
    let options = document.getElementById("prefs-homepage-options");
    let value = options.selectedItem.value;

    let url = "about:home";
    let display = null;

    switch (value) {
      case "none":
        url = "about:empty";
        break;
      case "default":
        url = "about:home";
        break;
      case "currentpage":
        // If the selected page is the about:home page, emulate the default case
        let currentURL = Browser.selectedBrowser.currentURI.spec;
        if (currentURL == "about:home") {
          value = "default";
        } else {
          url = currentURL;
          display = Browser.selectedBrowser.contentTitle || currentURL;
        }
        break;
      case "custom":
        // If value is custom, this means the user is trying to
        // set homepage to the same custom value. Do nothing in
        // this case.
        return;
    }

    // Show or hide the title or URL of the custom homepage
    this._showHomePageHint(display);

    // Is the helper already in the list
    let helper = null;
    let items = options.menupopup.getElementsByAttribute("value", "custom");
    if (items && items.length)
      helper = items[0];

    // Update the helper "Custom Page" item in the menulist
    if (value == "currentpage") {
      // If the helper item is not already in the list, we need to put it there
      // (this can happen when changing from one custom page to another)
      if (!helper)
        helper = options.appendItem(Strings.browser.GetStringFromName("homepage.custom2"), "custom");

      options.selectedItem = helper;
    } else {
      if (helper)
        options.menupopup.removeChild(helper);

      options.selectedItem = options.menupopup.getElementsByAttribute("value", value)[0];
    }

    // Save the homepage URL to a preference
    let pls = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(Ci.nsIPrefLocalizedString);
    pls.data = url;
    Services.prefs.setComplexValue("browser.startup.homepage", Ci.nsIPrefLocalizedString, pls);

    // Save the homepage title to a preference
    pls.data = display;
    Services.prefs.setComplexValue("browser.startup.homepage.title", Ci.nsIPrefLocalizedString, pls);
  }
};
