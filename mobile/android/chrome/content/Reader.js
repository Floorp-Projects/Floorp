// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");

let Reader = {
  // Version of the cache schema.
  CACHE_VERSION: 1,

  DEBUG: 0,

  // Don't try to parse the page if it has too many elements (for memory and
  // performance reasons)
  MAX_ELEMS_TO_PARSE: 3000,

  _requests: {},

  get isEnabledForParseOnLoad() {
    delete this.isEnabledForParseOnLoad;

    // Listen for future pref changes.
    Services.prefs.addObserver("reader.parse-on-load.", this, false);

    return this.isEnabledForParseOnLoad = this._getStateForParseOnLoad();
  },

  pageAction: {
    readerModeCallback: function(tabID) {
      Messaging.sendRequest({
        type: "Reader:Toggle",
        tabID: tabID
      });
    },

    readerModeActiveCallback: function(tabID) {
      Reader._addTabToReadingList(tabID).catch(e => Cu.reportError("Error adding tab to reading list: " + e));
      UITelemetry.addEvent("save.1", "pageaction", null, "reader");
    },
  },

  updatePageAction: function(tab) {
    if (this.pageAction.id) {
      PageActions.remove(this.pageAction.id);
      delete this.pageAction.id;
    }

    if (tab.readerActive) {
      this.pageAction.id = PageActions.add({
        title: Strings.browser.GetStringFromName("readerMode.exit"),
        icon: "drawable://reader_active",
        clickCallback: () => this.pageAction.readerModeCallback(tab.id),
        important: true
      });

      // Only start a reader session if the viewer is in the foreground. We do
      // not track background reader viewers.
      UITelemetry.startSession("reader.1", null);
      return;
    }

    // Only stop a reader session if the foreground viewer is not visible.
    UITelemetry.stopSession("reader.1", "", null);

    if (tab.readerEnabled) {
      this.pageAction.id = PageActions.add({
        title: Strings.browser.GetStringFromName("readerMode.enter"),
        icon: "drawable://reader",
        clickCallback: () => this.pageAction.readerModeCallback(tab.id),
        longClickCallback: () => this.pageAction.readerModeActiveCallback(tab.id),
        important: true
      });
    }
  },

  observe: function(aMessage, aTopic, aData) {
    switch(aTopic) {
      case "Reader:Removed": {
        let uri = Services.io.newURI(aData, null, null);
        this.removeArticleFromCache(uri).catch(e => Cu.reportError("Error removing article from cache: " + e));
        break;
      }

      case "nsPref:changed": {
        if (aData.startsWith("reader.parse-on-load.")) {
          this.isEnabledForParseOnLoad = this._getStateForParseOnLoad();
        }
        break;
      }
    }
  },

  _addTabToReadingList: Task.async(function* (tabID) {
    let tab = BrowserApp.getTabForId(tabID);
    if (!tab) {
      throw new Error("Can't add tab to reading list because no tab found for ID: " + tabID);
    }

    let uri = tab.browser.currentURI;
    let urlWithoutRef = uri.specIgnoringRef;

    let article = yield this.getArticle(urlWithoutRef, tabID).catch(e => {
      Cu.reportError("Error getting article for tab: " + e);
      return null;
    });
    if (!article) {
      // If there was a problem getting the article, just store the
      // URL and title from the tab.
      article = { url: urlWithoutRef, title: tab.browser.contentDocument.title };
    }

    this.addArticleToReadingList(article);
  }),

  addArticleToReadingList: function(article) {
    if (!article || !article.url) {
      Cu.reportError("addArticleToReadingList requires article with valid URL");
      return;
    }

    Messaging.sendRequest({
      type: "Reader:AddToList",
      url: truncate(article.url, MAX_URI_LENGTH),
      title: truncate(article.title || "", MAX_TITLE_LENGTH),
      length: article.length || 0,
      excerpt: article.excerpt || "",
    });

    this.storeArticleInCache(article).catch(e => Cu.reportError("Error storing article in cache: " + e));
  },

  _getStateForParseOnLoad: function () {
    let isEnabled = Services.prefs.getBoolPref("reader.parse-on-load.enabled");
    let isForceEnabled = Services.prefs.getBoolPref("reader.parse-on-load.force-enabled");
    // For low-memory devices, don't allow reader mode since it takes up a lot of memory.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=792603 for details.
    return isForceEnabled || (isEnabled && !BrowserApp.isOnLowMemoryPlatform);
  },

  /**
   * Gets an article for a given URL. This method will download and parse a document
   * if it does not find the article in the tab data or the cache.
   *
   * @param url The article URL.
   * @param tabId (optional) The id of the tab where we can look for a saved article.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  getArticle: Task.async(function* (url, tabId) {
    // First, look for an article object stored on the tab.
    let tab = BrowserApp.getTabForId(tabId);
    if (tab) {
      let article = tab.savedArticle;
      if (article && article.url == url) {
        this.log("Saved article found in tab");
        return article;
      }
    }

    // Next, try to find a parsed article in the cache.
    let uri = Services.io.newURI(url, null, null);
    let article = yield this.getArticleFromCache(uri);
    if (article) {
      this.log("Saved article found in cache");
      return article;
    }

    // Article hasn't been found in the cache, we need to
    // download the page and parse the article out of it.
    return yield this._downloadAndParseDocument(url);
  }),

  /**
   * Gets an article from a loaded tab's document. This method will parse the document
   * if it does not find the article in the tab data or the cache.
   *
   * @param tab The loaded tab.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  parseDocumentFromTab: Task.async(function* (tab) {
    let uri = tab.browser.currentURI;
    if (!this._shouldCheckUri(uri)) {
      this.log("Reader mode disabled for URI");
      return null;
    }

    // First, try to find a parsed article in the cache.
    let article = yield this.getArticleFromCache(uri);
    if (article) {
      this.log("Page found in cache, return article immediately");
      return article;
    }

    let doc = tab.browser.contentWindow.document;
    return yield this._readerParse(uri, doc);
  }),

  /**
   * Retrieves an article from the cache given an article URI.
   *
   * @param uri The article URI.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   * @rejects OS.File.Error
   */
  getArticleFromCache: Task.async(function* (uri) {
    let path = this._toHashedPath(uri.specIgnoringRef);
    try {
      let array = yield OS.File.read(path);
      return JSON.parse(new TextDecoder().decode(array));
    } catch (e if e instanceof OS.File.Error && e.becauseNoSuchFile) {
      return null;
    }
  }),

  /**
   * Stores an article in the cache.
   *
   * @param article JS object representing article.
   * @return {Promise}
   * @resolves When the article is stored.
   * @rejects OS.File.Error
   */
  storeArticleInCache: Task.async(function* (article) {
    let array = new TextEncoder().encode(JSON.stringify(article));
    let path = this._toHashedPath(article.url);
    yield this._ensureCacheDir();
    yield OS.File.writeAtomic(path, array, { tmpPath: path + ".tmp" });
  }),

  /**
   * Removes an article from the cache given an article URI.
   *
   * @param uri The article URI.
   * @return {Promise}
   * @resolves When the article is removed.
   * @rejects OS.File.Error
   */
  removeArticleFromCache: Task.async(function* (uri) {
    let path = this._toHashedPath(uri.specIgnoringRef);
    yield OS.File.remove(path);
  }),

  log: function(msg) {
    if (this.DEBUG)
      dump("Reader: " + msg);
  },

  _shouldCheckUri: function (uri) {
    if ((uri.prePath + "/") === uri.spec) {
      this.log("Not parsing home page: " + uri.spec);
      return false;
    }

    if (!(uri.schemeIs("http") || uri.schemeIs("https") || uri.schemeIs("file"))) {
      this.log("Not parsing URI scheme: " + uri.scheme);
      return false;
    }

    return true;
  },

  _readerParse: function (uri, doc) {
    return new Promise((resolve, reject) => {
      let numTags = doc.getElementsByTagName("*").length;
      if (numTags > this.MAX_ELEMS_TO_PARSE) {
        this.log("Aborting parse for " + uri.spec + "; " + numTags + " elements found");
        resolve(null);
        return;
      }

      let worker = new ChromeWorker("readerWorker.js");
      worker.onmessage = evt => {
        let article = evt.data;

        if (!article) {
          this.log("Worker did not return an article");
          resolve(null);
          return;
        }

        // Append URL to the article data. specIgnoringRef will ignore any hash
        // in the URL.
        article.url = uri.specIgnoringRef;
        let flags = Ci.nsIDocumentEncoder.OutputSelectionOnly | Ci.nsIDocumentEncoder.OutputAbsoluteLinks;
        article.title = Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils)
                                                        .convertToPlainText(article.title, flags, 0);
        resolve(article);
      };

      worker.onerror = evt => {
        reject("Error in worker: " + evt.message);
      };

      try {
        worker.postMessage({
          uri: {
            spec: uri.spec,
            host: uri.host,
            prePath: uri.prePath,
            scheme: uri.scheme,
            pathBase: Services.io.newURI(".", null, uri).spec
          },
          doc: new XMLSerializer().serializeToString(doc)
        });
      } catch (e) {
        reject("Reader: could not build Readability arguments: " + e);
      }
    });
  },

  _downloadDocument: function (url) {
    return new Promise((resolve, reject) => {
      // We want to parse those arbitrary pages safely, outside the privileged
      // context of chrome. We create a hidden browser element to fetch the
      // loaded page's document object then discard the browser element.
      let browser = document.createElement("browser");
      browser.setAttribute("type", "content");
      browser.setAttribute("collapsed", "true");
      browser.setAttribute("disablehistory", "true");

      document.documentElement.appendChild(browser);
      browser.stop();

      browser.webNavigation.allowAuth = false;
      browser.webNavigation.allowImages = false;
      browser.webNavigation.allowJavascript = false;
      browser.webNavigation.allowMetaRedirects = true;
      browser.webNavigation.allowPlugins = false;

      browser.addEventListener("DOMContentLoaded", event => {
        let doc = event.originalTarget;

        // ignore on frames and other documents
        if (doc != browser.contentDocument) {
          return;
        }

        this.log("Done loading: " + doc);
        if (doc.location.href == "about:blank") {
          reject("about:blank loaded; aborting");

          // Request has finished with error, remove browser element
          browser.parentNode.removeChild(browser);
          return;
        }

        resolve({ browser, doc });
      });

      browser.loadURIWithFlags(url, Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                               null, null, null);
    });
  },

  _downloadAndParseDocument: Task.async(function* (url) {
    this.log("Needs to fetch page, creating request: " + url);
    let { browser, doc } = yield this._downloadDocument(url);
    this.log("Finished loading page: " + doc);

    try {
      let uri = Services.io.newURI(url, null, null);
      let article = yield this._readerParse(uri, doc);
      this.log("Document parsed successfully");
      return article;
    } finally {
      browser.parentNode.removeChild(browser);
    }
  }),

  get _cryptoHash() {
    delete this._cryptoHash;
    return this._cryptoHash = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  },

  get _unicodeConverter() {
    delete this._unicodeConverter;
    this._unicodeConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                              .createInstance(Ci.nsIScriptableUnicodeConverter);
    this._unicodeConverter.charset = "utf8";
    return this._unicodeConverter;
  },

  /**
   * Calculate the hashed path for a stripped article URL.
   *
   * @param url The article URL. This should have referrers removed.
   * @return The file path to the cached article.
   */
  _toHashedPath: function (url) {
    let value = this._unicodeConverter.convertToByteArray(url);
    this._cryptoHash.init(this._cryptoHash.MD5);
    this._cryptoHash.update(value, value.length);

    let hash = CommonUtils.encodeBase32(this._cryptoHash.finish(false));
    let fileName = hash.substring(0, hash.indexOf("=")) + ".json";
    return OS.Path.join(OS.Constants.Path.profileDir, "readercache", fileName);
  },

  /**
   * Ensures the cache directory exists.
   *
   * @return Promise
   * @resolves When the cache directory exists.
   * @rejects OS.File.Error
   */
  _ensureCacheDir: function () {
    let dir = OS.Path.join(OS.Constants.Path.profileDir, "readercache");
    return OS.File.exists(dir).then(exists => {
      if (!exists) {
        return OS.File.makeDir(dir);
      }
    });
  }
};
