// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ReaderMode"];

// Constants for telemetry.
const DOWNLOAD_SUCCESS = 0;
const DOWNLOAD_ERROR_XHR = 1;
const DOWNLOAD_ERROR_NO_DOC = 2;

const PARSE_SUCCESS = 0;
const PARSE_ERROR_TOO_MANY_ELEMENTS = 1;
const PARSE_ERROR_WORKER = 2;
const PARSE_ERROR_NO_ARTICLE = 3;

// Class names to preserve in the readerized output. We preserve these class
// names so that rules in aboutReader.css can match them.
const CLASSES_TO_PRESERVE = [
  "caption",
  "hidden",
  "invisble",
  "sr-only",
  "visually-hidden",
  "visuallyhidden",
  "wp-caption",
  "wp-caption-text",
];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.importGlobalProperties(["XMLHttpRequest", "XMLSerializer"]);

ChromeUtils.defineModuleGetter(this, "CommonUtils", "resource://services-common/utils.js");
ChromeUtils.defineModuleGetter(this, "EventDispatcher", "resource://gre/modules/Messaging.jsm");
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(this, "ReaderWorker", "resource://gre/modules/reader/ReaderWorker.jsm");
ChromeUtils.defineModuleGetter(this, "LanguageDetector", "resource:///modules/translation/LanguageDetector.jsm");

XPCOMUtils.defineLazyGetter(this, "Readability", function() {
  let scope = {};
  scope.dump = this.dump;
  Services.scriptloader.loadSubScript("resource://gre/modules/reader/Readability.js", scope);
  return scope.Readability;
});

const gIsFirefoxDesktop = Services.appinfo.ID == "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";

var ReaderMode = {
  // Version of the cache schema.
  CACHE_VERSION: 1,

  DEBUG: 0,

  // Don't try to parse the page if it has too many elements (for memory and
  // performance reasons)
  get maxElemsToParse() {
    delete this.parseNodeLimit;

    Services.prefs.addObserver("reader.parse-node-limit", this);
    return this.parseNodeLimit = Services.prefs.getIntPref("reader.parse-node-limit");
  },

  get isEnabledForParseOnLoad() {
    delete this.isEnabledForParseOnLoad;

    // Listen for future pref changes.
    Services.prefs.addObserver("reader.parse-on-load.", this);

    return this.isEnabledForParseOnLoad = this._getStateForParseOnLoad();
  },

  _getStateForParseOnLoad() {
    let isEnabled = Services.prefs.getBoolPref("reader.parse-on-load.enabled");
    let isForceEnabled = Services.prefs.getBoolPref("reader.parse-on-load.force-enabled");
    return isForceEnabled || isEnabled;
  },

  observe(aMessage, aTopic, aData) {
    switch (aTopic) {
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
   * Enter the reader mode by going forward one step in history if applicable,
   * if not, append the about:reader page in the history instead.
   */
  enterReaderMode(docShell, win) {
    let url = win.document.location.href;
    let readerURL = "about:reader?url=" + encodeURIComponent(url);
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    let sh = webNav.sessionHistory;
    if (webNav.canGoForward) {
      let forwardEntry = sh.legacySHistory.getEntryAtIndex(sh.index + 1, false);
      let forwardURL = forwardEntry.URI.spec;
      if (forwardURL && (forwardURL == readerURL || !readerURL)) {
        webNav.goForward();
        return;
      }
    }

    win.document.location = readerURL;
  },

  /**
   * Exit the reader mode by going back one step in history if applicable,
   * if not, append the original page in the history instead.
   */
  leaveReaderMode(docShell, win) {
    let url = win.document.location.href;
    let originalURL = this.getOriginalUrl(url);
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    let sh = webNav.sessionHistory;
    if (webNav.canGoBack) {
      let prevEntry = sh.legacySHistory.getEntryAtIndex(sh.index - 1, false);
      let prevURL = prevEntry.URI.spec;
      if (prevURL && (prevURL == originalURL || !originalURL)) {
        webNav.goBack();
        return;
      }
    }

    let referrerURI, principal;
    try {
      referrerURI = Services.io.newURI(url);
      principal = Services.scriptSecurityManager.createCodebasePrincipal(
        referrerURI, win.document.nodePrincipal.originAttributes);
    } catch (e) {
      Cu.reportError(e);
      return;
    }
    let flags =  webNav.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL |
      webNav.LOAD_FLAGS_DISALLOW_INHERIT_OWNER;
    webNav.loadURI(originalURL, flags, referrerURI, null, null, principal);
  },

  /**
   * Returns original URL from an about:reader URL.
   *
   * @param url An about:reader URL.
   * @return The original URL for the article, or null if we did not find
   *         a properly formatted about:reader URL.
   */
  getOriginalUrl(url) {
    if (!url.startsWith("about:reader?")) {
      return null;
    }

    let outerHash = "";
    try {
      let uriObj = Services.io.newURI(url);
      url = uriObj.specIgnoringRef;
      outerHash = uriObj.ref;
    } catch (ex) { /* ignore, use the raw string */ }

    let searchParams = new URLSearchParams(url.substring("about:reader?".length));
    if (!searchParams.has("url")) {
      return null;
    }
    let originalUrl = searchParams.get("url");
    if (outerHash) {
      try {
        let uriObj = Services.io.newURI(originalUrl);
        uriObj = Services.io.newURI("#" + outerHash, null, uriObj);
        originalUrl = uriObj.spec;
      } catch (ex) {}
    }
    return originalUrl;
  },

  getOriginalUrlObjectForDisplay(url) {
    let originalUrl = this.getOriginalUrl(url);
    if (originalUrl) {
      let uriObj;
      try {
        uriObj = Services.uriFixup.createFixupURI(originalUrl, Services.uriFixup.FIXUP_FLAG_NONE);
      } catch (ex) {
        return null;
      }
      try {
        return Services.uriFixup.createExposableURI(uriObj);
      } catch (ex) {
        return null;
      }
    }
    return null;
  },

  /**
   * Decides whether or not a document is reader-able without parsing the whole thing.
   *
   * @param doc A document to parse.
   * @return boolean Whether or not we should show the reader mode button.
   */
  isProbablyReaderable(doc) {
    // Only care about 'real' HTML documents:
    if (doc.mozSyntheticDocument || !(doc instanceof doc.defaultView.HTMLDocument)) {
      return false;
    }

    let uri = Services.io.newURI(doc.location.href);
    if (!this._shouldCheckUri(uri)) {
      return false;
    }

    let utils = this.getUtilsForWin(doc.defaultView);
    // We pass in a helper function to determine if a node is visible, because
    // it uses gecko APIs that the engine-agnostic readability code can't rely
    // upon.
    return new Readability(uri, doc).isProbablyReaderable(this.isNodeVisible.bind(this, utils));
  },

  isNodeVisible(utils, node) {
    let bounds = utils.getBoundsWithoutFlushing(node);
    return bounds.height > 0 && bounds.width > 0;
  },

  getUtilsForWin(win) {
    return win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
  },

  /**
   * Gets an article from a loaded browser's document. This method will not attempt
   * to parse certain URIs (e.g. about: URIs).
   *
   * @param doc A document to parse.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  parseDocument(doc) {
    if (!this._shouldCheckUri(doc.documentURIObject) || !this._shouldCheckUri(doc.baseURIObject, true)) {
      this.log("Reader mode disabled for URI");
      return null;
    }

    return this._readerParse(doc);
  },

  /**
   * Downloads and parses a document from a URL.
   *
   * @param url URL to download and parse.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  async downloadAndParseDocument(url) {
    let doc = await this._downloadDocument(url);
    if (!doc) {
      return null;
    }
    if (!this._shouldCheckUri(doc.documentURIObject) || !this._shouldCheckUri(doc.baseURIObject, true)) {
      this.log("Reader mode disabled for URI");
      return null;
    }

    return this._readerParse(doc);
  },

  _downloadDocument(url) {
    try {
      if (!this._shouldCheckUri(Services.io.newURI(url))) {
        return null;
      }
    } catch (ex) {
      Cu.reportError(new Error(`Couldn't create URI from ${url} to download: ${ex}`));
      return null;
    }
    let histogram = Services.telemetry.getHistogramById("READER_MODE_DOWNLOAD_RESULT");
    return new Promise((resolve, reject) => {
      let xhr = new XMLHttpRequest();
      xhr.open("GET", url, true);
      xhr.onerror = evt => reject(evt.error);
      xhr.responseType = "document";
      xhr.onload = evt => {
        if (xhr.status !== 200) {
          reject("Reader mode XHR failed with status: " + xhr.status);
          histogram.add(DOWNLOAD_ERROR_XHR);
          return;
        }

        let doc = xhr.responseXML;
        if (!doc) {
          reject("Reader mode XHR didn't return a document");
          histogram.add(DOWNLOAD_ERROR_NO_DOC);
          return;
        }

        // Manually follow a meta refresh tag if one exists.
        let meta = doc.querySelector("meta[http-equiv=refresh]");
        if (meta) {
          let content = meta.getAttribute("content");
          if (content) {
            let urlIndex = content.toUpperCase().indexOf("URL=");
            if (urlIndex > -1) {
              let baseURI = Services.io.newURI(url);
              let newURI = Services.io.newURI(content.substring(urlIndex + 4), null, baseURI);
              let newURL = newURI.spec;
              let ssm = Services.scriptSecurityManager;
              let flags = ssm.LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT |
                          ssm.DISALLOW_INHERIT_PRINCIPAL;
              try {
                ssm.checkLoadURIStrWithPrincipal(doc.nodePrincipal, newURL, flags);
              } catch (ex) {
                let errorMsg = "Reader mode disallowed meta refresh (reason: " + ex + ").";

                if (Services.prefs.getBoolPref("reader.errors.includeURLs"))
                  errorMsg += " Refresh target URI: '" + newURL + "'.";
                reject(errorMsg);
                return;
              }
              // Otherwise, pass an object indicating our new URL:
              if (!baseURI.equalsExceptRef(newURI)) {
                reject({newURL});
                return;
              }
            }
          }
        }
        let responseURL = xhr.responseURL;
        let givenURL = url;
        // Convert these to real URIs to make sure the escaping (or lack
        // thereof) is identical:
        try {
          responseURL = Services.io.newURI(responseURL).specIgnoringRef;
        } catch (ex) { /* Ignore errors - we'll use what we had before */ }
        try {
          givenURL = Services.io.newURI(givenURL).specIgnoringRef;
        } catch (ex) { /* Ignore errors - we'll use what we had before */ }

        if (responseURL != givenURL) {
          // We were redirected without a meta refresh tag.
          // Force redirect to the correct place:
          reject({newURL: xhr.responseURL});
          return;
        }
        resolve(doc);
        histogram.add(DOWNLOAD_SUCCESS);
      };
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
  async getArticleFromCache(url) {
    let path = this._toHashedPath(url);
    try {
      let array = await OS.File.read(path);
      return JSON.parse(new TextDecoder().decode(array));
    } catch (e) {
      if (!(e instanceof OS.File.Error) || !e.becauseNoSuchFile)
        throw e;
      return null;
    }
  },

  /**
   * Stores an article in the cache.
   *
   * @param article JS object representing article.
   * @return {Promise}
   * @resolves When the article is stored.
   * @rejects OS.File.Error
   */
  async storeArticleInCache(article) {
    let array = new TextEncoder().encode(JSON.stringify(article));
    let path = this._toHashedPath(article.url);
    await this._ensureCacheDir();
    return OS.File.writeAtomic(path, array, { tmpPath: path + ".tmp" })
      .then(success => {
        OS.File.stat(path).then(info => {
          return EventDispatcher.instance.sendRequest({
            type: "Reader:AddedToCache",
            url: article.url,
            size: info.size,
            path,
          });
        });
      });
  },

  /**
   * Removes an article from the cache given an article URI.
   *
   * @param url The article URL.
   * @return {Promise}
   * @resolves When the article is removed.
   * @rejects OS.File.Error
   */
  async removeArticleFromCache(url) {
    let path = this._toHashedPath(url);
    await OS.File.remove(path);
  },

  log(msg) {
    if (this.DEBUG)
      dump("Reader: " + msg);
  },

  _blockedHosts: [
    "amazon.com",
    "github.com",
    "mail.google.com",
    "pinterest.com",
    "reddit.com",
    "twitter.com",
    "youtube.com",
  ],

  _shouldCheckUri(uri, isBaseUri = false) {
    if (!(uri.schemeIs("http") || uri.schemeIs("https"))) {
      this.log("Not parsing URI scheme: " + uri.scheme);
      return false;
    }

    try {
      uri.QueryInterface(Ci.nsIURL);
    } catch (ex) {
      // If this doesn't work, presumably the URL is not well-formed or something
      return false;
    }
    // Sadly, some high-profile pages have false positives, so bail early for those:
    let asciiHost = uri.asciiHost;
    if (!isBaseUri && this._blockedHosts.some(blockedHost => asciiHost.endsWith(blockedHost))) {
      return false;
    }

    if (!isBaseUri && (!uri.filePath || uri.filePath == "/")) {
      this.log("Not parsing home page: " + uri.spec);
      return false;
    }

    return true;
  },

  /**
   * Attempts to parse a document into an article. Heavy lifting happens
   * in readerWorker.js.
   *
   * @param doc The document to parse.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  async _readerParse(doc) {
    let histogram = Services.telemetry.getHistogramById("READER_MODE_PARSE_RESULT");
    if (this.parseNodeLimit) {
      let numTags = doc.getElementsByTagName("*").length;
      if (numTags > this.parseNodeLimit) {
        this.log("Aborting parse for " + doc.baseURIObject.spec + "; " + numTags + " elements found");
        histogram.add(PARSE_ERROR_TOO_MANY_ELEMENTS);
        return null;
      }
    }

    // Fetch this here before we send `doc` off to the worker thread, as later on the
    // document might be nuked but we will still want the URI.
    let {documentURI} = doc;

    let uriParam = {
      spec: doc.baseURIObject.spec,
      host: doc.baseURIObject.host,
      prePath: doc.baseURIObject.prePath,
      scheme: doc.baseURIObject.scheme,
      pathBase: Services.io.newURI(".", null, doc.baseURIObject).spec
    };

    let serializer = new XMLSerializer();
    let serializedDoc = serializer.serializeToString(doc);

    let options = {
      classesToPreserve: CLASSES_TO_PRESERVE,
    };

    let article = null;
    try {
      article = await ReaderWorker.post("parseDocument", [uriParam, serializedDoc, options]);
    } catch (e) {
      Cu.reportError("Error in ReaderWorker: " + e);
      histogram.add(PARSE_ERROR_WORKER);
    }

    // Explicitly null out doc to make it clear it might not be available from this
    // point on.
    doc = null;

    if (!article) {
      this.log("Worker did not return an article");
      histogram.add(PARSE_ERROR_NO_ARTICLE);
      return null;
    }

    // Readability returns a URI object based on the baseURI, but we only care
    // about the original document's URL from now on. This also avoids spoofing
    // attempts where the baseURI doesn't match the domain of the documentURI
    article.url = documentURI;
    delete article.uri;

    let flags = Ci.nsIDocumentEncoder.OutputSelectionOnly | Ci.nsIDocumentEncoder.OutputAbsoluteLinks;
    article.title = Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils)
                                                    .convertToPlainText(article.title, flags, 0);
    if (gIsFirefoxDesktop) {
      await this._assignLanguage(article);
      this._maybeAssignTextDirection(article);
    }

    this._assignReadTime(article);

    histogram.add(PARSE_SUCCESS);
    return article;
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
  _toHashedPath(url) {
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
  _ensureCacheDir() {
    let dir = OS.Path.join(OS.Constants.Path.profileDir, "readercache");
    return OS.File.exists(dir).then(exists => {
      if (!exists) {
        return OS.File.makeDir(dir);
      }
      return undefined;
    });
  },

  /**
   * Sets a global language string value if the result is confident
   *
   * @return Promise
   * @resolves when the language is detected
   */
  _assignLanguage(article) {
    return LanguageDetector.detectLanguage(article.textContent).then(result => {
      article.language = result.confident ? result.language : null;
    });
  },

  _maybeAssignTextDirection(article) {
    // TODO: Remove the hardcoded language codes below once bug 1320265 is resolved.
    if (!article.dir && ["ar", "fa", "he", "ug", "ur"].includes(article.language)) {
      article.dir = "rtl";
    }
  },

  /**
   * Assigns the estimated reading time range of the article to the article object.
   *
   * @param article the article object to assign the reading time estimate to.
   */
  _assignReadTime(article) {
    let lang = article.language || "en";
    const readingSpeed = this._getReadingSpeedForLanguage(lang);
    const charactersPerMinuteLow = readingSpeed.cpm - readingSpeed.variance;
    const charactersPerMinuteHigh = readingSpeed.cpm + readingSpeed.variance;
    const length = article.length;

    article.readingTimeMinsSlow = Math.ceil(length / charactersPerMinuteLow);
    article.readingTimeMinsFast  = Math.ceil(length / charactersPerMinuteHigh);
  },

  /**
   * Returns the reading speed of a selection of languages with likely variance.
   *
   * Reading speed estimated from a study done on reading speeds in various languages.
   * study can be found here: http://iovs.arvojournals.org/article.aspx?articleid=2166061
   *
   * @return object with characters per minute and variance. Defaults to English
   *         if no suitable language is found in the collection.
   */
  _getReadingSpeedForLanguage(lang) {
    const readingSpeed = new Map([
      [ "en", {cpm: 987,  variance: 118 } ],
      [ "ar", {cpm: 612,  variance: 88 } ],
      [ "de", {cpm: 920,  variance: 86 } ],
      [ "es", {cpm: 1025, variance: 127 } ],
      [ "fi", {cpm: 1078, variance: 121 } ],
      [ "fr", {cpm: 998,  variance: 126 } ],
      [ "he", {cpm: 833,  variance: 130 } ],
      [ "it", {cpm: 950,  variance: 140 } ],
      [ "jw", {cpm: 357,  variance: 56 } ],
      [ "nl", {cpm: 978,  variance: 143 } ],
      [ "pl", {cpm: 916,  variance: 126 } ],
      [ "pt", {cpm: 913,  variance: 145 } ],
      [ "ru", {cpm: 986,  variance: 175 } ],
      [ "sk", {cpm: 885,  variance: 145 } ],
      [ "sv", {cpm: 917,  variance: 156 } ],
      [ "tr", {cpm: 1054, variance: 156 } ],
      [ "zh", {cpm: 255,  variance: 29 } ],
    ]);

    return readingSpeed.get(lang) || readingSpeed.get("en");
  },
};
