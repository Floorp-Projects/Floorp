// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Snackbars", "resource://gre/modules/Snackbars.jsm");

/*globals MAX_URI_LENGTH, MAX_TITLE_LENGTH */

var Reader = {
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

  get _buttonHistogram() {
    delete this._buttonHistogram;
    return this._buttonHistogram = Services.telemetry.getHistogramById("FENNEC_READER_VIEW_BUTTON");
  },

  // Values for "FENNEC_READER_VIEW_BUTTON" histogram.
  _buttonHistogramValues: {
    HIDDEN: 0,
    SHOWN: 1,
    TAP_ENTER: 2,
    TAP_EXIT: 3
  },

  /**
   * BackPressListener (listeners / ReaderView Ids).
   */
  _backPressListeners: [],
  _backPressViewIds: [],

  /**
   * Set a backPressListener for this tabId / ReaderView Id pair.
   */
  _addBackPressListener: function(tabId, viewId, listener) {
    this._backPressListeners[tabId] = listener;
    this._backPressViewIds[viewId] = tabId;
  },

  /**
   * Remove a backPressListener for this ReaderView Id.
   */
  _removeBackPressListener: function(viewId) {
    let tabId = this._backPressViewIds[viewId];
    if (tabId != undefined) {
      this._backPressListeners[tabId] = null;
      delete this._backPressViewIds[viewId];
    }
  },

  /**
   * If the requested tab has a backPress listener, return its results, else false.
   */
  onBackPress: function(tabId) {
    let listener = this._backPressListeners[tabId];
    return { handled: (listener ? listener() : false) };
  },

  observe: function Reader_observe(aMessage, aTopic, aData) {
    switch (aTopic) {
      case "Reader:RemoveFromCache": {
        ReaderMode.removeArticleFromCache(aData).catch(e => Cu.reportError("Error removing article from cache: " + e));
        break;
      }

      case "Reader:AddToCache": {
        let tab = BrowserApp.getTabForId(aData);
        if (!tab) {
          throw new Error("No tab for tabID = " + aData + " when trying to save reader view article");
        }

        // If the article is coming from reader mode, we must have fetched it already.
        this._getArticleData(tab.browser).then((article) => {
          ReaderMode.storeArticleInCache(article);
        }).catch(e => Cu.reportError("Error storing article in cache: " + e));
        break;
      }
    }
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Reader:ArticleGet":
        this._getArticle(message.data.url).then((article) => {
          // Make sure the target browser is still alive before trying to send data back.
          if (message.target.messageManager) {
            message.target.messageManager.sendAsyncMessage("Reader:ArticleData", { article: article });
          }
        }, e => {
          if (e && e.newURL) {
            message.target.loadURI("about:reader?url=" + encodeURIComponent(e.newURL));
          }
        });
        break;

      // On DropdownClosed in ReaderView, we cleanup / clear existing BackPressListener.
      case "Reader:DropdownClosed": {
        this._removeBackPressListener(message.data);
        break;
      }

      // On DropdownOpened in ReaderView, we add BackPressListener to handle a subsequent BACK request.
      case "Reader:DropdownOpened": {
        let tabId = BrowserApp.selectedTab.id;
        this._addBackPressListener(tabId, message.data, () => {
          // User hit BACK key while ReaderView has the banner font-dropdown opened.
          // Close it and return prevent-default.
          if (message.target.messageManager) {
            message.target.messageManager.sendAsyncMessage("Reader:CloseDropdown");
            return true;
          }
          // We can assume ReaderView banner's font-dropdown doesn't need to be closed.
          return false;
        });

        break;
      }

      case "Reader:FaviconRequest": {
        Messaging.sendRequestForResult({
          type: "Reader:FaviconRequest",
          url: message.data.url
        }).then(data => {
          message.target.messageManager.sendAsyncMessage("Reader:FaviconReturn", JSON.parse(data));
        });
        break;
      }

      case "Reader:SystemUIVisibility":
        Messaging.sendRequest({
          type: "SystemUI:Visibility",
          visible: message.data.visible
        });
        break;

      case "Reader:ToolbarHidden":
        if (!this._hasUsedToolbar) {
          Snackbars.show(Strings.browser.GetStringFromName("readerMode.toolbarTip"), Snackbars.LENGTH_LONG);
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
    }
  },

  pageAction: {
    readerModeCallback: function(browser) {
      let url = browser.currentURI.spec;
      if (url.startsWith("about:reader")) {
        Reader._buttonHistogram.add(Reader._buttonHistogramValues.TAP_EXIT);
      } else {
        Reader._buttonHistogram.add(Reader._buttonHistogramValues.TAP_ENTER);
      }
      browser.messageManager.sendAsyncMessage("Reader:ToggleReaderMode");
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

    let showPageAction = (icon, title) => {
      this.pageAction.id = PageActions.add({
        icon: icon,
        title: title,
        clickCallback: () => this.pageAction.readerModeCallback(browser),
        important: true
      });
    };

    let browser = tab.browser;
    if (browser.currentURI.spec.startsWith("about:reader")) {
      showPageAction("drawable://reader_active", Strings.reader.GetStringFromName("readerView.close"));
      // Only start a reader session if the viewer is in the foreground. We do
      // not track background reader viewers.
      UITelemetry.startSession("reader.1", null);
      return;
    }

    // Only stop a reader session if the foreground viewer is not visible.
    UITelemetry.stopSession("reader.1", "", null);

    if (browser.isArticle) {
      showPageAction("drawable://reader", Strings.reader.GetStringFromName("readerView.enter"));
      this._buttonHistogram.add(this._buttonHistogramValues.SHOWN);
    } else {
      this._buttonHistogram.add(this._buttonHistogramValues.HIDDEN);
    }
  },

  /**
   * Gets an article for a given URL. This method will download and parse a document
   * if it does not find the article in the cache.
   *
   * @param url The article URL.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  _getArticle: Task.async(function* (url) {
    // First try to find a parsed article in the cache.
    let article = yield ReaderMode.getArticleFromCache(url);
    if (article) {
      return article;
    }

    // Article hasn't been found in the cache, we need to
    // download the page and parse the article out of it.
    return yield ReaderMode.downloadAndParseDocument(url).catch(e => {
      if (e && e.newURL) {
        // Pass up the error so we can navigate the browser in question to the new URL:
        throw e;
      }
      Cu.reportError("Error downloading and parsing document: " + e);
      return null;
    });
  }),

  _getArticleData: function(browser) {
    return new Promise((resolve, reject) => {
      if (browser == null) {
        reject("_getArticleData needs valid browser");
      }

      let mm = browser.messageManager;
      let listener = (message) => {
        mm.removeMessageListener("Reader:StoredArticleData", listener);
        resolve(message.data.article);
      };
      mm.addMessageListener("Reader:StoredArticleData", listener);
      mm.sendAsyncMessage("Reader:GetStoredArticleData");
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
