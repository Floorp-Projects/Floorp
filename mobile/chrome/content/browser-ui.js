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

XPCOMUtils.defineLazyGetter(this, "PluralForm", function() {
  Cu.import("resource://gre/modules/PluralForm.jsm");
  return PluralForm;
});

XPCOMUtils.defineLazyGetter(this, "PlacesUtils", function() {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
  return PlacesUtils;
});

XPCOMUtils.defineLazyServiceGetter(window, "gHistSvc", "@mozilla.org/browser/nav-history-service;1", "nsINavHistoryService", "nsIBrowserHistory");
XPCOMUtils.defineLazyServiceGetter(window, "gURIFixup", "@mozilla.org/docshell/urifixup;1", "nsIURIFixup");
XPCOMUtils.defineLazyServiceGetter(window, "gFaviconService", "@mozilla.org/browser/favicon-service;1", "nsIFaviconService");
XPCOMUtils.defineLazyServiceGetter(window, "gFocusManager", "@mozilla.org/focus-manager;1", "nsIFocusManager");

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

    let willShowPanel = (!this._activePanel && aPanel);
    if (willShowPanel) {
      this.pushDialog(aPanel);
      this._edit.attachController();
      this._editURI();
      awesomePanel.hidden = awesomeHeader.hidden = false;
    };

    if (aPanel) {
      aPanel.open();
      if (this._edit.value == "")
        this._showURI();
    }

    let willHidePanel = (this._activePanel && !aPanel);
    if (willHidePanel) {
      awesomePanel.hidden = true;
      awesomeHeader.hidden = false;
      this._edit.reset();
      this._edit.detachController();
      this.popDialog();
    }

    if (this._activePanel)
      this._activePanel.close();

    // The readOnly state of the field enabled/disabled the VKB
    let isReadOnly = !(aPanel == AllPagesList && Util.isPortrait() && (willShowPanel || !this._edit.readOnly));
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
    let button = document.getElementsByAttribute("linkedpanel", aPanelId)[0];
    if (button)
      button.checked = true;

    this.blurFocusedElement();

    let pane = document.getElementById(aPanelId);
    document.getElementById("panel-items").selectedPanel = pane;
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

  get starButton() {
    delete this.starButton;
    return this.starButton = document.getElementById("tool-star");
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
    let contentHelper = document.getElementById("content-navigator");
    contentHelper.top = windowH - contentHelper.getBoundingClientRect().height;
  },

  init: function() {
    this._edit = document.getElementById("urlbar-edit");
    this._title = document.getElementById("urlbar-title");
    this._throbber = document.getElementById("urlbar-throbber");
    this._favicon = document.getElementById("urlbar-favicon");
    this._favicon.addEventListener("error", this, false);

    this._edit.addEventListener("click", this, false);
    this._edit.addEventListener("mousedown", this, false);

    BadgeHandlers.register(this._edit.popup);

    window.addEventListener("NavigationPanelShown", this, false);
    window.addEventListener("NavigationPanelHidden", this, false);

    let tabs = document.getElementById("tabs");
    tabs.addEventListener("TabSelect", this, true);
    tabs.addEventListener("TabOpen", this, true);
    tabs.addEventListener("TabOpen", NewTabPopup, true);

    Elements.browsers.addEventListener("PanFinished", this, true);
    Elements.browsers.addEventListener("SizeChanged", this, true);

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

    // Delay the panel UI and Sync initialization.
    window.addEventListener("UIReadyDelayed", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, false);

      // We unhide the panelUI so the XBL and settings can initialize
      Elements.panelUI.hidden = false;

      // Init the views
      ExtensionsView.init();
      DownloadsView.init();
      PreferencesView.init();
      ConsoleView.init();
      FullScreenVideo.init();

#ifdef MOZ_IPC
      // Pre-start the content process
      Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
          .ensureContentProcess();
#endif

#ifdef MOZ_SERVICES_SYNC
      // Init the sync system
      WeaveGlue.init();
#endif
    }, false);

    FormHelperUI.init();
    FindHelperUI.init();
    PageActions.init();
  },

  uninit: function() {
    ExtensionsView.uninit();
    ConsoleView.uninit();
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
  updateURI: function() {
    let browser = Browser.selectedBrowser;

    // FIXME: deckbrowser should not fire TabSelect on the initial tab (bug 454028)
    if (!browser.currentURI)
      return;

    // Update the navigation buttons
    this._updateButtons(browser);

    // Check for a bookmarked page
    this.updateStar();

    let urlString = this.getDisplayURI(browser);
    if (Util.isURLEmpty(urlString))
      urlString = "";

    this._setURL(urlString);
  },

  goToURI: function(aURI) {
    aURI = aURI || this._edit.value;
    if (!aURI)
      return;

    // Make sure we're online before attempting to load
    Util.forceOnline();

    // Give the new page lots of room
    Browser.hideSidebars();
    this.closeAutoComplete();

    this._edit.value = aURI;

    let postData = {};
    aURI = Browser.getShortcutOrURI(aURI, postData);
    Browser.loadURI(aURI, { flags: Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP, postData: postData });

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
    PlacesUtils.asyncGetBookmarkIds(getBrowser().currentURI, function (aItemIds) {
      if (aItemIds.length)
        this.starButton.setAttribute("starred", "true");
      else
        this.starButton.removeAttribute("starred");
    }, this);
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

  showPanel: function showPanel(aPage) {
    if (this.activePanel)
      this.activePanel = null;

    Elements.panelUI.left = 0;
    Elements.panelUI.hidden = false;
    Elements.contentShowing.setAttribute("disabled", "true");

    if (aPage != undefined)
      this.switchPane(aPage);
  },

  hidePanel: function hidePanel() {
    if (!this.isPanelVisible())
      return;
    Elements.panelUI.hidden = true;
    Elements.contentShowing.removeAttribute("disabled");
    this.blurFocusedElement();
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
      let phone = Cc["@mozilla.org/phone/support;1"].createInstance(Ci.nsIPhoneSupport);
      phone.switchTask();
    } catch(e) { }
  },

  handleEscape: function (aEvent) {
    aEvent.stopPropagation();

    // Check open popups
    if (this._popup) {
      this._hidePopup();
      return;
    }

    // Check active panel
    if (this.activePanel) {
      this.activePanel = null;
      return;
    }

    // Check open dialogs
    let dialog = this.activeDialog;
    if (dialog) {
      dialog.close();
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
      {
        // Workaround to hide the tabstrip if it is partially visible
        // See bug 524469
        let [tabsVisibility,,,] = Browser.computeSidebarVisibility();
        if (tabsVisibility > 0.0 && tabsVisibility < 1.0)
          Browser.hideSidebars();

        break;
      }
      case "PanFinished":
        let [tabsVisibility,,,] = Browser.computeSidebarVisibility();
        if (tabsVisibility == 0.0)
          document.getElementById("tabs").removeClosedTab();
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
    let elem = document.getElementById(cmd);
    if (elem && (elem.getAttribute("disabled") == "true"))
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
        if (!this.starButton.hasAttribute("starred")) {
          this.starButton.setAttribute("starred", "true");
          BookmarkPopup.autoHide();
        }

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
        this.activePanel = RemoteTabsList;
        break;
      case "cmd_quit":
        goQuitApplication();
        break;
      case "cmd_close":
        this._closeOrQuit();
        break;
      case "cmd_menu":
        AppMenu.toggle();
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
      {
        if (BrowserUI.isPanelVisible())
          this.hidePanel();
        else
          this.showPanel();
        break;
      }
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
        let alerts = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
        alerts.showAlertNotification(null, strings.GetStringFromName("alertLockScreen"),
                                     strings.GetStringFromName("alertLockScreen." + (!locked ? "locked" : "unlocked")), false, "", null);
        break;
      }
    }
  }
};

var TapHighlightHelper = {
  get _overlay() {
    if (Browser.selectedTab)
      return Browser.selectedTab.overlay;
    return null;
  },

  show: function show(aRects) {
    let overlay = this._overlay;
    if (!overlay)
      return;

    let browser = getBrowser();
    let scroll = browser.getPosition();

    let canvasArea = aRects.reduce(function(a, b) {
      return a.expandToContain(b);
    }, new Rect(0, 0, 0, 0)).map(function(val) val * browser.scale)
                            .translate(-scroll.x, -scroll.y);

    overlay.setAttribute("width", canvasArea.width);
    overlay.setAttribute("height", canvasArea.height);

    let ctx = overlay.getContext("2d");
    ctx.save();
    ctx.translate(-canvasArea.left, -canvasArea.top);
    ctx.scale(browser.scale, browser.scale);

    overlay.setAttribute("left", canvasArea.left);
    overlay.setAttribute("top", canvasArea.top);
    ctx.clearRect(0, 0, canvasArea.width, canvasArea.height);
    ctx.fillStyle = "rgba(0, 145, 255, .5)";
    for (let i = aRects.length - 1; i >= 0; i--) {
      let rect = aRects[i];
      ctx.fillRect(rect.left - scroll.x / browser.scale, rect.top - scroll.y / browser.scale, rect.width, rect.height);
    }
    ctx.restore();
    overlay.style.display = "block";

    addEventListener("MozBeforePaint", this, false);
    mozRequestAnimationFrame();
  },

  /**
   * Hide the highlight. aGuaranteeShowMsecs specifies how many milliseconds the
   * highlight should be shown before it disappears.
   */
  hide: function hide(aGuaranteeShowMsecs) {
    if (!this._overlay || this._overlay.style.display == "none")
      return;

    this._guaranteeShow = Math.max(0, aGuaranteeShowMsecs);
    if (this._guaranteeShow) {
      // _shownAt is set once highlight has been painted
      if (this._shownAt)
        setTimeout(this._hide.bind(this),
                   Math.max(0, this._guaranteeShow - (mozAnimationStartTime - this._shownAt)));
    } else {
      this._hide();
    }
  },

  /** Helper function that hides popup immediately. */
  _hide: function _hide() {
    this._shownAt = 0;
    this._guaranteeShow = 0;
    this._overlay.style.display = "none";
  },

  handleEvent: function handleEvent(ev) {
    removeEventListener("MozBeforePaint", this, false);
    this._shownAt = ev.timeStamp;
    // hide has been called, so hide the tap highlight after it has
    // been shown for a moment.
    if (this._guaranteeShow)
      this.hide(this._guaranteeShow);
  }
};

var PageActions = {
  init: function init() {
    document.getElementById("pageactions-container").addEventListener("click", this, true);

    this.register("pageaction-reset", this.updatePagePermissions, this);
    this.register("pageaction-password", this.updateForgetPassword, this);
#ifdef NS_PRINTING
    this.register("pageaction-saveas", this.updatePageSaveAs, this);
#endif
    this.register("pageaction-share", this.updateShare, this);
    this.register("pageaction-search", BrowserSearch.updatePageSearchEngines, BrowserSearch);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        getIdentityHandler().hide();
        break;
    }
  },

  /**
   * @param aId id of a pageaction element
   * @param aCallback function that takes an element and returns true if it should be visible
   * @param aThisObj (optional) scope object for aCallback
   */
  register: function register(aId, aCallback, aThisObj) {
    this._handlers.push({id: aId, callback: aCallback, obj: aThisObj});
  },

  _handlers: [],

  updateSiteMenu: function updateSiteMenu() {
    this._handlers.forEach(function(action) {
      let node = document.getElementById(action.id);
      node.hidden = !action.callback.call(action.obj, node);
    });
    this._updateAttributes();
  },

  get _loginManager() {
    delete this._loginManager;
    return this._loginManager = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
  },

  // This is easy for an addon to add his own perm type here
  _permissions: ["popup", "offline-app", "geo"],

  _forEachPermissions: function _forEachPermissions(aHost, aCallback) {
    let pm = Services.perms;
    for (let i = 0; i < this._permissions.length; i++) {
      let type = this._permissions[i];
      if (!pm.testPermission(aHost, type))
        continue;

      let perms = pm.enumerator;
      while (perms.hasMoreElements()) {
        let permission = perms.getNext().QueryInterface(Ci.nsIPermission);
        if (permission.host == aHost.asciiHost && permission.type == type)
          aCallback(type);
      }
    }
  },

  updatePagePermissions: function updatePagePermissions(aNode) {
    let host = Browser.selectedBrowser.currentURI;
    let permissions = [];

    this._forEachPermissions(host, function(aType) {
      permissions.push("pageactions." + aType);
    });

    if (!this._loginManager.getLoginSavingEnabled(host.prePath)) {
      // If rememberSignons is false, then getLoginSavingEnabled returns false
      // for all pages, so we should just ignore it (Bug 601163).
      if (Services.prefs.getBoolPref("signon.rememberSignons"))
        permissions.push("pageactions.password");
    }

    let descriptions = permissions.map(function(s) Strings.browser.GetStringFromName(s));
    aNode.setAttribute("description", descriptions.join(", "));

    return (permissions.length > 0);
  },

  updateForgetPassword: function updateForgetPassword(aNode) {
    let host = Browser.selectedBrowser.currentURI;
    let logins = this._loginManager.findLogins({}, host.prePath, "", "");

    return logins.some(function(login) login.hostname == host.prePath);
  },

  forgetPassword: function forgetPassword(aEvent) {
    let host = Browser.selectedBrowser.currentURI;
    let lm = this._loginManager;

    lm.findLogins({}, host.prePath, "", "").forEach(function(login) {
      if (login.hostname == host.prePath)
        lm.removeLogin(login);
    });

    this.hideItem(aEvent.target);
    aEvent.stopPropagation(); // Don't hide the site menu.
  },

  clearPagePermissions: function clearPagePermissions(aEvent) {
    let pm = Services.perms;
    let host = Browser.selectedBrowser.currentURI;
    this._forEachPermissions(host, function(aType) {
      pm.remove(host.asciiHost, aType);
    });

    let lm = this._loginManager;
    if (!lm.getLoginSavingEnabled(host.prePath))
      lm.setLoginSavingEnabled(host.prePath, true);

    this.hideItem(aEvent.target);
    aEvent.stopPropagation(); // Don't hide the site menu.
  },

  savePageAsPDF: function saveAsPDF() {
    let browser = Browser.selectedBrowser;
    let fileName = getDefaultFileName(browser.contentTitle, browser.documentURI, null, null);
    fileName = fileName.trim() + ".pdf";
    let displayName = fileName;
#ifdef MOZ_PLATFORM_MAEMO
    fileName = fileName.replace(/[\*\:\?]+/g, " ");
#endif

    let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    let downloadsDir = dm.defaultDownloadsDirectory;

#ifdef ANDROID
    // Create the final destination file location
    // Try the intended filename, and if that doesn't work, try a safer one
    // (the intended one may have special characters)
    let file = null;
    [fileName, 'download.pdf'].forEach(function(potentialName, i) {
      if (file) return;
      let attemptedFile = downloadsDir.clone();
      attemptedFile.append(potentialName);
      // The filename is used below to save the file to a temp location in 
      // the content process. Make sure it's up to date.
      try {
        attemptedFile.createUnique(attemptedFile.NORMAL_FILE_TYPE, 0666);
      } catch (e) {
        // The first try may fail if the filename has special characters. If so
        // we will try with a safer name.
        if (i != 0)
          throw e;
        return;
      }
      file = attemptedFile;
      fileName = file.leafName;
    });
#else
    let picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    picker.init(window, Strings.browser.GetStringFromName("pageactions.saveas.pdf"), Ci.nsIFilePicker.modeSave);
    picker.appendFilter("PDF", "*.pdf");
    picker.defaultExtension = "pdf";

    picker.defaultString = fileName;

    picker.displayDirectory = downloadsDir;
    let rv = picker.show();
    if (rv == Ci.nsIFilePicker.returnCancel)
      return;

    let file = picker.file;
#endif

    // We must manually add this to the download system
    let db = dm.DBConnection;

    let stmt = db.createStatement(
      "INSERT INTO moz_downloads (name, source, target, startTime, endTime, state, referrer) " +
      "VALUES (:name, :source, :target, :startTime, :endTime, :state, :referrer)"
    );

    let current = browser.currentURI.spec;
    stmt.params.name = displayName;
    stmt.params.source = current;
    stmt.params.target = Services.io.newFileURI(file).spec;
    stmt.params.startTime = Date.now() * 1000;
    stmt.params.endTime = Date.now() * 1000;
    stmt.params.state = Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED;
    stmt.params.referrer = current;
    stmt.execute();
    stmt.finalize();

    let newItemId = db.lastInsertRowID;
    let download = dm.getDownload(newItemId);
    try {
      DownloadsView.downloadStarted(download);
    }
    catch(e) {}
    Services.obs.notifyObservers(download, "dl-start", null);

#ifdef ANDROID
    let tmpDir = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);  
    
    file = tmpDir.clone();
    file.append(fileName);

#endif

    let data = {
      type: Ci.nsIPrintSettings.kOutputFormatPDF,
      id: newItemId,
      referrer: current,
      filePath: file.path
    };

    Browser.selectedBrowser.messageManager.sendAsyncMessage("Browser:SaveAs", data);
  },

  updatePageSaveAs: function updatePageSaveAs(aNode) {
    // Check for local XUL content
    let contentWindow = Browser.selectedBrowser.contentWindow;
    return !(contentWindow && contentWindow.document instanceof XULDocument);
  },

  updateShare: function updateShare(aNode) {
    return Util.isShareableScheme(Browser.selectedBrowser.currentURI.scheme);
  },

  hideItem: function hideItem(aNode) {
    aNode.hidden = true;
    this._updateAttributes();
  },

  _updateAttributes: function _updateAttributes() {
    let container = document.getElementById("pageactions-container");
    let visibleNodes = container.querySelectorAll("pageaction:not([hidden=true])");
    let visibleCount = visibleNodes.length;

    for (let i = 0; i < visibleCount; i++)
      visibleNodes[i].classList.remove("odd-last-child");

    if (visibleCount % 2)
      visibleNodes[visibleCount - 1].classList.add("odd-last-child");
  }
};

var NewTabPopup = {
  _timeout: 0,
  _tabs: [],

  get box() {
    delete this.box;
    let box = document.getElementById("newtab-popup");

    // Move the popup on the other side if we are in RTL
    let [leftSidebar, rightSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
    if (leftSidebar.left > rightSidebar.left) {
      let margin = box.getAttribute("left");
      box.removeAttribute("left");
      box.setAttribute("right", margin);
    }

    return this.box = box;
  },

  _updateLabel: function nt_updateLabel() {
    let newtabStrings = Strings.browser.GetStringFromName("newtabpopup.opened");
    let label = PluralForm.get(this._tabs.length, newtabStrings).replace("#1", this._tabs.length);

    this.box.firstChild.setAttribute("value", label);
  },

  hide: function nt_hide() {
    if (this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = 0;
    }

    this._tabs = [];
    this.box.hidden = true;
    BrowserUI.popPopup(this);
  },

  show: function nt_show(aTab) {
    BrowserUI.pushPopup(this, this.box);

    this._tabs.push(aTab);
    this._updateLabel();

    this.box.top = aTab.getBoundingClientRect().top + (aTab.getBoundingClientRect().height / 3);
    this.box.hidden = false;

    if (this._timeout)
      clearTimeout(this._timeout);

    this._timeout = setTimeout(function(self) {
      self.hide();
    }, 2000, this);
  },

  selectTab: function nt_selectTab() {
    BrowserUI.selectTab(this._tabs.pop());
    this.hide();
  },

  handleEvent: function nt_handleEvent(aEvent) {
    // Bail early and fast
    if (!aEvent.detail)
      return;

    let [tabsVisibility,,,] = Browser.computeSidebarVisibility();
    if (tabsVisibility != 1.0)
      this.show(aEvent.originalTarget);
  }
};

var AwesomePanel = function(aElementId, aCommandId) {
  let command = document.getElementById(aCommandId);

  this.panel = document.getElementById(aElementId),

  this.open = function aw_open() {
    command.setAttribute("checked", "true");
    this.panel.hidden = false;

    if (this.panel.hasAttribute("onshow")) {
      let func = new Function("panel", this.panel.getAttribute("onshow"));
      func.call(this.panel);
    }

    if (this.panel.open)
      this.panel.open();
  },

  this.close = function aw_close() {
    if (this.panel.hasAttribute("onhide")) {
      let func = new Function("panel", this.panel.getAttribute("onhide"));
      func.call(this.panel);
    }

    if (this.panel.close)
      this.panel.close();

    this.panel.hidden = true;
    command.removeAttribute("checked", "true");
  },

  this.doCommand = function aw_doCommand() {
    BrowserUI.doCommand(aCommandId);
  },

  this.openLink = function aw_openLink(aEvent) {
    let item = aEvent.originalTarget;
    let uri = item.getAttribute("url") || item.getAttribute("uri");
    if (uri != "") {
      Browser.selectedBrowser.userTypedValue = uri;
      BrowserUI.goToURI(uri);
    }
  }
};

var BookmarkPopup = {
  get box() {
    delete this.box;
    this.box = document.getElementById("bookmark-popup");

    let [tabsSidebar, controlsSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
    this.box.setAttribute(tabsSidebar.left < controlsSidebar.left ? "right" : "left", controlsSidebar.width - this.box.offset);
    this.box.top = BrowserUI.starButton.getBoundingClientRect().top - this.box.offset;

    // Hide the popup if there is any new page loading
    let self = this;
    messageManager.addMessageListener("pagehide", function(aMessage) {
      self.hide();
    });

    return this.box;
  },

  _bookmarkPopupTimeout: -1,

  hide : function hide() {
    if (this._bookmarkPopupTimeout != -1) {
      clearTimeout(this._bookmarkPopupTimeout);
      this._bookmarkPopupTimeout = -1;
    }
    this.box.hidden = true;
    BrowserUI.popPopup(this);
  },

  show : function show() {
    this.box.hidden = false;
    this.box.anchorTo(BrowserUI.starButton);

    // include starButton here, so that click-to-dismiss works as expected
    BrowserUI.pushPopup(this, [this.box, BrowserUI.starButton]);
  },

  autoHide: function autoHide() {
    if (this._bookmarkPopupTimeout != -1 || this.box.hidden)
      return;

    this._bookmarkPopupTimeout = setTimeout(function (self) {
      self._bookmarkPopupTimeout = -1;
      self.hide();
    }, 2000, this);
  },

  toggle : function toggle() {
    if (this.box.hidden)
      this.show();
    else
      this.hide();
  }
};

var BookmarkHelper = {
  _panel: null,
  _editor: null,

  edit: function BH_edit(aURI) {
    if (!aURI)
      aURI = getBrowser().currentURI;

    let itemId = PlacesUtils.getMostRecentBookmarkForURI(aURI);
    if (itemId == -1)
      return;

    let title = PlacesUtils.bookmarks.getItemTitle(itemId);
    let tags = PlacesUtils.tagging.getTagsForURI(aURI, {});

    const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    this._editor = document.createElementNS(XULNS, "placeitem");
    this._editor.setAttribute("id", "bookmark-item");
    this._editor.setAttribute("flex", "1");
    this._editor.setAttribute("type", "bookmark");
    this._editor.setAttribute("ui", "manage");
    this._editor.setAttribute("title", title);
    this._editor.setAttribute("uri", aURI.spec);
    this._editor.setAttribute("itemid", itemId);
    this._editor.setAttribute("tags", tags.join(", "));
    this._editor.setAttribute("onclose", "BookmarkHelper.hide()");
    document.getElementById("bookmark-form").appendChild(this._editor);

    let toolbar = document.getElementById("toolbar-main");
    let top = toolbar.top + toolbar.boxObject.height;

    this._panel = document.getElementById("bookmark-container");
    this._panel.top = (top < 0 ? 0 : top);
    this._panel.hidden = false;
    BrowserUI.pushPopup(this, this._panel);

    let self = this;
    BrowserUI.lockToolbar();
    Browser.forceChromeReflow();
    self._editor.startEditing();
  },

  save: function BH_save() {
    this._editor.stopEditing(true);
  },

  hide: function BH_hide() {
    BrowserUI.unlockToolbar();
    BrowserUI.updateStar();

    // Note: the _editor will have already saved the data, if needed, by the time
    // this method is called, since this method is called via the "close" event.
    this._editor.parentNode.removeChild(this._editor);
    this._editor = null;

    this._panel.hidden = true;
    BrowserUI.popPopup(this);
  },

  removeBookmarksForURI: function BH_removeBookmarksForURI(aURI) {
    //XXX blargle xpconnect! might not matter, but a method on
    // nsINavBookmarksService that takes an array of items to
    // delete would be faster. better yet, a method that takes a URI!
    let itemIds = PlacesUtils.getBookmarksForURI(aURI);
    itemIds.forEach(PlacesUtils.bookmarks.removeItem);

    BrowserUI.updateStar();
  }
};

var FindHelperUI = {
  type: "find",
  commands: {
    next: "cmd_findNext",
    previous: "cmd_findPrevious",
    close: "cmd_findClose"
  },

  _open: false,
  _status: null,

  get status() {
    return this._status;
  },

  set status(val) {
    if (val != this._status) {
      this._status = val;
      this._textbox.setAttribute("status", val);
      this.updateCommands(this._textbox.value);
    }
  },

  init: function findHelperInit() {
    this._textbox = document.getElementById("find-helper-textbox");
    this._container = document.getElementById("content-navigator");

    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);

    // Listen for form assistant messages from content
    messageManager.addMessageListener("FindAssist:Show", this);
    messageManager.addMessageListener("FindAssist:Hide", this);

    // Listen for events where form assistant should be closed
    document.getElementById("tabs").addEventListener("TabSelect", this, true);
    Elements.browsers.addEventListener("URLChanged", this, true);
  },

  receiveMessage: function findHelperReceiveMessage(aMessage) {
    let json = aMessage.json;
    switch(aMessage.name) {
      case "FindAssist:Show":
        this.status = json.result;
        if (json.rect)
          this._zoom(Rect.fromRect(json.rect));
        break;

      case "FindAssist:Hide":
        if (this._container.getAttribute("type") == this.type)
          this.hide();
        break;
    }
  },

  handleEvent: function findHelperHandleEvent(aEvent) {
    switch (aEvent.type) {
      case "TabSelect":
        this.hide();
        break;

      case "URLChanged":
        if (aEvent.detail && aEvent.target == getBrowser())
          this.hide();
        break;
    }
  },

  show: function findHelperShow() {
    this._container.show(this);
    this.search(this._textbox.value);
    this._textbox.select();
    this._textbox.focus();
    this._open = true;

    // Prevent the view to scroll automatically while searching
    Browser.selectedBrowser.scrollSync = false;
  },

  hide: function findHelperHide() {
    if (!this._open)
      return;

    this._textbox.value = "";
    this.status = null;
    this._textbox.blur();
    this._container.hide(this);
    this._open = false;

    // Restore the scroll synchronisation
    Browser.selectedBrowser.scrollSync = true;
  },

  goToPrevious: function findHelperGoToPrevious() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Previous", { });
  },

  goToNext: function findHelperGoToNext() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Next", { });
  },

  search: function findHelperSearch(aValue) {
    this.updateCommands(aValue);

    // Don't bother searching if the value is empty
    if (aValue == "")
      return;

    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Find", { searchString: aValue });
  },

  updateCommands: function findHelperUpdateCommands(aValue) {
    let disabled = (this._status == Ci.nsITypeAheadFind.FIND_NOTFOUND) || (aValue == "");
    this._cmdPrevious.setAttribute("disabled", disabled);
    this._cmdNext.setAttribute("disabled", disabled);
  },

  _zoom: function _findHelperZoom(aElementRect) {
    // Zoom to a specified Rect
    if (aElementRect && Browser.selectedTab.allowZoom && Services.prefs.getBoolPref("findhelper.autozoom")) {
      let zoomLevel = Browser._getZoomLevelForRect(aElementRect);
      zoomLevel = Math.min(Math.max(kBrowserFormZoomLevelMin, zoomLevel), kBrowserFormZoomLevelMax);
      zoomLevel = Browser.selectedTab.clampZoomLevel(zoomLevel);

      let zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, zoomLevel);
      Browser.animatedZoomTo(zoomRect);
    }
  }
};

/**
 * Responsible for navigating forms and filling in information.
 *  - Navigating forms is handled by next and previous commands.
 *  - When an element is focused, the browser view zooms in to the control.
 *  - The caret positionning and the view are sync to keep the type
 *    in text into view for input fields (text/textarea).
 *  - Provides autocomplete box for input fields.
 */
var FormHelperUI = {
  type: "form",
  commands: {
    next: "cmd_formNext",
    previous: "cmd_formPrevious",
    close: "cmd_formClose"
  },

  get enabled() {
    return Services.prefs.getBoolPref("formhelper.enabled");
  },

  init: function formHelperInit() {
    this._container = document.getElementById("content-navigator");
    this._autofillContainer = document.getElementById("form-helper-autofill");
    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);

    // Listen for form assistant messages from content
    messageManager.addMessageListener("FormAssist:Show", this);
    messageManager.addMessageListener("FormAssist:Hide", this);
    messageManager.addMessageListener("FormAssist:Update", this);
    messageManager.addMessageListener("FormAssist:Resize", this);
    messageManager.addMessageListener("FormAssist:AutoComplete", this);

    // Listen for events where form assistant should be closed or updated
    let tabs = document.getElementById("tabs");
    tabs.addEventListener("TabSelect", this, true);
    tabs.addEventListener("TabClose", this, true);
    Elements.browsers.addEventListener("URLChanged", this, true);
    Elements.browsers.addEventListener("SizeChanged", this, true);

    // Listen for modal dialog to show/hide the UI
    messageManager.addMessageListener("DOMWillOpenModalDialog", this);
    messageManager.addMessageListener("DOMModalDialogClosed", this);
  },

  _currentBrowser: null,
  show: function formHelperShow(aElement, aHasPrevious, aHasNext) {
    this._currentBrowser = Browser.selectedBrowser;

    // Update the next/previous commands
    this._cmdPrevious.setAttribute("disabled", !aHasPrevious);
    this._cmdNext.setAttribute("disabled", !aHasNext);
    this._open = true;

    let lastElement = this._currentElement || null;
    this._currentElement = {
      id: aElement.id,
      name: aElement.name,
      value: aElement.value,
      maxLength: aElement.maxLength,
      type: aElement.type,
      isAutocomplete: aElement.isAutocomplete,
      list: aElement.choices
    }

    this._updateContainer(lastElement, this._currentElement);
    this._zoom(Rect.fromRect(aElement.rect), Rect.fromRect(aElement.caretRect));

    // Prevent the view to scroll automatically while typing
    this._currentBrowser.scrollSync = false;
  },

  hide: function formHelperHide() {
    if (!this._open)
      return;

    // Restore the scroll synchonisation
    this._currentBrowser.scrollSync = true;

    // reset current Element and Caret Rect
    this._currentElementRect = null;
    this._currentCaretRect = null;

    this._updateContainerForSelect(this._currentElement, null);

    this._currentBrowser.messageManager.sendAsyncMessage("FormAssist:Closed", { });
    this._open = false;
  },

  // for VKB that does not resize the window
  _currentCaretRect: null,
  _currentElementRect: null,
  handleEvent: function formHelperHandleEvent(aEvent) {
    if (!this._open)
      return;

    switch (aEvent.type) {
      case "TabSelect":
      case "TabClose":
        this.hide();
        break;

      case "URLChanged":
        if (aEvent.detail && aEvent.target == getBrowser())
          this.hide();
        break;

      case "SizeChanged":
        setTimeout(function(self) {
          SelectHelperUI.resize();
          self._container.contentHasChanged();

          self._zoom(this._currentElementRect, this._currentCaretRect);
        }, 0, this);
        break;
    }
  },

  receiveMessage: function formHelperReceiveMessage(aMessage) {
    if (!this._open && aMessage.name != "FormAssist:Show" && aMessage.name != "FormAssist:Hide")
      return;

    let json = aMessage.json;
    switch (aMessage.name) {
      case "FormAssist:Show":
        // if the user has manually disabled the Form Assistant UI we still
        // want to show a UI for <select /> element but not managed by
        // FormHelperUI
        this.enabled ? this.show(json.current, json.hasPrevious, json.hasNext)
                     : SelectHelperUI.show(json.current.choices);
        break;

      case "FormAssist:Hide":
        this.enabled ? this.hide()
                     : SelectHelperUI.hide();
        break;

      case "FormAssist:Resize":
        let element = json.current;
        this._zoom(Rect.fromRect(element.rect), Rect.fromRect(element.caretRect));
        break;

      case "FormAssist:AutoComplete":
        this._updateAutocompleteFor(json.current);
        this._container.contentHasChanged();
        break;

       case "FormAssist:Update":
        Browser.hideSidebars();
        Browser.hideTitlebar();
        this._zoom(null, Rect.fromRect(json.caretRect));
        break;

      case "DOMWillOpenModalDialog":
        if (aMessage.target == Browser.selectedBrowser) {
          this._container.style.display = "none";
          this._container._spacer.hidden = true;
        }
        break;

      case "DOMModalDialogClosed":
        if (aMessage.target == Browser.selectedBrowser) {
          this._container.style.display = "-moz-box";
          this._container._spacer.hidden = false;
        }
        break;
    }
  },

  goToPrevious: function formHelperGoToPrevious() {
    this._currentBrowser.messageManager.sendAsyncMessage("FormAssist:Previous", { });
  },

  goToNext: function formHelperGoToNext() {
    this._currentBrowser.messageManager.sendAsyncMessage("FormAssist:Next", { });
  },

  doAutoComplete: function formHelperDoAutoComplete(aElement) {
    // Suggestions are only in <label>s. Ignore the rest.
    if (aElement instanceof Ci.nsIDOMXULLabelElement)
      this._currentBrowser.messageManager.sendAsyncMessage("FormAssist:AutoComplete", { value: aElement.value });
  },

  get _open() {
    return (this._container.getAttribute("type") == this.type);
  },

  set _open(aVal) {
    if (aVal == this._open)
      return;

    this._container.hidden = !aVal;
    this._container.contentHasChanged();

    if (aVal) {
      this._zoomStart();
      this._container.show(this);
    } else {
      this._zoomFinish();
      this._currentElement = null;
      this._container.hide(this);
    }

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("FormUI", true, true, window, aVal);
    this._container.dispatchEvent(evt);
  },

  _updateAutocompleteFor: function _formHelperUpdateAutocompleteFor(aElement) {
    let suggestions = this._getAutocompleteSuggestions(aElement);
    this._displaySuggestions(suggestions);
  },

  _displaySuggestions: function _formHelperDisplaySuggestions(aSuggestions) {
    let autofill = this._autofillContainer;
    while (autofill.hasChildNodes())
      autofill.removeChild(autofill.lastChild);

    let fragment = document.createDocumentFragment();
    for (let i = 0; i < aSuggestions.length; i++) {
      let value = aSuggestions[i];
      let button = document.createElement("label");
      button.setAttribute("value", value);
      button.className = "form-helper-autofill-label";
      fragment.appendChild(button);
    }
    autofill.appendChild(fragment);
    autofill.collapsed = !aSuggestions.length;
  },

  /** Retrieve the autocomplete list from the autocomplete service for an element */
  _getAutocompleteSuggestions: function _formHelperGetAutocompleteSuggestions(aElement) {
    if (!aElement.isAutocomplete)
      return [];

    let suggestions = [];

    let autocompleteService = Cc["@mozilla.org/satchel/form-autocomplete;1"].getService(Ci.nsIFormAutoComplete);
    let results = autocompleteService.autoCompleteSearch(aElement.name, aElement.value, aElement, null);
    if (results.matchCount > 0) {
      for (let i = 0; i < results.matchCount; i++) {
        let value = results.getValueAt(i);
        suggestions.push(value);
      }
    }

    return suggestions;
  },

  /** Update the form helper container to reflect new element user is editing. */
  _updateContainer: function _formHelperUpdateContainer(aLastElement, aCurrentElement) {
    this._updateContainerForSelect(aLastElement, aCurrentElement);

    // Setup autofill UI
    this._updateAutocompleteFor(aCurrentElement);
    this._container.contentHasChanged();
  },

  /** Helper for _updateContainer that handles the case where the new element is a select. */
  _updateContainerForSelect: function _formHelperUpdateContainerForSelect(aLastElement, aCurrentElement) {
    let lastHasChoices = aLastElement && (aLastElement.list != null);
    let currentHasChoices = aCurrentElement && (aCurrentElement.list != null);

    if (!lastHasChoices && currentHasChoices) {
      SelectHelperUI.dock(this._container);
      SelectHelperUI.show(aCurrentElement.list);
    } else if (lastHasChoices && currentHasChoices) {
      SelectHelperUI.reset();
      SelectHelperUI.show(aCurrentElement.list);
    } else if (lastHasChoices && !currentHasChoices) {
      SelectHelperUI.hide();
    }
  },

  /** Zoom and move viewport so that element is legible and touchable. */
  _zoom: function _formHelperZoom(aElementRect, aCaretRect) {
    let browser = getBrowser();
    let zoomRect = Rect.fromRect(browser.getBoundingClientRect());

    // Zoom to a specified Rect
    let autozoomEnabled = Services.prefs.getBoolPref("formhelper.autozoom");
    if (aElementRect && Browser.selectedTab.allowZoom && autozoomEnabled) {
      this._currentElementRect = aElementRect;
      // Zoom to an element by keeping the caret into view
      let zoomLevel = Browser.selectedTab.clampZoomLevel(this._getZoomLevelForRect(aElementRect));

      zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, zoomLevel);
      Browser.animatedZoomTo(zoomRect);
    } else if (aElementRect && !Browser.selectedTab.allowZoom && autozoomEnabled) {
      // Even if zooming is disabled we could need to reposition the view in
      // order to keep the element on-screen
      Browser.animatedZoomTo(zoomRect);
    }

    this._ensureCaretVisible(aCaretRect);
  },

  _ensureCaretVisible: function _ensureCaretVisible(aCaretRect) {
    if (!aCaretRect)
      return;

    // the scrollX/scrollY position can change because of the animated zoom so
    // delay the caret adjustment
    if (AnimatedZoom.isZooming()) {
      let self = this;
      window.addEventListener("AnimatedZoomEnd", function() {
        window.removeEventListener("AnimatedZoomEnd", arguments.callee, true);
          self._ensureCaretVisible(aCaretRect);
      }, true);
      return;
    }

    let browser = getBrowser();
    let zoomRect = Rect.fromRect(browser.getBoundingClientRect());

    this._currentCaretRect = aCaretRect;
    let caretRect = aCaretRect.scale(browser.scale, browser.scale);

    let scroll = browser.getPosition();
    zoomRect = new Rect(scroll.x, scroll.y, zoomRect.width, zoomRect.height);
    if (zoomRect.contains(caretRect))
      return;

    let [deltaX, deltaY] = this._getOffsetForCaret(caretRect, zoomRect);
    if (deltaX != 0 || deltaY != 0)
      browser.scrollBy(deltaX, deltaY);
  },

  /* Store the current zoom level, and scroll positions to restore them if needed */
  _zoomStart: function _formHelperZoomStart() {
    if (!Services.prefs.getBoolPref("formhelper.restore"))
      return;

    this._restore = {
      scale: getBrowser().scale,
      contentScrollOffset: Browser.getScrollboxPosition(Browser.contentScrollboxScroller),
      pageScrollOffset: Browser.getScrollboxPosition(Browser.pageScrollboxScroller)
    };
  },

  /** Element is no longer selected. Restore zoom level if setting is enabled. */
  _zoomFinish: function _formHelperZoomFinish() {
    if(!Services.prefs.getBoolPref("formhelper.restore"))
      return;

    let restore = this._restore;
    getBrowser().scale = restore.scale;
    Browser.contentScrollboxScroller.scrollTo(restore.contentScrollOffset.x, restore.contentScrollOffset.y);
    Browser.pageScrollboxScroller.scrollTo(restore.pageScrollOffset.x, restore.pageScrollOffset.y);
  },

  _getZoomLevelForRect: function _getZoomLevelForRect(aRect) {
    const margin = 30;
    let zoomLevel = getBrowser().getBoundingClientRect().width / (aRect.width + margin);
    return Util.clamp(zoomLevel, kBrowserFormZoomLevelMin, kBrowserFormZoomLevelMax);
  },

  _getOffsetForCaret: function _formHelperGetOffsetForCaret(aCaretRect, aRect) {
    // Determine if we need to move left or right to bring the caret into view
    let deltaX = 0;
    if (aCaretRect.right > aRect.right)
      deltaX = aCaretRect.right - aRect.right;
    if (aCaretRect.left < aRect.left)
      deltaX = aCaretRect.left - aRect.left;

    // Determine if we need to move up or down to bring the caret into view
    let deltaY = 0;
    if (aCaretRect.bottom > aRect.bottom)
      deltaY = aCaretRect.bottom - aRect.bottom;
    if (aCaretRect.top < aRect.top)
      deltaY = aCaretRect.top - aRect.top;

    return [deltaX, deltaY];
  }
};

/**
 * SelectHelperUI: Provides an interface for making a choice in a list.
 *   Supports simultaneous selection of choices and group headers.
 */
var SelectHelperUI = {
  _list: null,
  _selectedIndexes: null,

  get _panel() {
    delete this._panel;
    return this._panel = document.getElementById("select-container");
  },

  get _textbox() {
    delete this._textbox;
    return this._textbox = document.getElementById("select-helper-textbox");
  },

  show: function selectHelperShow(aList) {
    this.showFilter = false;
    this._textbox.blur();
    this._list = aList;

    this._container = document.getElementById("select-list");
    this._container.setAttribute("multiple", aList.multiple ? "true" : "false");

    this._selectedIndexes = this._getSelectedIndexes();
    let firstSelected = null;

    // Using a fragment prevent us to hang on huge list
    let fragment = document.createDocumentFragment();
    let choices = aList.choices;
    for (let i = 0; i < choices.length; i++) {
      let choice = choices[i];
      let item = document.createElement("option");
      item.className = "chrome-select-option";
      item.setAttribute("label", choice.text);
      choice.disabled ? item.setAttribute("disabled", choice.disabled)
                      : item.removeAttribute("disabled");
      fragment.appendChild(item);

      if (choice.group) {
        item.classList.add("optgroup");
        continue;
      }

      item.optionIndex = choice.optionIndex;
      item.choiceIndex = i;

      if (choice.inGroup)
        item.classList.add("in-optgroup");

      if (choice.selected) {
        item.setAttribute("selected", "true");
        firstSelected = firstSelected || item;
      }
    }
    this._container.appendChild(fragment);

    this._panel.hidden = false;
    this._panel.height = this._panel.getBoundingClientRect().height;

    if (!this._docked)
      BrowserUI.pushPopup(this, this._panel);

    this._scrollElementIntoView(firstSelected);

    this._container.addEventListener("click", this, false);
    this._panel.addEventListener("overflow", this, true);
  },

  _showFilter: false,
  get showFilter() {
    return this._showFilter;
  },

  set showFilter(val) {
    this._showFilter = val;
    if (!this._panel.hidden)
      this._textbox.hidden = !val;
  },

  dock: function selectHelperDock(aContainer) {
    aContainer.insertBefore(this._panel, aContainer.lastChild);
    this.resize();
    this._docked = true;
  },

  undock: function selectHelperUndock() {
    let rootNode = Elements.stack;
    rootNode.insertBefore(this._panel, rootNode.lastChild);
    this._panel.style.maxHeight = "";
    this._docked = false;
  },

  reset: function selectHelperReset() {
    this._updateControl();
    let empty = this._container.cloneNode(false);
    this._container.parentNode.replaceChild(empty, this._container);
    this._container = empty;
    this._list = null;
    this._selectedIndexes = null;
    this._panel.height = "";
    this._textbox.value = "";
  },

  resize: function selectHelperResize() {
    this._panel.style.maxHeight = (window.innerHeight / 1.8) + "px";
  },

  hide: function selectHelperResize() {
    this.showFilter = false;
    this._container.removeEventListener("click", this, false);
    this._panel.removeEventListener("overflow", this, true);

    this._panel.hidden = true;

    if (this._docked)
      this.undock();
    else
      BrowserUI.popPopup(this);

    this.reset();
  },

  filter: function selectHelperFilter(aValue) {
    let reg = new RegExp(aValue, "gi");
    let options = this._container.childNodes;
    for (let i = 0; i < options.length; i++) {
      let option = options[i];
      option.getAttribute("label").match(reg) ? option.removeAttribute("filtered")
                                              : option.setAttribute("filtered", "true");
    }
  },

  unselectAll: function selectHelperUnselectAll() {
    if (!this._list)
      return;

    let choices = this._list.choices;
    this._forEachOption(function(aItem, aIndex) {
      aItem.selected = false;
      choices[aIndex].selected = false;
    });
  },

  selectByIndex: function selectHelperSelectByIndex(aIndex) {
    if (!this._list)
      return;

    let choices = this._list.choices;
    for (let i = 0; i < this._container.childNodes.length; i++) {
      let option = this._container.childNodes[i];
      if (option.optionIndex == aIndex) {
        option.selected = true;
        this._choices[i].selected = true;
        this._scrollElementIntoView(option);
        break;
      }
    }
  },

  _getSelectedIndexes: function _selectHelperGetSelectedIndexes() {
    let indexes = [];
    if (!this._list)
      return indexes;

    let choices = this._list.choices;
    let choiceLength = choices.length;
    for (let i = 0; i < choiceLength; i++) {
      let choice = choices[i];
      if (choice.selected)
        indexes.push(choice.optionIndex);
    }
    return indexes;
  },

  _scrollElementIntoView: function _selectHelperScrollElementIntoView(aElement) {
    if (!aElement)
      return;

    let index = -1;
    this._forEachOption(
      function(aItem, aIndex) {
        if (aElement.optionIndex == aItem.optionIndex)
          index = aIndex;
      }
    );

    if (index == -1)
      return;

    let scrollBoxObject = this._container.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    let itemHeight = aElement.getBoundingClientRect().height;
    let visibleItemsCount = this._container.boxObject.height / itemHeight;
    if ((index + 1) > visibleItemsCount) {
      let delta = Math.ceil(visibleItemsCount / 2);
      scrollBoxObject.scrollTo(0, ((index + 1) - delta) * itemHeight);
    }
    else {
      scrollBoxObject.scrollTo(0, 0);
    }
  },

  _forEachOption: function _selectHelperForEachOption(aCallback) {
    let children = this._container.children;
    for (let i = 0; i < children.length; i++) {
      let item = children[i];
      if (!item.hasOwnProperty("optionIndex"))
        continue;
      aCallback(item, i);
    }
  },

  _updateControl: function _selectHelperUpdateControl() {
    let currentSelectedIndexes = this._getSelectedIndexes();

    let isIdentical = (this._selectedIndexes && this._selectedIndexes.length == currentSelectedIndexes.length);
    if (isIdentical) {
      for (let i = 0; i < currentSelectedIndexes.length; i++) {
        if (currentSelectedIndexes[i] != this._selectedIndexes[i]) {
          isIdentical = false;
          break;
        }
      }
    }

    if (isIdentical)
      return;

    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceChange", { });
  },

  handleEvent: function selectHelperHandleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        let item = aEvent.target;
        if (item && item.hasOwnProperty("optionIndex")) {
          if (this._list.multiple) {
            // Toggle the item state
            item.selected = !item.selected;
          }
          else {
            this.unselectAll();

            // Select the new one and update the control
            item.selected = true;
          }
          this.onSelect(item.optionIndex, item.selected, !this._list.multiple);
        }
        break;
      case "overflow":
        if (!this._textbox.value)
          this.showFilter = true;
        break;
    }
  },

  onSelect: function selectHelperOnSelect(aIndex, aSelected, aClearAll) {
    let json = {
      index: aIndex,
      selected: aSelected,
      clearAll: aClearAll
    };
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceSelect", json);

    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-button-element.html#the-select-element
    if (!this._list.multiple) {
      this._updateControl();
      // Update the selectedIndex so the field will fire a new change event if
      // needed
      this._selectedIndexes = [aIndex];
    }

  }
};

var MenuListHelperUI = {
  get _container() {
    delete this._container;
    return this._container = document.getElementById("menulist-container");
  },

  get _popup() {
    delete this._popup;
    return this._popup = document.getElementById("menulist-popup");
  },

  get _title() {
    delete this._title;
    return this._title = document.getElementById("menulist-title");
  },

  _firePopupEvent: function firePopupEvent(aEventName) {
    let menupopup = this._currentList.menupopup;
    if (menupopup.hasAttribute(aEventName)) {
      let func = new Function("event", menupopup.getAttribute(aEventName));
      func.call(this);
    }
  },

  _currentList: null,
  show: function mn_show(aMenulist) {
    this._currentList = aMenulist;
    this._container.setAttribute("for", aMenulist.id);
    this._title.value = aMenulist.title || "";
    this._firePopupEvent("onpopupshowing");

    let container = this._container;
    let listbox = this._popup.lastChild;
    while (listbox.firstChild)
      listbox.removeChild(listbox.firstChild);

    let children = this._currentList.menupopup.children;
    for (let i = 0; i < children.length; i++) {
      let child = children[i];
      let item = document.createElement("richlistitem");
      if (child.disabled)
        item.setAttribute("disabled", "true");
      if (child.hidden)
        item.setAttribute("hidden", "true");

      // Add selected as a class name instead of an attribute to not being overidden
      // by the richlistbox behavior (it sets the "current" and "selected" attribute
      item.setAttribute("class", "menulist-command prompt-button" + (child.selected ? " selected" : ""));

      let image = document.createElement("image");
      image.setAttribute("src", child.image || "");
      item.appendChild(image);

      let label = document.createElement("label");
      label.setAttribute("value", child.label);
      item.appendChild(label);

      listbox.appendChild(item);
    }

    window.addEventListener("resize", this, true);
    container.hidden = false;
    this.sizeToContent();
    BrowserUI.pushPopup(this, [this._popup]);
  },

  hide: function mn_hide() {
    this._currentList = null;
    this._container.removeAttribute("for");
    this._container.hidden = true;
    window.removeEventListener("resize", this, true);
    BrowserUI.popPopup(this);
  },

  selectByIndex: function mn_selectByIndex(aIndex) {
    this._currentList.selectedIndex = aIndex;

    // Dispatch a xul command event to the attached menulist
    if (this._currentList.dispatchEvent) {
      let evt = document.createEvent("XULCommandEvent");
      evt.initCommandEvent("command", true, true, window, 0, false, false, false, false, null);
      this._currentList.dispatchEvent(evt);
    }

    this.hide();
  },

  sizeToContent: function sizeToContent() {
    this._popup.maxWidth = window.innerWidth * 0.75;
  },

  handleEvent: function handleEvent(aEvent) {
    this.sizeToContent();
  }
}

var ContextHelper = {
  popupState: null,

  get _panel() {
    delete this._panel;
    return this._panel = document.getElementById("context-container");
  },

  get _popup() {
    delete this._popup;
    return this._popup = document.getElementById("context-popup");
  },

  showPopup: function ch_showPopup(aMessage) {
    this.popupState = aMessage.json;
    this.popupState.target = aMessage.target;

    let first = null;
    let last = null;
    let commands = document.getElementById("context-commands");
    for (let i=0; i<commands.childElementCount; i++) {
      let command = commands.children[i];
      command.removeAttribute("selector");
      command.hidden = true;

      let types = command.getAttribute("type").split(/\s+/);
      for (let i=0; i<types.length; i++) {
        if (this.popupState.types.indexOf(types[i]) != -1) {
          first = first || command;
          last = command;
          command.hidden = false;
          break;
        }
      }
    }

    if (!first) {
      this.popupState = null;
      return false;
    }

    // Allow the first and last *non-hidden* elements to be selected in CSS.
    first.setAttribute("selector", "first-child");
    last.setAttribute("selector", "last-child");

    let label = document.getElementById("context-hint");
    label.value = this.popupState.label || "";

    this._panel.hidden = false;
    window.addEventListener("resize", this, true);
    window.addEventListener("keypress", this, true);

    this.sizeToContent();
    BrowserUI.pushPopup(this, [this._popup]);
    return true;
  },

  hide: function ch_hide() {
    if (this._panel.hidden)
      return;
    this.popupState = null;
    this._panel.hidden = true;
    window.removeEventListener("resize", this, true);
    window.removeEventListener("keypress", this, true);

    BrowserUI.popPopup(this);
  },

  sizeToContent: function sizeToContent() {
    this._popup.maxWidth = window.innerWidth * 0.75;
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "resize":
        this.sizeToContent();
        break;
      case "keypress":
        // Hide the context menu so you can't type behind it.
        aEvent.stopPropagation();
        aEvent.preventDefault();
        if (aEvent.keyCode != aEvent.DOM_VK_ESCAPE)
          this.hide();
        break;
    }
  }
};

var ContextCommands = {
  copy: function cc_copy() {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(ContextHelper.popupState.string);

    let target = ContextHelper.popupState.target;
    if (target)
      target.focus();
  },

#ifdef ANDROID
  selectInput: function cc_selectInput() {
    let imePicker = Cc["@mozilla.org/imepicker;1"].getService(Ci.nsIIMEPicker);
    imePicker.show();
  },
#endif

  paste: function cc_paste() {
    let data = ContextHelper.popupState.data;
    let target = ContextHelper.popupState.target;
    target.editor.paste(Ci.nsIClipboard.kGlobalClipboard);
    target.focus();
  },

  selectAll: function cc_selectAll() {
    let target = ContextHelper.popupState.target;
    target.editor.selectAll();
    target.focus();
  },

  openInNewTab: function cc_openInNewTab() {
    Browser.addTab(ContextHelper.popupState.linkURL, false, Browser.selectedTab);
  },

  saveLink: function cc_saveLink() {
    let browser = ContextHelper.popupState.target;
    saveURL(ContextHelper.popupState.linkURL, null, "SaveLinkTitle", false, true, browser.documentURI);
  },

  saveImage: function cc_saveImage() {
    let browser = ContextHelper.popupState.target;
    saveImageURL(ContextHelper.popupState.mediaURL, null, "SaveImageTitle", false, true, browser.documentURI);
  },

  shareLink: function cc_shareLink() {
    let state = ContextHelper.popupState;
    SharingUI.show(state.linkURL, state.linkTitle);
  },

  shareMedia: function cc_shareMedia() {
    SharingUI.show(ContextHelper.popupState.mediaURL, null);
  },

  sendCommand: function cc_playVideo(aCommand) {
    let browser = ContextHelper.popupState.target;
    browser.messageManager.sendAsyncMessage("Browser:ContextCommand", { command: aCommand });
  },

  editBookmark: function cc_editBookmark() {
    let target = ContextHelper.popupState.target;
    target.startEditing();
  },

  removeBookmark: function cc_removeBookmark() {
    let target = ContextHelper.popupState.target;
    target.remove();
  }
}

var SharingUI = {
  _dialog: null,

  show: function show(aURL, aTitle) {
    try {
      this.showSharingUI(aURL, aTitle);
    } catch (ex) {
      this.showFallback(aURL, aTitle);
    }
  },

  showSharingUI: function showSharingUI(aURL, aTitle) {
    let sharingSvc = Cc["@mozilla.org/uriloader/external-sharing-app-service;1"].getService(Ci.nsIExternalSharingAppService);
    sharingSvc.shareWithDefault(aURL, "text/plain", aTitle);
  },

  showFallback: function showFallback(aURL, aTitle) {
    this._dialog = importDialog(window, "chrome://browser/content/share.xul", null);
    document.getElementById("share-title").value = aTitle || aURL;

    BrowserUI.pushPopup(this, this._dialog);

    let bbox = document.getElementById("share-buttons-box");
    this._handlers.forEach(function(handler) {
      let button = document.createElement("button");
      button.className = "prompt-button";
      button.setAttribute("label", handler.name);
      button.addEventListener("command", function() {
        SharingUI.hide();
        handler.callback(aURL || "", aTitle || "");
      }, false);
      bbox.appendChild(button);
    });
    this._dialog.waitForClose();
    BrowserUI.popPopup(this);
  },

  hide: function hide() {
    this._dialog.close();
    this._dialog = null;
  },

  _handlers: [
    {
      name: "Email",
      callback: function callback(aURL, aTitle) {
        let url = "mailto:?subject=" + encodeURIComponent(aTitle) +
                  "&body=" + encodeURIComponent(aURL);
        let uri = Services.io.newURI(url, null, null);
        let extProtocolSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"]
                             .getService(Ci.nsIExternalProtocolService);
        extProtocolSvc.loadUrl(uri);
      }
    },
    {
      name: "Twitter",
      callback: function callback(aURL, aTitle) {
        let url = "http://twitter.com/home?status=" + encodeURIComponent((aTitle ? aTitle+": " : "")+aURL);
        BrowserUI.newTab(url, Browser.selectedTab);
      }
    },
    {
      name: "Google Reader",
      callback: function callback(aURL, aTitle) {
        let url = "http://www.google.com/reader/link?url=" + encodeURIComponent(aURL) +
                  "&title=" + encodeURIComponent(aTitle);
        BrowserUI.addTab(url, Browser.selectedTab);
      }
    },
    {
      name: "Facebook",
      callback: function callback(aURL, aTitle) {
        let url = "http://www.facebook.com/share.php?u=" + encodeURIComponent(aURL);
        BrowserUI.newTab(url, Browser.selectedTab);
      }
    }
  ]
};

var BadgeHandlers = {
  _handlers: [
    {
      _lastUpdate: 0,
      _lastCount: 0,
      url: "https://mail.google.com/mail",
      updateBadge: function(aBadge) {
        // Use the cache if possible
        let now = Date.now();
        if (this._lastCount && this._lastUpdate > now - 1000) {
          aBadge.set(this._lastCount);
          return;
        }

        this._lastUpdate = now;

        // Use any saved username and password. If we don't have any login and we are not
        // currently logged into Gmail, we won't get any count.
        let login = BadgeHandlers.getLogin("https://www.google.com");

        // Get the feed and read the count, passing any saved username and password
        // but do not show any security dialogs if we fail
        let req = new XMLHttpRequest();
        req.mozBackgroundRequest = true;
        req.open("GET", "https://mail.google.com/mail/feed/atom", true, login.username, login.password);
        req.onreadystatechange = function(aEvent) {
          if (req.readyState == 4) {
            if (req.status == 200 && req.responseXML) {
              let count = req.responseXML.getElementsByTagName("fullcount");
              this._lastCount = count ? count[0].childNodes[0].nodeValue : 0;
            } else {
              this._lastCount = 0;
            }
            this._lastCount = BadgeHandlers.setNumberBadge(aBadge, this._lastCount);
          }
        };
        req.send(null);
      }
    }
  ],

  register: function(aPopup) {
    let handlers = this._handlers;
    for (let i = 0; i < handlers.length; i++)
      aPopup.registerBadgeHandler(handlers[i].url, handlers[i]);
  },

  getLogin: function(aURL) {
    let lm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
    let logins = lm.findLogins({}, aURL, aURL, null);
    let username = logins.length > 0 ? logins[0].username : "";
    let password = logins.length > 0 ? logins[0].password : "";
    return { username: username, password: password };
  },

  clampBadge: function(aValue) {
    if (aValue > 100)
      aValue = "99+";
    return aValue;
  },

  setNumberBadge: function(aBadge, aValue) {
    if (parseInt(aValue) != 0) {
      aValue = this.clampBadge(aValue);
      aBadge.set(aValue);
    } else {
      aBadge.set("");
    }
    return aValue;
  }
};

var FullScreenVideo = {
  browser: null,

  init: function fsv_init() {
    messageManager.addMessageListener("Browser:FullScreenVideo:Start", this.show.bind(this));
    messageManager.addMessageListener("Browser:FullScreenVideo:Close", this.hide.bind(this));
    messageManager.addMessageListener("Browser:FullScreenVideo:Play", this.play.bind(this));
    messageManager.addMessageListener("Browser:FullScreenVideo:Pause", this.pause.bind(this));

    // If the screen supports brightness locks, we will utilize that, see checkBrightnessLocking()
    try {
      this.screen = null;
      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"].getService(Ci.nsIScreenManager);
      let screen = screenManager.primaryScreen.QueryInterface(Ci.nsIScreen_MOZILLA_2_0_BRANCH);
      this.screen = screen;
    }
    catch (e) {} // The screen does not support brightness locks
  },

  play: function() {
    this.playing = true;
    this.checkBrightnessLocking();
  },

  pause: function() {
    this.playing = false;
    this.checkBrightnessLocking();
  },

  // We lock the screen brightness - prevent it from dimming or turning off - when
  // we are fullscreen, and are playing (and the screen supports that feature).
  checkBrightnessLocking: function() {
    var shouldLock = !!this.screen && !!window.fullScreen && !!this.playing;
    var locking = !!this.brightnessLocked;
    if (shouldLock == locking)
      return;

    if (shouldLock)
      this.screen.lockMinimumBrightness(this.screen.BRIGHTNESS_FULL);
    else
      this.screen.unlockMinimumBrightness(this.screen.BRIGHTNESS_FULL);
    this.brightnessLocked = shouldLock;
  },

  show: function fsv_show() {
    this.createBrowser();
    window.fullScreen = true;
    BrowserUI.pushPopup(this, this.browser);
    this.checkBrightnessLocking();
  },

  hide: function fsv_hide() {
    this.destroyBrowser();
    window.fullScreen = false;
    BrowserUI.popPopup(this);
    this.checkBrightnessLocking();
  },

  createBrowser: function fsv_createBrowser() {
    let browser = this.browser = document.createElement("browser");
    browser.className = "window-width window-height full-screen";
    browser.setAttribute("type", "content");
    browser.setAttribute("remote", "true");
    browser.setAttribute("src", "chrome://browser/content/fullscreen-video.xhtml");
    document.getElementById("main-window").appendChild(browser);

    let mm = browser.messageManager;
    mm.loadFrameScript("chrome://browser/content/fullscreen-video.js", true);

    browser.addEventListener("TapDown", this, true);
    browser.addEventListener("TapSingle", this, false);

    return browser;
  },

  destroyBrowser: function fsv_destroyBrowser() {
    let browser = this.browser;
    browser.removeEventListener("TapDown", this, false);
    browser.removeEventListener("TapSingle", this, false);
    browser.parentNode.removeChild(browser);
    this.browser = null;
  },

  handleEvent: function fsv_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "TapDown":
        this._dispatchMouseEvent("Browser:MouseDown", aEvent.clientX, aEvent.clientY);
        break;
      case "TapSingle":
        this._dispatchMouseEvent("Browser:MouseUp", aEvent.clientX, aEvent.clientY);
        break;
    }
  },

  _dispatchMouseEvent: function fsv_dispatchMouseEvent(aName, aX, aY) {
    let pos = this.browser.transformClientToBrowser(aX, aY);
    this.browser.messageManager.sendAsyncMessage(aName, {
      x: pos.x,
      y: pos.y,
      messageId: null
    });
  }
};

var AppMenu = {
  get panel() {
    delete this.panel;
    return this.panel = document.getElementById("appmenu");
  },

  show: function show() {
    if (BrowserUI.activePanel || BrowserUI.isPanelVisible())
      return;
    this.panel.setAttribute("count", this.panel.childNodes.length);
    this.panel.collapsed = false;

    addEventListener("keypress", this, true);

    BrowserUI.lockToolbar();
    BrowserUI.pushPopup(this, [this.panel, Elements.toolbarContainer]);
  },

  hide: function hide() {
    this.panel.collapsed = true;

    removeEventListener("keypress", this, true);

    BrowserUI.unlockToolbar();
    BrowserUI.popPopup(this);
  },

  toggle: function toggle() {
    this.panel.collapsed ? this.show() : this.hide();
  },

  handleEvent: function handleEvent(aEvent) {
    this.hide();
  }
};
