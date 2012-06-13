/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm")
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(window, "gChromeWin", function ()
  window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow));

function dump(s) {
  Services.console.logStringMessage("Reader: " + s);
}

let gStrings = Services.strings.createBundle("chrome://browser/locale/aboutReader.properties");

let AboutReader = {
  init: function Reader_init() {
    dump("Init()");

    dump("Feching header and content notes from about:reader");
    this._titleElement = document.getElementById("reader-header");
    this._contentElement = document.getElementById("reader-content");

    dump("Decoding query arguments");
    let queryArgs = this._decodeQueryString(window.location.href);

    let url = queryArgs.url;
    if (url) {
      dump("Fetching page with URL: " + url);
      this._loadFromURL(url);
    } else {
      var tabId = queryArgs.tabId;
      if (tabId) {
        dump("Loading from tab with ID: " + tabId);
        this._loadFromTab(tabId);
      }
    }
  },

  uninit: function Reader_uninit() {
    this._hideContent();

    delete this._titleElement;
    delete this._contentElement;
  },

  _loadFromURL: function Reader_loadFromURL(url) {
    this._showProgress();

    gChromeWin.Reader.parseDocumentFromURL(url, function(article) {
      if (article)
        this._showContent(article);
      else
        this._showError(gStrings.GetStringFromName("aboutReader.loadError"));
    }.bind(this));
  },

  _loadFromTab: function Reader_loadFromTab(tabId) {
    this._showProgress();

    gChromeWin.Reader.parseDocumentFromTab(tabId, function(article) {
      if (article)
        this._showContent(article);
      else
        this._showError(gStrings.GetStringFromName("aboutReader.loadError"));
    }.bind(this));
  },

  _showError: function Reader_showError(error) {
    this._titleElement.style.display = "none";
    this._contentElement.innerHTML = error;
    this._contentElement.style.display = "block";

    document.title = error;
  },

  _showContent: function Reader_showContent(article) {
    this._titleElement.innerHTML = article.title;
    this._titleElement.style.display = "block";

    this._contentElement.innerHTML = article.content;
    this._contentElement.style.display = "block";

    document.title = article.title;
  },

  _hideContent: function Reader_hideContent() {
    this._titleElement.style.display = "none";
    this._contentElement.style.display = "none";
  },

  _showProgress: function Reader_showProgress() {
    this._titleElement.style.display = "none";
    this._contentElement.innerHTML = gStrings.GetStringFromName("aboutReader.loading");
    this._contentElement.style.display = "block";
  },

  _decodeQueryString: function Reader_decodeQueryString(url) {
    let result = {};
    let query = url.split("?")[1];
    if (query) {
      let pairs = query.split("&");
      for (let i = 0; i < pairs.length; i++) {
        let [name, value] = pairs[i].split("=");
        result[name] = decodeURIComponent(value);
      }
    }

    return result;
  }
}
