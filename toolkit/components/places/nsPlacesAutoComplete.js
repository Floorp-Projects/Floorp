/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

// This SQL query fragment provides the following:
//   - whether the entry is bookmarked (kQueryIndexBookmarked)
//   - the bookmark title, if it is a bookmark (kQueryIndexBookmarkTitle)
//   - the tags associated with a bookmarked entry (kQueryIndexTags)
const kBookTagSQLFragment =
  "EXISTS(SELECT 1 FROM moz_bookmarks WHERE fk = h.id) AS bookmarked, "
+ "( "
+   "SELECT title FROM moz_bookmarks WHERE fk = h.id AND title NOTNULL "
+   "ORDER BY lastModified DESC LIMIT 1 "
+ ") AS btitle, "
+ "( "
+   "SELECT GROUP_CONCAT(t.title, ',') "
+   "FROM moz_bookmarks b "
+   "JOIN moz_bookmarks t ON t.id = +b.parent AND t.parent = :parent "
+   "WHERE b.fk = h.id "
+ ") AS tags";

// observer topics
const kTopicShutdown = "places-shutdown";
const kPrefChanged = "nsPref:changed";

// Match type constants.  These indicate what type of search function we should
// be using.
const MATCH_ANYWHERE = Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE;
const MATCH_BOUNDARY_ANYWHERE = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY_ANYWHERE;
const MATCH_BOUNDARY = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY;
const MATCH_BEGINNING = Ci.mozIPlacesAutoComplete.MATCH_BEGINNING;

// AutoComplete index constants.  All AutoComplete queries will provide these
// columns in this order.
const kQueryIndexURL = 0;
const kQueryIndexTitle = 1;
const kQueryIndexFaviconURL = 2;
const kQueryIndexBookmarked = 3;
const kQueryIndexBookmarkTitle = 4;
const kQueryIndexTags = 5;
const kQueryIndexVisitCount = 6;
const kQueryIndexTyped = 7;
const kQueryIndexPlaceId = 8;
const kQueryIndexQueryType = 9;
const kQueryIndexOpenPageCount = 10;

// AutoComplete query type constants.  Describes the various types of queries
// that we can process.
const kQueryTypeKeyword = 0;
const kQueryTypeFiltered = 1;

// This separator is used as an RTL-friendly way to split the title and tags.
// It can also be used by an nsIAutoCompleteResult consumer to re-split the
// "comment" back into the title and the tag.
const kTitleTagsSeparator = " \u2013 ";

const kBrowserUrlbarBranch = "browser.urlbar.";

// Toggle autoFill.
const kBrowserUrlbarAutofillPref = "autoFill";
// Whether to search only typed entries.
const kBrowserUrlbarAutofillTypedPref = "autoFill.typed";

////////////////////////////////////////////////////////////////////////////////
//// Globals

XPCOMUtils.defineLazyServiceGetter(this, "gTextURIService",
                                   "@mozilla.org/intl/texttosuburi;1",
                                   "nsITextToSubURI");

////////////////////////////////////////////////////////////////////////////////
//// Helpers

/**
 * Initializes our temporary table on a given database.
 *
 * @param aDatabase
 *        The mozIStorageConnection to set up the temp table on.
 */
function initTempTable(aDatabase)
{
  // Note: this should be kept up-to-date with the definition in
  //       nsPlacesTables.h.
  let stmt = aDatabase.createAsyncStatement(
    "CREATE TEMP TABLE moz_openpages_temp ( "
  + "  url TEXT PRIMARY KEY "
  + ", open_count INTEGER "
  + ") "
  );
  stmt.executeAsync();
  stmt.finalize();

  // Note: this should be kept up-to-date with the definition in
  //       nsPlacesTriggers.h.
  stmt = aDatabase.createAsyncStatement(
    "CREATE TEMPORARY TRIGGER moz_openpages_temp_afterupdate_trigger "
  + "AFTER UPDATE OF open_count ON moz_openpages_temp FOR EACH ROW "
  + "WHEN NEW.open_count = 0 "
  + "BEGIN "
  +   "DELETE FROM moz_openpages_temp "
  +   "WHERE url = NEW.url; "
  + "END "
  );
  stmt.executeAsync();
  stmt.finalize();
}

/**
 * Used to unescape encoded URI strings, and drop information that we do not
 * care about for searching.
 *
 * @param aURIString
 *        The text to unescape and modify.
 * @return the modified uri.
 */
function fixupSearchText(aURIString)
{
  let uri = aURIString;

  if (uri.indexOf("http://") == 0) {
    uri = uri.slice(7);
  }
  else if (uri.indexOf("https://") == 0) {
    uri = uri.slice(8);
  }
  else if (uri.indexOf("ftp://") == 0) {
    uri = uri.slice(6);
  }

  if (uri.indexOf("www.") == 0) {
    uri = uri.slice(4);
  }

  return gTextURIService.unEscapeURIForUI("UTF-8", uri);
}

/**
 * safePrefGetter get the pref with typo safety.
 * This will return the default value provided if no pref is set.
 *
 * @param aPrefBranch
 *        The nsIPrefBranch containing the required preference
 * @param aName
 *        A preference name
 * @param aDefault
 *        The preference's default value
 * @return the preference value or provided default
 */

function safePrefGetter(aPrefBranch, aName, aDefault) {
  let types = {
    boolean: "Bool",
    number: "Int",
    string: "Char"
  };
  let type = types[typeof(aDefault)];
  if (!type) {
    throw "Unknown type!";
  }
  // If the pref isn't set, we want to use the default.
  try {
    return aPrefBranch["get" + type + "Pref"](aName);
  }
  catch (e) {
    return aDefault;
  }
}


////////////////////////////////////////////////////////////////////////////////
//// AutoCompleteStatementCallbackWrapper class

/**
 * Wraps a callback and ensures that handleCompletion is not dispatched if the
 * query is no longer tracked.
 *
 * @param aCallback
 *        A reference to a nsPlacesAutoComplete.
 * @param aDBConnection
 *        The database connection to execute the queries on.
 */
function AutoCompleteStatementCallbackWrapper(aCallback,
                                              aDBConnection)
{
  this._callback = aCallback;
  this._db = aDBConnection;
}

AutoCompleteStatementCallbackWrapper.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  handleResult: function ACSCW_handleResult(aResultSet)
  {
    this._callback.handleResult.apply(this._callback, arguments);
  },

  handleError: function ACSCW_handleError(aError)
  {
    this._callback.handleError.apply(this._callback, arguments);
  },

  handleCompletion: function ACSCW_handleCompletion(aReason)
  {
    // Only dispatch handleCompletion if we are not done searching and are a
    // pending search.
    let callback = this._callback;
    if (!callback.isSearchComplete() && callback.isPendingSearch(this._handle)) {
      callback.handleCompletion.apply(callback, arguments);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// AutoCompleteStatementCallbackWrapper

  /**
   * Executes the specified query asynchronously.  This object will notify
   * this._callback if we should notify (logic explained in handleCompletion).
   *
   * @param aQueries
   *        The queries to execute asynchronously.
   * @return a mozIStoragePendingStatement that can be used to cancel the
   *         queries.
   */
  executeAsync: function ACSCW_executeAsync(aQueries)
  {
    return this._handle = this._db.executeAsync(aQueries, aQueries.length,
                                                this);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([
    Ci.mozIStorageStatementCallback,
  ])
};

////////////////////////////////////////////////////////////////////////////////
//// nsPlacesAutoComplete class

function nsPlacesAutoComplete()
{
  //////////////////////////////////////////////////////////////////////////////
  //// Shared Constants for Smart Getters

  // TODO bug 412736 in case of a frecency tie, break it with h.typed and
  // h.visit_count which is better than nothing.  This is slow, so not doing it
  // yet...
  const SQL_BASE = "SELECT h.url, h.title, f.url, " + kBookTagSQLFragment + ", "
                 +        "h.visit_count, h.typed, h.id, :query_type, "
                 +        "t.open_count "
                 + "FROM moz_places h "
                 + "LEFT JOIN moz_favicons f ON f.id = h.favicon_id "
                 + "LEFT JOIN moz_openpages_temp t ON t.url = h.url "
                 + "WHERE h.frecency <> 0 "
                 +   "AND AUTOCOMPLETE_MATCH(:searchString, h.url, "
                 +                          "IFNULL(btitle, h.title), tags, "
                 +                          "h.visit_count, h.typed, "
                 +                          "bookmarked, t.open_count, "
                 +                          ":matchBehavior, :searchBehavior) "
                 +  "{ADDITIONAL_CONDITIONS} "
                 + "ORDER BY h.frecency DESC, h.id DESC "
                 + "LIMIT :maxResults";

  //////////////////////////////////////////////////////////////////////////////
  //// Smart Getters

  XPCOMUtils.defineLazyGetter(this, "_db", function() {
    // Get a cloned, read-only version of the database.  We'll only ever write
    // to our own in-memory temp table, and having a cloned copy means we do not
    // run the risk of our queries taking longer due to the main database
    // connection performing a long-running task.
    let db = Cc["@mozilla.org/browser/nav-history-service;1"].
             getService(Ci.nsPIPlacesDatabase).
             DBConnection.
             clone(true);

    // Autocomplete often fallbacks to a table scan due to lack of text indices.
    // In such cases a larger cache helps reducing IO.  The default Storage
    // value is MAX_CACHE_SIZE_BYTES in storage/src/mozStorageConnection.cpp.
    let stmt = db.createAsyncStatement("PRAGMA cache_size = -6144"); // 6MiB
    stmt.executeAsync();
    stmt.finalize();

    // Create our in-memory tables for tab tracking.
    initTempTable(db);

    // Populate the table with current open pages cache contents.
    if (this._openPagesCache.length > 0) {
      // Avoid getter re-entrance from the _registerOpenPageQuery lazy getter.
      let stmt = this._registerOpenPageQuery =
        db.createAsyncStatement(this._registerOpenPageQuerySQL);
      let params = stmt.newBindingParamsArray();
      for (let i = 0; i < this._openPagesCache.length; i++) {
        let bp = params.newBindingParams();
        bp.bindByName("page_url", this._openPagesCache[i]);
        params.addParams(bp);
      }
      stmt.bindParameters(params);
      stmt.executeAsync();
      stmt.finalize();
      delete this._openPagesCache;
    }

    return db;
  });

  XPCOMUtils.defineLazyServiceGetter(this, "_bh",
                                     "@mozilla.org/browser/global-history;2",
                                     "nsIBrowserHistory");

  XPCOMUtils.defineLazyServiceGetter(this, "_bs",
                                     "@mozilla.org/browser/nav-bookmarks-service;1",
                                     "nsINavBookmarksService");

  XPCOMUtils.defineLazyServiceGetter(this, "_ioService",
                                     "@mozilla.org/network/io-service;1",
                                     "nsIIOService");

  XPCOMUtils.defineLazyServiceGetter(this, "_faviconService",
                                     "@mozilla.org/browser/favicon-service;1",
                                     "nsIFaviconService");

  XPCOMUtils.defineLazyGetter(this, "_defaultQuery", function() {
    let replacementText = "";
    return this._db.createAsyncStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_historyQuery", function() {
    // Enforce ignoring the visit_count index, since the frecency one is much
    // faster in this case.  ANALYZE helps the query planner to figure out the
    // faster path, but it may not have run yet.
    let replacementText = "AND +h.visit_count > 0";
    return this._db.createAsyncStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_bookmarkQuery", function() {
    let replacementText = "AND bookmarked";
    return this._db.createAsyncStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_tagsQuery", function() {
    let replacementText = "AND tags IS NOT NULL";
    return this._db.createAsyncStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_openPagesQuery", function() {
    return this._db.createAsyncStatement(
      "SELECT t.url, t.url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "
    +         ":query_type, t.open_count, NULL "
    + "FROM moz_openpages_temp t "
    + "LEFT JOIN moz_places h ON h.url = t.url "
    + "WHERE h.id IS NULL "
    +   "AND AUTOCOMPLETE_MATCH(:searchString, t.url, t.url, NULL, "
    +                          "NULL, NULL, NULL, t.open_count, "
    +                          ":matchBehavior, :searchBehavior) "
    + "ORDER BY t.ROWID DESC "
    + "LIMIT :maxResults "
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_typedQuery", function() {
    let replacementText = "AND h.typed = 1";
    return this._db.createAsyncStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_adaptiveQuery", function() {
    return this._db.createAsyncStatement(
      "/* do not warn (bug 487789) */ "
    + "SELECT h.url, h.title, f.url, " + kBookTagSQLFragment + ", "
    +         "h.visit_count, h.typed, h.id, :query_type, t.open_count "
    + "FROM ( "
    + "SELECT ROUND( "
    +     "MAX(use_count) * (1 + (input = :search_string)), 1 "
    +   ") AS rank, place_id "
    +   "FROM moz_inputhistory "
    +   "WHERE input BETWEEN :search_string AND :search_string || X'FFFF' "
    +   "GROUP BY place_id "
    + ") AS i "
    + "JOIN moz_places h ON h.id = i.place_id "
    + "LEFT JOIN moz_favicons f ON f.id = h.favicon_id "
    + "LEFT JOIN moz_openpages_temp t ON t.url = h.url "
    + "WHERE AUTOCOMPLETE_MATCH(NULL, h.url, "
    +                          "IFNULL(btitle, h.title), tags, "
    +                          "h.visit_count, h.typed, bookmarked, "
    +                          "t.open_count, "
    +                          ":matchBehavior, :searchBehavior) "
    + "ORDER BY rank DESC, h.frecency DESC "
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_keywordQuery", function() {
    return this._db.createAsyncStatement(
      "/* do not warn (bug 487787) */ "
    + "SELECT "
    +  "(SELECT REPLACE(url, '%s', :query_string) FROM moz_places WHERE id = b.fk) "
    +  "AS search_url, h.title, "
    +  "IFNULL(f.url, (SELECT f.url "
    +                 "FROM moz_places "
    +                 "JOIN moz_favicons f ON f.id = favicon_id "
    +                 "WHERE rev_host = (SELECT rev_host FROM moz_places WHERE id = b.fk) "
    +                 "ORDER BY frecency DESC "
    +                 "LIMIT 1) "
    + "), 1, b.title, NULL, h.visit_count, h.typed, IFNULL(h.id, b.fk), "
    +  ":query_type, t.open_count "
    +  "FROM moz_keywords k "
    +  "JOIN moz_bookmarks b ON b.keyword_id = k.id "
    +  "LEFT JOIN moz_places h ON h.url = search_url "
    +  "LEFT JOIN moz_favicons f ON f.id = h.favicon_id "
    +  "LEFT JOIN moz_openpages_temp t ON t.url = search_url "
    +  "WHERE LOWER(k.keyword) = LOWER(:keyword) "
    +  "ORDER BY h.frecency DESC "
    );
  });

  this._registerOpenPageQuerySQL = "INSERT OR REPLACE INTO moz_openpages_temp "
                                 +   "(url, open_count) "
                                 + "VALUES (:page_url, "
                                 +   "IFNULL("
                                 +     "("
                                 +        "SELECT open_count + 1 "
                                 +        "FROM moz_openpages_temp "
                                 +        "WHERE url = :page_url "
                                 +      "), "
                                 +     "1"
                                 +   ")"
                                 + ")";
  XPCOMUtils.defineLazyGetter(this, "_registerOpenPageQuery", function() {
    return this._db.createAsyncStatement(this._registerOpenPageQuerySQL);
  });

  XPCOMUtils.defineLazyGetter(this, "_unregisterOpenPageQuery", function() {
    return this._db.createAsyncStatement(
      "UPDATE moz_openpages_temp "
    + "SET open_count = open_count - 1 "
    + "WHERE url = :page_url"
    );
  });

  //////////////////////////////////////////////////////////////////////////////
  //// Initialization

  // load preferences
  this._prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefService).
                getBranch(kBrowserUrlbarBranch);
  this._loadPrefs(true);

  // register observers
  this._os = Cc["@mozilla.org/observer-service;1"].
              getService(Ci.nsIObserverService);
  this._os.addObserver(this, kTopicShutdown, false);

}

nsPlacesAutoComplete.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSearch

  startSearch: function PAC_startSearch(aSearchString, aSearchParam,
                                        aPreviousResult, aListener)
  {
    // Stop the search in case the controller has not taken care of it.
    this.stopSearch();

    // Note: We don't use aPreviousResult to make sure ordering of results are
    //       consistent.  See bug 412730 for more details.

    // We want to store the original string with no leading or trailing
    // whitespace for case sensitive searches.
    this._originalSearchString = aSearchString.trim();

    this._currentSearchString =
      fixupSearchText(this._originalSearchString.toLowerCase());

    let searchParamParts = aSearchParam.split(" ");
    this._enableActions = searchParamParts.indexOf("enable-actions") != -1;

    this._listener = aListener;
    let result = Cc["@mozilla.org/autocomplete/simple-result;1"].
                 createInstance(Ci.nsIAutoCompleteSimpleResult);
    result.setSearchString(aSearchString);
    result.setListener(this);
    this._result = result;

    // If we are not enabled, we need to return now.
    if (!this._enabled) {
      this._finishSearch(true);
      return;
    }

    // Reset our search behavior to the default.
    if (this._currentSearchString) {
      this._behavior = this._defaultBehavior;
    }
    else {
      this._behavior = this._emptySearchDefaultBehavior;
    }
    // For any given search, we run up to four queries:
    // 1) keywords (this._keywordQuery)
    // 2) adaptive learning (this._adaptiveQuery)
    // 3) open pages not supported by history (this._openPagesQuery)
    // 4) query from this._getSearch
    // (1) only gets ran if we get any filtered tokens from this._getSearch,
    // since if there are no tokens, there is nothing to match, so there is no
    // reason to run the query).
    let {query, tokens} =
      this._getSearch(this._getUnfilteredSearchTokens(this._currentSearchString));
    let queries = tokens.length ?
      [this._getBoundKeywordQuery(tokens), this._getBoundAdaptiveQuery(), this._getBoundOpenPagesQuery(tokens), query] :
      [this._getBoundAdaptiveQuery(), this._getBoundOpenPagesQuery(tokens), query];

    // Start executing our queries.
    this._telemetryStartTime = Date.now();
    this._executeQueries(queries);

    // Set up our persistent state for the duration of the search.
    this._searchTokens = tokens;
    this._usedPlaces = {};
  },

  stopSearch: function PAC_stopSearch()
  {
    // We need to cancel our searches so we do not get any [more] results.
    // However, it's possible we haven't actually started any searches, so this
    // method may throw because this._pendingQuery may be undefined.
    if (this._pendingQuery) {
      this._stopActiveQuery();
    }

    this._finishSearch(false);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSimpleResultListener

  onValueRemoved: function PAC_onValueRemoved(aResult, aURISpec, aRemoveFromDB)
  {
    if (aRemoveFromDB) {
      this._bh.removePage(this._ioService.newURI(aURISpec, null, null));
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// mozIPlacesAutoComplete

  // If the connection has not yet been started, use this local cache.  This
  // prevents autocomplete from initing the database till the first search.
  _openPagesCache: [],
  registerOpenPage: function PAC_registerOpenPage(aURI)
  {
    if (!this._databaseInitialized) {
      this._openPagesCache.push(aURI.spec);
      return;
    }

    let stmt = this._registerOpenPageQuery;
    stmt.params.page_url = aURI.spec;
    stmt.executeAsync();
  },

  unregisterOpenPage: function PAC_unregisterOpenPage(aURI)
  {
    if (!this._databaseInitialized) {
      let index = this._openPagesCache.indexOf(aURI.spec);
      if (index != -1) {
        this._openPagesCache.splice(index, 1);
      }
      return;
    }

    let stmt = this._unregisterOpenPageQuery;
    stmt.params.page_url = aURI.spec;
    stmt.executeAsync();
  },

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  handleResult: function PAC_handleResult(aResultSet)
  {
    let row, haveMatches = false;
    while (row = aResultSet.getNextRow()) {
      let match = this._processRow(row);
      haveMatches = haveMatches || match;

      if (this._result.matchCount == this._maxRichResults) {
        // We have enough results, so stop running our search.
        this._stopActiveQuery();

        // And finish our search.
        this._finishSearch(true);
        return;
      }

    }

    // Notify about results if we've gotten them.
    if (haveMatches) {
      this._notifyResults(true);
    }
  },

  handleError: function PAC_handleError(aError)
  {
    Components.utils.reportError("Places AutoComplete: An async statement encountered an " +
                                 "error: " + aError.result + ", '" + aError.message + "'");
  },

  handleCompletion: function PAC_handleCompletion(aReason)
  {
    // If we have already finished our search, we should bail out early.
    if (this.isSearchComplete()) {
      return;
    }

    // If we do not have enough results, and our match type is
    // MATCH_BOUNDARY_ANYWHERE, search again with MATCH_ANYWHERE to get more
    // results.
    if (this._matchBehavior == MATCH_BOUNDARY_ANYWHERE &&
        this._result.matchCount < this._maxRichResults && !this._secondPass) {
      this._secondPass = true;
      let queries = [
        this._getBoundAdaptiveQuery(MATCH_ANYWHERE),
        this._getBoundSearchQuery(MATCH_ANYWHERE, this._searchTokens),
      ];
      this._executeQueries(queries);
      return;
    }

    this._finishSearch(true);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function PAC_observe(aSubject, aTopic, aData)
  {
    if (aTopic == kTopicShutdown) {
      this._os.removeObserver(this, kTopicShutdown);

      // Remove our preference observer.
      this._prefs.removeObserver("", this);
      delete this._prefs;

      // Finalize the statements that we have used.
      let stmts = [
        "_defaultQuery",
        "_historyQuery",
        "_bookmarkQuery",
        "_tagsQuery",
        "_openPagesQuery",
        "_typedQuery",
        "_adaptiveQuery",
        "_keywordQuery",
        "_registerOpenPageQuery",
        "_unregisterOpenPageQuery",
      ];
      for (let i = 0; i < stmts.length; i++) {
        // We do not want to create any query we haven't already created, so
        // see if it is a getter first.
        if (Object.getOwnPropertyDescriptor(this, stmts[i]).value !== undefined) {
          this[stmts[i]].finalize();
        }
      }

      if (this._databaseInitialized) {
        this._db.asyncClose();
      }
    }
    else if (aTopic == kPrefChanged) {
      this._loadPrefs();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsPlacesAutoComplete

  get _databaseInitialized()
    Object.getOwnPropertyDescriptor(this, "_db").value !== undefined,

  /**
   * Generates the tokens used in searching from a given string.
   *
   * @param aSearchString
   *        The string to generate tokens from.
   * @return an array of tokens.
   */
  _getUnfilteredSearchTokens: function PAC_unfilteredSearchTokens(aSearchString)
  {
    // Calling split on an empty string will return an array containing one
    // empty string.  We don't want that, as it'll break our logic, so return an
    // empty array then.
    return aSearchString.length ? aSearchString.split(" ") : [];
  },

  /**
   * Properly cleans up when searching is completed.
   *
   * @param aNotify
   *        Indicates if we should notify the AutoComplete listener about our
   *        results or not.
   */
  _finishSearch: function PAC_finishSearch(aNotify)
  {
    // Notify about results if we are supposed to.
    if (aNotify) {
      this._notifyResults(false);
    }

    // Clear our state
    delete this._originalSearchString;
    delete this._currentSearchString;
    delete this._strippedPrefix;
    delete this._searchTokens;
    delete this._listener;
    delete this._result;
    delete this._usedPlaces;
    delete this._pendingQuery;
    this._secondPass = false;
    this._enableActions = false;
  },

  /**
   * Executes the given queries asynchronously.
   *
   * @param aQueries
   *        The queries to execute.
   */
  _executeQueries: function PAC_executeQueries(aQueries)
  {
    // Because we might get a handleCompletion for canceled queries, we want to
    // filter out queries we no longer care about (described in the
    // handleCompletion implementation of AutoCompleteStatementCallbackWrapper).

    // Create our wrapper object and execute the queries.
    let wrapper = new AutoCompleteStatementCallbackWrapper(this, this._db);
    this._pendingQuery = wrapper.executeAsync(aQueries);
  },

  /**
   * Stops executing our active query.
   */
  _stopActiveQuery: function PAC_stopActiveQuery()
  {
    this._pendingQuery.cancel();
    delete this._pendingQuery;
  },

  /**
   * Notifies the listener about results.
   *
   * @param aSearchOngoing
   *        Indicates if the search is ongoing or not.
   */
  _notifyResults: function PAC_notifyResults(aSearchOngoing)
  {
    let result = this._result;
    let resultCode = result.matchCount ? "RESULT_SUCCESS" : "RESULT_NOMATCH";
    if (aSearchOngoing) {
      resultCode += "_ONGOING";
    }
    result.setSearchResult(Ci.nsIAutoCompleteResult[resultCode]);
    this._listener.onSearchResult(this, result);
    if (this._telemetryStartTime) {
      let elapsed = Date.now() - this._telemetryStartTime;
      if (elapsed > 50) {
        try {
          Services.telemetry
                  .getHistogramById("PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS")
                  .add(elapsed);
        } catch (ex) {
          Components.utils.reportError("Unable to report telemetry.");
        }
      }
      this._telemetryStartTime = null;
    }
  },

  /**
   * Loads the preferences that we care about.
   *
   * @param [optional] aRegisterObserver
   *        Indicates if the preference observer should be added or not.  The
   *        default value is false.
   */
  _loadPrefs: function PAC_loadPrefs(aRegisterObserver)
  {
    this._enabled = safePrefGetter(this._prefs, "autocomplete.enabled", true);
    this._matchBehavior = safePrefGetter(this._prefs,
                                         "matchBehavior",
                                         MATCH_BOUNDARY_ANYWHERE);
    this._filterJavaScript = safePrefGetter(this._prefs, "filter.javascript", true);
    this._maxRichResults = safePrefGetter(this._prefs, "maxRichResults", 25);
    this._restrictHistoryToken = safePrefGetter(this._prefs,
                                                "restrict.history", "^");
    this._restrictBookmarkToken = safePrefGetter(this._prefs,
                                                 "restrict.bookmark", "*");
    this._restrictTypedToken = safePrefGetter(this._prefs, "restrict.typed", "~");
    this._restrictTagToken = safePrefGetter(this._prefs, "restrict.tag", "+");
    this._restrictOpenPageToken = safePrefGetter(this._prefs,
                                                 "restrict.openpage", "%");
    this._matchTitleToken = safePrefGetter(this._prefs, "match.title", "#");
    this._matchURLToken = safePrefGetter(this._prefs, "match.url", "@");
    this._defaultBehavior = safePrefGetter(this._prefs, "default.behavior", 0);
    // Further restrictions to apply for "empty searches" (i.e. searches for "").
    this._emptySearchDefaultBehavior =
      this._defaultBehavior |
      safePrefGetter(this._prefs, "default.behavior.emptyRestriction",
                     Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY |
                     Ci.mozIPlacesAutoComplete.BEHAVIOR_TYPED);

    // Validate matchBehavior; default to MATCH_BOUNDARY_ANYWHERE.
    if (this._matchBehavior != MATCH_ANYWHERE &&
        this._matchBehavior != MATCH_BOUNDARY &&
        this._matchBehavior != MATCH_BEGINNING) {
      this._matchBehavior = MATCH_BOUNDARY_ANYWHERE;
    }
    // register observer
    if (aRegisterObserver) {
      this._prefs.addObserver("", this, false);
    }
  },

  /**
   * Given an array of tokens, this function determines which query should be
   * ran.  It also removes any special search tokens.
   *
   * @param aTokens
   *        An array of search tokens.
   * @return an object with two properties:
   *         query: the correctly optimized, bound query to search the database
   *                with.
   *         tokens: the filtered list of tokens to search with.
   */
  _getSearch: function PAC_getSearch(aTokens)
  {
    // Set the proper behavior so our call to _getBoundSearchQuery gives us the
    // correct query.
    for (let i = aTokens.length - 1; i >= 0; i--) {
      switch (aTokens[i]) {
        case this._restrictHistoryToken:
          this._setBehavior("history");
          break;
        case this._restrictBookmarkToken:
          this._setBehavior("bookmark");
          break;
        case this._restrictTagToken:
          this._setBehavior("tag");
          break;
        case this._restrictOpenPageToken:
          if (!this._enableActions) {
            continue;
          }
          this._setBehavior("openpage");
          break;
        case this._matchTitleToken:
          this._setBehavior("title");
          break;
        case this._matchURLToken:
          this._setBehavior("url");
          break;
        case this._restrictTypedToken:
          this._setBehavior("typed");
          break;
        default:
          // We do not want to remove the token if we did not match.
          continue;
      };

      aTokens.splice(i, 1);
    }

    // Set the right JavaScript behavior based on our preference.  Note that the
    // preference is whether or not we should filter JavaScript, and the
    // behavior is if we should search it or not.
    if (!this._filterJavaScript) {
      this._setBehavior("javascript");
    }

    return {
      query: this._getBoundSearchQuery(this._matchBehavior, aTokens),
      tokens: aTokens
    };
  },

  /**
   * Obtains the search query to be used based on the previously set search
   * behaviors (accessed by this._hasBehavior).  The query is bound and ready to
   * execute.
   *
   * @param aMatchBehavior
   *        How this query should match its tokens to the search string.
   * @param aTokens
   *        An array of search tokens.
   * @return the correctly optimized query to search the database with and the
   *         new list of tokens to search with.  The query has all the needed
   *         parameters bound, so consumers can execute it without doing any
   *         additional work.
   */
  _getBoundSearchQuery: function PAC_getBoundSearchQuery(aMatchBehavior,
                                                         aTokens)
  {
    // We use more optimized queries for restricted searches, so we will always
    // return the most restrictive one to the least restrictive one if more than
    // one token is found.
    // Note: "openpages" behavior is supported by the default query.
    //       _openPagesQuery instead returns only pages not supported by
    //       history and it is always executed.
    let query = this._hasBehavior("tag") ? this._tagsQuery :
                this._hasBehavior("bookmark") ? this._bookmarkQuery :
                this._hasBehavior("typed") ? this._typedQuery :
                this._hasBehavior("history") ? this._historyQuery :
                this._defaultQuery;

    // Bind the needed parameters to the query so consumers can use it.
    let (params = query.params) {
      params.parent = this._bs.tagsFolder;
      params.query_type = kQueryTypeFiltered;
      params.matchBehavior = aMatchBehavior;
      params.searchBehavior = this._behavior;

      // We only want to search the tokens that we are left with - not the
      // original search string.
      params.searchString = aTokens.join(" ");

      // Limit the query to the the maximum number of desired results.
      // This way we can avoid doing more work than needed.
      params.maxResults = this._maxRichResults;
    }

    return query;
  },

  _getBoundOpenPagesQuery: function PAC_getBoundOpenPagesQuery(aTokens)
  {
    let query = this._openPagesQuery;

    // Bind the needed parameters to the query so consumers can use it.
    let (params = query.params) {
      params.query_type = kQueryTypeFiltered;
      params.matchBehavior = this._matchBehavior;
      params.searchBehavior = this._behavior;
      // We only want to search the tokens that we are left with - not the
      // original search string.
      params.searchString = aTokens.join(" ");
      params.maxResults = this._maxRichResults;
    }

    return query;
  },

  /**
   * Obtains the keyword query with the properly bound parameters.
   *
   * @param aTokens
   *        The array of search tokens to check against.
   * @return the bound keyword query.
   */
  _getBoundKeywordQuery: function PAC_getBoundKeywordQuery(aTokens)
  {
    // The keyword is the first word in the search string, with the parameters
    // following it.
    let searchString = this._originalSearchString;
    let queryString = "";
    let queryIndex = searchString.indexOf(" ");
    if (queryIndex != -1) {
      queryString = searchString.substring(queryIndex + 1);
    }
    // We need to escape the parameters as if they were the query in a URL
    queryString = encodeURIComponent(queryString).replace("%20", "+", "g");

    // The first word could be a keyword, so that's what we'll search.
    let keyword = aTokens[0];

    let query = this._keywordQuery;
    let (params = query.params) {
      params.keyword = keyword;
      params.query_string = queryString;
      params.query_type = kQueryTypeKeyword;
    }

    return query;
  },

  /**
   * Obtains the adaptive query with the properly bound parameters.
   *
   * @return the bound adaptive query.
   */
  _getBoundAdaptiveQuery: function PAC_getBoundAdaptiveQuery(aMatchBehavior)
  {
    // If we were not given a match behavior, use the stored match behavior.
    if (arguments.length == 0) {
      aMatchBehavior = this._matchBehavior;
    }

    let query = this._adaptiveQuery;
    let (params = query.params) {
      params.parent = this._bs.tagsFolder;
      params.search_string = this._currentSearchString;
      params.query_type = kQueryTypeFiltered;
      params.matchBehavior = aMatchBehavior;
      params.searchBehavior = this._behavior;
    }

    return query;
  },

  /**
   * Processes a mozIStorageRow to generate the proper data for the AutoComplete
   * result.  This will add an entry to the current result if it matches the
   * criteria.
   *
   * @param aRow
   *        The row to process.
   * @return true if the row is accepted, and false if not.
   */
  _processRow: function PAC_processRow(aRow)
  {
    // Before we do any work, make sure this entry isn't already in our results.
    let entryId = aRow.getResultByIndex(kQueryIndexPlaceId);
    let escapedEntryURL = aRow.getResultByIndex(kQueryIndexURL);
    let openPageCount = aRow.getResultByIndex(kQueryIndexOpenPageCount) || 0;

    // If actions are enabled and the page is open, add only the switch-to-tab
    // result.  Otherwise, add the normal result.
    let [url, action] = this._enableActions && openPageCount > 0 ?
                        ["moz-action:switchtab," + escapedEntryURL, "action "] :
                        [escapedEntryURL, ""];

    if (this._inResults(entryId, url)) {
      return false;
    }

    let entryTitle = aRow.getResultByIndex(kQueryIndexTitle) || "";
    let entryFavicon = aRow.getResultByIndex(kQueryIndexFaviconURL) || "";
    let entryBookmarked = aRow.getResultByIndex(kQueryIndexBookmarked);
    let entryBookmarkTitle = entryBookmarked ?
      aRow.getResultByIndex(kQueryIndexBookmarkTitle) : null;
    let entryTags = aRow.getResultByIndex(kQueryIndexTags) || "";

    // Always prefer the bookmark title unless it is empty
    let title = entryBookmarkTitle || entryTitle;

    let style;
    if (aRow.getResultByIndex(kQueryIndexQueryType) == kQueryTypeKeyword) {
      // If we do not have a title, then we must have a keyword, so let the UI
      // know it is a keyword.  Otherwise, we found an exact page match, so just
      // show the page like a regular result.  Because the page title is likely
      // going to be more specific than the bookmark title (keyword title).
      if (!entryTitle) {
        style = "keyword";
      }
      else {
        title = entryTitle;
      }
    }

    // We will always prefer to show tags if we have them.
    let showTags = !!entryTags;

    // However, we'll act as if a page is not bookmarked or tagged if the user
    // only wants only history and not bookmarks or tags.
    if (this._hasBehavior("history") &&
        !(this._hasBehavior("bookmark") || this._hasBehavior("tag"))) {
      showTags = false;
      style = "favicon";
    }

    // If we have tags and should show them, we need to add them to the title.
    if (showTags) {
      title += kTitleTagsSeparator + entryTags;
    }
    // We have to determine the right style to display.  Tags show the tag icon,
    // bookmarks get the bookmark icon, and keywords get the keyword icon.  If
    // the result does not fall into any of those, it just gets the favicon.
    if (!style) {
      // It is possible that we already have a style set (from a keyword
      // search or because of the user's preferences), so only set it if we
      // haven't already done so.
      if (showTags) {
        style = "tag";
      }
      else if (entryBookmarked) {
        style = "bookmark";
      }
      else {
        style = "favicon";
      }
    }

    this._addToResults(entryId, url, title, entryFavicon, action + style);
    return true;
  },

  /**
   * Checks to see if the given place has already been added to the results.
   *
   * @param aPlaceId
   *        The place id to check for, may be null.
   * @param aUrl
   *        The url to check for.
   * @return true if the place has been added, false otherwise.
   *
   * @note Must check both the id and the url for a negative match, since
   *       autocomplete may run in the middle of a new page addition.  In such
   *       a case the switch-to-tab query would hash the page by url, then a
   *       next query, running after the page addition, would hash it by id.
   *       It's not possible to just rely on url though, since keywords
   *       dynamically modify the url to include their search string.
   */
  _inResults: function PAC_inResults(aPlaceId, aUrl)
  {
    if (aPlaceId && aPlaceId in this._usedPlaces) {
      return true;
    }
    return aUrl in this._usedPlaces;
  },

  /**
   * Adds a result to the AutoComplete results.  Also tracks that we've added
   * this place_id into the result set.
   *
   * @param aPlaceId
   *        The place_id of the item to be added to the result set.  This is
   *        used by _inResults.
   * @param aURISpec
   *        The URI spec for the entry.
   * @param aTitle
   *        The title to give the entry.
   * @param aFaviconSpec
   *        The favicon to give to the entry.
   * @param aStyle
   *        Indicates how the entry should be styled when displayed.
   */
  _addToResults: function PAC_addToResults(aPlaceId, aURISpec, aTitle,
                                           aFaviconSpec, aStyle)
  {
    // Add this to our internal tracker to ensure duplicates do not end up in
    // the result.  _usedPlaces is an Object that is being used as a set.
    // Not all entries have a place id, thus we fallback to the url for them.
    // We cannot use only the url since keywords entries are modified to
    // include the search string, and would be returned multiple times.  Ids
    // are faster too.
    this._usedPlaces[aPlaceId || aURISpec] = true;

    // Obtain the favicon for this URI.
    let favicon;
    if (aFaviconSpec) {
      let uri = this._ioService.newURI(aFaviconSpec, null, null);
      favicon = this._faviconService.getFaviconLinkForIcon(uri).spec;
    }
    favicon = favicon || this._faviconService.defaultFavicon.spec;

    this._result.appendMatch(aURISpec, aTitle, favicon, aStyle);
  },

  /**
   * Determines if the specified AutoComplete behavior is set.
   *
   * @param aType
   *        The behavior type to test for.
   * @return true if the behavior is set, false otherwise.
   */
  _hasBehavior: function PAC_hasBehavior(aType)
  {
    return (this._behavior &
            Ci.mozIPlacesAutoComplete["BEHAVIOR_" + aType.toUpperCase()]);
  },

  /**
   * Enables the desired AutoComplete behavior.
   *
   * @param aType
   *        The behavior type to set.
   */
  _setBehavior: function PAC_setBehavior(aType)
  {
    this._behavior |=
      Ci.mozIPlacesAutoComplete["BEHAVIOR_" + aType.toUpperCase()];
  },

  /**
   * Determines if we are done searching or not.
   *
   * @return true if we have completed searching, false otherwise.
   */
  isSearchComplete: function PAC_isSearchComplete()
  {
    // If _pendingQuery is null, we should no longer do any work since we have
    // already called _finishSearch.  This means we completed our search.
    return this._pendingQuery == null;
  },

  /**
   * Determines if the given handle of a pending statement is a pending search
   * or not.
   *
   * @param aHandle
   *        A mozIStoragePendingStatement to check and see if we are waiting for
   *        results from it still.
   * @return true if it is a pending query, false otherwise.
   */
  isPendingSearch: function PAC_isPendingSearch(aHandle)
  {
    return this._pendingQuery == aHandle;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  classID: Components.ID("d0272978-beab-4adc-a3d4-04b76acfa4e7"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(nsPlacesAutoComplete),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIAutoCompleteSearch,
    Ci.nsIAutoCompleteSimpleResultListener,
    Ci.mozIPlacesAutoComplete,
    Ci.mozIStorageStatementCallback,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ])
};

////////////////////////////////////////////////////////////////////////////////
//// urlInlineComplete class

function urlInlineComplete()
{
  this._loadPrefs(true);
  Services.obs.addObserver(this, kTopicShutdown, true);
}

urlInlineComplete.prototype = {

/////////////////////////////////////////////////////////////////////////////////
//// Database and query getters

  __db: null,

  get _db()
  {
    if (!this.__db && this._autofill) {
      this.__db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).
                  DBConnection.clone(true);
    }
    return this.__db;
  },

  __syncQuery: null,

  get _syncQuery()
  {
    if (!this.__syncQuery) {
      // Add a trailing slash at the end of the hostname, since we always
      // want to complete up to and including a URL separator.
      this.__syncQuery = this._db.createStatement(
          "/* do not warn (bug no): could index on (typed,frecency) but not worth it */ "
        + "SELECT host || '/' "
        + "FROM moz_hosts "
        + "WHERE host BETWEEN :search_string AND :search_string || X'FFFF' "
        + (this._autofillTyped ? "AND typed = 1 " : "")
        + "ORDER BY frecency DESC "
        + "LIMIT 1"
      );
    }
    return this.__syncQuery;
  },

  __asyncQuery: null,

  get _asyncQuery()
  {
    if (!this.__asyncQuery) {
      this.__asyncQuery = this._db.createAsyncStatement(
          "/* do not warn (bug no): can't use an index */ "
        + "SELECT h.url "
        + "FROM moz_places h "
        + "WHERE h.frecency <> 0 "
        + (this._autofillTyped ? "AND h.typed = 1 " : "")
        +   "AND AUTOCOMPLETE_MATCH(:searchString, h.url, "
        +                          "h.title, '', "
        +                          "h.visit_count, h.typed, 0, 0, "
        +                          ":matchBehavior, :searchBehavior) "
        + "ORDER BY h.frecency DESC, h.id DESC "
        + "LIMIT 1"
      );
    }
    return this.__asyncQuery;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSearch

  startSearch: function UIC_startSearch(aSearchString, aSearchParam,
                                        aPreviousResult, aListener)
  {
    // Stop the search in case the controller has not taken care of it.
    if (this._pendingQuery) {
      this.stopSearch();
    }

    // We want to store the original string with no leading or trailing
    // whitespace for case sensitive searches.
    this._originalSearchString = aSearchString;
    this._currentSearchString =
      fixupSearchText(this._originalSearchString.toLowerCase());
    // The protocol and the domain are lowercased by nsIURI, so it's fine to
    // lowercase the typed prefix to add it back to the results later.
    this._strippedPrefix = this._originalSearchString.slice(
      0, this._originalSearchString.length - this._currentSearchString.length
    ).toLowerCase();

    let result = Cc["@mozilla.org/autocomplete/simple-result;1"].
                 createInstance(Ci.nsIAutoCompleteSimpleResult);
    result.setSearchString(aSearchString);
    result.setTypeAheadResult(true);

    this._result = result;
    this._listener = aListener;

    // Don't autoFill if the search term is recognized as a keyword, otherwise
    // it will override default keywords behavior.  Note that keywords are
    // hashed on first use, so while the first query may delay a little bit,
    // next ones will just hit the memory hash.
    if (this._currentSearchString.length == 0 || !this._db ||
        PlacesUtils.bookmarks.getURIForKeyword(this._currentSearchString)) {
      this._finishSearch();
      return;
    }

    // Do a synchronous search on the table of domains.
    let query = this._syncQuery;
    query.params.search_string = this._currentSearchString.toLowerCase();

    // Domains have no "/" in them.
    let lastSlashIndex = this._currentSearchString.lastIndexOf("/");
    if (lastSlashIndex == -1) {
      var hasDomainResult = false;
      var domain;
      try {
        hasDomainResult = query.executeStep();
        if (hasDomainResult) {
          domain = query.getString(0);
        }
      } finally {
        query.reset();
      }

      if (hasDomainResult) {
        // We got a match for a domain, we can add it immediately.
        result.appendMatch(this._strippedPrefix + domain, "");

        this._finishSearch();
        return;
      }
    }

    // We did not get a result from the synchronous domain search.
    // We now do an asynchronous search through places, and complete
    // up to the next URL separator.

    // First, check if this is necessary.
    // We don't need to search if we have no "/" separator, or if it's at
    // the end of the search text.
    if (lastSlashIndex == -1 ||
        lastSlashIndex == this._currentSearchString.length - 1) {
      this._finishSearch();
      return;
    }

    // Within the standard autocomplete query, we only search the beginning
    // of URLs for 1 result.
    let query = this._asyncQuery;
    let (params = query.params) {
      params.matchBehavior = MATCH_BEGINNING;
      params.searchBehavior = Ci.mozIPlacesAutoComplete["BEHAVIOR_URL"];
      params.searchString = this._currentSearchString;
    }

    // Execute the async query
    let wrapper = new AutoCompleteStatementCallbackWrapper(this, this._db);
    this._pendingQuery = wrapper.executeAsync([query]);
  },

  stopSearch: function UIC_stopSearch()
  {
    delete this._originalSearchString;
    delete this._currentSearchString;
    delete this._result;
    delete this._listener;

    if (this._pendingQuery) {
      this._pendingQuery.cancel();
      delete this._pendingQuery;
    }
  },

  /**
   * Loads the preferences that we care about.
   *
   * @param [optional] aRegisterObserver
   *        Indicates if the preference observer should be added or not.  The
   *        default value is false.
   */
  _loadPrefs: function UIC_loadPrefs(aRegisterObserver)
  {
    let prefBranch = Services.prefs.getBranch(kBrowserUrlbarBranch);
    this._autofill = safePrefGetter(prefBranch,
                                    kBrowserUrlbarAutofillPref,
                                    true);
    this._autofillTyped = safePrefGetter(prefBranch,
                                         kBrowserUrlbarAutofillTypedPref,
                                         true);
    if (aRegisterObserver) {
      Services.prefs.addObserver(kBrowserUrlbarBranch, this, true);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSearchDescriptor
  get searchType() Ci.nsIAutoCompleteSearchDescriptor.SEARCH_TYPE_IMMEDIATE,

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  handleResult: function UIC_handleResult(aResultSet)
  {
    let row = aResultSet.getNextRow();
    let url = fixupSearchText(row.getResultByIndex(0));

    // We must complete the URL up to the next separator (which is /, ? or #).
    let separatorIndex = url.slice(this._currentSearchString.length)
                            .search(/[\/\?\#]/);
    if (separatorIndex != -1) {
      separatorIndex += this._currentSearchString.length;
      if (url[separatorIndex] == "/") {
        separatorIndex++; // Include the "/" separator
      }
      url = url.slice(0, separatorIndex);
    }

    // Add the result
    this._result.appendMatch(this._strippedPrefix + url, "");

    // handleCompletion() will cause the result listener to be called, and
    // will display the result in the UI.
  },

  handleError: function UIC_handleError(aError)
  {
    Components.utils.reportError("URL Inline Complete: An async statement encountered an " +
                                 "error: " + aError.result + ", '" + aError.message + "'");
  },

  handleCompletion: function UIC_handleCompletion(aReason)
  {
    this._finishSearch();
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function UIC_observe(aSubject, aTopic, aData)
  {
    if (aTopic == kTopicShutdown) {
      this._closeDatabase();
    }
    else if (aTopic == kPrefChanged &&
             (aData.substr(kBrowserUrlbarBranch.length) == kBrowserUrlbarAutofillPref ||
              aData.substr(kBrowserUrlbarBranch.length) == kBrowserUrlbarAutofillTypedPref)) {
      let previousAutofillTyped = this._autofillTyped;
      this._loadPrefs();
      if (!this._autofill) {
        this.stopSearch();
        this._closeDatabase();
      }
      else if (this._autofillTyped != previousAutofillTyped) {
        // Invalidate the statements to update them for the new typed status.
        this._invalidateStatements();
      }
    }
  },

  /**
   * Finalizes and invalidates cached statements.
   */
  _invalidateStatements: function UIC_invalidateStatements()
  {
    // Finalize the statements that we have used.
    let stmts = [
      "__syncQuery",
      "__asyncQuery",
    ];
    for (let i = 0; i < stmts.length; i++) {
      // We do not want to create any query we haven't already created, so
      // see if it is a getter first.
      if (this[stmts[i]]) {
        this[stmts[i]].finalize();
        this[stmts[i]] = null;
      }
    }
  },

  /**
   * Closes the database.
   */
  _closeDatabase: function UIC_closeDatabase()
  {
    this._invalidateStatements();
    if (this.__db) {
      this._db.asyncClose();
      this.__db = null;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// urlInlineComplete

  _finishSearch: function UIC_finishSearch()
  {
    // Notify the result object
    let result = this._result;

    if (result.matchCount) {
      result.setDefaultIndex(0);
      result.setSearchResult(Ci.nsIAutoCompleteResult["RESULT_SUCCESS"]);
    } else {
      result.setDefaultIndex(-1);
      result.setSearchResult(Ci.nsIAutoCompleteResult["RESULT_NOMATCH"]);
    }

    this._listener.onSearchResult(this, result);
    this.stopSearch();
  },

  isSearchComplete: function UIC_isSearchComplete()
  {
    return this._pendingQuery == null;
  },

  isPendingSearch: function UIC_isPendingSearch(aHandle)
  {
    return this._pendingQuery == aHandle;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  classID: Components.ID("c88fae2d-25cf-4338-a1f4-64a320ea7440"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(urlInlineComplete),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIAutoCompleteSearch,
    Ci.nsIAutoCompleteSearchDescriptor,
    Ci.mozIStorageStatementCallback,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ])
};

let components = [nsPlacesAutoComplete, urlInlineComplete];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
