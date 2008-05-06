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

const TOOLBARSTATE_LOADING        = 1;
const TOOLBARSTATE_LOADED         = 2;
const TOOLBARSTATE_INDETERMINATE  = 3;

const PANELMODE_VIEW              = 1;
const PANELMODE_VIEWMENU          = 2;
const PANELMODE_EDIT              = 3;
const PANELMODE_BOOKMARK          = 4;

var HUDBar = {
  _panel : null,
  _caption : null,
  _edit : null,
  _throbber : null,
  _favicon : null,
  _faviconAdded : false,
  _fadeoutID : null,
  _allowHide : true,

  _titleChanged : function(aEvent) {
    if (aEvent.target != getBrowser().contentDocument)
      return;

      this._caption.value = aEvent.target.title;
  },

  _linkAdded : function(aEvent) {
    var link = aEvent.originalTarget;
    var rel = link.rel && link.rel.toLowerCase();
    if (!link || !link.ownerDocument || !rel || !link.href)
      return;

    var rels = rel.split(/\s+/);
    if (rels.indexOf("icon") != -1) {
      this._throbber.setAttribute("src", "");
      this._setIcon(link.href);
    }
  },

  _setIcon : function(aURI) {
    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    var faviconURI = ios.newURI(aURI, null, null);

    var fis = Cc["@mozilla.org/browser/favicon-service;1"].getService(Ci.nsIFaviconService);
    if (fis.isFailedFavicon(faviconURI))
      faviconURI = ios.newURI("chrome://browser/skin/images/default-favicon.png", null, null);
    fis.setAndLoadFaviconForPage(getBrowser().currentURI, faviconURI, true);
    this._favicon.setAttribute("src", faviconURI.spec);
    this._faviconAdded = true;
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

  init : function() {
    this._panel = document.getElementById("hud-ui");
    this._panel.addEventListener("popuphiding", this, false);
    this._caption = document.getElementById("hudbar-caption");
    this._caption.addEventListener("click", this, false);
    this._edit = document.getElementById("hudbar-edit");
    this._edit.addEventListener("focus", this, false);
    this._edit.addEventListener("keypress", this, false);
    this._throbber = document.getElementById("hudbar-throbber");
    this._favicon = document.getElementById("hudbar-favicon");
    this._favicon.addEventListener("error", this, false);

    this._menu = document.getElementById("hudmenu");

    Browser.content.addEventListener("DOMTitleChanged", this, true);
    Browser.content.addEventListener("DOMLinkAdded", this, true);
  },

  update : function(aState) {
    if (aState == TOOLBARSTATE_INDETERMINATE) {
      this._faviconAdded = false;
      aState = TOOLBARSTATE_LOADED;
      this.setURI();
    }

    var hudbar = document.getElementById("hudbar-container");
    if (aState == TOOLBARSTATE_LOADING) {
      this._showMode(PANELMODE_VIEW);

      hudbar.setAttribute("mode", "loading");
      this._throbber.setAttribute("src", "chrome://browser/skin/images/throbber.gif");
      this._favicon.setAttribute("src", "");
      this._faviconAdded = false;
    }
    else if (aState == TOOLBARSTATE_LOADED) {
      hudbar.setAttribute("mode", "view");
      this._throbber.setAttribute("src", "");
      if (this._faviconAdded == false) {
        var faviconURI = getBrowser().currentURI.prePath + "/favicon.ico";
        this._setIcon(faviconURI);
      }

      if (this._allowHide)
        this.hide(3000);
      this._allowHide = true;
    }
  },

  /* Set the location to the current content */
  setURI : function() {
    var browser = getBrowser();
    var back = document.getElementById("cmd_back");
    var forward = document.getElementById("cmd_forward");

    back.setAttribute("disabled", !browser.canGoBack);
    forward.setAttribute("disabled", !browser.canGoForward);

    // Check for a bookmarked page
    var star = document.getElementById("hudbar-star");
    star.removeAttribute("starred");
    var bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
    var bookmarks = bms.getBookmarkIdsForURI(browser.currentURI, {});
    if (bookmarks.length > 0) {
      star.setAttribute("starred", "true");
    }

    var uri = browser.currentURI.spec;
    if (uri == "about:blank") {
      uri = "";
      this._showMode(PANELMODE_EDIT);
      this._allowHide = false;
    }

    this._caption.value = uri;
    this._edit.value = uri;
  },

  goToURI : function(aURI) {
    if (!aURI)
      aURI = this._edit.value;

    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    if (ios.newURI(aURI, null, null) == null)
      this.search();
    else
      getBrowser().loadURI(aURI, null, null, false);

    if (this._panel.state == "open")
      this._showMode(PANELMODE_VIEW);
  },

  search : function() {
    var queryURI = "http://www.google.com/search?q=" + this._edit.value + "&hl=en&lr=&btnG=Search";
    getBrowser().loadURI(queryURI, null, null, false);
    if (this._panel.state == "open")
      this._showMode(PANELMODE_VIEW);
  },

  _showMode : function(aMode) {
    if (this._fadeoutID) {
      clearTimeout(this._fadeoutID);
      this._fadeoutID = null;
    }

    var hudbar = document.getElementById("hudbar-container");
    var hudbmk = document.getElementById("hudbookmark-container");
    var hudexpand = document.getElementById("hudexpand-container");
    if (aMode == PANELMODE_VIEW || aMode == PANELMODE_VIEWMENU) {
      hudbar.setAttribute("mode", "view");
      this._edit.hidden = true;
      this._caption.hidden = false;
      hudbmk.hidden = true;
      hudexpand.hidden = true;

      if (aMode == PANELMODE_VIEWMENU) {
        this._menu.hidden = false;
        this._menu.openPopupAtScreen((window.innerWidth - this._menu.boxObject.width) / 2, (window.innerHeight - this._menu.boxObject.height) / 2);
      }
    }
    else if (aMode == PANELMODE_EDIT) {
      hudbar.setAttribute("mode", "edit");
      this._caption.hidden = true;
      this._edit.hidden = false;
      this._edit.focus();

      this._menu.hidePopup();

      this.showHistory();

      hudbmk.hidden = true;
      //hudexpand.style.height = window.innerHeight - (30 + hudbar.boxObject.height);
      hudexpand.hidden = false;
    }
    else if (aMode == PANELMODE_BOOKMARK) {
      hudbar.setAttribute("mode", "view");
      this._edit.hidden = true;
      this._caption.hidden = false;

      this._menu.hidePopup();

      hudexpand.hidden = true;
      hudbmk.hidden = false;
    }
  },

  _showPlaces : function(aItems) {
    var list = document.getElementById("hudlist-items");
    while (list.childNodes.length > 0) {
      list.removeChild(list.childNodes[0]);
    }

    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    var fis = Cc["@mozilla.org/browser/favicon-service;1"].getService(Ci.nsIFaviconService);

    if (aItems.length > 0) {
      for (var i=0; i<aItems.length; i++) {
        let node = aItems[i];
        let listItem = document.createElement("richlistitem");
        listItem.setAttribute("class", "hudlist-item");

        let box = document.createElement("vbox");
        box.setAttribute("pack", "center");
        let image = document.createElement("image");
        image.setAttribute("class", "hudlist-image");
        let icon = node.icon ? node.icon.spec : fis.getFaviconImageForPage(ios.newURI(node.uri, null, null)).spec
        image.setAttribute("src", icon);
        box.appendChild(image);
        listItem.appendChild(box);

        let label = document.createElement("label");
        label.setAttribute("class", "hudlist-text");
        label.setAttribute("value", node.title);
        label.setAttribute("flex", "1");
        label.setAttribute("crop", "end");
        listItem.appendChild(label);
        list.appendChild(listItem);
        listItem.addEventListener("click", function() { HUDBar.goToURI(node.uri); }, true);
      }
    }

    list.focus();
  },

  showHistory : function() {
    this._showPlaces(this._getHistory(6));
  },

  showBookmarks : function () {
    var bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
    this._showPlaces(this._getBookmarks([bms.bookmarksMenuFolder]));
  },

  show : function() {
    this._panel.hidden = false; // was initially hidden to reduce Ts
    this._panel.width = window.innerWidth - 20;
    this._showMode(PANELMODE_VIEWMENU);
    this._panel.openPopup(null, "", 10, 5, false, false);
    this._allowHide = false;
  },

  hide : function(aFadeout) {
    if (!aFadeout) {
      if (this._allowHide) {
        this._menu.hidePopup();
        this._panel.hidePopup();
      }
      this._allowHide = true;
    }
    else {
      var self = this;
      this.fadeoutID = setTimeout(function() { self.hide(); }, aFadeout);
    }
  },

  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      case "DOMTitleChanged":
        this._titleChanged(aEvent);
        break;
      case "DOMLinkAdded":
        this._linkAdded(aEvent);
        break;
      case "popuphiding":
        this._showMode(PANELMODE_VIEW);
        break;
      case "click":
        this._showMode(PANELMODE_EDIT);
        break;
      case "focus":
        setTimeout(function() { aEvent.target.select(); }, 0);
        break;
      case "keypress":
        if (aEvent.keyCode == 13)
          this.goToURI();
        break;
      case "error":
        this._favicon.setAttribute("src", "chrome://browser/skin/images/default-favicon.png");
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
      case "cmd_newTab":
      case "cmd_closeTab":
      case "cmd_switchTab":
      case "cmd_menu":
      case "cmd_fullscreen":
      case "cmd_addons":
      case "cmd_downloads":
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
        browser.stop();
        browser.goBack();
        break;
      case "cmd_forward":
        browser.stop();
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

        var bookmarks = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
        if (bookmarks.getBookmarkIdsForURI(bookmarkURI, {}).length == 0) {
          var bookmarkId = bookmarks.insertBookmark(bookmarks.bookmarksMenuFolder, bookmarkURI, bookmarks.DEFAULT_INDEX, bookmarkTitle);
          document.getElementById("hudbar-star").setAttribute("starred", "true");

          var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
          var favicon = document.getElementById("hudbar-favicon");
          var faviconURI = ios.newURI(favicon.src, null, null);

          var fis = Cc["@mozilla.org/browser/favicon-service;1"].getService(Ci.nsIFaviconService);
          fis.setAndLoadFaviconForPage(bookmarkURI, faviconURI, true);
        }
        else {
          this._showMode(PANELMODE_BOOKMARK);
          BookmarkHelper.edit(bookmarkURI);
        }
        break;
      }
      case "cmd_bookmarks":
        Bookmarks.list();
        break;
      case "cmd_addons":
      {
        const EMTYPE = "Extension:Manager";

        var aOpenMode = "extensions";
        var wm = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
        var needToOpen = true;
        var windowType = EMTYPE + "-" + aOpenMode;
        var windows = wm.getEnumerator(windowType);
        while (windows.hasMoreElements()) {
          var theEM = windows.getNext().QueryInterface(Ci.nsIDOMWindowInternal);
          if (theEM.document.documentElement.getAttribute("windowtype") == windowType) {
            theEM.focus();
            needToOpen = false;
            break;
          }
        }

        if (needToOpen) {
          const EMURL = "chrome://mozapps/content/extensions/extensions.xul?type=" + aOpenMode;
          const EMFEATURES = "chrome,dialog=no,resizable=yes";
          window.openDialog(EMURL, "", EMFEATURES);
        }
        break;
      }
      case "cmd_downloads":
        Cc["@mozilla.org/download-manager-ui;1"].getService(Ci.nsIDownloadManagerUI).show(window);
    }
  }
};

var BookmarkHelper = {
  _item : null,
  _uri : null,
  _bmksvc : null,
  _tagsvc : null,

  edit : function(aURI) {
    this._bmksvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
    this._tagsvc = Cc["@mozilla.org/browser/tagging-service;1"].getService(Ci.nsITaggingService);

    this._uri = aURI;
    var bookmarkIDs = this._bmksvc.getBookmarkIdsForURI(this._uri, {});
    if (bookmarkIDs.length > 0) {
      this._item = bookmarkIDs[0];
      document.getElementById("hudbookmark-name").value = this._bmksvc.getItemTitle(this._item);
      var currentTags = this._tagsvc.getTagsForURI(this._uri, {});
      document.getElementById("hudbookmark-tags").value = currentTags.join(" ");
    }
  },

  remove : function() {
    if (this._item) {
      this._bmksvc.removeItem(this._item);
      document.getElementById("hudbar-star").removeAttribute("starred");
    }
    this.close();
  },

  save : function() {
    if (this._item) {
      // Update the name
      this._bmksvc.setItemTitle(this._item, document.getElementById("hudbookmark-name").value);

      // Update the tags
      var taglist = document.getElementById("hudbookmark-tags").value;
      var currentTags = this._tagsvc.getTagsForURI(this._uri, {});
      var tags = taglist.split(" ");;
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
          this._tagsvc.tagURI(this._uri, tagsToAdd);
        if (tagsToRemove.length > 0)
          this._tagsvc.untagURI(this._uri, tagsToRemove);
      }

    }
    this.close();
  },

  close : function() {
    this._item = null;
    HUDBar.hide();
  }
};
