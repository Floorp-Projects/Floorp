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

Components.utils.import("resource://gre/modules/utils.js");

const TOOLBARSTATE_LOADING        = 1;
const TOOLBARSTATE_LOADED         = 2;
const TOOLBARSTATE_INDETERMINATE  = 3;

const UIMODE_NONE              = 0;
const UIMODE_URLVIEW           = 1;
const UIMODE_URLEDIT           = 2;
const UIMODE_BOOKMARK          = 3;
const UIMODE_BOOKMARKLIST      = 4;
const UIMODE_TABS              = 5;
const UIMODE_CONTROLS          = 6;
const UIMODE_PANEL             = 7;

const kMaxEngines = 4;
const kDefaultFavIconURL = "chrome://browser/skin/images/default-favicon.png";

[
  ["gContentBox",            "contentBox"],
].forEach(function (elementGlobal) {
  var [name, id] = elementGlobal;
  window.__defineGetter__(name, function () {
    var element = document.getElementById(id);
    if (!element)
      return null;
    delete window[name];
    return window[name] = element;
  });
  window.__defineSetter__(name, function (val) {
    delete window[name];
    return window[name] = val;
  });
});


var BrowserUI = {
  _panel : null,
  _caption : null,
  _edit : null,
  _throbber : null,
  _autocompleteNavbuttons : null,
  _favicon : null,
  _faviconLink : null,

  _setContentPosition : function (aProp, aValue) {
    let value = Math.round(aValue);
    if (aProp == "left") {
      gContentBox.style.marginLeft = value + "px";
      gContentBox.style.marginRight = -value + "px";
    } else if (aProp == "top") {
      gContentBox.style.marginTop = value + "px";
      gContentBox.style.marginBottom = -value + "px";
    }
  },
  get _contentTop() {
    return parseInt(gContentBox.style.marginTop);
  },

  _titleChanged : function(aDocument) {
    var browser = Browser.currentBrowser;
    if (browser && aDocument != browser.contentDocument)
      return;

    var caption = aDocument.title;
    if (!caption) {
      caption = this.getDisplayURI(browser);
      if (caption == "about:blank")
        caption = "";
    }
    this._caption.value = caption;

    var docElem = document.documentElement;
    var title = "";
    if (aDocument.title)
      title = aDocument.title + docElem.getAttribute("titleseparator");
    document.title = title + docElem.getAttribute("titlemodifier");
  },

  _linkAdded : function(aEvent) {
    var link = aEvent.originalTarget;
    if (!link || !link.href)
      return;

    if (/\bicon\b/i(link.rel)) {
      this._faviconLink = link.href 
    }
  },

  _tabSelect : function(aEvent) {
    var browser = Browser.currentBrowser;
    this.setURI();
    this._titleChanged(browser.contentDocument);
    this._favicon.src = browser.mIconURL || kDefaultFavIconURL;
    this.updateIcon(browser);

    let toolbar = document.getElementById("toolbar-main");
    if (Browser.content.currentTab.chromeTop) {
      // content box was panned, so let's reset it
      this._setContentPosition("top", Browser.content.currentTab.chromeTop);
      this._setContentPosition("left", 0);
      toolbar.top = this._contentTop - toolbar.boxObject.height;
    }
    else {
      // Must be initial conditions
      toolbar.top = 0;
      this._setContentPosition("top", toolbar.boxObject.height);
      this._setContentPosition("left", 0);
    }

    this.show(UIMODE_NONE);
  },

  _setIcon : function(aURI) {
    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    var faviconURI = ios.newURI(aURI, null, null);

    var fis = Cc["@mozilla.org/browser/favicon-service;1"].getService(Ci.nsIFaviconService);
    if (faviconURI.schemeIs("javascript") ||
        fis.isFailedFavicon(faviconURI))
      faviconURI = ios.newURI(kDefaultFavIconURL, null, null);

    var browser = getBrowser();
    browser.mIconURL = faviconURI.spec;

    fis.setAndLoadFaviconForPage(browser.currentURI, faviconURI, true);
    this._favicon.src = faviconURI.spec;
  },

  _getBookmarks : function(aFolders) {
    var items = [];

    var hs = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
    var options = hs.getNewQueryOptions();
    //options.resultType = options.RESULTS_AS_URI;
    var query = hs.getNewQuery();
    query.setFolders(aFolders, 1);
    var result = hs.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    for (var i=0; i<cc; ++i) {
      var node = rootNode.getChild(i);
      items.push(node);
    }
    rootNode.containerOpen = false;

    return items;
  },

  _getHistory : function(aCount) {
    var items = [];

    var hs = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
    var options = hs.getNewQueryOptions();
    options.queryType = options.QUERY_TYPE_HISTORY;
    //options.sortingMode = options.SORT_BY_VISITCOUNT_DESCENDING;
    options.maxResults = aCount;
    //options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_URI;
    var query = hs.getNewQuery();
    var result = hs.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    for (var i=0; i<cc; ++i) {
      var node = rootNode.getChild(i);
      items.push(node);
    }
    rootNode.containerOpen = false;

    return items;
  },

  _dragData :  {
    dragging : false,
    startX : 0,
    startY : 0,
    dragX : 0,
    dragY : 0,
    lastX : 0,
    lastY : 0,
    sTop : 0,
    sLeft : 0,
    uiMode : UIMODE_NONE
  },

  _scrollToolbar : function bui_scrollToolbar(aEvent) {
    var [scrollWidth, ] = Browser.content._contentAreaDimensions;
    var [viewportW, ] = Browser.content._effectiveViewportDimensions;

    var pannedUI = false;

    if (this._dragData.dragging && Browser.content.scrollY == 0) {
      let toolbar = document.getElementById("toolbar-main");
      let dy = this._dragData.lastY - aEvent.screenY;
      this._dragData.dragY += dy;

      // NOTE: We should only be scrolling the toolbar if the sidebars are not
      // visible (gContentBox.style.marginLeft == "0px")
      let sidebarVisible = gContentBox.style.marginLeft != "0px";
      let newTop = null;
      if (dy > 0 && (toolbar.top > -toolbar.boxObject.height && !sidebarVisible)) {
        // Scroll the toolbar up unless it is already scrolled up
        newTop = this._dragData.sTop - dy;

        // Clip the adjustment to just enough to hide the toolbar
        if (newTop < -toolbar.boxObject.height)
          newTop = -toolbar.boxObject.height;

        // Reset the browser start point
        Browser.content.dragData.sX = aEvent.screenX;
        Browser.content.dragData.sY = aEvent.screenY;
      }
      else if (dy < 0 && (toolbar.top < 0 && !sidebarVisible)) {
        // Scroll the toolbar down unless it is already down
        newTop = this._dragData.sTop - dy;

        // Clip the adjustment to just enough to fully show the toolbar
        if (newTop > 0)
          newTop = 0;
      }

      // Update the toolbar and browser tops. Stop the mousemove from
      // getting to the deckbrowser.
      if (newTop != null) {
        toolbar.top = newTop;
        this._setContentPosition("top", newTop + toolbar.boxObject.height);

        // Cache the current top so we can use it when switching tabs
        Browser.content.currentTab.chromeTop = this._contentTop;

        pannedUI = true;
      }
    }

    if (this._dragData.dragging && (Browser.content.scrollX == 0 || (Browser.content.scrollX + viewportW) == scrollWidth)) {
      let tabbar = document.getElementById("tab-list-container");
      let sidebar = document.getElementById("browser-controls");
      let panelUI = document.getElementById("panel-container");
      let toolbar = document.getElementById("toolbar-main");
      let dx = this._dragData.lastX - aEvent.screenX;
      this._dragData.dragX += dx;

      if (Math.abs(this._dragData.screenX - aEvent.screenX) > 30) {
        let newLeft = this._dragData.sLeft - dx;
        let oldLeft = tabbar.left;

        let tabbarW = tabbar.boxObject.width;
        let sidebarW = sidebar.boxObject.width;
        let contentW = gContentBox.boxObject.width;

        // Limit the panning
        if (newLeft > 0)
          newLeft = 0;
        else if (newLeft < -(tabbarW + sidebarW))
          newLeft = -(tabbarW + sidebarW);

        // Set the UI mode based on where we ended up
        var noneMode = (gContentBox.style.marginTop == "0px" ? UIMODE_NONE : UIMODE_URLVIEW);
        if (newLeft > -(tabbarW - tabbarW / 3) && newLeft <= 0) {
          if (this._dragData.uiMode == UIMODE_CONTROLS) {
            this.mode = noneMode;
            return;
          }
          this.mode = UIMODE_TABS;
        }
        else if (newLeft >= -(tabbarW + sidebarW) && newLeft < -(tabbarW + sidebarW / 3)) {
          if (this._dragData.uiMode == UIMODE_TABS) {
            this.mode = noneMode;
            return;
          }
          this.mode = UIMODE_CONTROLS;
        }
        else
          this.mode = noneMode;

        tabbar.left = newLeft;

        // Never let the toolbar pan off the screen
        let newToolbarLeft = newLeft;
        if (newToolbarLeft < 0)
          newToolbarLeft = 0;
        toolbar.left = newToolbarLeft;

        // Make the toolbar appear/disappear depending on the state of the sidebars
        if (newLeft + tabbarW != 0)
          toolbar.top = 0;
        else
          toolbar.top = this._contentTop - toolbar.boxObject.height;

        this._setContentPosition("left", newLeft + tabbarW);
        sidebar.left = newLeft + tabbarW + contentW;
        panelUI.left = newLeft + tabbarW + contentW + sidebarW;

        pannedUI = true;
      }
    }

    if (pannedUI) {
      aEvent.stopPropagation();

      // Force a sync redraw
      window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
            .getInterface(Components.interfaces.nsIDOMWindowUtils)
            .processUpdates();
    }
    else {
      // Reset our start point while the browser is doing its panning
      this._dragData.lastX = aEvent.screenX;
      this._dragData.lastY = aEvent.screenY;
    }
  },

  _showToolbar : function(aShow) {
    var toolbar = document.getElementById("toolbar-main");

    if (aShow) {
      // Always show the toolbar, either by floating or panning
      if (toolbar.top == -toolbar.boxObject.height) {
        // Float the toolbar over content
        toolbar.top = 0;
      }
      else if (toolbar.top < 0) {
        // Partially showing, so show it completely
        toolbar.top = 0;
        this._setContentPosition("top", toolbar.boxObject.height);
      }
    }
    else {
      // If we are floating the toolbar, then hide it again
      if (gContentBox.style.marginTop == "0px") {
        toolbar.top = -toolbar.boxObject.height;
      }
    }
  },

  _editToolbar : function(aEdit) {
    var toolbar = document.getElementById("toolbar-main");
    if (aEdit) {
      toolbar.setAttribute("mode", "edit");
      this._caption.hidden = true;
      this._edit.hidden = false;
      this._edit.inputField.focus();
    }
    else {
      toolbar.setAttribute("mode", "view");
      this._edit.hidden = true;
      this._edit.reallyClosePopup();
      this._caption.hidden = false;
    }
  },

  _showPanel : function(aMode) {
      let tabbar = document.getElementById("tab-list-container");
      let sidebar = document.getElementById("browser-controls");
      let panelUI = document.getElementById("panel-container");
      let toolbar = document.getElementById("toolbar-main");

      let tabbarW = tabbar.boxObject.width;
      let sidebarW = sidebar.boxObject.width;
      let contentW = gContentBox.boxObject.width;

      let newLeft = -tabbarW;
      switch (aMode) {
        case UIMODE_NONE:
          Shortcuts.deinit();
          break;
        case UIMODE_PANEL:
          newLeft = -contentW;
          this._initPanel();
          break;
        case UIMODE_CONTROLS:
          newLeft = -(tabbarW + sidebarW);
          break;
        case UIMODE_TABS:
          newLeft = 0;
          break;
      }
      tabbar.left = newLeft;

      let newToolbarLeft = newLeft;
      if (newToolbarLeft < 0 && aMode != UIMODE_PANEL)
        newToolbarLeft = 0;
      else if (newToolbarLeft < 0 && aMode == UIMODE_PANEL)
        newToolbarLeft += tabbarW + sidebarW;
      toolbar.left = newToolbarLeft;

      this._setContentPosition("left", newLeft + tabbarW);
      sidebar.left = newLeft + tabbarW + contentW;
      panelUI.left = newLeft + tabbarW + contentW + sidebarW;
      panelUI.width = contentW;
  },

  _initPanel : function() {
    let addons = document.getElementById("addons-container");
    if (!addons.hasAttribute("src"))
      addons.setAttribute("src", "chrome://mozapps/content/extensions/extensions.xul");
    let dloads = document.getElementById("downloads-container");
    if (!dloads.hasAttribute("src"))
      dloads.setAttribute("src", "chrome://mozapps/content/downloads/downloads.xul");
    Shortcuts.init();
  },

  switchPane : function(id) {
    document.getElementById("panel-items").selectedPanel = document.getElementById(id);
  },

  _sizeControls : function(aEvent) {
    if (window != aEvent.target) {
      return
    }
    var rect = document.getElementById("browser-container").getBoundingClientRect();
    var containerW = rect.right - rect.left;
    var containerH = rect.bottom - rect.top;
    var toolbar = document.getElementById("toolbar-main");
    var toolbarH = toolbar.boxObject.height;

    var sidebar = document.getElementById("browser-controls");
    var panelUI = document.getElementById("panel-container");
    var tabbar = document.getElementById("tab-list-container");
    tabbar.left = -tabbar.boxObject.width;
    panelUI.left = containerW + sidebar.boxObject.width;
    sidebar.left = containerW;
    sidebar.height = tabbar.height = (panelUI.height = containerH) - toolbarH;
    panelUI.width = containerW - sidebar.boxObject.width - tabbar.boxObject.width;

    var popup = document.getElementById("popup_autocomplete");
    toolbar.width = containerW;
    popup.height = containerH - toolbarH;
  },

  init : function() {
    this._caption = document.getElementById("urlbar-caption");
    this._caption.addEventListener("click", this, false);
    this._edit = document.getElementById("urlbar-edit");
    this._edit.addEventListener("blur", this, false);
    this._edit.addEventListener("keypress", this, true);
    this._edit.addEventListener("input", this, false);
    this._throbber = document.getElementById("urlbar-throbber");
    this._favicon = document.getElementById("urlbar-favicon");
    this._favicon.addEventListener("error", this, false);
    this._autocompleteNavbuttons = document.getElementById("autocomplete_navbuttons");

    Browser.content.addEventListener("DOMTitleChanged", this, true);
    Browser.content.addEventListener("DOMLinkAdded", this, true);

    document.getElementById("tab-list").addEventListener("TabSelect", this, true);

    Browser.content.addEventListener("mousedown", this, true);
    Browser.content.addEventListener("mouseup", this, true);
    Browser.content.addEventListener("mousemove", this, true);

    window.addEventListener("resize", this, false);
    Shortcuts.restore();
  },

  update : function(aState, aBrowser) {
    if (aState == TOOLBARSTATE_INDETERMINATE) {
      this._faviconLink = null;
      aState = TOOLBARSTATE_LOADED;
      this.setURI();
    }

    var toolbar = document.getElementById("toolbar-main");
    if (aState == TOOLBARSTATE_LOADING) {
      this.show(UIMODE_URLVIEW);
      Browser.content.setLoading(aBrowser, true);

      toolbar.top = 0;
      toolbar.setAttribute("mode", "loading");
      this._favicon.src = "";
      this._faviconLink = null;
      this.updateIcon(aBrowser);
    }
    else if (aState == TOOLBARSTATE_LOADED) {
      this._setContentPosition("top", toolbar.boxObject.height);
      Browser.content.setLoading(aBrowser, false);

      toolbar.setAttribute("mode", "view");

      if (!this._faviconLink) {
        this._faviconLink = aBrowser.currentURI.prePath + "/favicon.ico";
      }
      this._setIcon(this._faviconLink);
      this.updateIcon(aBrowser);
    }
  },

  updateIcon : function(browser) {
    if (Browser.content.isLoading(browser)) {
      document.getElementById("urlbar-image-deck").selectedIndex = 0;
      this._throbber.setAttribute("loading", "true");
    }
    else {
      document.getElementById("urlbar-image-deck").selectedIndex = 1;
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
    var browser = Browser.currentBrowser;

    // FIXME: deckbrowser should not fire TebSelect on the initial tab (bug 454028)
    if (!browser.currentURI)
      return;

    var back = document.getElementById("cmd_back");
    var forward = document.getElementById("cmd_forward");

    back.setAttribute("disabled", !browser.canGoBack);
    forward.setAttribute("disabled", !browser.canGoForward);

    // Check for a bookmarked page
    var star = document.getElementById("tool-star");
    if (PlacesUtils.getMostRecentBookmarkForURI(browser.currentURI) != -1) {
      star.setAttribute("starred", "true");
    }
    else {
      star.removeAttribute("starred");
    }

    var urlString = this.getDisplayURI(browser);
    if (urlString == "about:blank") {
      urlString = "";
      this.show(UIMODE_URLEDIT);
    }

    this._caption.value = urlString;
    this._edit.value = urlString;
  },

  goToURI : function(aURI) {
    this._edit.reallyClosePopup();

    if (!aURI)
      aURI = this._edit.value;

    var flags = Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    getBrowser().loadURIWithFlags(aURI, flags, null, null);
    this.show(UIMODE_URLVIEW);
  },

  search : function() {
    var queryURI = "http://www.google.com/search?q=" + this._edit.value + "&hl=en&lr=&btnG=Search";
    getBrowser().loadURI(queryURI, null, null, false);

    this.show(UIMODE_URLVIEW);
  },

  updateAutoComplete : function(showDefault)
  {
    this.updateSearchEngines();
    if (showDefault || this._edit.getAttribute("nomatch"))
      this._edit.showHistoryPopup();
  },

  doButtonSearch : function(button)
  {
    if (!("engine" in button) || !button.engine)
      return;

    var urlbar = this._edit;
    urlbar.open = false;
    var value = urlbar.value;

    var submission = button.engine.getSubmission(value, null);
    getBrowser().loadURI(submission.uri.spec, null, submission.postData, false);
  },

  engines : null,
  updateSearchEngines : function () {
    if (this.engines)
      return;

    // XXXndeakin remove the try-catch once the search service is properly built
    try {
      var searchService = Cc["@mozilla.org/browser/search-service;1"].
                          getService(Ci.nsIBrowserSearchService);
    } catch (ex) {
      this.engines = [ ];
      return;
    }

    var engines = searchService.getVisibleEngines({ });
    this.engines = engines;
    const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    var container = this._autocompleteNavbuttons;
    for (var e = 0; e < kMaxEngines && e < engines.length; e++) {
      var button = document.createElementNS(kXULNS, "toolbarbutton");
      var engine = engines[e];
      button.id = engine.name;
      button.setAttribute("label", engine.name);
      button.className = "searchengine";
      if (engine.iconURI)
        button.setAttribute("image", engine.iconURI.spec);
      container.appendChild(button);
      button.engine = engine;
    }
  },

  mode : UIMODE_NONE,
  show : function(aMode) {
    if (this.mode == aMode)
      return;

    if (this.mode == UIMODE_BOOKMARKLIST && aMode != UIMODE_BOOKMARKLIST)
      window.removeEventListener("keypress", BrowserUI.closePopup, false);

    this.mode = aMode;

    var toolbar = document.getElementById("toolbar-main");
    var bookmark = document.getElementById("bookmark-container");
    var urllist = document.getElementById("urllist-container");
    var container = document.getElementById("browser-container");

    if (aMode == UIMODE_URLVIEW)
    {
      this._showToolbar(true);
      this._editToolbar(false);

      bookmark.hidden = true;
      urllist.hidden = true;

      this._showPanel(UIMODE_NONE);
    }
    else if (aMode == UIMODE_URLEDIT) {
      this._showToolbar(true);
      this._editToolbar(true);

      bookmark.hidden = true;
      urllist.hidden = true;

      this._showPanel(UIMODE_NONE);
    }
    else if (aMode == UIMODE_BOOKMARK) {
      this._showToolbar(true);
      this._editToolbar(false);

      urllist.hidden = true;
      bookmark.hidden = false;
      bookmark.width = container.boxObject.width;

      this._showPanel(UIMODE_NONE);
    }
    else if (aMode == UIMODE_BOOKMARKLIST) {
      this._showToolbar(false);
      this._editToolbar(false);

      window.addEventListener("keypress", this.closePopup, false);

      bookmark.hidden = true;
      urllist.hidden = false;
      urllist.width = container.boxObject.width;
      urllist.height = container.boxObject.height;

      this._showPanel(UIMODE_NONE);
    }
    else if (aMode == UIMODE_PANEL) {
      this._showToolbar(true);
      this._editToolbar(false);

      bookmark.hidden = true;

      this._showPanel(aMode);
    }
    else if (aMode == UIMODE_TABS || aMode == UIMODE_CONTROLS) {
      this._showPanel(aMode);
    }
    else if (aMode == UIMODE_NONE) {
      this._showToolbar(false);
      this._edit.reallyClosePopup();
      urllist.hidden = true;
      bookmark.hidden = true;
      this._showPanel(aMode);
    }
  },

  _showPlaces : function(aItems) {
    var list = document.getElementById("urllist-items");
    while (list.firstChild) {
      list.removeChild(list.firstChild);
    }

    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    var fis = Cc["@mozilla.org/browser/favicon-service;1"].getService(Ci.nsIFaviconService);

    for (var i=0; i<aItems.length; i++) {
      let node = aItems[i];
      let listItem = document.createElement("richlistitem");
      listItem.setAttribute("class", "urllist-item");
      listItem.setAttribute("value", node.uri);

      let box = document.createElement("vbox");
      box.setAttribute("pack", "center");
      let image = document.createElement("image");
      image.setAttribute("class", "urllist-image");
      let icon = node.icon ? node.icon.spec : fis.getFaviconImageForPage(ios.newURI(node.uri, null, null)).spec
      image.setAttribute("src", icon);
      box.appendChild(image);
      listItem.appendChild(box);

      let label = document.createElement("label");
      label.setAttribute("class", "urllist-text");
      label.setAttribute("value", node.title);
      label.setAttribute("flex", "1");
      label.setAttribute("crop", "end");
      listItem.appendChild(label);
      list.appendChild(listItem);
      listItem.addEventListener("click", function() { BrowserUI.goToURI(node.uri); }, true);
    }

    list.focus();
  },

  closePopup : function(aEvent)
  {
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE)
      BrowserUI.show(UIMODE_NONE);
  },

  showHistory : function() {
    this._showPlaces(this._getHistory(6));
  },

  showBookmarks : function () {
    this.show(UIMODE_BOOKMARKLIST);

    var bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
    this._showPlaces(this._getBookmarks([bms.bookmarksMenuFolder]));
  },

  newTab : function() {
    Browser.content.newTab(true);
    this.show(UIMODE_URLEDIT);
  },

  closeTab : function(aTab) {
    Browser.content.removeTab(aTab);
    this.show(UIMODE_NONE);
  },

  selectTab : function(aTab) {
    Browser.content.selectTab(aTab);
    this.show(UIMODE_NONE);
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
      case "TabSelect":
        this._tabSelect(aEvent);
        break;
      // URL textbox events
      case "click":
        this.show(UIMODE_URLEDIT);
        setTimeout(function () {BrowserUI.updateAutoComplete(true)}, 0);
        break;
      case "input":
        this.updateAutoComplete(false);
        break;
      case "keypress":
        if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
          this._edit.reallyClosePopup();
          this.show(UIMODE_URLVIEW);
        }
        break;
      // Favicon events
      case "error":
        this._favicon.src = "chrome://browser/skin/images/default-favicon.png";
        break;
      // UI panning events
      case "mousedown":
        this._dragData.dragging = true;
        this._dragData.dragX = 0;
        this._dragData.dragY = 0;
        this._dragData.screenX = this._dragData.lastX = aEvent.screenX;
        this._dragData.screenY = this._dragData.lastY = aEvent.screenY;
        this._dragData.sTop = document.getElementById("toolbar-main").top;
        this._dragData.sLeft = document.getElementById("tab-list-container").left;
        this._dragData.uiMode = this.mode;
        break;
      case "mouseup":
        this._dragData.dragging = false;
        this._dragData.uiMode = UIMODE_NONE;
        // Cause the UI to snap, if needed
        this._showPanel(this.mode);
        break;
      case "mousemove":
        this._scrollToolbar(aEvent);
        break;
      // Window size events
      case "resize":
        this._sizeControls(aEvent);
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
      case "cmd_star":
      case "cmd_bookmarks":
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
      case "cmd_star":
      {
        var bookmarkURI = browser.currentURI;
        var bookmarkTitle = browser.contentDocument.title;

        if (PlacesUtils.getMostRecentBookmarkForURI(bookmarkURI) == -1) {
          var bookmarkId = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.bookmarksMenuFolder, bookmarkURI, PlacesUtils.bookmarks.DEFAULT_INDEX, bookmarkTitle);
          document.getElementById("tool-star").setAttribute("starred", "true");

          var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
          var favicon = document.getElementById("urlbar-favicon");
          var faviconURI = ios.newURI(favicon.src, null, null);

          PlacesUtils.favicons.setAndLoadFaviconForPage(bookmarkURI, faviconURI, true);

          this.show(UIMODE_NONE);
        }
        else {
          this.show(UIMODE_BOOKMARK);
          BookmarkHelper.edit(bookmarkURI);
        }
        break;
      }
      case "cmd_bookmarks":
        this.showBookmarks();
        break;
      case "cmd_menu":
        break;
      case "cmd_newTab":
        this.newTab();
        break;
      case "cmd_closeTab":
        Browser.content.removeTab(Browser.content.browser);
        break;
      case "cmd_sanitize":
        Sanitizer.sanitize();
        break;
      case "cmd_panel":
      {
        var mode = (this.mode != UIMODE_PANEL ? UIMODE_PANEL : UIMODE_CONTROLS);
        this.show(mode);
        break;
      }
      case "cmd_zoomin":
        Browser.content.zoom(-1);
        break;
      case "cmd_zoomout":
        Browser.content.zoom(1);
        break;
    }
  }
};

var BookmarkHelper = {
  _item : null,
  _uri : null,

  _getTagsArrayFromTagField : function() {
    // we don't require the leading space (after each comma)
    var tags = document.getElementById("bookmark-tags").value.split(",");
    for (var i=0; i<tags.length; i++) {
      // remove trailing and leading spaces
      tags[i] = tags[i].replace(/^\s+/, "").replace(/\s+$/, "");

      // remove empty entries from the array.
      if (tags[i] == "") {
        tags.splice(i, 1);
        i--;
      }
    }
    return tags;
  },

  edit : function(aURI) {
    this._uri = aURI;
    this._item = PlacesUtils.getMostRecentBookmarkForURI(this._uri);

    if (this._item != -1) {
      document.getElementById("bookmark-name").value = PlacesUtils.bookmarks.getItemTitle(this._item);
      var currentTags = PlacesUtils.tagging.getTagsForURI(this._uri, {});
      document.getElementById("bookmark-tags").value = currentTags.join(", ");
      document.getElementById("bookmark-folder").value = ""; // XXX either use this or remove it
    }
    document.getElementById("bookmark-name").focus();

    window.addEventListener("keypress", this, true);
    document.getElementById("bookmark-form").addEventListener("change", this, true);
  },

  remove : function() {
    if (this._item) {
      // Remove bookmark itself
      PlacesUtils.bookmarks.removeItem(this._item);

      // If this was the last bookmark (excluding tag-items and livemark
      // children, see getMostRecentBookmarkForURI) for the bookmark's url,
      // remove the url from tag containers as well.
      if (PlacesUtils.getMostRecentBookmarkForURI(this._uri) == -1) {
        var tags = PlacesUtils.tagging.getTagsForURI(this._uri, {});
        PlacesUtils.tagging.untagURI(this._uri, tags);
      }

      document.getElementById("tool-star").removeAttribute("starred");
    }
    this.close();
  },

  save : function() {
    if (this._item) {
      // Update the name
      PlacesUtils.bookmarks.setItemTitle(this._item, document.getElementById("bookmark-name").value);

      // Update the tags
      var tags = this._getTagsArrayFromTagField();
      var currentTags = PlacesUtils.tagging.getTagsForURI(this._uri, {});
      if (tags.length > 0 || currentTags.length > 0) {
        var tagsToRemove = [];
        var tagsToAdd = [];
        var i;
        for (i=0; i<currentTags.length; i++) {
          if (tags.indexOf(currentTags[i]) == -1)
            tagsToRemove.push(currentTags[i]);
        }
        for (i=0; i<tags.length; i++) {
          if (currentTags.indexOf(tags[i]) == -1)
            tagsToAdd.push(tags[i]);
        }

        if (tagsToAdd.length > 0)
          PlacesUtils.tagging.tagURI(this._uri, tagsToAdd);
        if (tagsToRemove.length > 0)
          PlacesUtils.tagging.untagURI(this._uri, tagsToRemove);
      }

    }
  },

  close : function() {
    window.removeEventListener("keypress", this, true);
    document.getElementById("bookmark-form").removeEventListener("change", this, true);

    this._item = null;
    BrowserUI.show(UIMODE_NONE);
  },

  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      case "keypress":
        if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
          document.getElementById("bookmark-close").focus();
          this.close();
        }
        break;
      case "change":
        this.save();
        break;
    }
  }
};
