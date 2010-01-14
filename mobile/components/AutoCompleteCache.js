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
 * The Original Code is AutoComplete Cache.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

// Current cache version. This should be incremented if the format of the cache
// file is modified.
const CACHE_VERSION = 1;

// Lazily get the base Places AutoComplete Search
XPCOMUtils.defineLazyGetter(this, "PACS", function() {
  return Components.classesByID["{d0272978-beab-4adc-a3d4-04b76acfa4e7}"]
                   .getService(Ci.nsIAutoCompleteSearch);
});

XPCOMUtils.defineLazyServiceGetter(this, "gDirSvc",
                                   "@mozilla.org/file/directory_service;1",
                                   "nsIProperties");

// Gets a directory from the directory service
function getDir(aKey) {
  return gDirSvc.get(aKey, Ci.nsIFile);
}

// -----------------------------------------------------------------------
// AutoCompleteUtils support the cache and prefetching system used by
// the AutoCompleteCache service
// -----------------------------------------------------------------------

var AutoCompleteUtils = {
  cache: null,
  query: "",
  busy: false,
  timer: null,
  DELAY: 10000,

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
          onResult(result);

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
      ostream.init(this.cacheFile, (MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE), PERMS_FILE, 0);
      converter.charset = "UTF-8";
      let data = converter.convertToInputStream(JSON.stringify(cache));

      // Write to the cache file asynchronously
      NetUtil.asyncCopy(data, ostream, function(rv) {
        if (!Components.isSuccessCode(rv))
          Cu.reportError("AutoCompleteUtils: failure during asyncCopy: " + rv);
      });
    } catch (ex) {
      Cu.reportError("AutoCompleteUtils: Could not write to cache file: " + ex);
    }
  },

  loadCache: function loadCache() {
    try {
      // Load the cached results from the file
      let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
      let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);

      stream.init(this.cacheFile, MODE_RDONLY, PERMS_FILE, 0);
      let cache = json.decodeFromStream(stream, stream.available());

      if (cache.version != CACHE_VERSION) {
        this.fetch(this.query);
        return;
      }

      let result = cache.result;

      // Add back functions to the result
      result.getValueAt = function(index) this.data[index][0];
      result.getCommentAt = function(index) this.data[index][1];
      result.getStyleAt = function(index) this.data[index][2];
      result.getImageAt = function(index) this.data[index][3];

      this.cache = result;
    } catch (ex) {
      Cu.reportError("AutoCompleteUtils: Could not read from cache file: " + ex);
    }
  }
};

// -----------------------------------------------------------------------
// AutoCompleteCache bypasses SQLite backend for common searches
// -----------------------------------------------------------------------

function AutoCompleteCache() {
  AutoCompleteUtils.init();
}

AutoCompleteCache.prototype = {
  classDescription: "AutoComplete Cache",
  contractID: "@mozilla.org/autocomplete/search;1?name=history",
  classID: Components.ID("{a65f9dca-62ab-4b36-a870-972927c78b56}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteSearch]),

  startSearch: function(query, param, prev, listener) {
    let self = this;
    let done = function(result) {
      listener.onSearchResult(self, result);
    };

    // Strip out leading/trailing spaces
    query = query.trim();

    if (AutoCompleteUtils.query == query && AutoCompleteUtils.cache) {
      // On a cache-hit, give the results right away and fetch in the background
      done(AutoCompleteUtils.cache);
    } else {
      // Otherwise, fetch the result, cache it, and pass it on
      AutoCompleteUtils.fetch(query, done);
    }

    // Keep the cache warm
    AutoCompleteUtils.update();
  },

  stopSearch: function() {
    // Stop any active queries
    AutoCompleteUtils.stop();
  }
};

function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([AutoCompleteCache]);
}
