// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["ReaderMode"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.importGlobalProperties(["XMLHttpRequest"]);

XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils", "resource://services-common/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderWorker", "resource://gre/modules/reader/ReaderWorker.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyGetter(this, "Readability", function() {
  let scope = {};
  Services.scriptloader.loadSubScript("resource://gre/modules/reader/Readability.js", scope);
  return scope["Readability"];
});

this.ReaderMode = {
  // Version of the cache schema.
  CACHE_VERSION: 1,

  DEBUG: 0,

  // Don't try to parse the page if it has too many elements (for memory and
  // performance reasons)
  get maxElemsToParse() {
    delete this.parseNodeLimit;

    Services.prefs.addObserver("reader.parse-node-limit", this, false);
    return this.parseNodeLimit = Services.prefs.getIntPref("reader.parse-node-limit");
  },

  get isEnabledForParseOnLoad() {
    delete this.isEnabledForParseOnLoad;

    // Listen for future pref changes.
    Services.prefs.addObserver("reader.parse-on-load.", this, false);

    return this.isEnabledForParseOnLoad = this._getStateForParseOnLoad();
  },

  get isOnLowMemoryPlatform() {
    let memory = Cc["@mozilla.org/xpcom/memory-service;1"].getService(Ci.nsIMemory);
    delete this.isOnLowMemoryPlatform;
    return this.isOnLowMemoryPlatform = memory.isLowMemoryPlatform();
  },

  _getStateForParseOnLoad: function () {
    let isEnabled = Services.prefs.getBoolPref("reader.parse-on-load.enabled");
    let isForceEnabled = Services.prefs.getBoolPref("reader.parse-on-load.force-enabled");
    // For low-memory devices, don't allow reader mode since it takes up a lot of memory.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=792603 for details.
    return isForceEnabled || (isEnabled && !this.isOnLowMemoryPlatform);
  },

  observe: function(aMessage, aTopic, aData) {
    switch(aTopic) {
      case "nsPref:changed":
        if (aData.startsWith("reader.parse-on-load.")) {
          this.isEnabledForParseOnLoad = this._getStateForParseOnLoad();
        } else if (aData === "reader.parse-node-limit") {
          this.parseNodeLimit = Services.prefs.getIntPref(aData);
        }
        break;
    }
  },

  /**
   * Returns original URL from an about:reader URL.
   *
   * @param url An about:reader URL.
   * @return The original URL for the article, or null if we did not find
   *         a properly formatted about:reader URL.
   */
  getOriginalUrl: function(url) {
    if (!url.startsWith("about:reader?")) {
      return null;
    }

    let searchParams = new URLSearchParams(url.substring("about:reader?".length));
    if (!searchParams.has("url")) {
      return null;
    }
    let encodedURL = searchParams.get("url");
    try {
      return decodeURIComponent(encodedURL);
    } catch (e) {
      Cu.reportError("Error decoding original URL: " + e);
      return encodedURL;
    }
  },

  /**
   * Decides whether or not a document is reader-able without parsing the whole thing.
   *
   * @param doc A document to parse.
   * @return boolean Whether or not we should show the reader mode button.
   */
  isProbablyReaderable: function(doc) {
    // Only care about 'real' HTML documents:
    if (doc.mozSyntheticDocument || !(doc instanceof doc.defaultView.HTMLDocument)) {
      return false;
    }

    let uri = Services.io.newURI(doc.location.href, null, null);
    if (!this._shouldCheckUri(uri)) {
      return false;
    }

    return new Readability(uri, doc).isProbablyReaderable();
  },

  /**
   * Gets an article from a loaded browser's document. This method will not attempt
   * to parse certain URIs (e.g. about: URIs).
   *
   * @param doc A document to parse.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  parseDocument: Task.async(function* (doc) {
    let uri = Services.io.newURI(doc.documentURI, null, null);
    if (!this._shouldCheckUri(uri)) {
      this.log("Reader mode disabled for URI");
      return null;
    }

    return yield this._readerParse(uri, doc);
  }),

  /**
   * Downloads and parses a document from a URL.
   *
   * @param url URL to download and parse.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  downloadAndParseDocument: Task.async(function* (url) {
    let uri = Services.io.newURI(url, null, null);
    let doc = yield this._downloadDocument(url);
    return yield this._readerParse(uri, doc);
  }),

  _downloadDocument: function (url) {
    return new Promise((resolve, reject) => {
      let xhr = new XMLHttpRequest();
      xhr.open("GET", url, true);
      xhr.onerror = evt => reject(evt.error);
      xhr.responseType = "document";
      xhr.onload = evt => {
        if (xhr.status !== 200) {
          reject("Reader mode XHR failed with status: " + xhr.status);
          return;
        }

        let doc = xhr.responseXML;
        if (!doc) {
          reject("Reader mode XHR didn't return a document");
          return;
        }

        // Manually follow a meta refresh tag if one exists.
        let meta = doc.querySelector("meta[http-equiv=refresh]");
        if (meta) {
          let content = meta.getAttribute("content");
          if (content) {
            let urlIndex = content.indexOf("URL=");
            if (urlIndex > -1) {
              let url = content.substring(urlIndex + 4);
              this._downloadDocument(url).then((doc) => resolve(doc));
              return;
            }
          }
        }
        resolve(doc);
      }
      xhr.send();
    });
  },


  /**
   * Retrieves an article from the cache given an article URI.
   *
   * @param url The article URL.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   * @rejects OS.File.Error
   */
  getArticleFromCache: Task.async(function* (url) {
    let path = this._toHashedPath(url);
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
   * @param url The article URL.
   * @return {Promise}
   * @resolves When the article is removed.
   * @rejects OS.File.Error
   */
  removeArticleFromCache: Task.async(function* (url) {
    let path = this._toHashedPath(url);
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

  /**
   * Attempts to parse a document into an article. Heavy lifting happens
   * in readerWorker.js.
   *
   * @param uri The article URI.
   * @param doc The document to parse.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  _readerParse: Task.async(function* (uri, doc) {
    if (this.parseNodeLimit) {
      let numTags = doc.getElementsByTagName("*").length;
      if (numTags > this.parseNodeLimit) {
        this.log("Aborting parse for " + uri.spec + "; " + numTags + " elements found");
        return null;
      }
    }

    let uriParam = {
      spec: uri.spec,
      host: uri.host,
      prePath: uri.prePath,
      scheme: uri.scheme,
      pathBase: Services.io.newURI(".", null, uri).spec
    };

    let serializer = Cc["@mozilla.org/xmlextras/xmlserializer;1"].
                     createInstance(Ci.nsIDOMSerializer);
    let serializedDoc = yield Promise.resolve(serializer.serializeToString(doc));

    let article = null;
    try {
      article = yield ReaderWorker.post("parseDocument", [uriParam, serializedDoc]);
    } catch (e) {
      Cu.reportError("Error in ReaderWorker: " + e);
    }

    if (!article) {
      this.log("Worker did not return an article");
      return null;
    }

    // Readability returns a URI object, but we only care about the URL.
    article.url = article.uri.spec;
    delete article.uri;

    let flags = Ci.nsIDocumentEncoder.OutputSelectionOnly | Ci.nsIDocumentEncoder.OutputAbsoluteLinks;
    article.title = Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils)
                                                    .convertToPlainText(article.title, flags, 0);
    return article;
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
