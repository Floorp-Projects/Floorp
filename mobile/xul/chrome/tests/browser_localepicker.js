/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const RELATIVE_DIR = "browser/mobile/chrome/tests/";
const TESTROOT     = "http://example.com/" + RELATIVE_DIR;
const PREF_GET_LOCALES = "extensions.getLocales.get.url";

var gAvailable = [];

var restartObserver = {
  observe: function(aSubject, aTopic, aData) {
    // cancel restart requests
    aSubject.QueryInterface(Ci.nsISupportsPRBool);
    aSubject.data = true;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
}

function test() {
  Services.obs.addObserver(restartObserver, "quit-application-requested", false);
  waitForExplicitFinish();
  Services.prefs.setCharPref(PREF_GET_LOCALES, TESTROOT + "locales_list.sjs?numvalid=2");

  let chromeReg = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService(Components.interfaces.nsIXULChromeRegistry);
  chromeReg.QueryInterface(Ci.nsIToolkitChromeRegistry);
  let availableLocales = chromeReg.getLocalesForPackage("browser");
  while (availableLocales.hasMore())
    gAvailable.push( availableLocales.getNext() );


  // in order to test restart notifications being shown, we much open the settings panel once
  let settingsButton = document.getElementById("tool-panel-open");
  settingsButton.click();
  waitForAndContinue(runNextTest, function() {
    return !document.getElementById("panel-container").hidden;
  });
}

function end_test() {
  BrowserUI.hidePanel();
  Services.prefs.clearUserPref(PREF_GET_LOCALES);
  Services.obs.removeObserver(restartObserver, "quit-application-requested");
}

registerCleanupFunction(end_test);

function CheckListLoad(aList, aLength) {
  return function() {
    return aList.childNodes.length == aLength;
  }
}

function CheckDeck(aDeck, aPanel) {
  return function() {
    return aDeck.selectedPanel == aPanel;
  }
}

function LocaleTest(aName, aOptions) {
  var install = null;
  return {
    desc: aName,
    win: null,
    run: function lt_run() {
      this.loadedWindow = this.loadedWindow.bind(this);
      this.windowClosed = this.windowClosed.bind(this);
      this.win = Services.ww.openWindow(aOptions.opener, "chrome://browser/content/localePicker.xul", "_browser", "chrome,dialog=no,all", null);
      this.win.addEventListener("load", this.loadedWindow, false);
    },
  
    loadedWindow: function lt_loadedWindow() {
      this.win.removeEventListener("load", this.loadedWindow, false);
      if (aOptions.opener)
        setTimeout(this.delayedLoadPicker.bind(this), 0);
      else
        setTimeout(this.delayedLoadMain.bind(this), 0);
    },
  
    delayedLoadMain: function lt_delayedLoadMain() {
      let deck = this.win.document.getElementById("language-deck");
      let mainPage = this.win.document.getElementById("main-page");
      is(deck.selectedPanel, mainPage, "Deck is showing the main page");

      if (aOptions.loadLocalesList) {
        // load the locales list
        let changeButton = this.win.document.getElementById("change-language");
        changeButton.click();
        this.delayedLoadPicker();
      } else {
        // click the "Continue in English" button
        let continueButton = this.win.document.getElementById("continue-in-button");
        ok(/english/i.test(continueButton.textContent), "Continue button says English");
        this.win.addEventListener("unload", this.windowClosed, false);
        continueButton.click();
      }
    },

    delayedLoadPicker: function lt_delayedLoadPicker() {
      let deck = this.win.document.getElementById("language-deck");
      let pickerPage = this.win.document.getElementById("picker-page");
      is(deck.selectedPanel, pickerPage, "Deck is showing the picker page");
  
      let list = this.win.document.getElementById("language-list");
      // wait till the list shows the number of locales bundled with this build + the 2 from the downloaded list
      waitForAndContinue(this.listLoaded.bind(this), CheckListLoad(list, gAvailable.length + 2));
    },
  
    listLoaded: function() {
      let continueButton = this.win.document.getElementById("continue-button");
      let cancelButton = this.win.document.getElementById("cancel-button");
      ok(/continue/i.test(continueButton.textContent), "Continue button has correct text");
      ok(/cancel/i.test(cancelButton.textContent), "Cancel button has correct text");
  
      let list = this.win.document.getElementById("language-list");
      is(list.childNodes.length, gAvailable.length + 2, "List has correct number of children");

      let nextSelected = null;
      let selectedItem = null;
      for(var i = 0; i < list.childNodes.length; i++) {
        let child = list.childNodes[i];
        if (/english/i.test(child.textContent)) {
          ok(child.hasAttribute("selected"), "English is initially selected");
          selectedItem = child;
        } else {
          ok(!child.hasAttribute("selected"), "Language is not selected");
          if (aOptions.selectAddon && child.textContent == aOptions.selectAddon.name)
            nextSelected = child;
        }
      }
      this.testInstallingItem(nextSelected);
    },
  
    testInstallingItem: function lt_testInstallingItem(aSelect) {
      let continueButton = this.win.document.getElementById("continue-button");
      let cancelButton = this.win.document.getElementById("cancel-button");

      if (aSelect) {
        aSelect.click();
        is(continueButton.textContent, aOptions.selectAddon.continueButton, "Continue button says " + aOptions.selectAddon.continueButton);
        is(cancelButton.textContent, aOptions.selectAddon.cancelButton, "Cancel button says " + aOptions.selectAddon.cancelButton);
        let title = this.win.document.getElementById("picker-title");
        is(title.textContent, aOptions.selectAddon.title, "Title says " + aOptions.selectAddon.title);
        continueButton.click();
 
        let deck = this.win.document.getElementById("language-deck");
        let installerPage = this.win.document.getElementById("installer-page");
        is(deck.selectedPanel, installerPage, "Deck is showing the installer page");

        let installingPage = this.win.document.getElementById("installer-page-installing");
        is(installerPage.selectedPanel, installingPage, "Installer is showing installing page");
        let installMsg = this.win.document.getElementById("installing-message");
        is(installMsg.textContent, aOptions.selectAddon.installMessage, "Installer is showing correct message");

        if (aOptions.selectAddon.willFail) {
          let failedPage = this.win.document.getElementById("installer-page-error");
          waitForAndContinue(this.installFailed.bind(this), CheckDeck(installerPage, failedPage));
        } else {
          let install = aSelect.locale;
          this.win.addEventListener("unload", this.windowClosed, false);
        }
      } else {
         this.cancelList();
      }
    },

    installFailed: function lt_installFailed() {
      let continueButton = this.win.document.getElementById("install-continue");
      is(continueButton.textContent, aOptions.selectAddon.installFailed, "Install failed button has correct label");
      continueButton.click();

      let deck = this.win.document.getElementById("language-deck");
      let pickerPage = this.win.document.getElementById("picker-page");
      is(deck.selectedPanel, pickerPage, "Deck is showing the picker page");
      this.cancelList();
    },

    cancelList: function lt_cancelList() {
      this.win.addEventListener("unload", this.windowClosed, false);

      let cancelButton = this.win.document.getElementById("cancel-button");
      cancelButton.click();
      if (!aOptions.opener) {
        // canceling out of the list, should revert back to english ui
        let deck = this.win.document.getElementById("language-deck");
        let mainPage = this.win.document.getElementById("main-page");
        is(deck.selectedPanel, mainPage, "Deck is showing the main page again");
        let continueButton = this.win.document.getElementById("continue-in-button");
        ok(/english/i.test(continueButton.textContent), "Cancelling returned the UI to English");
        continueButton.click();
      }
    },

    windowClosed: function lt_windowClosed(aEvent) {
      this.checkMainUI(aOptions.selectAddon);

      Services.prefs.clearUserPref("intl.locale.matchOS");
      Services.prefs.clearUserPref("general.useragent.locale");
      window.PreferencesView.hideRestart();

      if (install)
        install.uninstall();

      runNextTest();
    },

    checkMainUI: function(aAddon) {
      let systemPref = "";
      let userAgentPref = "";
      try {
        systemPref = Services.prefs.getBoolPref("intl.locale.matchOS");
        userAgentPref = Services.prefs.getCharPref("general.useragent.locale")
      } catch(ex) { }

      let notification = document.getElementById("prefs-messages").getNotificationWithValue("restart-app");
      let showRestart = aAddon ? !aAddon.willFail : false;
      is(!!notification, showRestart, "Restart message is " + (showRestart ? "" : "not ") + "shown");

      // check that locale pref has been updated
      let localeName = aAddon ? aAddon.locale : "en-US";
      is(systemPref, false, "Match system locale is false");
      is(userAgentPref, localeName, "User agent locale is " + localeName);
      let buttonLabel = aAddon ? aAddon.localeName : "English (US)";
      is(document.getElementById("prefs-uilanguage-button").getAttribute("label"), buttonLabel, "Locale button says " + buttonLabel);
    }
  }
}

let invalidInstall = {
  name: "Test Locale 0",
  installMessage: "INSTALLINGTest Locale 0",
  continueButton: "CONTINUE",
  cancelButton: "CANCEL",
  title: "CHOOSELANGUAGE",
  installFailed: "CONTINUE",
  locale: "en-US",
  localeName: "English (US)",
  willFail: true
};
let validInstall = {
  name: "Test Locale 1",
  installMessage: "INSTALLINGTest Locale 1",
  continueButton: "CONTINUE",
  cancelButton: "CANCEL",
  title: "CHOOSELANGUAGE",
  locale: "test1",
  localeName: "test1",
  willFail: false
}

gTests.push(new LocaleTest("Load locale picker with no opener and continue",
                           { opener: null,
                             loadLocalesList: false,
                             selectAddon: null
                           }));

gTests.push(new LocaleTest("Load locale picker with no opener and try to install an invalid language",
                           { opener: null,
                             loadLocalesList: true,
                             selectAddon: invalidInstall
                           }));

gTests.push(new LocaleTest("Load locale picker with no opener and try to install a valid language",
                           { opener: null,
                             loadLocalesList: true,
                             selectAddon: validInstall
                           }));

gTests.push(new LocaleTest("Load locale picker with opener and try to install an invalid language",
                           { opener: this.window,
                             loadLocalesList: true,
                             selectAddon: invalidInstall
                           }));

gTests.push(new LocaleTest("Load locale picker with opener and try to install a valid language",
                           { opener: this.window,
                             loadLocalesList: true,
                             selectAddon: validInstall
                           }));
