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
 * Mozilla Corporation.
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

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

// This is just a helper for the next constant.
function book_tag_sql_fragment(aName, aColumn, aForTag)
{
  return ["(",
    "SELECT ", aColumn, " ",
    "FROM moz_bookmarks b ",
    "JOIN moz_bookmarks t ",
      "ON t.id = b.parent ",
      "AND t.parent ", (aForTag ? "" : "!"), "= :parent ",
    "WHERE b.type = ", Ci.nsINavBookmarksService.TYPE_BOOKMARK, " ",
      "AND b.fk = h.id ",
    (aForTag ? "AND LENGTH(t.title) > 0" :
               "ORDER BY b.lastModified DESC LIMIT 1"),
  ") AS ", aName].join("");
}

// This SQL query fragment provides the following:
//   - the parent folder for bookmarked entries (kQueryIndexParent)
//   - the bookmark title, if it is a bookmark (kQueryIndexBookmarkTitle)
//   - the tags associated with a bookmarked entry (kQueryIndexTags)
const kBookTagSQLFragment =
  book_tag_sql_fragment("parent", "b.parent", false) + ", " +
  book_tag_sql_fragment("bookmark", "b.title", false) + ", " +
  book_tag_sql_fragment("tags", "GROUP_CONCAT(t.title, ',')", true);

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
const kQueryIndexParentId = 3;
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

////////////////////////////////////////////////////////////////////////////////
//// Global Functions

/**
 * Generates the SQL subquery to get the best favicon for a given revhost.  This
 * is the favicon for the most recent visit.
 *
 * @param aTableName
 *        The table to join to the moz_favicons table with.  This must have a
 *        column called favicon_id.
 * @return the SQL subquery (in string form) to get the best favicon.
 */
function best_favicon_for_revhost(aTableName)
{
  return "(" +
    "SELECT f.url " +
    "FROM " + aTableName + " " +
    "JOIN moz_favicons f ON f.id = favicon_id " +
    "WHERE rev_host = IFNULL( " +
      "(SELECT rev_host FROM moz_places_temp WHERE id = b.fk), " +
      "(SELECT rev_host FROM moz_places WHERE id = b.fk) " +
    ") " +
    "ORDER BY frecency DESC " +
    "LIMIT 1 " +
  ")";
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
    if (!callback.isSearchComplete() && callback.isPendingSearch(this._handle))
      callback.handleCompletion.apply(callback, arguments);
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

  // Define common pieces of various queries.
  // TODO bug 412736 in case of a frecency tie, break it with h.typed and
  // h.visit_count which is better than nothing.  This is slow, so not doing it
  // yet...
  // Note: h.frecency is only selected because we need it for ordering.
  function sql_base_fragment(aTableName) {
    return "SELECT h.url, h.title, f.url, " + kBookTagSQLFragment + ", " +
                  "h.visit_count, h.typed, h.id, :query_type, t.open_count, " +
                  "h.frecency " +
           "FROM " + aTableName + " h " +
           "LEFT OUTER JOIN moz_favicons f ON f.id = h.favicon_id " +
           "LEFT OUTER JOIN moz_openpages_temp t ON t.url = h.url " +
           "WHERE h.frecency <> 0 " +
           "AND AUTOCOMPLETE_MATCH(:searchString, h.url, " +
                                  "IFNULL(bookmark, h.title), tags, " +
                                  "h.visit_count, h.typed, parent, " +
                                  "t.open_count, " +
                                  ":matchBehavior, :searchBehavior) " +
          "{ADDITIONAL_CONDITIONS} ";
  }
  const SQL_BASE = sql_base_fragment("moz_places_temp") +
                   "UNION ALL " +
                   sql_base_fragment("moz_places") +
                   "AND +h.id NOT IN (SELECT id FROM moz_places_temp) " +
                   "ORDER BY h.frecency DESC, h.id DESC " +
                   "LIMIT :maxResults";

  //////////////////////////////////////////////////////////////////////////////
  //// Smart Getters

  XPCOMUtils.defineLazyGetter(this, "_db", function() {
    return Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsPIPlacesDatabase).
           DBConnection;
  });

  XPCOMUtils.defineLazyServiceGetter(this, "_bh",
                                     "@mozilla.org/browser/global-history;2",
                                     "nsIBrowserHistory");

  XPCOMUtils.defineLazyServiceGetter(this, "_textURIService",
                                     "@mozilla.org/intl/texttosuburi;1",
                                     "nsITextToSubURI");

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
    return this._db.createStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_historyQuery", function() {
    let replacementText = "AND h.visit_count > 0";
    return this._db.createStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_bookmarkQuery", function() {
    let replacementText = "AND bookmark IS NOT NULL";
    return this._db.createStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_tagsQuery", function() {
    let replacementText = "AND tags IS NOT NULL";
    return this._db.createStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_openPagesQuery", function() {
    return this._db.createStatement(
      "/* do not warn (bug 487789) */ " +
      "SELECT t.url, " +
             "COALESCE(h_t.title, h.title, t.url) AS c_title, f.url, " +
              kBookTagSQLFragment + ", " +
              "IFNULL(h_t.visit_count, h.visit_count) AS c_visit_count, " +
              "IFNULL(h_t.typed, h.typed) AS c_typed, " +
              "IFNULL(h_t.id, h.id), :query_type, t.open_count, " +
              "IFNULL(h_t.frecency, h.frecency) AS c_frecency " +
      "FROM moz_openpages_temp t " +
      "LEFT JOIN moz_places_temp h_t ON h_t.url = t.url " +
      "LEFT JOIN moz_places h ON h.url = t.url " +
      "LEFT JOIN moz_favicons f ON f.id = IFNULL(h_t.favicon_id, h.favicon_id) " + 
      "WHERE t.open_count > 0 " +
        "AND AUTOCOMPLETE_MATCH(:searchString, t.url, " +
                               "COALESCE(bookmark, c_title, t.url), tags, " +
                               "c_visit_count, c_typed, parent, " +
                               "t.open_count, " +
                               ":matchBehavior, :searchBehavior) " +
      "ORDER BY c_frecency DESC, t.ROWID DESC " +
      "LIMIT :maxResults"
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_typedQuery", function() {
    let replacementText = "AND h.typed = 1";
    return this._db.createStatement(
      SQL_BASE.replace("{ADDITIONAL_CONDITIONS}", replacementText, "g")
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_adaptiveQuery", function() {
    // In this query, we are taking kBookTagSQLFragment only for h.id because it
    // uses data from the moz_bookmarks table and we sync tables on bookmark
    // insert.  So, most likely, h.id will always be populated when we have any
    // bookmark.  We still need to join on moz_places_temp for other data (eg.
    // title).
    return this._db.createStatement(
      "/* do not warn (bug 487789) */ " +
      "SELECT IFNULL(h_t.url, h.url) AS c_url, " +
             "IFNULL(h_t.title, h.title) AS c_title, f.url, " +
              kBookTagSQLFragment + ", " +
              "IFNULL(h_t.visit_count, h.visit_count) AS c_visit_count, " +
              "IFNULL(h_t.typed, h.typed) AS c_typed, " +
              "IFNULL(h_t.id, h.id), :query_type, t.open_count, rank " +
      "FROM ( " +
        "SELECT ROUND(MAX(((i.input = :search_string) + " +
                          "(SUBSTR(i.input, 1, LENGTH(:search_string)) = :search_string)) * " +
                          "i.use_count), 1) AS rank, place_id " +
        "FROM moz_inputhistory i " +
        "GROUP BY i.place_id " +
        "HAVING rank > 0 " +
      ") AS i " +
      "LEFT JOIN moz_places h ON h.id = i.place_id " +
      "LEFT JOIN moz_places_temp h_t ON h_t.id = i.place_id " +
      "LEFT JOIN moz_favicons f ON f.id = IFNULL(h_t.favicon_id, h.favicon_id) " + 
      "LEFT JOIN moz_openpages_temp t ON t.url = c_url " +
      "WHERE c_url NOTNULL " +
      "AND AUTOCOMPLETE_MATCH(:searchString, c_url, " +
                             "IFNULL(bookmark, c_title), tags, " +
                             "c_visit_count, c_typed, parent, " +
                             "t.open_count, " +
                             ":matchBehavior, :searchBehavior) " +
      "ORDER BY rank DESC, IFNULL(h_t.frecency, h.frecency) DESC"
    );
  });

  XPCOMUtils.defineLazyGetter(this, "_keywordQuery", function() {
    return this._db.createStatement(
      "/* do not warn (bug 487787) */ " +
      "SELECT IFNULL( " +
          "(SELECT REPLACE(url, '%s', :query_string) FROM moz_places_temp WHERE id = b.fk), " +
          "(SELECT REPLACE(url, '%s', :query_string) FROM moz_places WHERE id = b.fk) " +
        ") AS search_url, IFNULL(h_t.title, h.title), " +
        "COALESCE(f.url, " + best_favicon_for_revhost("moz_places_temp") + "," +
                  best_favicon_for_revhost("moz_places") + "), b.parent, " +
        "b.title, NULL, IFNULL(h_t.visit_count, h.visit_count), " +
        "IFNULL(h_t.typed, h.typed), COALESCE(h_t.id, h.id, b.fk), " +
        ":query_type, t.open_count " +
      "FROM moz_keywords k " +
      "JOIN moz_bookmarks b ON b.keyword_id = k.id " +
      "LEFT JOIN moz_places AS h ON h.url = search_url " +
      "LEFT JOIN moz_places_temp AS h_t ON h_t.url = search_url " +
      "LEFT JOIN moz_favicons f ON f.id = IFNULL(h_t.favicon_id, h.favicon_id) " +
      "LEFT JOIN moz_openpages_temp t ON t.url = search_url " +
      "WHERE LOWER(k.keyword) = LOWER(:keyword) " +
      "ORDER BY IFNULL(h_t.frecency, h.frecency) DESC"
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
    // If a previous query is running and the controller has not taken care
    // of stopping it, kill it.
    if ("_pendingQuery" in this)
      this.stopSearch();

    // Note: We don't use aPreviousResult to make sure ordering of results are
    //       consistent.  See bug 412730 for more details.

    // We want to store the original string with no leading or trailing
    // whitespace for case sensitive searches.
    this._originalSearchString = aSearchString.trim();

    this._currentSearchString =
      this._fixupSearchText(this._originalSearchString.toLowerCase());

    var searchParamParts = aSearchParam.split(" ");
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
    if (this._currentSearchString)
      this._behavior = this._defaultBehavior;
    else
      this._behavior = this._emptySearchDefaultBehavior;

    // For any given search, we run up to four queries:
    // 1) keywords (this._keywordQuery)
    // 2) adaptive learning (this._adaptiveQuery)
    // 3) openPages (this._openPagesQuery)
    // 4) query from this._getSearch
    // (1) only gets ran if we get any filtered tokens from this._getSearch,
    // since if there are no tokens, there is nothing to match, so there is no
    // reason to run the query).
    let {query, tokens} =
      this._getSearch(this._getUnfilteredSearchTokens(this._currentSearchString));
    let queries = tokens.length ?
      [this._getBoundKeywordQuery(tokens), this._getBoundAdaptiveQuery(), this._getBoundOpenPagesQuery(), query] :
      [this._getBoundAdaptiveQuery(), this._getBoundOpenPagesQuery(), query];

    // Start executing our queries.
    this._executeQueries(queries);

    // Set up our persistent state for the duration of the search.
    this._searchTokens = tokens;
    this._usedPlaceIds = {};
  },

  stopSearch: function PAC_stopSearch()
  {
    // We need to cancel our searches so we do not get any [more] results.
    // However, it's possible we haven't actually started any searches, so this
    // method may throw because this._pendingQuery may be undefined.
    if (this._pendingQuery)
      this._stopActiveQuery();

    this._finishSearch(false);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSimpleResultListener

  onValueRemoved: function PAC_onValueRemoved(aResult, aURISpec, aRemoveFromDB)
  {
    if (aRemoveFromDB)
      this._bh.removePage(this._ioService.newURI(aURISpec, null, null));
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
    if (haveMatches)
      this._notifyResults(true);
  },

  handleError: function PAC_handleError(aError)
  {
    Components.utils.reportError("Places AutoComplete: An async statement encountered an " +
                                 "error: " + aError.result + ", '" + aError.message + "'");
  },

  handleCompletion: function PAC_handleCompletion(aReason)
  {
    // If we have already finished our search, we should bail out early.
    if (this.isSearchComplete())
      return;

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
      this._prefs.QueryInterface(Ci.nsIPrefBranch2).removeObserver("", this);
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
      ];
      for (let i = 0; i < stmts.length; i++) {
        // We do not want to create any query we haven't already created, so
        // see if it is a getter first.  __lookupGetter__ returns null if it is
        // actually a statement.
        if (!this.__lookupGetter__(stmts[i]))
          this[stmts[i]].finalize();
      }
    }
    else if (aTopic == kPrefChanged) {
      this._loadPrefs();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsPlacesAutoComplete

  /**
   * Used to unescape encoded URI strings, and drop information that we do not
   * care about for searching.
   *
   * @param aURIString
   *        The text to unescape and modify.
   * @return the modified uri.
   */
  _fixupSearchText: function PAC_fixupSearchText(aURIString)
  {
    let uri = aURIString;

    if (uri.indexOf("http://") == 0)
      uri = uri.slice(7);
    else if (uri.indexOf("https://") == 0)
      uri = uri.slice(8);
    else if (uri.indexOf("ftp://") == 0)
      uri = uri.slice(6);

    if (uri.indexOf("www.") == 0)
      uri = uri.slice(4);

    return this._textURIService.unEscapeURIForUI("UTF-8", uri);
  },

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
    if (aNotify)
      this._notifyResults(false);

    // Clear our state
    delete this._originalSearchString;
    delete this._currentSearchString;
    delete this._searchTokens;
    delete this._listener;
    delete this._result;
    delete this._usedPlaceIds;
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
    if (aSearchOngoing)
      resultCode += "_ONGOING";
    result.setSearchResult(Ci.nsIAutoCompleteResult[resultCode]);
    result.setDefaultIndex(result.matchCount ? 0 : -1);
    this._listener.onSearchResult(this, result);
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
    let self = this;
    function safeGetter(aName, aDefault) {
      let types = {
        boolean: "Bool",
        number: "Int",
        string: "Char"
      };
      let type = types[typeof(aDefault)];
      if (!type)
        throw "Unknown type!";

      // If the pref isn't set, we want to use the default.
      try {
        return self._prefs["get" + type + "Pref"](aName);
      }
      catch (e) {
        return aDefault;
      }
    }

    this._enabled = safeGetter("autocomplete.enabled", true);
    this._matchBehavior = safeGetter("matchBehavior", MATCH_BOUNDARY_ANYWHERE);
    this._filterJavaScript = safeGetter("filter.javascript", true);
    this._maxRichResults = safeGetter("maxRichResults", 25);
    this._restrictHistoryToken = safeGetter("restrict.history", "^");
    this._restrictBookmarkToken = safeGetter("restrict.bookmark", "*");
    this._restrictTypedToken = safeGetter("restrict.typed", "~");
    this._restrictTagToken = safeGetter("restrict.tag", "+");
    this._restrictOpenPageToken = safeGetter("restrict.openpage", "%");
    this._matchTitleToken = safeGetter("match.title", "#");
    this._matchURLToken = safeGetter("match.url", "@");
    this._defaultBehavior = safeGetter("default.behavior", 0);
    // Further restrictions to apply for "empty searches" (i.e. searches for "").
    this._emptySearchDefaultBehavior =
      this._defaultBehavior |
      safeGetter("default.behavior.emptyRestriction",
                 Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY |
                 Ci.mozIPlacesAutoComplete.BEHAVIOR_TYPED);

    // Validate matchBehavior; default to MATCH_BOUNDARY_ANYWHERE.
    if (this._matchBehavior != MATCH_ANYWHERE &&
        this._matchBehavior != MATCH_BOUNDARY &&
        this._matchBehavior != MATCH_BEGINNING)
      this._matchBehavior = MATCH_BOUNDARY_ANYWHERE;

    // register observer
    if (aRegisterObserver) {
      let pb = this._prefs.QueryInterface(Ci.nsIPrefBranch2);
      pb.addObserver("", this, false);
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
          if (!this._enableActions)
            continue;
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
    if (!this._filterJavaScript)
      this._setBehavior("javascript");

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
    let query = this._hasBehavior("tag") ? this._tagsQuery :
                this._hasBehavior("bookmark") ? this._bookmarkQuery :
                this._hasBehavior("typed") ? this._typedQuery :
                this._hasBehavior("history") ? this._historyQuery :
                this._hasBehavior("openpage") ? this._openPagesQuery :
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

  _getBoundOpenPagesQuery: function PAC_getBoundOpenPagesQuery()
  {
    let query = this._openPagesQuery;

    // Bind the needed parameters to the query so consumers can use it.
    let (params = query.params) {
      params.parent = this._bs.tagsFolder;
      params.query_type = kQueryTypeFiltered;
      params.matchBehavior = this._matchBehavior;
      params.searchBehavior = this._behavior;
      params.searchString = this._currentSearchString;
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
    if (queryIndex != -1)
      queryString = searchString.substring(queryIndex + 1);

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
    if (arguments.length == 0)
      aMatchBehavior = this._matchBehavior;

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
    if (this._inResults(entryId))
      return false;

    let escapedEntryURL = aRow.getResultByIndex(kQueryIndexURL);
    let entryTitle = aRow.getResultByIndex(kQueryIndexTitle) || "";
    let entryFavicon = aRow.getResultByIndex(kQueryIndexFaviconURL) || "";
    let entryParentId = aRow.getResultByIndex(kQueryIndexParentId);
    let entryBookmarkTitle = entryParentId ?
      aRow.getResultByIndex(kQueryIndexBookmarkTitle) : null;
    let entryTags = aRow.getResultByIndex(kQueryIndexTags) || "";
    let openPageCount = aRow.getResultByIndex(kQueryIndexOpenPageCount) || 0;

    // Always prefer the bookmark title unless it is empty
    let title = entryBookmarkTitle || entryTitle;

    let style;
    if (aRow.getResultByIndex(kQueryIndexQueryType) == kQueryTypeKeyword) {
      // If we do not have a title, then we must have a keyword, so let the UI
      // know it is a keyword.  Otherwise, we found an exact page match, so just
      // show the page like a regular result.  Because the page title is likely
      // going to be more specific than the bookmark title (keyword title).
      if (!entryTitle)
        style = "keyword";
      else
        title = entryTitle;
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
    if (showTags)
      title += kTitleTagsSeparator + entryTags;

    // We have to determine the right style to display.  Tags show the tag icon,
    // bookmarks get the bookmark icon, and keywords get the keyword icon.  If
    // the result does not fall into any of those, it just gets the favicon.
    if (!style) {
      // It is possible that we already have a style set (from a keyword
      // search or because of the user's preferences), so only set it if we
      // haven't already done so.
      if (showTags)
        style = "tag";
      else if (entryParentId)
        style = "bookmark";
      else
        style = "favicon";
    }

    // If actions are enabled and the page is open, add only the switch-to-tab
    // result.  Otherwise, add the normal result.
    let [url, action] = this._enableActions && openPageCount > 0 ?
                        ["moz-action:switchtab," + escapedEntryURL, "action "] :
                        [escapedEntryURL, ""];
    this._addToResults(entryId, url, title, entryFavicon, action + style);
    return true;
  },

  /**
   * Checks to see if the given place has already been added to the results.
   *
   * @param aPlaceId
   *        The place_id to check for.
   * @return true if the place has been added, false otherwise.
   */
  _inResults: function PAC_inResults(aPlaceId)
  {
    return (aPlaceId in this._usedPlaceIds);
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
    // the result.  _usedPlaceIds is an Object that is being used as a set.
    this._usedPlaceIds[aPlaceId] = true;

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

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIAutoCompleteSearch,
    Ci.nsIAutoCompleteSimpleResultListener,
    Ci.mozIStorageStatementCallback,
    Ci.nsIObserver,
  ])
};

let components = [nsPlacesAutoComplete];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
