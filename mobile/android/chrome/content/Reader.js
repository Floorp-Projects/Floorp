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

    return this.isEnabledForParseOnLoad = this.getStateForParseOnLoad();
  },

  pageAction: {
    readerModeCallback: function(tabID) {
      Messaging.sendRequest({
        type: "Reader:Toggle",
        tabID: tabID
      });
    },

    readerModeActiveCallback: function(tabID) {
      Reader._addTabToReadingList(tabID);
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
          this.isEnabledForParseOnLoad = this.getStateForParseOnLoad();
        }
        break;
      }
    }
  },

  _addTabToReadingList: function(tabID) {
    let tab = BrowserApp.getTabForId(tabID);
    if (!tab) {
      Cu.reportError("Can't add tab to reading list because no tab found for ID: " + tabID);
      return;
    }
    let uri = tab.browser.currentURI;

    this.getArticleFromCache(uri).then(article => {
      // If the article is already in the cache, just use that.
      if (article) {
        this.addArticleToReadingList(article);
        return;
      }

      // Otherwise, get the article data from the tab.
      this.getArticleForTab(tabID, uri.specIgnoringRef, article => {
        if (article) {
          this.addArticleToReadingList(article);
        } else {
          // If there was a problem getting the article, just store the
          // URL and title from the tab.
          this.addArticleToReadingList({
            url: urlWithoutRef,
            title: tab.browser.contentDocument.title,
          });
        }
      });
    }, e => Cu.reportError("Error trying to get article from cache: " + e));
  },

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

  getStateForParseOnLoad: function Reader_getStateForParseOnLoad() {
    let isEnabled = Services.prefs.getBoolPref("reader.parse-on-load.enabled");
    let isForceEnabled = Services.prefs.getBoolPref("reader.parse-on-load.force-enabled");
    // For low-memory devices, don't allow reader mode since it takes up a lot of memory.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=792603 for details.
    return isForceEnabled || (isEnabled && !BrowserApp.isOnLowMemoryPlatform);
  },

  parseDocumentFromURL: function Reader_parseDocumentFromURL(url, callback) {
    // If there's an on-going request for the same URL, simply append one
    // more callback to it to be called when the request is done.
    if (url in this._requests) {
      let request = this._requests[url];
      request.callbacks.push(callback);
      return;
    }

    let request = { url: url, callbacks: [callback] };
    this._requests[url] = request;

    let uri = Services.io.newURI(url, null, null);

    // First, try to find a parsed article in the cache.
    this.getArticleFromCache(uri).then(article => {
      if (article) {
        this.log("Page found in cache, return article immediately");
        this._runCallbacksAndFinish(request, article);
        return;
      }

      if (!this._requests) {
        this.log("Reader has been destroyed, abort");
        return;
      }

      // Article hasn't been found in the cache, we need to
      // download the page and parse the article out of it.
      this._downloadAndParseDocument(url, request);
    }, e => {
      Cu.reportError("Error trying to get article from cache: " + e);
      this._runCallbacksAndFinish(request, null);
    });
  },

  getArticleForTab: function Reader_getArticleForTab(tabId, url, callback) {
    let tab = BrowserApp.getTabForId(tabId);
    if (tab) {
      let article = tab.savedArticle;
      if (article && article.url == url) {
        this.log("Saved article found in tab");
        callback(article);
        return;
      }
    }

    this.parseDocumentFromURL(url, callback);
  },

  parseDocumentFromTab: function (tab, callback) {
    let uri = tab.browser.currentURI;
    if (!this._shouldCheckUri(uri)) {
      callback(null);
      return;
    }

    // First, try to find a parsed article in the cache.
    this.getArticleFromCache(uri).then(article => {
      if (article) {
        this.log("Page found in cache, return article immediately");
        callback(article);
        return;
      }

      let doc = tab.browser.contentWindow.document;
      this._readerParse(uri, doc, article => {
        if (!article) {
          this.log("Failed to parse page");
          callback(null);
          return;
        }
        callback(article);
      });
    }, e => {
      Cu.reportError("Error trying to get article from cache: " + e);
      callback(null);
    });
  },

  /**
   * Retrieves an article from the cache given an article URI.
   *
   * @param uri The article URI.
   * @return Promise
   * @resolve JS object representing the article, or null if no article is found.
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
   * @return Promise
   * @resolve When the article is stored.
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
   * @return Promise
   * @resolve When the article is removed.
   * @rejects OS.File.Error
   */
  removeArticleFromCache: Task.async(function* (uri) {
    let path = this._toHashedPath(uri.specIgnoringRef);
    yield OS.File.remove(path);
  }),

  uninit: function Reader_uninit() {
    Services.prefs.removeObserver("reader.parse-on-load.", this);

    Services.obs.removeObserver(this, "Reader:Removed");

    let requests = this._requests;
    for (let url in requests) {
      let request = requests[url];
      if (request.browser) {
        let browser = request.browser;
        browser.parentNode.removeChild(browser);
      }
    }
    delete this._requests;
  },

  log: function(msg) {
    if (this.DEBUG)
      dump("Reader: " + msg);
  },

  _shouldCheckUri: function Reader_shouldCheckUri(uri) {
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

  _readerParse: function Reader_readerParse(uri, doc, callback) {
    let numTags = doc.getElementsByTagName("*").length;
    if (numTags > this.MAX_ELEMS_TO_PARSE) {
      this.log("Aborting parse for " + uri.spec + "; " + numTags + " elements found");
      callback(null);
      return;
    }

    let worker = new ChromeWorker("readerWorker.js");
    worker.onmessage = function (evt) {
      let article = evt.data;

      // Append URL to the article data. specIgnoringRef will ignore any hash
      // in the URL.
      if (article) {
        article.url = uri.specIgnoringRef;
        let flags = Ci.nsIDocumentEncoder.OutputSelectionOnly | Ci.nsIDocumentEncoder.OutputAbsoluteLinks;
        article.title = Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils)
                                                        .convertToPlainText(article.title, flags, 0);
      }

      callback(article);
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
      Cu.reportError("Reader: could not build Readability arguments: " + e);
      callback(null);
    }
  },

  _runCallbacksAndFinish: function Reader_runCallbacksAndFinish(request, result) {
    delete this._requests[request.url];

    request.callbacks.forEach(function(callback) {
      callback(result);
    });
  },

  _downloadDocument: function Reader_downloadDocument(url, callback) {
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
      if (doc != browser.contentDocument)
        return;

      this.log("Done loading: " + doc);
      if (doc.location.href == "about:blank") {
        callback(null);

        // Request has finished with error, remove browser element
        browser.parentNode.removeChild(browser);
        return;
      }

      callback(doc);
    });

    browser.loadURIWithFlags(url, Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                             null, null, null);

    return browser;
  },

  _downloadAndParseDocument: function Reader_downloadAndParseDocument(url, request) {
    try {
      this.log("Needs to fetch page, creating request: " + url);

      request.browser = this._downloadDocument(url, doc => {
        this.log("Finished loading page: " + doc);

        if (!doc) {
          this.log("Error loading page");
          this._runCallbacksAndFinish(request, null);
          return;
        }

        this.log("Parsing response with Readability");

        let uri = Services.io.newURI(url, null, null);
        this._readerParse(uri, doc, article => {
          // Delete reference to the browser element as we've finished parsing.
          let browser = request.browser;
          if (browser) {
            browser.parentNode.removeChild(browser);
            delete request.browser;
          }

          if (!article) {
            this.log("Failed to parse page");
            this._runCallbacksAndFinish(request, null);
            return;
          }

          this.log("Parsing has been successful");
          this._runCallbacksAndFinish(request, article);
        });
      });
    } catch (e) {
      this.log("Error downloading and parsing document: " + e);
      this._runCallbacksAndFinish(request, null);
    }
  },

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
