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
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");

[
  ["AllPagesList", "popup_autocomplete", "cmd_openLocation"],
  ["HistoryList", "history-items", "cmd_history"],
  ["BookmarkList", "bookmarks-items", "cmd_bookmarks"],
#ifdef MOZ_SERVICES_SYNC
  ["RemoteTabsList", "remotetabs-items", "cmd_remoteTabs"]
#endif
].forEach(function(aPanel) {
  let [name, id, command] = aPanel;
  XPCOMUtils.defineLazyGetter(window, name, function() {
    return new AwesomePanel(id, command);
  });
});

/**
 * Cache of commonly used elements.
 */
let Elements = {};

[
  ["contentShowing",     "bcast_contentShowing"],
  ["urlbarState",        "bcast_urlbarState"],
  ["stack",              "stack"],
  ["tabs",               "tabs-container"],
  ["controls",           "browser-controls"],
  ["panelUI",            "panel-container"],
  ["toolbarContainer",   "toolbar-container"],
  ["browsers",           "browsers"],
  ["contentViewport",    "content-viewport"],
  ["contentNavigator",   "content-navigator"]
].forEach(function (aElementGlobal) {
  let [name, id] = aElementGlobal;
  XPCOMUtils.defineLazyGetter(Elements, name, function() {
    return document.getElementById(id);
  });
});

/**
 * Cache of commonly used string bundles.
 */
var Strings = {};
[
  ["browser",    "chrome://browser/locale/browser.properties"],
  ["brand",      "chrome://branding/locale/brand.properties"]
].forEach(function (aStringBundle) {
  let [name, bundle] = aStringBundle;
  XPCOMUtils.defineLazyGetter(Strings, name, function() {
    return Services.strings.createBundle(bundle);
  });
});

const TOOLBARSTATE_LOADING  = 1;
const TOOLBARSTATE_LOADED   = 2;

var BrowserUI = {
  _edit: null,
  _title: null,
  _throbber: null,
  _favicon: null,
  _dialogs: [],

  _domWillOpenModalDialog: function(aBrowser) {
    // We're about to open a modal dialog, make sure the opening
    // tab is brought to the front.

    for (let i = 0; i < Browser.tabs.length; i++) {
      if (Browser._tabs[i].browser == aBrowser) {
        Browser.selectedTab = Browser.tabs[i];
        break;
      }
    }
  },

  _titleChanged: function(aBrowser) {
    let browser = Browser.selectedBrowser;
    if (browser && aBrowser != browser)
      return;

    let url = this.getDisplayURI(browser);
    let caption = browser.contentTitle || url;

    if (browser.contentTitle == "" && !Util.isURLEmpty(browser.userTypedValue))
      caption = browser.userTypedValue;
    else if (Util.isURLEmpty(url))
      caption = "";

    if (caption) {
      this._title.value = caption;
      this._title.classList.remove("placeholder");
    } else {
      this._title.value = this._title.getAttribute("placeholder");
      this._title.classList.add("placeholder");
    }
  },

  /*
   * Dispatched by window.close() to allow us to turn window closes into tabs
   * closes.
   */
  _domWindowClose: function(aBrowser) {
     // Find the relevant tab, and close it.
     let browsers = Browser.browsers;
     for (let i = 0; i < browsers.length; i++) {
      if (browsers[i] == aBrowser) {
        Browser.closeTab(Browser.getTabAtIndex(i));
        return { preventDefault: true };
      }
    }
  },

  _updateButtons: function(aBrowser) {
    let back = document.getElementById("cmd_back");
    let forward = document.getElementById("cmd_forward");

    back.setAttribute("disabled", !aBrowser.canGoBack);
    forward.setAttribute("disabled", !aBrowser.canGoForward);
  },

  _updateToolbar: function _updateToolbar() {
    let mode = Elements.urlbarState.getAttribute("mode");
    if (mode == "edit" && this.activePanel)
      return;

    if (Browser.selectedTab.isLoading() && mode != "loading")
      Elements.urlbarState.setAttribute("mode", "loading");
    else if (mode != "view")
      Elements.urlbarState.setAttribute("mode", "view");
  },

  _tabSelect: function(aEvent) {
    let browser = Browser.selectedBrowser;
    this._titleChanged(browser);
    this._updateToolbar();
    this._updateButtons(browser);
    this._updateIcon(browser.mIconURL);
    this.updateStar();
  },

  _toolbarLocked: 0,

  isToolbarLocked: function isToolbarLocked() {
    return this._toolbarLocked;
  },

  lockToolbar: function lockToolbar() {
    this._toolbarLocked++;
    document.getElementById("toolbar-moveable-container").top = "0";
    if (this._toolbarLocked == 1)
      Browser.forceChromeReflow();
  },

  unlockToolbar: function unlockToolbar() {
    if (!this._toolbarLocked)
      return;

    this._toolbarLocked--;
    if (!this._toolbarLocked)
      document.getElementById("toolbar-moveable-container").top = "";
  },

  _setURL: function _setURL(aURL) {
    if (this.activePanel)
      this._edit.defaultValue = aURL;
    else
      this._edit.value = aURL;
  },

  _editURI: function _editURI(aEdit) {
    Elements.urlbarState.setAttribute("mode", "edit");
    this._edit.defaultValue = this._edit.value;
  },

  _showURI: function _showURI() {
    // Replace the web page title by the url of the page
    let urlString = this.getDisplayURI(Browser.selectedBrowser);
    if (Util.isURLEmpty(urlString))
      urlString = "";

    this._edit.value = urlString;
  },

  updateAwesomeHeader: function updateAwesomeHeader(aString) {
    document.getElementById("awesome-header").hidden = (aString != "");

    // During an awesome search we always show the popup_autocomplete/AllPagesList
    // panel since this looks in every places and the rationale behind typing
    // is to find something, whereever it is.
    if (this.activePanel != AllPagesList) {
      let inputField = this._edit;
      let oldClickSelectsAll = inputField.clickSelectsAll;
      inputField.clickSelectsAll = false;

      this.activePanel = AllPagesList;

      // changing the searchString property call updateAwesomeHeader again
      inputField.controller.searchString = aString;
      inputField.readOnly = false;
      inputField.clickSelectsAll = oldClickSelectsAll;
      return;
    }

    let event = document.createEvent("Events");
    event.initEvent("onsearchbegin", true, true);
    this._edit.dispatchEvent(event);
  },

  _closeOrQuit: function _closeOrQuit() {
    // Close active dialog, if we have one. If not then close the application.
    if (this.activePanel) {
      this.activePanel = null;
    } else if (this.activeDialog) {
      this.activeDialog.close();
    } else {
      // Check to see if we should really close the window
      if (Browser.closing())
        window.close();
    }
  },

  _activePanel: null,
  get activePanel() {
    return this._activePanel;
  },

  set activePanel(aPanel) {
    if (this._activePanel == aPanel)
      return;

    let awesomePanel = document.getElementById("awesome-panels");
    let awesomeHeader = document.getElementById("awesome-header");

    let willHidePanel = (this._activePanel && !aPanel);
    if (willHidePanel) {
      awesomePanel.hidden = true;
      awesomeHeader.hidden = false;
      this._edit.reset();
      this._edit.detachController();
    }

    if (this._activePanel) {
      this.popDialog();
      this._activePanel.close();
    }

    let willShowPanel = (!this._activePanel && aPanel);
    if (willShowPanel) {
      this._edit.attachController();
      this._editURI();
      awesomePanel.hidden = awesomeHeader.hidden = false;
    };

    if (aPanel) {
      this.pushDialog(aPanel);
      aPanel.open();

      if (this._edit.value == "")
        this._showURI();
    }

    // If the keyboard will cover the full screen, we do not want to show it right away.
    let isReadOnly = (aPanel != AllPagesList || this._isKeyboardFullscreen() || (!willShowPanel && this._edit.readOnly));
    this._edit.readOnly = isReadOnly;
    if (isReadOnly)
      this._edit.blur();

    this._activePanel = aPanel;
    if (willHidePanel || willShowPanel) {
      let event = document.createEvent("UIEvents");
      event.initUIEvent("NavigationPanel" + (willHidePanel ? "Hidden" : "Shown"), true, true, window, false);
      window.dispatchEvent(event);
    }
  },

  get activeDialog() {
    // Return the topmost dialog
    if (this._dialogs.length)
      return this._dialogs[this._dialogs.length - 1];
    return null;
  },

  pushDialog: function pushDialog(aDialog) {
    // If we have a dialog push it on the stack and set the attr for CSS
    if (aDialog) {
      this.lockToolbar();
      this._dialogs.push(aDialog);
      document.getElementById("toolbar-main").setAttribute("dialog", "true");
      Elements.contentShowing.setAttribute("disabled", "true");
    }
  },

  popDialog: function popDialog() {
    if (this._dialogs.length) {
      this._dialogs.pop();
      this.unlockToolbar();
    }

    // If no more dialogs are being displayed, remove the attr for CSS
    if (!this._dialogs.length) {
      document.getElementById("toolbar-main").removeAttribute("dialog");
      Elements.contentShowing.removeAttribute("disabled");
    }
  },

  pushPopup: function pushPopup(aPanel, aElements) {
    this._hidePopup();
    this._popup =  { "panel": aPanel,
                     "elements": (aElements instanceof Array) ? aElements : [aElements] };
    this._dispatchPopupChanged(true);
  },

  popPopup: function popPopup(aPanel) {
    if (!this._popup || aPanel != this._popup.panel)
      return;
    this._popup = null;
    this._dispatchPopupChanged(false);
  },

  // Will the on-screen keyboard cover the whole screen when opened?
  _isKeyboardFullscreen: function _isKeyboardFullscreen() {
#ifdef ANDROID
    if (!Util.isPortrait()) {
      switch (Services.prefs.getIntPref("widget.ime.android.landscape_fullscreen")) {
        case 1:
          return true;
        case -1: {
          let threshold = Services.prefs.getIntPref("widget.ime.android.fullscreen_threshold");
          let dpi = Util.displayDPI;
          return (window.innerHeight * 100 < threshold * dpi);
        }
      }
    }
#endif
    return false;
  },

  _dispatchPopupChanged: function _dispatchPopupChanged(aVisible) {
    let stack = document.getElementById("stack");
    let event = document.createEvent("UIEvents");
    event.initUIEvent("PopupChanged", true, true, window, aVisible);
    event.popup = this._popup;
    stack.dispatchEvent(event);
  },

  _hidePopup: function _hidePopup() {
    if (!this._popup)
      return;
    let panel = this._popup.panel;
    if (panel.hide)
      panel.hide();
  },

  _isEventInsidePopup: function _isEventInsidePopup(aEvent) {
    if (!this._popup)
      return false;
    let elements = this._popup.elements;
    let targetNode = aEvent ? aEvent.target : null;
    while (targetNode && elements.indexOf(targetNode) == -1)
      targetNode = targetNode.parentNode;
    return targetNode ? true : false;
  },

  switchPane: function switchPane(aPanelId) {
    this.blurFocusedElement();

    let panels = document.getElementById("panel-items")
    let panel = aPanelId ? document.getElementById(aPanelId) : panels.selectedPanel;
    let oldPanel = panels.selectedPanel;

    if (oldPanel != panel) {
      panels.selectedPanel = panel;
      let button = document.getElementsByAttribute("linkedpanel", aPanelId)[0];
      if (button)
        button.checked = true;

      let event = document.createEvent("Events");
      event.initEvent("ToolPanelHidden", true, true);
      oldPanel.dispatchEvent(event);
    }

    let event = document.createEvent("Events");
    event.initEvent("ToolPanelShown", true, true);
    panel.dispatchEvent(event);
  },

  get toolbarH() {
    if (!this._toolbarH) {
      let toolbar = document.getElementById("toolbar-main");
      this._toolbarH = toolbar.boxObject.height;
    }
    return this._toolbarH;
  },

  get sidebarW() {
    delete this._sidebarW;
    return this._sidebarW = Elements.controls.getBoundingClientRect().width;
  },

  sizeControls: function(windowW, windowH) {
    // tabs
    document.getElementById("tabs").resize();

    // awesomebar and related panels
    let popup = document.getElementById("awesome-panels");
    popup.top = this.toolbarH;
    popup.height = windowH - this.toolbarH;
    popup.width = windowW;

    // content navigator helper
    document.getElementById("content-navigator").contentHasChanged();
  },

  init: function() {
    this._edit = document.getElementById("urlbar-edit");
    this._title = document.getElementById("urlbar-title");
    this._throbber = document.getElementById("urlbar-throbber");
    this._favicon = document.getElementById("urlbar-favicon");
    this._favicon.addEventListener("error", this, false);

    this._edit.addEventListener("click", this, false);
    this._edit.addEventListener("mousedown", this, false);

    window.addEventListener("NavigationPanelShown", this, false);
    window.addEventListener("NavigationPanelHidden", this, false);

    Elements.browsers.addEventListener("PanFinished", this, true);
#if MOZ_PLATFORM_MAEMO == 6
    Elements.browsers.addEventListener("SizeChanged", this, true);
#endif

    // listen content messages
    messageManager.addMessageListener("DOMLinkAdded", this);
    messageManager.addMessageListener("DOMTitleChanged", this);
    messageManager.addMessageListener("DOMWillOpenModalDialog", this);
    messageManager.addMessageListener("DOMWindowClose", this);

    messageManager.addMessageListener("Browser:OpenURI", this);
    messageManager.addMessageListener("Browser:SaveAs:Return", this);

    // listening mousedown for automatically dismiss some popups (e.g. larry)
    window.addEventListener("mousedown", this, true);

    // listening escape to dismiss dialog on VK_ESCAPE
    window.addEventListener("keypress", this, true);

    // listening AppCommand to handle special keys
    window.addEventListener("AppCommand", this, true);

    // We can delay some initialization until after startup.  We wait until
    // the first page is shown, then dispatch a UIReadyDelayed event.
    messageManager.addMessageListener("pageshow", function() {
      if (getBrowser().currentURI.spec == "about:blank")
        return;

      messageManager.removeMessageListener("pageshow", arguments.callee, true);

      setTimeout(function() {
        let event = document.createEvent("Events");
        event.initEvent("UIReadyDelayed", true, false);
        window.dispatchEvent(event);
      }, 0);
    });

    // Only load IndexedDB.js when we actually need it. A general fix will happen in bug 647079.
    messageManager.addMessageListener("IndexedDB:Prompt", function(aMessage) {
      return IndexedDB.receiveMessage(aMessage);
    });

    // Delay the panel UI and Sync initialization.
    window.addEventListener("UIReadyDelayed", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, false);

      // We unhide the panelUI so the XBL and settings can initialize
      Elements.panelUI.hidden = false;

      // Login Manager and Form History initialization
      Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
      Cc["@mozilla.org/satchel/form-history;1"].getService(Ci.nsIFormHistory2);

      // Listen tabs event
      Elements.tabs.addEventListener("TabSelect", BrowserUI, true);
      Elements.tabs.addEventListener("TabOpen", BrowserUI, true);
      Elements.tabs.addEventListener("TabRemove", BrowserUI, true);

      // Init the tool panel views
      ExtensionsView.init();
      DownloadsView.init();
      ConsoleView.init();

      if (Services.prefs.getBoolPref("browser.tabs.remote")) {
          // Pre-start the content process
          Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
                                           .ensureContentProcess();
      }

#ifdef MOZ_SERVICES_SYNC
      // Init the sync system
      WeaveGlue.init();
#endif

      Services.prefs.addObserver("browser.ui.layout.tablet", BrowserUI, false);
      Services.obs.addObserver(BrowserSearch, "browser-search-engine-modified", false);
      messageManager.addMessageListener("Browser:MozApplicationManifest", OfflineApps);

      // Init helpers
      BadgeHandlers.register(BrowserUI._edit.popup);
      FormHelperUI.init();
      FindHelperUI.init();
      PageActions.init();
      FullScreenVideo.init();
      NewTabPopup.init();
      CharsetMenu.init();

      // If some add-ons were disabled during during an application update, alert user
      let addonIDs = AddonManager.getStartupChanges("disabled");
      if (addonIDs.length > 0) {
        let disabledStrings = Strings.browser.GetStringFromName("alertAddonsDisabled");
        let label = PluralForm.get(addonIDs.length, disabledStrings).replace("#1", addonIDs.length);
        let image = "chrome://browser/skin/images/alert-addons-30.png";

        let alerts = Cc["@mozilla.org/toaster-alerts-service;1"].getService(Ci.nsIAlertsService);
        alerts.showAlertNotification(image, Strings.browser.GetStringFromName("alertAddons"), label, false, "", null);
      }

#ifdef MOZ_UPDATER
      // Check for updates in progress
      let updatePrompt = Cc["@mozilla.org/updates/update-prompt;1"].createInstance(Ci.nsIUpdatePrompt);
      updatePrompt.checkForUpdates();
#endif
    }, false);

    let panels = document.getElementById("panel-items");
    let panelViews = { // Use strings to avoid lazy-loading objects too soon.
      "prefs-container": "PreferencesView",
      "downloads-container": "DownloadsView",
      "addons-container": "ExtensionsView",
      "console-container": "ConsoleView"
    };

    // Some initialization can be delayed until a panel is selected.
    panels.addEventListener("ToolPanelShown", function(aEvent) {
      let viewName = panelViews[panels.selectedPanel.id];
      if (viewName)
        window[viewName].delayedInit();
    }, true);

#ifndef MOZ_OFFICIAL_BRANDING
      setTimeout(function() {
        let startup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup).getStartupInfo();
        for (let name in startup) {
          if (name != "process")
            Services.console.logStringMessage("[timing] " + name + ": " + (startup[name] - startup.process) + "ms");
        }
      }, 3000);
#endif
  },

  uninit: function() {
    Services.obs.removeObserver(BrowserSearch, "browser-search-engine-modified");
    Services.prefs.removeObserver("browser.ui.layout.tablet", BrowserUI);
    messageManager.removeMessageListener("Browser:MozApplicationManifest", OfflineApps);
    ExtensionsView.uninit();
    ConsoleView.uninit();
  },

  observe: function observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed" && aData == "browser.ui.layout.tablet")
      this.updateTabletLayout();
  },

  updateTabletLayout: function updateTabletLayout() {
    let tabletPref = Services.prefs.getIntPref("browser.ui.layout.tablet");
    if (tabletPref == 1 || (tabletPref == -1 && Util.isTablet()))
      Elements.urlbarState.setAttribute("tablet", "true");
    else
      Elements.urlbarState.removeAttribute("tablet");
  },

  update: function(aState) {
    let browser = Browser.selectedBrowser;

    switch (aState) {
      case TOOLBARSTATE_LOADED:
        this._updateToolbar();

        this._updateIcon(browser.mIconURL);
        this.unlockToolbar();
        break;

      case TOOLBARSTATE_LOADING:
        this._updateToolbar();

        browser.mIconURL = "";
        this._updateIcon();
        this.lockToolbar();
        break;
    }
  },

  _updateIcon: function(aIconSrc) {
    this._favicon.src = aIconSrc || "";
    if (Browser.selectedTab.isLoading()) {
      this._throbber.hidden = false;
      this._throbber.setAttribute("loading", "true");
      this._favicon.hidden = true;
    }
    else {
      this._favicon.hidden = false;
      this._throbber.hidden = true;
      this._throbber.removeAttribute("loading");
    }
  },

  getDisplayURI: function(browser) {
    let uri = browser.currentURI;
    try {
      uri = gURIFixup.createExposableURI(uri);
    } catch (ex) {}

    return uri.spec;
  },

  /* Set the location to the current content */
  updateURI: function(aOptions) {
    aOptions = aOptions || {};

    let browser = Browser.selectedBrowser;
    let urlString = this.getDisplayURI(browser);
    if (Util.isURLEmpty(urlString))
      urlString = "";

    this._setURL(urlString);

    if ("captionOnly" in aOptions && aOptions.captionOnly)
      return;

    // Update the navigation buttons
    this._updateButtons(browser);

    // Check for a bookmarked page
    this.updateStar();
  },

  goToURI: function(aURI) {
    aURI = aURI || this._edit.value;
    if (!aURI)
      return;

    // Make sure we're online before attempting to load
    Util.forceOnline();

    // Close the autocomplete panel and quickly update the urlbar value
    this.closeAutoComplete();
    this._edit.value = aURI;

    let postData = {};
    aURI = Browser.getShortcutOrURI(aURI, postData);
    Browser.loadURI(aURI, { flags: Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP, postData: postData });

    // If a page goes from remote (about:blank) to local (about:home), fennec
    // create a tab for the local page and then close the about:blank tab.
    // If the newly created tab spawn a new column the sidebars position can
    // be messed up. Hopefully, the viewable area should be maximized when a
    // new page is opened, so a call to Browser.hideSidebars() fill this
    // requirement and fix the sidebars position.
    Browser.hideSidebars();

    // Delay doing the fixup so the raw URI is passed to loadURIWithFlags
    // and the proper third-party fixup can be done
    let fixupFlags = Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    let uri = gURIFixup.createFixupURI(aURI, fixupFlags);
    gHistSvc.markPageAsTyped(uri);

    this._titleChanged(Browser.selectedBrowser);
  },

  showAutoComplete: function showAutoComplete() {
    if (this.isAutoCompleteOpen())
      return;

    this.hidePanel();
    this._hidePopup();
    if (this.activeDialog)
      this.activeDialog.close();
    this.activePanel = AllPagesList;
  },

  closeAutoComplete: function closeAutoComplete() {
    if (this.isAutoCompleteOpen())
      this._edit.popup.closePopup();

    this.activePanel = null;
  },

  isAutoCompleteOpen: function isAutoCompleteOpen() {
    return this.activePanel == AllPagesList;
  },

  doOpenSearch: function doOpenSearch(aName) {
    // save the current value of the urlbar
    let searchValue = this._edit.value;

    // Give the new page lots of room
    Browser.hideSidebars();
    this.closeAutoComplete();

    // Make sure we're online before attempting to load
    Util.forceOnline();

    let engine = Services.search.getEngineByName(aName);
    let submission = engine.getSubmission(searchValue, null);
    Browser.selectedBrowser.userTypedValue = submission.uri.spec;
    Browser.loadURI(submission.uri.spec, { postData: submission.postData });

    this._titleChanged(Browser.selectedBrowser);
  },

  updateUIFocus: function _updateUIFocus() {
    if (Elements.contentShowing.getAttribute("disabled") == "true")
      Browser.selectedBrowser.messageManager.sendAsyncMessage("Browser:Blur", { });
  },

  updateStar: function() {
    let uri = getBrowser().currentURI;
    if (uri.spec == "about:blank") {
      this._setStar(false);
      return;
    }

    PlacesUtils.asyncGetBookmarkIds(uri, function(aItemIds) {
      this._setStar(aItemIds.length > 0)
    }, this);
  },

  _setStar: function _setStar(aIsStarred) {
    let buttons = document.getElementsByClassName("tool-star");
    for (let i = 0; i < buttons.length; i++) {
      if (aIsStarred)
        buttons[i].setAttribute("starred", "true");
      else
        buttons[i].removeAttribute("starred");
    }
  },

  newTab: function newTab(aURI, aOwner) {
    aURI = aURI || "about:blank";
    let tab = Browser.addTab(aURI, true, aOwner);

    this.hidePanel();

    if (aURI == "about:blank") {
      // Display awesomebar UI
      this.showAutoComplete();
    }
    else {
      // Give the new page lots of room
      Browser.hideSidebars();
      this.closeAutoComplete();
    }

    return tab;
  },

  newOrSelectTab: function newOrSelectTab(aURI, aOwner) {
    let tabs = Browser.tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.currentURI.spec == aURI) {
        Browser.selectedTab = tabs[i];
        return;
      }
    }
    this.newTab(aURI, aOwner);
  },

  closeTab: function closeTab(aTab) {
    // If no tab is passed in, assume the current tab
    Browser.closeTab(aTab || Browser.selectedTab);
  },

  selectTab: function selectTab(aTab) {
    this.activePanel = null;
    Browser.selectedTab = aTab;
  },

  undoCloseTab: function undoCloseTab(aIndex) {
    let tab = null;
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
    if (ss.getClosedTabCount(window) > (aIndex || 0)) {
      let chromeTab = ss.undoCloseTab(window, aIndex || 0);
      tab = Browser.getTabFromChrome(chromeTab);
    }
    return tab;
  },

  isTabsVisible: function isTabsVisible() {
    // The _1, _2 and _3 are to make the js2 emacs mode happy
    let [leftvis,_1,_2,_3] = Browser.computeSidebarVisibility();
    return (leftvis > 0.002);
  },

  showPanel: function showPanel(aPanelId) {
    if (this.activePanel)
      this.activePanel = null; // Hide the awesomescreen.

    Elements.panelUI.left = 0;
    Elements.panelUI.hidden = false;
    Elements.contentShowing.setAttribute("disabled", "true");

    this.switchPane(aPanelId);
  },

  hidePanel: function hidePanel() {
    if (!this.isPanelVisible())
      return;
    Elements.panelUI.hidden = true;
    Elements.contentShowing.removeAttribute("disabled");
    this.blurFocusedElement();

    let panels = document.getElementById("panel-items")
    let event = document.createEvent("Events");
    event.initEvent("ToolPanelHidden", true, true);
    panels.selectedPanel.dispatchEvent(event);
  },

  isPanelVisible: function isPanelVisible() {
    return (!Elements.panelUI.hidden && Elements.panelUI.left == 0);
  },

  blurFocusedElement: function blurFocusedElement() {
    let focusedElement = document.commandDispatcher.focusedElement;
    if (focusedElement)
      focusedElement.blur();
  },

  switchTask: function switchTask() {
    try {
      let shell = Cc["@mozilla.org/browser/shell-service;1"].createInstance(Ci.nsIShellService);
      shell.switchTask();
    } catch(e) { }
  },

  handleEscape: function (aEvent) {
    aEvent.stopPropagation();

    // Check open popups
    if (this._popup) {
      this._hidePopup();
      return;
    }

    // Check open dialogs
    let dialog = this.activeDialog;
    if (dialog && dialog != this.activePanel) {
      dialog.close();
      return;
    }

    // Check active panel
    if (this.activePanel) {
      this.activePanel = null;
      return;
    }

    // Check open modal elements
    let modalElementsLength = document.getElementsByClassName("modal-block").length;
    if (modalElementsLength > 0)
      return;

    // Check open panel
    if (this.isPanelVisible()) {
      this.hidePanel();
      return;
    }

    // Check content helper
    let contentHelper = document.getElementById("content-navigator");
    if (contentHelper.isActive) {
      contentHelper.model.hide();
      return;
    }

    // Only if there are no dialogs, popups, or panels open
    let tab = Browser.selectedTab;
    let browser = tab.browser;

    if (browser.canGoBack) {
      browser.goBack();
    } else if (tab.owner) {
      // When going back, always return to the owner (not a sibling).
      Browser.selectedTab = tab.owner;
      this.closeTab(tab);
    }
#ifdef ANDROID
    else {
      window.QueryInterface(Ci.nsIDOMChromeWindow).minimize();
      if (tab.closeOnExit)
        this.closeTab(tab);
    }
#endif
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      // Browser events
      case "TabSelect":
        this._tabSelect(aEvent);
        break;
      case "TabOpen":
      case "TabRemove":
      {
        // Workaround to hide the tabstrip if it is partially visible
        // See bug 524469 and bug 626660
        let [tabsVisibility,,,] = Browser.computeSidebarVisibility();
        if (tabsVisibility > 0.0 && tabsVisibility < 1.0)
          Browser.hideSidebars();

        break;
      }
      case "PanFinished":
        let tabs = document.getElementById("tabs");
        let [tabsVisibility,,oldLeftWidth, oldRightWidth] = Browser.computeSidebarVisibility();
        if (tabsVisibility == 0.0 && tabs.hasClosedTab) {
          let { x: x1, y: y1 } = Browser.getScrollboxPosition(Browser.controlsScrollboxScroller);
          tabs.removeClosedTab();

          // If the tabs sidebar lives on the left side of the window, width
          // variation should be taken into account to reposition the sidebars
          if (tabs.getBoundingClientRect().left < 0) {
            let [,, leftWidth, rightWidth] = Browser.computeSidebarVisibility();
            let delta = (oldLeftWidth - leftWidth) || (oldRightWidth - rightWidth);
            x1 += (x1 == leftWidth) ? delta : -delta;
          }

          Browser.controlsScrollboxScroller.scrollTo(x1, 0);
          Browser.tryFloatToolbar(0, 0);
        }
        break;
      case "SizeChanged":
        this.sizeControls(ViewableAreaObserver.width, ViewableAreaObserver.height);
        break;
      // Window events
      case "keypress":
        if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE)
          this.handleEscape(aEvent);
        break;
      case "AppCommand":
        aEvent.stopPropagation();
        switch (aEvent.command) {
          case "Menu":
            this.doCommand("cmd_menu");
            break;
          case "Search":
            if (!this.activePanel)
              AllPagesList.doCommand();
            else
              this.doCommand("cmd_opensearch");
            break;
          default:
            break;
        }
        break;
      // URL textbox events
      case "click":
        if (this._edit.readOnly)
          this._edit.readOnly = false;
        break;
      case "mousedown":
        if (!this._isEventInsidePopup(aEvent))
          this._hidePopup();
        break;
      // Favicon events
      case "error":
        this._favicon.src = "";
        break;
      // Awesome popup event
      case "NavigationPanelShown":
        this._edit.collapsed = false;
        this._title.collapsed = true;

        if (!this._edit.readOnly)
          this._edit.focus();

        // Disabled the search button if no search engines are available
        let button = document.getElementById("urlbar-icons");
        if (BrowserSearch.engines.length)
          button.removeAttribute("disabled");
        else
          button.setAttribute("disabled", "true");

        break;
      case "NavigationPanelHidden": {
        this._edit.collapsed = true;
        this._title.collapsed = false;
        this._updateToolbar();

        let button = document.getElementById("urlbar-icons");
        button.removeAttribute("open");
        button.removeAttribute("disabled");
        break;
      }
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    let browser = aMessage.target;
    let json = aMessage.json;
    switch (aMessage.name) {
      case "DOMTitleChanged":
        this._titleChanged(browser);
        break;
      case "DOMWillOpenModalDialog":
        return this._domWillOpenModalDialog(browser);
        break;
      case "DOMWindowClose":
        return this._domWindowClose(browser);
        break;
      case "DOMLinkAdded":
        // checks for an icon to use for a web app
        // apple-touch-icon size is 57px and default size is 16px
        let rel = json.rel.toLowerCase().split(" ");
        if (rel.indexOf("icon") != -1) {
          // We use the sizes attribute if available
          // see http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#rel-icon
          let size = 16;
          if (json.sizes) {
            let sizes = json.sizes.toLowerCase().split(" ");
            sizes.forEach(function(item) {
              if (item != "any") {
                let [w, h] = item.split("x");
                size = Math.max(Math.min(w, h), size);
              }
            });
          }
          if (size > browser.appIcon.size) {
            browser.appIcon.href = json.href;
            browser.appIcon.size = size;
          }
        }
        else if ((rel.indexOf("apple-touch-icon") != -1) && (browser.appIcon.size < 57)) {
          // XXX should we support apple-touch-icon-precomposed ?
          // see http://developer.apple.com/safari/library/documentation/appleapplications/reference/safariwebcontent/configuringwebapplications/configuringwebapplications.html
          browser.appIcon.href = json.href;
          browser.appIcon.size = 57;
        }

        // Handle favicon changes
        if (Browser.selectedBrowser == browser)
          this._updateIcon(Browser.selectedBrowser.mIconURL);
        break;
      case "Browser:SaveAs:Return":
        if (json.type != Ci.nsIPrintSettings.kOutputFormatPDF)
          return;

        let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
        let db = dm.DBConnection;
        let stmt = db.createStatement("UPDATE moz_downloads SET endTime = :endTime, state = :state WHERE id = :id");
        stmt.params.endTime = Date.now() * 1000;
        stmt.params.state = Ci.nsIDownloadManager.DOWNLOAD_FINISHED;
        stmt.params.id = json.id;
        stmt.execute();
        stmt.finalize();

        let download = dm.getDownload(json.id);
#ifdef ANDROID
        // since our content process doesn't have write permissions to the
        // downloads dir, we save it to the tmp dir and then move it here
        let dlFile = download.targetFile;
        if (!dlFile.exists())
          dlFile.create(file.NORMAL_FILE_TYPE, 0666);
        let tmpDir = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
        let tmpFile = tmpDir.clone();
        tmpFile.append(dlFile.leafName);

        // we sometimes race with the content process, so make sure its finished
        // creating/writing the file
        while (!tmpFile.exists());
        tmpFile.moveTo(dlFile.parent, dlFile.leafName);
#endif
        try {
          DownloadsView.downloadCompleted(download);
          let element = DownloadsView.getElementForDownload(json.id);
          element.setAttribute("state", Ci.nsIDownloadManager.DOWNLOAD_FINISHED);
          element.setAttribute("endTime", Date.now());
          element.setAttribute("referrer", json.referrer);
          DownloadsView._updateTime(element);
          DownloadsView._updateStatus(element);
        }
        catch(e) {}
        Services.obs.notifyObservers(download, "dl-done", null);
        break;

      case "Browser:OpenURI":
        let referrerURI = null;
        if (json.referrer)
          referrerURI = Services.io.newURI(json.referrer, null, null);
        Browser.addTab(json.uri, json.bringFront, Browser.selectedTab, { referrerURI: referrerURI });
        break;
    }
  },

  supportsCommand : function(cmd) {
    var isSupported = false;
    switch (cmd) {
      case "cmd_back":
      case "cmd_forward":
      case "cmd_reload":
      case "cmd_forceReload":
      case "cmd_stop":
      case "cmd_go":
      case "cmd_openLocation":
      case "cmd_star":
      case "cmd_opensearch":
      case "cmd_bookmarks":
      case "cmd_history":
      case "cmd_remoteTabs":
      case "cmd_quit":
      case "cmd_close":
      case "cmd_menu":
      case "cmd_showTabs":
      case "cmd_newTab":
      case "cmd_closeTab":
      case "cmd_undoCloseTab":
      case "cmd_actions":
      case "cmd_panel":
      case "cmd_sanitize":
      case "cmd_zoomin":
      case "cmd_zoomout":
      case "cmd_volumeLeft":
      case "cmd_volumeRight":
      case "cmd_lockscreen":
        isSupported = true;
        break;
      default:
        isSupported = false;
        break;
    }
    return isSupported;
  },

  isCommandEnabled : function(cmd) {
    // disable all commands during the first-run sidebar discovery
    let broadcaster = document.getElementById("bcast_uidiscovery");
    if (broadcaster && broadcaster.getAttribute("mode") == "discovery")
      return false;

    let elem = document.getElementById(cmd);
    if (elem && elem.getAttribute("disabled") == "true")
      return false;
    return true;
  },

  doCommand : function(cmd) {
    if (!this.isCommandEnabled(cmd))
      return;
    let browser = getBrowser();
    switch (cmd) {
      case "cmd_back":
        browser.goBack();
        break;
      case "cmd_forward":
        browser.goForward();
        break;
      case "cmd_reload":
        browser.reload();
        break;
      case "cmd_forceReload":
      {
        // Simulate a new page
        browser.lastLocation = null;

        const reloadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY |
                            Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
        browser.reloadWithFlags(reloadFlags);
        break;
      }
      case "cmd_stop":
        browser.stop();
        break;
      case "cmd_go":
        this.goToURI();
        break;
      case "cmd_openLocation":
        this.showAutoComplete();
        break;
      case "cmd_star":
      {
        BookmarkPopup.toggle();
        this._setStar(true);

        let bookmarkURI = browser.currentURI;
        PlacesUtils.asyncGetBookmarkIds(bookmarkURI, function (aItemIds) {
          if (!aItemIds.length) {
            let bookmarkTitle = browser.contentTitle || bookmarkURI.spec;
            try {
              let bookmarkService = PlacesUtils.bookmarks;
              let bookmarkId = bookmarkService.insertBookmark(BookmarkList.panel.mobileRoot, bookmarkURI,
                                                              bookmarkService.DEFAULT_INDEX,
                                                              bookmarkTitle);
            } catch (e) {
              // Insert failed; reset the star state.
              this.updateStar();
            }

            // XXX Used for browser-chrome tests
            let event = document.createEvent("Events");
            event.initEvent("BookmarkCreated", true, false);
            window.dispatchEvent(event);
          }
        }, this);
        break;
      }
      case "cmd_opensearch":
        this.blurFocusedElement();
        BrowserSearch.toggle();
        break;
      case "cmd_bookmarks":
        this.activePanel = BookmarkList;
        break;
      case "cmd_history":
        this.activePanel = HistoryList;
        break;
      case "cmd_remoteTabs":
        if (Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED) {
          // We have to set activePanel before showing sync's dialog
          // to make the sure the dialog stacking is correct.
          this.activePanel = RemoteTabsList;
          WeaveGlue.open();
        } else if (!Weave.Service.isLoggedIn && !Services.prefs.getBoolPref("browser.sync.enabled")) {
          // unchecked the relative command button
          document.getElementById("remotetabs-button").removeAttribute("checked");
          this.activePanel = null;

          BrowserUI.showPanel("prefs-container");
          let prefsBox = document.getElementById("prefs-list");
          let syncArea = document.getElementById("prefs-sync");
          if (prefsBox && syncArea) {
            let prefsBoxY = prefsBox.firstChild.boxObject.screenY;
            let syncAreaY = syncArea.boxObject.screenY;
            setTimeout(function() {
              prefsBox.scrollBoxObject.scrollTo(0, syncAreaY - prefsBoxY);
            }, 0);
          }
        } else {
          this.activePanel = RemoteTabsList;
        }

        break;
      case "cmd_quit":
        // Only close one window
        this._closeOrQuit();
        break;
      case "cmd_close":
        this._closeOrQuit();
        break;
      case "cmd_menu":
        AppMenu.toggle();
        break;
      case "cmd_showTabs":
        TabsPopup.toggle();
        break;
      case "cmd_newTab":
        this.newTab();
        break;
      case "cmd_closeTab":
        this.closeTab();
        break;
      case "cmd_undoCloseTab":
        this.undoCloseTab();
        break;
      case "cmd_sanitize":
      {
        let title = Strings.browser.GetStringFromName("clearPrivateData.title");
        let message = Strings.browser.GetStringFromName("clearPrivateData.message");
        let clear = Services.prompt.confirm(window, title, message);
        if (clear) {
          // disable the button temporarily to indicate something happened
          let button = document.getElementById("prefs-clear-data");
          button.disabled = true;
          setTimeout(function() { button.disabled = false; }, 5000);

          Sanitizer.sanitize();
        }
        break;
      }
      case "cmd_panel":
        if (this.isPanelVisible())
          this.hidePanel();
        else
          this.showPanel();
        break;
      case "cmd_zoomin":
        Browser.zoom(-1);
        break;
      case "cmd_zoomout":
        Browser.zoom(1);
        break;
      case "cmd_volumeLeft":
        // Zoom in (portrait) or out (landscape)
        Browser.zoom(Util.isPortrait() ? -1 : 1);
        break;
      case "cmd_volumeRight":
        // Zoom out (portrait) or in (landscape)
        Browser.zoom(Util.isPortrait() ? 1 : -1);
        break;
      case "cmd_lockscreen":
      {
        let locked = Services.prefs.getBoolPref("toolkit.screen.lock");
        Services.prefs.setBoolPref("toolkit.screen.lock", !locked);

        let strings = Strings.browser;
        let alerts = Cc["@mozilla.org/toaster-alerts-service;1"].getService(Ci.nsIAlertsService);
        alerts.showAlertNotification(null, strings.GetStringFromName("alertLockScreen"),
                                     strings.GetStringFromName("alertLockScreen." + (!locked ? "locked" : "unlocked")), false, "", null);
        break;
      }
    }
  }
};
