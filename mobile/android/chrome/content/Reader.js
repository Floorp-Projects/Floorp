// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/ReaderMode.jsm");

let Reader = {
  // These values should match those defined in BrowserContract.java.
  STATUS_UNFETCHED: 0,
  STATUS_FETCH_FAILED_TEMPORARY: 1,
  STATUS_FETCH_FAILED_PERMANENT: 2,
  STATUS_FETCH_FAILED_UNSUPPORTED_FORMAT: 3,
  STATUS_FETCHED_ARTICLE: 4,

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
    if (!tab.getActive()) {
      return;
    }

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

    if (tab.savedArticle) {
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
        ReaderMode.removeArticleFromCache(uri).catch(e => Cu.reportError("Error removing article from cache: " + e));
        break;
      }

      case "nsPref:changed":
        if (aData.startsWith("reader.parse-on-load.")) {
          this.isEnabledForParseOnLoad = this._getStateForParseOnLoad();
        }
        break;
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
      article = {
        url: urlWithoutRef,
        title: tab.browser.contentDocument.title,
        status: this.STATUS_FETCH_FAILED_UNSUPPORTED_FORMAT,
      };
    } else {
      article.status = this.STATUS_FETCHED_ARTICLE;
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
      status: article.status,
    });

    ReaderMode.storeArticleInCache(article).catch(e => Cu.reportError("Error storing article in cache: " + e));
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
        return article;
      }
    }

    // Next, try to find a parsed article in the cache.
    let uri = Services.io.newURI(url, null, null);
    let article = yield ReaderMode.getArticleFromCache(uri);
    if (article) {
      return article;
    }

    // Article hasn't been found in the cache, we need to
    // download the page and parse the article out of it.
    return yield ReaderMode.downloadAndParseDocument(url);
  }),

  /**
   * Migrates old indexedDB reader mode cache to new JSON cache.
   */
  migrateCache: Task.async(function* () {
    let cacheDB = yield new Promise((resolve, reject) => {
      let request = window.indexedDB.open("about:reader", 1);
      request.onsuccess = event => resolve(event.target.result);
      request.onerror = event => reject(request.error);

      // If there is no DB to migrate, don't do anything.
      request.onupgradeneeded = event => resolve(null);
    });

    if (!cacheDB) {
      return;
    }

    let articles = yield new Promise((resolve, reject) => {
      let articles = [];

      let transaction = cacheDB.transaction(cacheDB.objectStoreNames);
      let store = transaction.objectStore(cacheDB.objectStoreNames[0]);

      let request = store.openCursor();
      request.onsuccess = event => {
        let cursor = event.target.result;
        if (!cursor) {
          resolve(articles);
        } else {
          articles.push(cursor.value);
          cursor.continue();
        }
      };
      request.onerror = event => reject(request.error);
    });

    for (let article of articles) {
      yield ReaderMode.storeArticleInCache(article);
    }

    // Delete the database.
    window.indexedDB.deleteDatabase("about:reader");
  }),
};
