// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Reader = {
  // These values should match those defined in BrowserContract.java.
  STATUS_UNFETCHED: 0,
  STATUS_FETCH_FAILED_TEMPORARY: 1,
  STATUS_FETCH_FAILED_PERMANENT: 2,
  STATUS_FETCH_FAILED_UNSUPPORTED_FORMAT: 3,
  STATUS_FETCHED_ARTICLE: 4,

  get _hasUsedToolbar() {
    delete this._hasUsedToolbar;
    return this._hasUsedToolbar = Services.prefs.getBoolPref("reader.has_used_toolbar");
  },

  observe: function Reader_observe(aMessage, aTopic, aData) {
    switch (aTopic) {
      case "Reader:FetchContent": {
        let data = JSON.parse(aData);
        this._fetchContent(data.url, data.id);
        break;
      }

      case "Reader:Added": {
        let mm = window.getGroupMessageManager("browsers");
        mm.broadcastAsyncMessage("Reader:Added", { url: aData });
        break;
      }

      case "Reader:Removed": {
        ReaderMode.removeArticleFromCache(aData).catch(e => Cu.reportError("Error removing article from cache: " + e));

        let mm = window.getGroupMessageManager("browsers");
        mm.broadcastAsyncMessage("Reader:Removed", { url: aData });
        break;
      }
    }
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Reader:AddToList": {
        // If the article is coming from reader mode, we must have fetched it already.
        let article = message.data.article;
        article.status = this.STATUS_FETCHED_ARTICLE;
        this._addArticleToReadingList(article);
        break;
      }
      case "Reader:ArticleGet":
        this._getArticle(message.data.url, message.target).then((article) => {
          message.target.messageManager.sendAsyncMessage("Reader:ArticleData", { article: article });
        });
        break;

      case "Reader:FaviconRequest": {
        Messaging.sendRequestForResult({
          type: "Reader:FaviconRequest",
          url: message.data.url
        }).then(data => {
          message.target.messageManager.sendAsyncMessage("Reader:FaviconReturn", JSON.parse(data));
        });
        break;
      }

      case "Reader:ListStatusRequest":
        Messaging.sendRequestForResult({
          type: "Reader:ListStatusRequest",
          url: message.data.url
        }).then((data) => {
          message.target.messageManager.sendAsyncMessage("Reader:ListStatusData", JSON.parse(data));
        });
        break;

      case "Reader:RemoveFromList":
        Messaging.sendRequest({
          type: "Reader:RemoveFromList",
          url: message.data.url
        });
        break;

      case "Reader:Share":
        Messaging.sendRequest({
          type: "Reader:Share",
          url: message.data.url,
          title: message.data.title
        });
        break;

      case "Reader:SystemUIVisibility":
        Messaging.sendRequest({
          type: "SystemUI:Visibility",
          visible: message.data.visible
        });
        break;

      case "Reader:ToolbarHidden":
        if (!this._hasUsedToolbar) {
          NativeWindow.toast.show(Strings.browser.GetStringFromName("readerMode.toolbarTip"), "short");
          Services.prefs.setBoolPref("reader.has_used_toolbar", true);
          this._hasUsedToolbar = true;
        }
        break;

      case "Reader:UpdateReaderButton": {
        let tab = BrowserApp.getTabForBrowser(message.target);
        tab.browser.isArticle = message.data.isArticle;
        this.updatePageAction(tab);
        break;
      }
      case "Reader:SetIntPref": {
        if (message.data && message.data.name !== undefined) {
          Services.prefs.setIntPref(message.data.name, message.data.value);
        }
        break;
      }
      case "Reader:SetCharPref": {
        if (message.data && message.data.name !== undefined) {
          Services.prefs.setCharPref(message.data.name, message.data.value);
        }
        break;
      }
    }
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

    let browser = tab.browser;
    if (browser.currentURI.spec.startsWith("about:reader")) {
      this.pageAction.id = PageActions.add({
        title: Strings.reader.GetStringFromName("readerView.close"),
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

    if (browser.isArticle) {
      this.pageAction.id = PageActions.add({
        title: Strings.reader.GetStringFromName("readerView.enter"),
        icon: "drawable://reader",
        clickCallback: () => this.pageAction.readerModeCallback(tab.id),
        longClickCallback: () => this.pageAction.readerModeActiveCallback(tab.id),
        important: true
      });
    }
  },

  /**
   * Downloads and caches content for a reading list item with a given URL and id.
   */
  _fetchContent: function(url, id) {
    this._downloadAndCacheArticle(url).then(article => {
      if (article == null) {
        Messaging.sendRequest({
          type: "Reader:UpdateList",
          id: id,
          status: this.STATUS_FETCH_FAILED_UNSUPPORTED_FORMAT,
        });
      } else {
        Messaging.sendRequest({
          type: "Reader:UpdateList",
          id: id,
          url: truncate(article.url, MAX_URI_LENGTH),
          title: truncate(article.title, MAX_TITLE_LENGTH),
          length: article.length,
          excerpt: article.excerpt,
          status: this.STATUS_FETCHED_ARTICLE,
        });
      }
    }).catch(e => {
      Cu.reportError("Error fetching content: " + e);
      Messaging.sendRequest({
        type: "Reader:UpdateList",
        id: id,
        status: this.STATUS_FETCH_FAILED_TEMPORARY,
      });
    });
  },

  _downloadAndCacheArticle: Task.async(function* (url) {
    let article = yield ReaderMode.downloadAndParseDocument(url);
    if (article != null) {
      yield ReaderMode.storeArticleInCache(article);
    }
    return article;
  }),

  _addTabToReadingList: Task.async(function* (tabID) {
    let tab = BrowserApp.getTabForId(tabID);
    if (!tab) {
      throw new Error("Can't add tab to reading list because no tab found for ID: " + tabID);
    }

    let url = tab.browser.currentURI.spec;
    let article = yield this._getArticle(url, tab.browser).catch(e => {
      Cu.reportError("Error getting article for tab: " + e);
      return null;
    });
    if (!article) {
      // If there was a problem getting the article, just store the
      // URL and title from the tab.
      article = {
        url: url,
        title: tab.browser.contentDocument.title,
        length: 0,
        excerpt: "",
        status: this.STATUS_FETCH_FAILED_UNSUPPORTED_FORMAT,
      };
    } else {
      article.status = this.STATUS_FETCHED_ARTICLE;
    }

    this._addArticleToReadingList(article);
  }),

  _addArticleToReadingList: function(article) {
    Messaging.sendRequestForResult({
      type: "Reader:AddToList",
      url: truncate(article.url, MAX_URI_LENGTH),
      title: truncate(article.title, MAX_TITLE_LENGTH),
      length: article.length,
      excerpt: article.excerpt,
      status: article.status,
    }).then((url) => {
      ReaderMode.storeArticleInCache(article).catch(e => Cu.reportError("Error storing article in cache: " + e));
    }).catch(Cu.reportError);
  },

  /**
   * Gets an article for a given URL. This method will download and parse a document
   * if it does not find the article in the tab data or the cache.
   *
   * @param url The article URL.
   * @param browser The browser where the article is currently loaded.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  _getArticle: Task.async(function* (url, browser) {
    // First, look for a saved article.
    let article = yield this._getSavedArticle(browser);
    if (article && article.url == url) {
      return article;
    }

    // Next, try to find a parsed article in the cache.
    article = yield ReaderMode.getArticleFromCache(url);
    if (article) {
      return article;
    }

    // Article hasn't been found in the cache, we need to
    // download the page and parse the article out of it.
    return yield ReaderMode.downloadAndParseDocument(url).catch(e => {
      Cu.reportError("Error downloading and parsing document: " + e);
      return null;
    });
  }),

  _getSavedArticle: function(browser) {
    return new Promise((resolve, reject) => {
      let mm = browser.messageManager;
      let listener = (message) => {
        mm.removeMessageListener("Reader:SavedArticleData", listener);
        resolve(message.data.article);
      };
      mm.addMessageListener("Reader:SavedArticleData", listener);
      mm.sendAsyncMessage("Reader:SavedArticleGet");
    });
  },

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
