/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PERMS_FILE      = 0600;
const PERMS_DIRECTORY = 0755;

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

// Current cache version. This should be incremented if the format of the cache
// file is modified.
const CACHE_VERSION = 1;

const RESULT_CACHE = 1;
const RESULT_NEW = 2;

// Lazily get the base Places AutoComplete Search
XPCOMUtils.defineLazyGetter(this, "PACS", function() {
  return Components.classesByID["{d0272978-beab-4adc-a3d4-04b76acfa4e7}"]
                   .getService(Ci.nsIAutoCompleteSearch);
});

// Gets a directory from the directory service
function getDir(aKey) {
  return Services.dirsvc.get(aKey, Ci.nsIFile);
}

// -----------------------------------------------------------------------
// AutoCompleteUtils support the cache and prefetching system used by
// the AutoCompleteCache service
// -----------------------------------------------------------------------

var AutoCompleteUtils = {
  cacheFile: null,
  cache: null,
  query: "",
  busy: false,
  timer: null,
  DELAY: 5000,

  // Use the base places search to get results
  fetch: function fetch(query, onResult) {
    // We're requested to start something new so stop any active queries
    this.stop();

    // Flag that we're busy using the base places autocomplete search
    this.busy = true;
    PACS.startSearch(query, "", null, {
      onSearchResult: function(search, result) {
        // Let the listener know about the result right away
        if (typeof onResult == "function")
          onResult(result, RESULT_NEW);

        // Don't do any more processing if we're not completely done
        if (result.searchResult == result.RESULT_NOMATCH_ONGOING ||
            result.searchResult == result.RESULT_SUCCESS_ONGOING)
          return;

        // We must be done, so cache the results
        if (AutoCompleteUtils.query == query)
          AutoCompleteUtils.cache = result;
        AutoCompleteUtils.busy = false;

        // Save special query to cache
        if (AutoCompleteUtils.query == query)
          AutoCompleteUtils.saveCache();
      }
    });
  },

  // Stop an active fetch if necessary
  stop: function stop() {
    // Nothing to stop if nothing is querying
    if (!this.busy)
      return;

    // Stop the base implementation
    PACS.stopSearch();
    this.busy = false;
  },

  // Prepare to fetch some data to fill up the cache
  update: function update() {
    // No need to reschedule or delay an existing timer
    if (this.timer != null)
      return;

    // Start a timer that will fetch some data
    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.timer.initWithCallback({
      notify: function() {
        AutoCompleteUtils.timer = null;

        // Do the actual fetch if we aren't busy
        if (!AutoCompleteUtils.busy)
          AutoCompleteUtils.fetch(AutoCompleteUtils.query);
      }
    }, this.DELAY, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  init: function init() {
    if (this.cacheFile)
      return;

    this.cacheFile = getDir("ProfD");
    this.cacheFile.append("autocomplete.json");

    if (this.cacheFile.exists()) {
      // Load the existing cache
      this.loadCache();
    } else {
      // Make the empty query cache
      this.fetch(this.query);
    }
  },

  saveCache: function saveCache() {
    if (!this.cache)
      return;

    let cache = {};
    cache.version = CACHE_VERSION;

    // Make a clone of the result thst is safe to JSON-ify
    let result = this.cache;
    let copy = JSON.parse(JSON.stringify(result));
    copy.data = [];
    for (let i = 0; i < result.matchCount; i++)
      copy.data[i] = [result.getValueAt(i), result.getCommentAt(i), result.getStyleAt(i), result.getImageAt(i)];

    cache.result = copy;

    // Convert to json to save to disk..
    let ostream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(Ci.nsIScriptableUnicodeConverter);

    try {
      ostream.init(this.cacheFile, (MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE), PERMS_FILE, ostream.DEFER_OPEN);
      converter.charset = "UTF-8";
      let data = converter.convertToInputStream(JSON.stringify(cache));

      // Write to the cache file asynchronously
      NetUtil.asyncCopy(data, ostream, function(rv) {
        if (!Components.isSuccessCode(rv))
          Cu.reportError("AutoCompleteUtils: failure during asyncCopy: " + rv);
        else
          Services.obs.notifyObservers(null, "browser:cache-session-history-write-complete", "");
      });
    } catch (ex) {
      Cu.reportError("AutoCompleteUtils: Could not write to cache file: " + this.cacheFile + " | " + ex);
    }
  },

  loadCache: function loadCache() {
    if (!this.cacheFile.exists())
      return;

    try {
      let self = this;
      let channel = NetUtil.newChannel(this.cacheFile);
      channel.contentType = "application/json";
      NetUtil.asyncFetch(channel, function(aInputStream, aResultCode) {
        if (Components.isSuccessCode(aResultCode)) {
          let cache = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON).
                      decodeFromStream(aInputStream, aInputStream.available());

          if (cache.version != CACHE_VERSION) {
            self.fetch(self.query);
            return;
          }
          self.cache = new cacheResult(cache.result.searchString, cache.result.data);
          Services.obs.notifyObservers(null, "browser:cache-session-history-read-complete", "");
        } else {
          Cu.reportError("AutoCompleteUtils: Could not read from cache file");
        }
      });
    } catch (ex) {
      Cu.reportError("AutoCompleteUtils: Could not read from cache file: " + ex);
    }
  }
};

function cacheResult(aSearchString, aData) {
  if (aData)
    this.data = aData;
  this.searchString = aSearchString;
}

cacheResult.prototype = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIAutoCompleteSimpleResult, Ci.nsIAutoCompleteResult, Ci.nsISupportsWeakReference]),
  searchString : "",
  data: [],
  errorDescription : "",
  defaultIndex : 0,
  get matchCount() { return this.data.length; },
  searchResult : Ci.nsIAutoCompleteResult.RESULT_SUCCESS,

  getValueAt : function(index) this.data[index][0],
  getLabelAt : function(index) this.data[index][0],
  getCommentAt : function(index) this.data[index][1],
  getStyleAt : function(index) this.data[index][2],
  getImageAt : function(index) this.data[index][3],
  
  appendMatch : function(aValue, aComment, aImage, aStyle) { this.data.push([aValue, aComment, aStyle, aImage]) },
  setErrorDescription : function(aErrorDescription) { this.errorDescription = aErrorDescription; },
  setDefaultIndex : function(aDefaultIndex) { this.defaultIndex = aDefaultIndex; },
  setSearchString : function(aSearchString) { this.searchString = aSearchString; },
  setSearchResult : function(aSearchResult) { this.searchResult = aSearchResult; },
  setListener : function(aListener) { return; }
}

// -----------------------------------------------------------------------
// AutoCompleteCache bypasses SQLite backend for common searches
// -----------------------------------------------------------------------

function AutoCompleteCache() {
  this.searchEngines = Services.search.getVisibleEngines();
  AutoCompleteUtils.init();

  Services.obs.addObserver(this, "browser:cache-session-history-reload", true);
  Services.obs.addObserver(this, "places-expiration-finished", true);
  Services.obs.addObserver(this, "browser-search-engine-modified", true);
}

AutoCompleteCache.prototype = {
  classID: Components.ID("{a65f9dca-62ab-4b36-a870-972927c78b56}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteSearch, Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  searchEngines: [],

  get _searchThreshold() {
    delete this._searchCount;
    return this._searchCount = Services.prefs.getIntPref("browser.urlbar.autocomplete.search_threshold");    
  },

  startSearch: function(query, param, prev, listener) {
    let self = this;
    let done = function(aResult, aType) {
      let showSearch = (aResult.matchCount < self._searchThreshold) && (aType == RESULT_NEW);

      if (showSearch && (aResult.searchResult == Ci.nsIAutoCompleteResult.RESULT_SUCCESS ||
                         aResult.searchResult == Ci.nsIAutoCompleteResult.RESULT_NOMATCH)) {
        self._addSearchProviders(aResult);
      }
      listener.onSearchResult(self, aResult);
    };

    // Strip out leading/trailing spaces
    query = query.trim();
    let usedCache = false;

    if (AutoCompleteUtils.query == query && AutoCompleteUtils.cache) {
      // On a cache-hit, give the results right away and fetch in the background
      done(AutoCompleteUtils.cache, RESULT_CACHE);
      usedCache = true;
    } else if (prev) {
      // Otherwise, check if this is the same as the prev search,
      // and if the previous search was null. We have to special
      // case 'www.' here due to it being ignore in autocomplete
      // searches (see bug 461483).
      let prevSearch = prev.searchString;
      if (prev.matchCount == this.searchEngines.length &&
          prevSearch !== "www." &&
          (query.indexOf(prevSearch) == 0)) {
        done(new cacheResult(query, []), RESULT_NEW);
        usedCache = true;
      }
    }

    // Only start a fetch if we think we actually need to update the cache
    if (!usedCache)
      AutoCompleteUtils.fetch(query, done);

    // Keep the cache warm
    AutoCompleteUtils.update();
  },

  _addSearchProviders: function(aResult) {
    try {
      aResult.QueryInterface(Ci.nsIAutoCompleteSimpleResult);
      if (this.searchEngines.length > 0) {  
        for (let i = 0; i < this.searchEngines.length; i++) {
          let engine = this.searchEngines[i];
          let url = engine.getSubmission(aResult.searchString).uri.spec;
          let iconURI = engine.iconURI;
          aResult.appendMatch(url, engine.name, iconURI ? iconURI.spec : "", "search");
        }
        aResult.setSearchResult(Ci.nsIAutoCompleteResult.RESULT_SUCCESS);
      }
    } catch(ex) {}
  },

  stopSearch: function() {
    // Stop any active queries
    AutoCompleteUtils.stop();
  },

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "browser:cache-session-history-reload":
        if (AutoCompleteUtils.cacheFile.exists())
          AutoCompleteUtils.loadCache();
        else
          AutoCompleteUtils.fetch(AutoCompleteUtils.query);
        break;
      case "places-expiration-finished":
        AutoCompleteUtils.fetch(AutoCompleteUtils.query);
        break;
      case "browser-search-engine-modified":
        this.searchEngines = Services.search.getVisibleEngines();
        break;
    }
  }
};

// -----------------------------------------------------------------------
// BookmarkObserver updates the cache when a bookmark is added
// -----------------------------------------------------------------------
function BookmarkObserver() {
  AutoCompleteUtils.init();
  this._batch = false;
}

BookmarkObserver.prototype = {
  onBeginUpdateBatch: function() {
    this._batch = true;
  },
  onEndUpdateBatch: function() {
    this._batch = false;
    AutoCompleteUtils.update();
  },
  onItemAdded: function(aItemId, aParentId, aIndex, aItemType) {
    if (!this._batch)
      AutoCompleteUtils.update();
  },
  onItemChanged: function () {
    if (!this._batch)
      AutoCompleteUtils.update();
  },
  onBeforeItemRemoved: function() {},
  onItemRemoved: function() {
    if (!this._batch)
      AutoCompleteUtils.update();
  },
  onItemVisited: function() {},
  onItemMoved: function() {},

  classID: Components.ID("f570982e-4f15-48ab-b6a0-ed851ac551b2"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver])
};

const components = [AutoCompleteCache, BookmarkObserver];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
