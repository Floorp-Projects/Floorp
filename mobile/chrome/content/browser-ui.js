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
  ["gContentBox",            "content"],
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
      this._faviconLink = link.href;
    }
  },

  _tabSelect : function(aEvent) {
    var browser = Browser.currentBrowser;
    this._titleChanged(browser.contentDocument);
    this._favicon.src = browser.mIconURL || kDefaultFavIconURL;

    // for new tabs, _tabSelect & update(TOOLBARSTATE_LOADED) are called when
    // about:blank is loaded. set _faviconLink here so it is not overriden in update
    this._faviconLink = this._favicon.src;
    this.updateIcon();
  },

  _setIcon : function(aURI) {
    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    var faviconURI = ios.newURI(aURI, null, null);

    var fis = Cc["@mozilla.org/browser/favicon-service;1"].getService(Ci.nsIFaviconService);
    if (faviconURI.schemeIs("javascript") || fis.isFailedFavicon(faviconURI))
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

  _showToolbar : function(aShow) {
    if (aShow) {
      ws.freeze("toolbar-main");
      ws.moveFrozenTo("toolbar-main", 0, 0);
    }
    else {
      ws.unfreeze("toolbar-main");
    }
  },

  _editToolbar : function(aEdit) {
    var icons = document.getElementById("urlbar-icons");
    if (aEdit) {
      icons.setAttribute("mode", "edit");
      this._caption.hidden = true;
      this._edit.hidden = false;
      this._edit.inputField.focus();
      this._edit.editor.selectAll();
    }
    else {
      icons.setAttribute("mode", "view");
      this._edit.hidden = true;
      this._edit.reallyClosePopup();
      this._caption.hidden = false;
    }
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

    var toolbarH = document.getElementById("toolbar-main").boxObject.height;
    var popup = document.getElementById("popup_autocomplete");
    popup.height = window.innerHeight - toolbarH;
    popup.width = window.innerWidth;

    // XXX need to handle make some of these work again
/*
    var sidebar = document.getElementById("browser-controls");
    var panelUI = document.getElementById("panel-container");
    var tabbar = document.getElementById("tabs-container");
    tabbar.left = -tabbar.boxObject.width;
    panelUI.left = containerW + sidebar.boxObject.width;
    sidebar.left = containerW;
    sidebar.height = tabbar.height = (panelUI.height = containerH) - toolbarH;
    panelUI.width = containerW - sidebar.boxObject.width - tabbar.boxObject.width;
    toolbar.width = containerW;
*/
  },

  init : function() {
    this._caption = document.getElementById("urlbar-caption");
    this._caption.addEventListener("click", this, false);
    this._edit = document.getElementById("urlbar-edit");
    this._edit.addEventListener("blur", this, false);
    this._edit.addEventListener("keypress", this, true);
    this._throbber = document.getElementById("urlbar-throbber");
    this._favicon = document.getElementById("urlbar-favicon");
    this._favicon.addEventListener("error", this, false);
    this._autocompleteNavbuttons = document.getElementById("autocomplete_navbuttons");

    // XXX these really want to listen whatever is the current browser, not any browser
    let browsers = document.getElementById("browsers");
    browsers.addEventListener("DOMTitleChanged", this, true);
    browsers.addEventListener("DOMLinkAdded", this, true);

    document.getElementById("tabs").addEventListener("TabSelect", this, true);

    window.addEventListener("resize", this, false);
    Shortcuts.restore();
  },

  update : function(aState) {
    var icons = document.getElementById("urlbar-icons");

    switch (aState) {
      case TOOLBARSTATE_LOADED:
        icons.setAttribute("mode", "view");

        if (!this._faviconLink) {
          this._faviconLink = Browser.currentBrowser.currentURI.prePath + "/favicon.ico";
        }
        this._setIcon(this._faviconLink);
        this.updateIcon();
        this._faviconLink = null;
        break;

      case TOOLBARSTATE_LOADING:
        this.show(UIMODE_URLVIEW);
        icons.setAttribute("mode", "loading");

        ws.panTo(0,0, true);

        this._favicon.src = "";
        this._faviconLink = null;
        this.updateIcon();
        break;
    }
  },

  updateIcon : function() {
    if (Browser.currentTab.isLoading()) {
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
    var browser = Browser.currentBrowser;

    // FIXME: deckbrowser should not fire TabSelect on the initial tab (bug 454028)
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

  showAutoComplete : function(showDefault) {
    this.updateSearchEngines();
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

  engines : null,
  updateSearchEngines : function () {
    if (this.engines)
      return;

    var searchService = Cc["@mozilla.org/browser/search-service;1"].getService(Ci.nsIBrowserSearchService);
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

    let toolbar = document.getElementById("toolbar-main");
    let bookmark = document.getElementById("bookmark-container");
    let urllist = document.getElementById("urllist-container");
    let container = document.getElementById("browser-container");
    let panelUI = document.getElementById("panel-container");

    if (aMode == UIMODE_URLVIEW) {
      this._showToolbar(true);
      this._editToolbar(false);

      bookmark.hidden = true;
      urllist.hidden = true;
      panelUI.hidden = true;
    }
    else if (aMode == UIMODE_URLEDIT) {
      this._showToolbar(true);
      this._editToolbar(true);

      bookmark.hidden = true;
      urllist.hidden = true;
      panelUI.hidden = true;
    }
    else if (aMode == UIMODE_BOOKMARK) {
      this._showToolbar(true);
      this._editToolbar(false);

      urllist.hidden = true;
      panelUI.hidden = true;
      bookmark.hidden = false;
      bookmark.width = container.boxObject.width;
    }
    else if (aMode == UIMODE_BOOKMARKLIST) {
      this._showToolbar(false);
      this._editToolbar(false);

      window.addEventListener("keypress", this.closePopup, false);

      bookmark.hidden = true;
      panelUI.hidden = true;
      urllist.hidden = false;
      urllist.width = container.boxObject.width;
      urllist.height = container.boxObject.height;
    }
    else if (aMode == UIMODE_PANEL) {
      this._showToolbar(true);
      this._editToolbar(false);

      bookmark.hidden = true;
      urllist.hidden = true;
      panelUI.hidden = false;
      panelUI.width = container.boxObject.width;
      panelUI.height = container.boxObject.height;
      this._initPanel();
    }
    else if (aMode == UIMODE_TABS || aMode == UIMODE_CONTROLS) {
      panelUI.hidden = true;
    }
    else if (aMode == UIMODE_NONE) {
      this._showToolbar(false);
      this._edit.reallyClosePopup();
      urllist.hidden = true;
      bookmark.hidden = true;
      panelUI.hidden = true;
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
    ws.panTo(0,0, true);
    Browser.newTab(true);
    this.show(UIMODE_URLEDIT);
  },

  closeTab : function(aTab) {
    Browser.closeTab(aTab);
    this.show(UIMODE_NONE);
  },

  selectTab : function(aTab) {
    Browser.selectTab(aTab);
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
        this.doCommand("cmd_openLocation");
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
      case "cmd_openLocation":
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
      case "cmd_openLocation":
        this.show(UIMODE_URLEDIT);
        setTimeout(function () { BrowserUI.showAutoComplete(); }, 0);
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
        this.closeTab();
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
        Browser.canvasBrowser.zoom(-1);
        break;
      case "cmd_zoomout":
        Browser.canvasBrowser.zoom(1);
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
