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

Cu.import("resource://gre/modules/utils.js");
Cu.import("resource://gre/modules/PluralForm.jsm");

const TOOLBARSTATE_LOADING  = 1;
const TOOLBARSTATE_LOADED   = 2;

const kDefaultFavIconURL = "chrome://browser/skin/images/favicon-default-30.png";

[
  [
    "gHistSvc",
    "@mozilla.org/browser/nav-history-service;1",
    [Ci.nsINavHistoryService, Ci.nsIBrowserHistory]
  ],
  [
    "gURIFixup",
    "@mozilla.org/docshell/urifixup;1",
    [Ci.nsIURIFixup]
  ],
  [
    "gPrefService",
    "@mozilla.org/preferences-service;1",
    [Ci.nsIPrefBranch2]
  ]
].forEach(function (service) {
  let [name, contract, ifaces] = service;
  window.__defineGetter__(name, function () {
    delete window[name];
    window[name] = Cc[contract].getService(ifaces.splice(0, 1)[0]);
    if (ifaces.length)
      ifaces.forEach(function (i) { return window[name].QueryInterface(i); });
    return window[name];
  });
});

var BrowserUI = {
  _edit : null,
  _throbber : null,
  _favicon : null,
  _faviconLink : null,
  _dialogs: [],

  _titleChanged : function(aDocument) {
    var browser = Browser.selectedBrowser;
    if (browser && aDocument != browser.contentDocument)
      return;

    var caption = aDocument.title;
    if (!caption) {
      caption = this.getDisplayURI(browser);
      if (caption == "about:blank")
        caption = "";
    }
    this._edit.value = caption;
  },

  /*
   * Dispatched by window.close() to allow us to turn window closes into tabs
   * closes.
   */
  _domWindowClose: function (aEvent) {
    if (!aEvent.isTrusted)
      return;

    // Find the relevant tab, and close it.
    let browsers = Browser.browsers;
    for (let i = 0; i < browsers.length; i++) {
      if (browsers[i].contentWindow == aEvent.target) {
        Browser.closeTab(Browser.getTabAtIndex(i));
        aEvent.preventDefault();
        break;
      }
    }
  },

  _linkAdded : function(aEvent) {
    let link = aEvent.originalTarget;
    if (!link || !link.href)
      return;

    if (/\bicon\b/i(link.rel)) {
      this._faviconLink = link.href;

      // If the link changes after pageloading, update it right away.
      // otherwise we wait until the pageload finishes
      if (this._favicon.src != "")
        this._setIcon(this._faviconLink);
    }
    else if (/\bsearch\b/i(link.rel)) {
      var type = link.type && link.type.toLowerCase();
      type = type.replace(/^\s+|\s*(?:;.*)?$/g, "");
      if (type == "application/opensearchdescription+xml" && link.title && /^(?:https?|ftp):/i.test(link.href)) {
        var engine = { title: link.title, href: link.href };

        BrowserSearch.addPageSearchEngine(engine, link.ownerDocument);
      }
    }
  },

  _updateButtons : function(aBrowser) {
    var back = document.getElementById("cmd_back");
    var forward = document.getElementById("cmd_forward");

    back.setAttribute("disabled", !aBrowser.canGoBack);
    forward.setAttribute("disabled", !aBrowser.canGoForward);
  },

  _tabSelect : function(aEvent) {
    var browser = Browser.selectedBrowser;
    this._titleChanged(browser.contentDocument);
    this._updateButtons(browser);
    this.updateStar();
    this._favicon.src = browser.mIconURL || kDefaultFavIconURL;

    // for new tabs, _tabSelect & update(TOOLBARSTATE_LOADED) are called when
    // about:blank is loaded. set _faviconLink here so it is not overriden in update
    this._faviconLink = this._favicon.src;
    this.updateIcon();
  },

  _setIcon : function(aURI) {
    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    try {
      var faviconURI = ios.newURI(aURI, null, null);
    }
    catch (e) {
      faviconURI = null;
    }

    var fis = Cc["@mozilla.org/browser/favicon-service;1"].getService(Ci.nsIFaviconService);
    if (!faviconURI || faviconURI.schemeIs("javascript") || fis.isFailedFavicon(faviconURI))
      faviconURI = ios.newURI(kDefaultFavIconURL, null, null);

    var browser = getBrowser();
    browser.mIconURL = faviconURI.spec;

    fis.setAndLoadFaviconForPage(browser.currentURI, faviconURI, true);
    this._favicon.src = faviconURI.spec;
  },

  showToolbar : function showToolbar(aEdit) {
    this.hidePanel();
    this._editToolbar(aEdit);
  },

  _toolbarLocked: 0,
  lockToolbar: function lockToolbar() {
    this._toolbarLocked++;
    document.getElementById("toolbar-moveable-container").top = "0";
  },
  
  unlockToolbar: function unlockToolbar() {
    if (!this._toolbarLocked)
      return;
    
    this._toolbarLocked--;
    if (!this._toolbarLocked)
      document.getElementById("toolbar-moveable-container").top = "";
  },

  _editToolbar : function _editToolbar(aEdit) {
    var icons = document.getElementById("urlbar-icons");
    if (aEdit && icons.getAttribute("mode") != "edit") {
      icons.setAttribute("mode", "edit");
      this._edit.defaultValue = this._edit.value;

      let urlString = this.getDisplayURI(Browser.selectedBrowser);
      if (urlString == "about:blank")
        urlString = "";
      this._edit.value = urlString;
      this._edit.inputField.focus();
    }
    else if (!aEdit && icons.getAttribute("mode") != "view") {
      icons.setAttribute("mode", "view");
      this._edit.inputField.blur();
      this._edit.reallyClosePopup();
    }
  },

  _closeOrQuit: function _closeOrQuit() {
    // Close active dialog, if we have one. If not then close the application.
    let dialog = this.activeDialog;
    if (dialog)
      dialog.close();
    else
      window.close();
  },

  get activeDialog() {
    // Return the topmost dialog
    if (this._dialogs.length)
      return this._dialogs[this._dialogs.length - 1];
    return null;
  },

  pushDialog : function pushDialog(aDialog) {
    // If we have a dialog push it on the stack and set the attr for CSS
    if (aDialog) {
      this.lockToolbar();
      this._dialogs.push(aDialog);
      document.getElementById("toolbar-main").setAttribute("dialog", "true");
    }
  },

  popDialog : function popDialog() {
    // Passing null means we pop the topmost dialog
    if (this._dialogs.length) {
      this._dialogs.pop();
      this.unlockToolbar();
    }

    // If no more dialogs are being displayed, remove the attr for CSS
    if (!this._dialogs.length)
      document.getElementById("toolbar-main").removeAttribute("dialog");
  },

  pushPopup: function pushPopup(aPanel, aElements) {
    this._popup =  { "panel": aPanel, 
                     "elements": (aElements instanceof Array) ? aElements : [aElements] };
  },

  popPopup: function popPopup() {
    this._popup = null;
  },

  _updatePopup: function _updateContextualPanel(aEvent) {
    if (!this._popup)
      return;

    let element = this._popup.elements;
    let targetNode = aEvent.target;
    while (targetNode && element.indexOf(targetNode) == -1)
      targetNode = targetNode.parentNode;

    if (targetNode == null) {
      let panel = this._popup.panel;
      if (panel.hide)
        panel.hide();
      this.popPopup();
    }
  },

  switchPane : function(id) {
    document.getElementById("panel-items").selectedPanel = document.getElementById(id);
  },

  get toolbarH() {
    if (!this._toolbarH) {
      let toolbar = document.getElementById("toolbar-main");
      this._toolbarH = toolbar.boxObject.height;
    }
    return this._toolbarH;
  },
  
  get sidebarW() {
    if (!this._sidebarW) {
      let sidebar = document.getElementById("browser-controls");
      this._sidebarW = sidebar.boxObject.width;
    }
    return this._sidebarW;
  },
  
  get starButton() {
    delete this.starButton;
    return this.starButton = document.getElementById("tool-star");
  },

  sizeControls : function(windowW, windowH) {
    // tabs
    document.getElementById("tabs").resize();

    // awesomebar
    let popup = document.getElementById("popup_autocomplete");
    popup.top = this.toolbarH;
    popup.height = windowH - this.toolbarH;
    popup.width = windowW;

    // toolbar UI
    document.getElementById("toolbar-container").height = this.toolbarH;
    document.getElementById("toolbar-main").width = windowW;

    // notification box
    document.getElementById("notifications").width = windowW;

    // findbar
    document.getElementById("findbar-container").width = windowW;

    // identity
    document.getElementById("identity-container").width = windowW;

    // bookmark editor
    let bmkeditor = document.getElementById("bookmark-container");
    bmkeditor.width = windowW;

    // bookmark list
    let bookmarks = document.getElementById("bookmarklist-container");
    bookmarks.height = windowH;
    bookmarks.width = windowW;
    
    // bookmark popup
    let bookmarkPopup = document.getElementById("bookmark-popup");
    let bookmarkPopupW = windowW / 4;
    bookmarkPopup.height = windowH / 4;
    bookmarkPopup.width = bookmarkPopupW;
    let starRect = this.starButton.getBoundingClientRect();
    const popupMargin = 10;
    bookmarkPopup.top = Math.round(starRect.top) + popupMargin;
    bookmarkPopup.left = windowW - this.sidebarW - bookmarkPopupW - popupMargin;

    // select list UI
    let selectlist = document.getElementById("select-container");
    selectlist.height = windowH;
    selectlist.width = windowW;

    // tools panel
    let panel = document.getElementById("panel-container");
    panel.height = windowH;
    panel.width = windowW;
  },

  init : function() {
    this._edit = document.getElementById("urlbar-edit");
    this._throbber = document.getElementById("urlbar-throbber");
    this._favicon = document.getElementById("urlbar-favicon");
    this._favicon.addEventListener("error", this, false);

    let urlbarEditArea = document.getElementById("urlbar-editarea");
    urlbarEditArea.addEventListener("click", this, false);
    urlbarEditArea.addEventListener("mousedown", this, false);

    document.getElementById("toolbar-main").ignoreDrag = true;

    let tabs = document.getElementById("tabs");
    tabs.addEventListener("TabSelect", this, true);
    tabs.addEventListener("TabOpen", this, true);

    let browsers = document.getElementById("browsers");
    browsers.addEventListener("DOMWindowClose", this, true);
    browsers.addEventListener("UIShowSelect", this, false, true);

    // XXX these really want to listen to only the current browser
    browsers.addEventListener("DOMTitleChanged", this, true);
    browsers.addEventListener("DOMLinkAdded", this, true);
    
    // listening mousedown for automatically dismiss some popups (e.g. larry)
    window.addEventListener("mousedown", this, true);

    // listening escape to dismiss dialog on VK_ESCAPE
    window.addEventListener("keypress", this, true);

    ExtensionsView.init();
    DownloadsView.init();
  },

  uninit : function() {
    ExtensionsView.uninit();
  },

  update : function(aState) {
    let icons = document.getElementById("urlbar-icons");

    // Use documentURIObject in the favicon construction so that we
    // do the right thing with about:-style error pages.  Bug 515188
    let uri = Browser.selectedBrowser.contentDocument.documentURIObject;

    switch (aState) {
      case TOOLBARSTATE_LOADED:
        icons.setAttribute("mode", "view");
        
        // We handle showing the toolbar for new tabs in BrowserUI.newTab()
        if (uri.spec != "about:blank") {
          this.showToolbar(false);
        }

        if (!this._faviconLink)
          this._faviconLink = uri.prePath + "/favicon.ico";
        this._setIcon(this._faviconLink);
        this.updateIcon();
        this._faviconLink = null;
        break;

      case TOOLBARSTATE_LOADING:
        this.showToolbar();
        
        // Force the mode back to "loading" since showToolbar() changes it to "view"
        // and that messes up the CSS rules depending on the "mode" attribute
        icons.setAttribute("mode", "loading");

        this._favicon.src = "";
        this._faviconLink = null;
        this.updateIcon();
        break;
    }
  },

  updateIcon : function() {
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

  getDisplayURI : function(browser) {
    var uri = browser.currentURI;

    if (!this._URIFixup)
      this._URIFixup = Cc["@mozilla.org/docshell/urifixup;1"].getService(Ci.nsIURIFixup);

    try {
      uri = this._URIFixup.createExposableURI(uri);
    } catch (ex) {}

    return uri.spec;
  },

  /* Set the location to the current content */
  setURI : function() {
    var browser = Browser.selectedBrowser;

    // FIXME: deckbrowser should not fire TabSelect on the initial tab (bug 454028)
    if (!browser.currentURI)
      return;

    // Update the navigation buttons
    this._updateButtons(browser);

    // Check for a bookmarked page
    this.updateStar();

    var urlString = this.getDisplayURI(browser);
    if (urlString == "about:blank")
      urlString = "";

    this._edit.value = urlString;
  },

  goToURI : function(aURI) {
    this._edit.reallyClosePopup();

    if (!aURI)
      aURI = this._edit.value;

    var flags = Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    getBrowser().loadURIWithFlags(aURI, flags, null, null);

    gHistSvc.markPageAsTyped(gURIFixup.createFixupURI(aURI, 0));
  },

  search : function() {
    var queryURI = "http://www.google.com/search?q=" + this._edit.value + "&hl=en&lr=&btnG=Search";
    getBrowser().loadURI(queryURI, null, null, false);
  },

  showAutoComplete : function(showDefault) {
    BrowserSearch.updateSearchButtons();
    this._edit.showHistoryPopup();
  },

  doButtonSearch : function(button) {
    if (!("engine" in button) || !button.engine)
      return;

    var urlbar = this._edit;
    urlbar.open = false;
    var value = urlbar.value;

    var submission = button.engine.getSubmission(value, null);
    getBrowser().loadURI(submission.uri.spec, null, submission.postData, false);
  },

  updateStar : function() {
    if (PlacesUtils.getMostRecentBookmarkForURI(Browser.selectedBrowser.currentURI) != -1)
      this.starButton.setAttribute("starred", "true");
    else
      this.starButton.removeAttribute("starred");
  },

  goToBookmark : function goToBookmark(aEvent) {
    if (aEvent.originalTarget.localName == "button")
      return;

    var list = document.getElementById("urllist-items");
    BrowserUI.goToURI(list.selectedItem.value);
  },

  showHistory : function() {
    // XXX Fix me with a real UI
  },

  showBookmarks : function () {
    BookmarkList.show();
  },

  newTab : function newTab(aURI) {
    aURI = aURI || "about:blank";
    let tab = Browser.addTab(aURI, true);

    if (aURI == "about:blank")
      this.showAutoComplete();
    return tab;
  },

  closeTab : function closeTab(aTab) {
    // If no tab is passed in, assume the current tab
    Browser.closeTab(aTab || Browser.selectedTab);
  },

  selectTab : function selectTab(aTab) {
    Browser.selectedTab = aTab;
  },

  hideTabs: function hideTabs() {
/*
    if (ws.isWidgetVisible("tabs-container")) {
      let widthOfTabs = document.getElementById("tabs-container").boxObject.width;
      ws.panBy(widthOfTabs, 0, true);
    }
*/
  },

  hideControls: function hideControls() {
/*
    if (ws.isWidgetVisible("browser-controls")) {
      let widthOfControls = document.getElementById("browser-controls").boxObject.width;
      ws.panBy(-widthOfControls, 0, true);
    }
*/
  },

  isTabsVisible: function isTabsVisible() {
    // The _1, _2 and _3 are to make the js2 emacs mode happy
    let [leftvis,_1,_2,_3] = Browser.computeSidebarVisibility();
    return (leftvis > 0.002);
  },

  showPanel: function showPanel(aPage) {
    let panelUI = document.getElementById("panel-container");

    panelUI.hidden = false;
    panelUI.width = window.innerWidth;
    panelUI.height = window.innerHeight;

    if (aPage != undefined)
      this.switchPane(aPage);
  },

  hidePanel: function hidePanel() {
    let panelUI = document.getElementById("panel-container");
    panelUI.hidden = true;
  },

  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      // Browser events
      case "DOMTitleChanged":
        this._titleChanged(aEvent.target);
        break;
      case "DOMLinkAdded":
        this._linkAdded(aEvent);
        break;
      case "DOMWindowClose":
        this._domWindowClose(aEvent);
        break;
      case "UIShowSelect":
        SelectHelper.show(aEvent.target);
        break;
      case "TabSelect":
        this._tabSelect(aEvent);
        break;
      case "TabOpen":
        if (!this.isTabsVisible() && 
            Browser.selectedTab.chromeTab != aEvent.target)
          NewTabPopup.show(aEvent.target);
        break;
      // URL textbox events
      case "click":
        this.doCommand("cmd_openLocation");
        break;
      case "keypress":
        if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
          let dialog = BrowserUI.activeDialog;
          if (dialog)
            dialog.close();
        }
        break;
      case "mousedown":
        this._updatePopup(aEvent);

        if (aEvent.detail == 2 &&
            aEvent.button == 0 &&
            gPrefService.getBoolPref("browser.urlbar.doubleClickSelectsAll")) {
          this._edit.editor.selectAll();
          aEvent.preventDefault();
        }
        break;
      // Favicon events
      case "error":
        this._favicon.src = "chrome://browser/skin/images/default-favicon.png";
        break;
    }
  },

  supportsCommand : function(cmd) {
    var isSupported = false;
    switch (cmd) {
      case "cmd_back":
      case "cmd_forward":
      case "cmd_reload":
      case "cmd_stop":
      case "cmd_search":
      case "cmd_go":
      case "cmd_openLocation":
      case "cmd_star":
      case "cmd_bookmarks":
      case "cmd_quit":
      case "cmd_close":
      case "cmd_menu":
      case "cmd_newTab":
      case "cmd_closeTab":
      case "cmd_actions":
      case "cmd_panel":
      case "cmd_sanitize":
      case "cmd_zoomin":
      case "cmd_zoomout":
        isSupported = true;
        break;
      default:
        isSupported = false;
        break;
    }
    return isSupported;
  },

  isCommandEnabled : function(cmd) {
    return true;
  },

  doCommand : function(cmd) {
    var browser = getBrowser();
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
      case "cmd_stop":
        browser.stop();
        break;
      case "cmd_search":
        this.search();
        break;
      case "cmd_go":
        this.goToURI();
        break;
      case "cmd_openLocation":
        this.showToolbar(true);
        BrowserUI.showAutoComplete();
        break;
      case "cmd_star":
      {
        var bookmarkURI = browser.currentURI;
        var bookmarkTitle = browser.contentDocument.title || bookmarkURI.spec;

        let autoClose = false;

        if (PlacesUtils.getMostRecentBookmarkForURI(bookmarkURI) == -1) {
          var bookmarkId = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.unfiledBookmarksFolder, bookmarkURI, PlacesUtils.bookmarks.DEFAULT_INDEX, bookmarkTitle);
          this.updateStar();

          // autoclose the bookmark popup
          autoClose = true;
        }

        // Show/hide bookmark popup
        BookmarkPopup.toggle(autoClose);
        break;
      }
      case "cmd_bookmarks":
        this.showBookmarks();
        break;
      case "cmd_quit":
        goQuitApplication();
        break;
      case "cmd_close":
        this._closeOrQuit();
        break;
      case "cmd_menu":
        break;
      case "cmd_newTab":
        this.newTab();
        break;
      case "cmd_closeTab":
        this.closeTab();
        break;
      case "cmd_sanitize":
      {
        // disable the button temporarily to indicate something happened
        let button = document.getElementById("prefs-clear-data");
        button.disabled = true;
        setTimeout(function() { button.disabled = false; }, 5000);

        Sanitizer.sanitize();
        break;
      }
      case "cmd_panel":
      {
        let panelUI = document.getElementById("panel-container");
        if (panelUI.hidden)
          this.showPanel();
        else
          this.hidePanel();
        break;
      }
      case "cmd_zoomin":
        Browser._browserView.zoom(-1);
        break;
      case "cmd_zoomout":
        Browser._browserView.zoom(1);
        break;
    }
  }
};

var NewTabPopup = {
  _timeout: 0,
  _tabs: [],

  get box() {
    delete this.box;
    return this.box = document.getElementById("newtab-popup");
  },

  _updateLabel: function() {
    let newtabStrings = document.getElementById("bundle_browser").getString("newtabpopup.opened");
    let label = PluralForm.get(this._tabs.length, newtabStrings).replace("#1", this._tabs.length);

    this.box.firstChild.setAttribute("value", label);
  },

  hide: function() {
    if (this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = 0;
    }

    this._tabs = [];
    this.box.hidden = true;
  },
  
  show: function(aTab) {
    this._tabs.push(aTab);
    this._updateLabel();

    this.box.top = aTab.getBoundingClientRect().top + (aTab.getBoundingClientRect().height / 3);
    this.box.hidden = false;

    if (this._timeout)
      clearTimeout(this._timeout);

    this._timeout = setTimeout(function(self) {
      self.hide();
    }, 2000, this);

    BrowserUI.pushPopup(this, this.box);
  },

  selectTab: function() {
    BrowserUI.selectTab(this._tabs.pop());
    this.hide();
  }
}

var BookmarkPopup = {
  get box() {
    delete this.box;
    return this.box = document.getElementById("bookmark-popup");
  },

  _bookmarkPopupTimeout: -1,

  hide : function () {
    if (this._bookmarkPopupTimeout != -1) {
      clearTimeout(this._bookmarkPopupTimeout);
      this._bookmarkPopupTimeout = -1;
    }
    this.box.hidden = true;
  },
  
  show : function (aAutoClose) {
    this.box.hidden = false;

    if (aAutoClose) {
      this._bookmarkPopupTimeout = setTimeout(function (self) {
        self._bookmarkPopupTimeout = -1;
        self.hide();
      }, 2000, this);
    }

    // include starButton here, so that click-to-dismiss works as expected
    BrowserUI.pushPopup(this, [this.box, BrowserUI.starButton]);
  },
  
  toggle : function (aAutoClose) {
    if (this.box.hidden)
      this.show(aAutoClose);
    else
      this.hide();
  }
}

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

    this._editor = document.createElement("placeitem");
    this._editor.setAttribute("id", "bookmark-item");
    this._editor.setAttribute("flex", "1");
    this._editor.setAttribute("type", "bookmark");
    this._editor.setAttribute("ui", "manage");
    this._editor.setAttribute("title", title);
    this._editor.setAttribute("uri", aURI.spec);
    this._editor.setAttribute("tags", tags.join(", "));
    this._editor.setAttribute("onmove", "FolderPicker.show(this);");
    this._editor.setAttribute("onclose", "BookmarkHelper.close()");
    document.getElementById("bookmark-form").appendChild(this._editor);

    let toolbar = document.getElementById("toolbar-main");
    let top = toolbar.top + toolbar.boxObject.height;

    this._panel = document.getElementById("bookmark-container");
    this._panel.top = (top < 0 ? 0 : top);
    this._panel.hidden = false;
    BrowserUI.pushDialog(this);

    let self = this;
    setTimeout(function() {
      self._editor.init(itemId);
      self._editor.startEditing();
    }, 0);
  },

  close: function BH_close() {
    BrowserUI.updateStar();

    // Note: the _editor will have already saved the data, if needed, by the time
    // this method is called, since this method is called via the "close" event.
    this._editor.parentNode.removeChild(this._editor);
    this._editor = null;

    this._panel.hidden = true;
    BrowserUI.popDialog();
  },
};

var BookmarkList = {
  _panel: null,
  _bookmarks: null,

  show: function() {
    this._panel = document.getElementById("bookmarklist-container");
    this._panel.width = window.innerWidth;
    this._panel.height = window.innerHeight;
    this._panel.hidden = false;
    BrowserUI.pushDialog(this);

    this._bookmarks = document.getElementById("bookmark-items");
    this._bookmarks.manageUI = false;
    this._bookmarks.openFolder();
  },

  close: function() {
    BrowserUI.updateStar();

    if (this._bookmarks.isEditing)
      this._bookmarks.stopEditing();
    this._bookmarks.blur();

    this._panel.hidden = true;
    BrowserUI.popDialog();
  },

  toggleManage: function() {
    this._bookmarks.manageUI = !(this._bookmarks.manageUI);
  },

  openBookmark: function() {
    let item = this._bookmarks.activeItem;
    if (item.spec) {
      this.close();
      BrowserUI.goToURI(item.spec);
    }
  },
};

var FolderPicker = {
  _control: null,
  _panel: null,

  show: function(aControl) {
    this._panel = document.getElementById("folder-container");
    this._panel.width = window.innerWidth;
    this._panel.height = window.innerHeight;
    this._panel.hidden = false;
    BrowserUI.pushDialog(this);

    this._control = aControl;

    let folders = document.getElementById("folder-items");
    folders.openFolder();
  },

  close: function() {
    this._panel.hidden = true;
    BrowserUI.popDialog();
  },

  moveItem: function() {
    let folders = document.getElementById("folder-items");
    let itemId = (this._control.activeItem ? this._control.activeItem.itemId : this._control.itemId);
    let folderId = PlacesUtils.bookmarks.getFolderIdForItem(itemId);
    if (folders.selectedItem.itemId != folderId) {
      PlacesUtils.bookmarks.moveItem(itemId, folders.selectedItem.itemId, PlacesUtils.bookmarks.DEFAULT_INDEX);
      if (this._control.removeItem)
        this._control.removeItem(this._control.activeItem);
    }
    this.close();
  }
};

var SelectHelper = {
  _panel: null,
  _list: null,
  _control: null,
  _selectedIndexes: [],

  _getSelectedIndexes: function() {
    let indexes = [];
    let control = this._control;

    if (control.type == 'select-one') {
      indexes.push(control.selectedIndex);
    }
    else {
      for (let i = 0; i < control.options.length; i++) {
        if (control.options[i].selected)
          indexes.push(i);
      }
    }

    return indexes;
  },

  show: function(aControl) {
    if (!aControl)
      return;

    this._control = aControl;
    this._selectedIndexes = this._getSelectedIndexes();

    this._list = document.getElementById("select-list");
    this._list.setAttribute("multiple", this._control.multiple ? "true" : "false");

    let firstSelected = null;

    let optionIndex = 0;
    let children = this._control.children;
    for (let i=0; i<children.length; i++) {
      let child = children[i];
      if (child instanceof HTMLOptGroupElement) {
        let group = document.createElement("option");
        group.setAttribute("label", child.label);
        this._list.appendChild(group);
        group.className = "optgroup";

        let subchildren = child.children;
        for (let ii=0; ii<subchildren.length; ii++) {
          let subchild = subchildren[ii];
          let item = document.createElement("option");
          item.setAttribute("label", subchild.text);
          this._list.appendChild(item);
          item.className = "in-optgroup";
          item.optionIndex = optionIndex++;
          if (subchild.selected) {
            item.setAttribute("selected", "true");
            firstSelected = firstSelected ? firstSelected : item;
          }
        }
      } else if (child instanceof HTMLOptionElement) {
        let item = document.createElement("option");
        item.setAttribute("label", child.textContent);
        this._list.appendChild(item);
        item.optionIndex = optionIndex++;
        if (child.selected) {
          item.setAttribute("selected", "true");
          firstSelected = firstSelected ? firstSelected : item;
        }
      }
    }

    this._panel = document.getElementById("select-container");
    this._panel.hidden = false;

    this._scrollElementIntoView(firstSelected);

    this._list.focus();
    this._list.addEventListener("click", this, false);
  },

  _scrollElementIntoView: function(aElement) {
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

    let itemHeight = aElement.getBoundingClientRect().height;
    let visibleItemsCount = this._list.boxObject.height / itemHeight;
    if ((index + 1) > visibleItemsCount) {
      let delta = Math.ceil(visibleItemsCount / 2);
      let scrollBoxObject = this._list.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
      scrollBoxObject.scrollTo(0, ((index + 1) - delta) * itemHeight);
    }
  },

  _forEachOption: function(aCallback) {
      let children = this._list.children;
      for (let i = 0; i < children.length; i++) {
        let item = children[i];
        if (!item.hasOwnProperty("optionIndex"))
          continue;
        aCallback(item, i);
      }
  },

  _updateControl: function() {
    let currentSelectedIndexes = this._getSelectedIndexes();

    let isIdentical = currentSelectedIndexes.length == this._selectedIndexes.length;
    if (isIdentical) {
      for (let i = 0; i < currentSelectedIndexes.length; i++) {
        if (currentSelectedIndexes[i] != this._selectedIndexes[i]) {
          isIdentical = false;
          break;
        }
      }
    }

    if (!isIdentical) {
      let control = this._control.wrappedJSObject;
      if ("onchange" in control)
        control.onchange();
    }
  },

  close: function() {
    this._updateControl();

    this._list.removeEventListener("click", this, false);
    this._panel.hidden = true;

    // Clear out the list for the next show
    let empty = this._list.cloneNode(false);
    this._list.parentNode.replaceChild(empty, this._list);
    this._list = empty;

    this._control.focus();
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "click":
        let item = aEvent.target;
        let selectElement = this._control.wrappedJSObject.selectElement;
        if (item && item.hasOwnProperty("optionIndex")) {
          if (this._control.multiple) {
            // Toggle the item state
            item.selected = !item.selected;
            selectElement.setOptionsSelectedByIndex(item.optionIndex, item.optionIndex, item.selected, false, false, true);
          }
          else {
            // Unselect all options
            this._forEachOption(
              function(aItem, aIndex) {
                aItem.selected = false;
              }
            );

            // Select the new one and update the control
            item.selected = true;
            selectElement.setOptionsSelectedByIndex(item.optionIndex, item.optionIndex, true, true, false, true);
          }
        }
        break;
    }
  }
};

function removeBookmarksForURI(aURI) {
  //XXX blargle xpconnect! might not matter, but a method on
  // nsINavBookmarksService that takes an array of items to
  // delete would be faster. better yet, a method that takes a URI!
  let itemIds = PlacesUtils.getBookmarksForURI(aURI);
  itemIds.forEach(PlacesUtils.bookmarks.removeItem);

  BrowserUI.updateStar();
}
