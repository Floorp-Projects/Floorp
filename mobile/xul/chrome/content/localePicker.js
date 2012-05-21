/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource:///modules/LocaleRepository.jsm");

let stringPrefs = [
  { selector: "#continue-in-button", pref: "continueIn", data: ["CURRENT_LOCALE"] },
  { selector: "#change-language", pref: "choose", data: null },
  { selector: "#picker-title", pref: "chooseLanguage", data: null },
  { selector: "#continue-button", pref: "continue", data: null },
  { selector: "#cancel-button", pref: "cancel", data: null },
  { selector: "#installing-message", pref: "installing", data: ["CURRENT_LOCALE"]  },
  { selector: "#cancel-install-button", pref: "cancel", data: null },
  { selector: "#installing-error", pref: "installerror", data: null },
  { selector: "#install-continue", pref: "continue", data: null },
  { selector: "#loading-label", pref: "loading", data: null }
];

let LocaleUI = {
  _strings: null,

  get strings() {
    if (!this._strings)
      this._strings = Services.strings.createBundle("chrome://browser/locale/localepicker.properties");
    return this._strings;
  },

  set strings(aVal) {
    this._strings = aVal;
  },

  get mainPage() {
    delete this.mainPage;
    return this.mainPage = document.getElementById("main-page");
  },

  get pickerpage() {
    delete this.pickerpage;
    return this.pickerpage = document.getElementById("picker-page");
  },

  get installerPage() {
    delete this.installerPage;
    return this.installerPage = document.getElementById("installer-page");
  },

  get deck() {
    delete this.deck;
    return this.deck = document.getElementById("language-deck");
  },

  _availableLocales: null,
  get availableLocales() {
    if (!this._availableLocales) {
      let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
      chrome.QueryInterface(Ci.nsIToolkitChromeRegistry);
      let strings = Services.strings.createBundle("chrome://browser/content/languages.properties");
      let availableLocales = chrome.getLocalesForPackage("browser");

      this._availableLocales = [];
      while (availableLocales.hasMore()) {
        let locale = availableLocales.getNext();
        let label = locale;
        try { label = strings.GetStringFromName(locale); }
        catch (e) { }
        this._availableLocales.push({ addon: { id: locale, name: label, targetLocale: locale }});
      }
    }
    return this._availableLocales;
  },

  pendingInstall: null, // used to cancel an install

  get selectedPanel() {
    return this.deck.selectedPanel;
  },

  set selectedPanel(aPanel) {
    this.deck.selectedPanel = aPanel;
  },

  get list() {
    delete this.list;
    return this.list = document.getElementById("language-list");
  },

  _createItem: function lp__createItem(aId, aText, aLocale) {
    let item = document.createElement("richlistitem");
    item.setAttribute("id", aId);

    let description = document.createElement("description");
    description.appendChild(document.createTextNode(aText));
    description.setAttribute('flex', 1);
    item.appendChild(description);
    item.setAttribute("locale", getTargetLocale(aLocale.addon));

    if (aLocale) {
      item.locale = aLocale.addon;
      let checkbox = document.createElement("image");
      checkbox.classList.add("checkbox");
      item.appendChild(checkbox);
    } else {
      item.classList.add("message");
    }
    return item;
  },

  addLocales: function lp_addLocales(aLocales) {
    let fragment = document.createDocumentFragment();
    let selectedItem = null;
    let bestMatch = NO_MATCH;

    for each (let locale in aLocales) {
      let targetLocale = getTargetLocale(locale.addon);
      if (document.querySelector('[locale="' + targetLocale + '"]'))
        continue;

      let item = this._createItem(targetLocale, "\u202A" + locale.addon.name + "\u202C", locale);
      let match = localesMatch(targetLocale, this.locale);
      if (match > bestMatch) {
        bestMatch = match;
        selectedItem = item;
      }
      fragment.appendChild(item);
    }
    this.list.appendChild(fragment);
    if (selectedItem && !this.list.selectedItem);
      this.list.selectedItem = selectedItem;
  },

  loadLocales: function() {
    while (this.list.firstChild)
      this.list.removeChild(this.list.firstChild);
    this.addLocales(this.availableLocales);
    LocaleRepository.getLocales(this.addLocales.bind(this));
  },

  showPicker: function() {
    LocaleUI.selectedPanel = LocaleUI.pickerpage;
    LocaleUI.loadLocales();
  },

  closePicker: function() {
    if (this.pendingInstall) {
      Services.prefs.setBoolPref("intl.locale.matchOS", false);
      Services.prefs.setCharPref("general.useragent.locale", getTargetLocale(this.pendingInstall));
    }
    if (window.opener)
      this.closeWindow();
    else
      this.selectedPanel = this.mainPage;
  },

  _locale: "",

  set locale(aVal) {
    Services.prefs.setBoolPref("intl.locale.matchOS", false);
    Services.prefs.setCharPref("general.useragent.locale", aVal);
    this._locale = aVal;

    this.strings = null;
    this.updateStrings();
  },

  get locale() {
    return this._locale;
  },

  set installStatus(aVal) {
    this.installerPage.selectedPanel = document.getElementById("installer-page-" + aVal);
  },

  clearInstallError: function() {
    this.installStatus = "installing";
    this.selectedPanel = this.pickerpage;
  },

  selectLocale: function(aEvent) {
    let locale = this.list.selectedItem.locale;
    if (locale.install) {
      LocaleUI.strings = new FakeStringBundle(locale);
      this.updateStrings(locale);
    } else {
      this.locale = getTargetLocale(locale);
      if (this.pendingInstall)
        this.pendingInstall = null;
    }
  },

  installAddon: function lp_installAddon() {
    let locale = LocaleUI.list.selectedItem.locale;

    if (locale.install) {
      LocaleUI.pendingInstall = locale;
      LocaleUI.selectedPanel = LocaleUI.installerPage;
      locale.install.addListener(installListener);
      locale.install.install();
    } else {
      this.closeWindow();
    }
  },

  goBack: function goBack() {
    switch (this.selectedPanel) {
      case this.mainPage:
        // Do nothing on the "Loading..." screen.
        break;
      case this.pickerpage:
        this.cancelPicker();
        break;
      case this.installerPage:
        this.cancelInstall();
        this.showPicker();
        break;
    }
  },

  cancelPicker: function() {
    if (this.pendingInstall)
      this.pendingInstall = null;

    // restore the last known "good" locale
    this.locale = this.defaultLocale;
    this.closePicker();
  },

  closeWindow : function() {
    var buildID =  Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo).platformBuildID;
    Services.prefs.setCharPref("extensions.compatability.locales.buildid", buildID);

    // Trying to close this window and open a new one results in a corrupt UI.
    if (!window.opener && LocaleUI.pendingInstall) {
      // a new locale was installed, restart the browser
      let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");
    
      if (cancelQuit.data == false) {
        let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
        appStartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eForceQuit);
        return;
      }
    }

    // just open the window
    let argString = null;
    if (window.arguments) {
      argString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
      argString.data = window.arguments.join(",");
    }

    if (!Services.wm.getMostRecentWindow("navigator:browser"))
      Services.ww.openWindow(window, "chrome://browser/content/browser.xul", "_blank", "chrome,dialog=no,all", argString);

    window.close();
  },

  cancelInstall: function () {
    if (LocaleUI.pendingInstall) {
      let addonInstall = LocaleUI.pendingInstall.install;
      try { addonInstall.cancel(); }
      catch(ex) { }
      LocaleUI.pendingInstall = null;
    }
  },

  updateStrings: function (aAddon) {
    stringPrefs.forEach(function(aPref) {
      if (!aPref.element)
        aPref.element = document.querySelector(aPref.selector);
  
      let string = "";
      try {
        string = getString(aPref.pref, aPref.data, aAddon);
      } catch(ex) { }
      aPref.element.textContent = string;
    });
  }
}

// Gets the target locale for an addon
// For now this returns the targetLocale, although if and addon doesn't
// specify a targetLocale we could attempt to guess the locale from the addon's name
function getTargetLocale(aAddon) {
  return aAddon.targetLocale;
}

// Gets a particular string for the passed in locale
// Parameters: aStringName - The name of the string property to get
//             aDataset - an array of properties to use in a formatted string
//             aAddon - An addon to attempt to get dataset properties from
function getString(aStringName, aDataSet, aAddon) {
  if (aDataSet) {
    let databundle = aDataSet.map(function(aData) {
      switch (aData) {
        case "CURRENT_LOCALE" :
          if (aAddon)
            return aAddon.name;
          try { return LocaleUI.strings.GetStringFromName("name"); }
          catch(ex) { return null; }
          break;
        default :
      }
      return "";
    });
    if (databundle.some(function(aItem) aItem === null))
      throw("String not found");
    return LocaleUI.strings.formatStringFromName(aStringName, databundle, databundle.length);
  }

  return LocaleUI.strings.GetStringFromName(aStringName);
}

let installListener = {
  onNewInstall: function(install) { },
  onDownloadStarted: function(install) { },
  onDownloadProgress: function(install) { },
  onDownloadEnded: function(install) { },
  onDownloadCancelled: function(install) {
    LocaleUI.cancelInstall();
    LocaleUI.showPicker();
  },
  onDownloadFailed: function(install) {
    LocaleUI.cancelInstall();
    LocaleUI.installStatus = "error";
  },
  onInstallStarted: function(install) { },
  onInstallEnded: function(install, addon) {
    LocaleUI.locale = getTargetLocale(LocaleUI.pendingInstall);
    LocaleUI.closeWindow();
  },
  onInstallCancelled: function(install) {
    LocaleUI.cancelInstall();
    LocaleUI.showPicker();
  },
  onInstallFailed: function(install) {
    LocaleUI.cancelInstall();
    LocaleUI.installStatus = "error";
  },
  onExternalInstall: function(install, existingAddon, needsRestart) { }
}

const PERFECT_MATCH = 2;
const GOOD_MATCH = 1;
const NO_MATCH = 0;
//Compares two locales of the form AB or AB-CD
//returns GOOD_MATCH if AB == AB in both locales, PERFECT_MATCH if AB-CD == AB-CD
function localesMatch(aLocale1, aLocale2) {
  if (aLocale1 == aLocale2)
    return PERFECT_MATCH;

  let short1 = aLocale1.split("-")[0];
  let short2 = aLocale2.split("-")[0];
  return (short1 == short2) ? GOOD_MATCH : NO_MATCH;
}

function start() {
  let mouseModule = new MouseModule();

  // if we have gotten this far, we can assume that we don't have anything matching the system
  // locale and we should show the locale picker
  LocaleUI.mainPage.setAttribute("mode", "loading");
  let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
  chrome.QueryInterface(Ci.nsIToolkitChromeRegistry);
  LocaleUI._locale = chrome.getSelectedLocale("browser");
  LocaleUI.updateStrings();

  // if we haven't gotten the list of available locales from AMO within 5 seconds, we give up
  // users can try downloading the list again by selecting "Choose another locale"
  let timeout = setTimeout(function() {
    LocaleUI.mainPage.removeAttribute("mode");
    timeout = null;
  }, 5000);

  // update the page strings and show the correct page
  LocaleUI.defaultLocale = LocaleUI._locale;
  window.addEventListener("resize", resizeHandler, false);

  // if we have an opener, we are probably coming from the prefs pane
  // and can jump straight to the list of languages
  if (window.opener) {
    LocaleUI.updateStrings();
    LocaleUI.showPicker();
    resizeHandler();
    return;
  }

  // Look on AMO for something that matches the system locale
  LocaleRepository.getLocales(function lp_initalDownload(aLocales) {
    if (!LocaleUI.mainPage.hasAttribute("mode")) return;

    clearTimeout(timeout);
    LocaleUI.mainPage.removeAttribute("mode");

    let localeService = Cc["@mozilla.org/intl/nslocaleservice;1"].getService(Ci.nsILocaleService);
    let currentLocale = localeService.getSystemLocale().getCategory("NSILOCALE_CTYPE");
    if (Services.prefs.prefHasUserValue("general.useragent.locale")) {
      currentLocale = Services.prefs.getCharPref("general.useragent.locale");
    }

    let match = NO_MATCH;
    let matchingLocale = null;

    for each (let locale in aLocales) {
      let targetLocale = getTargetLocale(locale.addon);
      let newMatch = localesMatch(targetLocale, currentLocale);
      if (newMatch > match) {
        matchingLocale = locale;
        match = newMatch;
      }
    }

    if (matchingLocale) {
      // if we found something, try to install it automatically
      AddonManager.getAddonByID(matchingLocale.addon.id, function (aAddon) {
        // if this locale is already installed, but is user disabled,
        // bail out of here.
        if (aAddon && aAddon.userDisabled) {
          Services.prefs.clearUserPref("general.useragent.locale");
          LocaleUI.closeWindow();
          return;
        }
        // if we found something, try to install it automatically
        LocaleUI.strings = new FakeStringBundle(matchingLocale.addon);
        LocaleUI.updateStrings();
        LocaleUI.pendingInstall = matchingLocale.addon;
  
        LocaleUI.selectedPanel = LocaleUI.installerPage;
        matchingLocale.addon.install.addListener(installListener);
        matchingLocale.addon.install.install();
      });
    }
  });
}

function resizeHandler() {
  let elements = document.getElementsByClassName("window-width");
  for (let i = 0; i < elements.length; i++)
    elements[i].setAttribute("width", Math.min(800, window.innerWidth));
}


function FakeStringBundle(aAddon) {
  let regex = /(.*)(?:=)(.*)/g;
  this.strings = {};
  let res = regex.exec(aAddon.strings);
  while (res) {
    this.strings[res[1].trim()] = res[2].trim();
    res = regex.exec(aAddon.strings);
  }
}

FakeStringBundle.prototype = {
  formatStringFromName: function(aName, aParams, aLength) {
    let txt = this.strings[aName];
    for (var i = 0; i < aLength; i++) {
      txt = txt.replace("%S", aParams[i]);
    }
    return txt;
  },
  GetStringFromName: function(aName) {
    return this.strings[aName];
  }
}
