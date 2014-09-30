/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Constants

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

const TOPIC_SHUTDOWN = "places-shutdown";
const TOPIC_PREFCHANGED = "nsPref:changed";

const DEFAULT_BEHAVIOR = 0;

const PREF_BRANCH = "browser.urlbar.";

// Prefs are defined as [pref name, default value].
const PREF_ENABLED =                [ "autocomplete.enabled",   true ];
const PREF_AUTOFILL =               [ "autoFill",               true ];
const PREF_AUTOFILL_TYPED =         [ "autoFill.typed",         true ];
const PREF_AUTOFILL_SEARCHENGINES = [ "autoFill.searchEngines", true ];
const PREF_DELAY =                  [ "delay",                  50 ];
const PREF_BEHAVIOR =               [ "matchBehavior", MATCH_BOUNDARY_ANYWHERE ];
const PREF_DEFAULT_BEHAVIOR =       [ "default.behavior", DEFAULT_BEHAVIOR ];
const PREF_EMPTY_BEHAVIOR =         [ "default.behavior.emptyRestriction",
                                      Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY |
                                      Ci.mozIPlacesAutoComplete.BEHAVIOR_TYPED ];
const PREF_FILTER_JS =              [ "filter.javascript",      true ];
const PREF_MAXRESULTS =             [ "maxRichResults",         25 ];
const PREF_RESTRICT_HISTORY =       [ "restrict.history",       "^" ];
const PREF_RESTRICT_BOOKMARKS =     [ "restrict.bookmark",      "*" ];
const PREF_RESTRICT_TYPED =         [ "restrict.typed",         "~" ];
const PREF_RESTRICT_TAG =           [ "restrict.tag",           "+" ];
const PREF_RESTRICT_SWITCHTAB =     [ "restrict.openpage",      "%" ];
const PREF_MATCH_TITLE =            [ "match.title",            "#" ];
const PREF_MATCH_URL =              [ "match.url",              "@" ];

// Match type constants.
// These indicate what type of search function we should be using.
const MATCH_ANYWHERE = Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE;
const MATCH_BOUNDARY_ANYWHERE = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY_ANYWHERE;
const MATCH_BOUNDARY = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY;
const MATCH_BEGINNING = Ci.mozIPlacesAutoComplete.MATCH_BEGINNING;
const MATCH_BEGINNING_CASE_SENSITIVE = Ci.mozIPlacesAutoComplete.MATCH_BEGINNING_CASE_SENSITIVE;

// AutoComplete query type constants.
// Describes the various types of queries that we can process rows for.
const QUERYTYPE_KEYWORD       = 0;
const QUERYTYPE_FILTERED      = 1;
const QUERYTYPE_AUTOFILL_HOST = 2;
const QUERYTYPE_AUTOFILL_URL  = 3;
const QUERYTYPE_AUTOFILL_PREDICTURL  = 4;

// This separator is used as an RTL-friendly way to split the title and tags.
// It can also be used by an nsIAutoCompleteResult consumer to re-split the
// "comment" back into the title and the tag.
const TITLE_TAGS_SEPARATOR = " \u2013 ";

// This separator identifies the search engine name in the title.
const TITLE_SEARCH_ENGINE_SEPARATOR = " \u00B7\u2013\u00B7 ";

// Telemetry probes.
const TELEMETRY_1ST_RESULT = "PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS";
const TELEMETRY_6_FIRST_RESULTS = "PLACES_AUTOCOMPLETE_6_FIRST_RESULTS_TIME_MS";
// The default frecency value used when inserting matches with unknown frecency.
const FRECENCY_SEARCHENGINES_DEFAULT = 1000;

// Sqlite result row index constants.
const QUERYINDEX_QUERYTYPE     = 0;
const QUERYINDEX_URL           = 1;
const QUERYINDEX_TITLE         = 2;
const QUERYINDEX_ICONURL       = 3;
const QUERYINDEX_BOOKMARKED    = 4;
const QUERYINDEX_BOOKMARKTITLE = 5;
const QUERYINDEX_TAGS          = 6;
const QUERYINDEX_VISITCOUNT    = 7;
const QUERYINDEX_TYPED         = 8;
const QUERYINDEX_PLACEID       = 9;
const QUERYINDEX_SWITCHTAB     = 10;
const QUERYINDEX_FRECENCY      = 11;

// This SQL query fragment provides the following:
//   - whether the entry is bookmarked (QUERYINDEX_BOOKMARKED)
//   - the bookmark title, if it is a bookmark (QUERYINDEX_BOOKMARKTITLE)
//   - the tags associated with a bookmarked entry (QUERYINDEX_TAGS)
const SQL_BOOKMARK_TAGS_FRAGMENT =
  `EXISTS(SELECT 1 FROM moz_bookmarks WHERE fk = h.id) AS bookmarked,
   ( SELECT title FROM moz_bookmarks WHERE fk = h.id AND title NOTNULL
     ORDER BY lastModified DESC LIMIT 1
   ) AS btitle,
   ( SELECT GROUP_CONCAT(t.title, ', ')
     FROM moz_bookmarks b
     JOIN moz_bookmarks t ON t.id = +b.parent AND t.parent = :parent
     WHERE b.fk = h.id
   ) AS tags`;

// TODO bug 412736: in case of a frecency tie, we might break it with h.typed
// and h.visit_count.  That is slower though, so not doing it yet...
function defaultQuery(conditions = "") {
  let query =
    `SELECT :query_type, h.url, h.title, f.url, ${SQL_BOOKMARK_TAGS_FRAGMENT},
            h.visit_count, h.typed, h.id, t.open_count, h.frecency
     FROM moz_places h
     LEFT JOIN moz_favicons f ON f.id = h.favicon_id
     LEFT JOIN moz_openpages_temp t ON t.url = h.url
     WHERE h.frecency <> 0
       AND AUTOCOMPLETE_MATCH(:searchString, h.url,
                              IFNULL(btitle, h.title), tags,
                              h.visit_count, h.typed,
                              bookmarked, t.open_count,
                              :matchBehavior, :searchBehavior)
       ${conditions}
     ORDER BY h.frecency DESC, h.id DESC
     LIMIT :maxResults`;
  return query;
}

const SQL_DEFAULT_QUERY = defaultQuery();

// Enforce ignoring the visit_count index, since the frecency one is much
// faster in this case.  ANALYZE helps the query planner to figure out the
// faster path, but it may not have up-to-date information yet.
const SQL_HISTORY_QUERY = defaultQuery("AND +h.visit_count > 0");

const SQL_BOOKMARK_QUERY = defaultQuery("AND bookmarked");

const SQL_TAGS_QUERY = defaultQuery("AND tags NOTNULL");

const SQL_TYPED_QUERY = defaultQuery("AND h.typed = 1");

const SQL_SWITCHTAB_QUERY =
  `SELECT :query_type, t.url, t.url, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          t.open_count, NULL
   FROM moz_openpages_temp t
   LEFT JOIN moz_places h ON h.url = t.url
   WHERE h.id IS NULL
     AND AUTOCOMPLETE_MATCH(:searchString, t.url, t.url, NULL,
                            NULL, NULL, NULL, t.open_count,
                            :matchBehavior, :searchBehavior)
   ORDER BY t.ROWID DESC
   LIMIT :maxResults`;

const SQL_ADAPTIVE_QUERY =
  `/* do not warn (bug 487789) */
   SELECT :query_type, h.url, h.title, f.url, ${SQL_BOOKMARK_TAGS_FRAGMENT},
          h.visit_count, h.typed, h.id, t.open_count, h.frecency
   FROM (
     SELECT ROUND(MAX(use_count) * (1 + (input = :search_string)), 1) AS rank,
            place_id
     FROM moz_inputhistory
     WHERE input BETWEEN :search_string AND :search_string || X'FFFF'
     GROUP BY place_id
   ) AS i
   JOIN moz_places h ON h.id = i.place_id
   LEFT JOIN moz_favicons f ON f.id = h.favicon_id
   LEFT JOIN moz_openpages_temp t ON t.url = h.url
   WHERE AUTOCOMPLETE_MATCH(NULL, h.url,
                            IFNULL(btitle, h.title), tags,
                            h.visit_count, h.typed, bookmarked,
                            t.open_count,
                            :matchBehavior, :searchBehavior)
   ORDER BY rank DESC, h.frecency DESC`;

const SQL_KEYWORD_QUERY =
  `/* do not warn (bug 487787) */
   SELECT :query_type,
     (SELECT REPLACE(url, '%s', :query_string) FROM moz_places WHERE id = b.fk)
     AS search_url, h.title,
     IFNULL(f.url, (SELECT f.url
                    FROM moz_places
                    JOIN moz_favicons f ON f.id = favicon_id
                    WHERE rev_host = (SELECT rev_host FROM moz_places WHERE id = b.fk)
                    ORDER BY frecency DESC
                    LIMIT 1)
           ),
     1, b.title, NULL, h.visit_count, h.typed, IFNULL(h.id, b.fk),
     t.open_count, h.frecency
   FROM moz_keywords k
   JOIN moz_bookmarks b ON b.keyword_id = k.id
   LEFT JOIN moz_places h ON h.url = search_url
   LEFT JOIN moz_favicons f ON f.id = h.favicon_id
   LEFT JOIN moz_openpages_temp t ON t.url = search_url
   WHERE LOWER(k.keyword) = LOWER(:keyword)
   ORDER BY h.frecency DESC`;

function hostQuery(conditions = "") {
  let query =
    `/* do not warn (bug NA): not worth to index on (typed, frecency) */
     SELECT :query_type, host || '/', IFNULL(prefix, '') || host || '/',
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, frecency
     FROM moz_hosts
     WHERE host BETWEEN :searchString AND :searchString || X'FFFF'
     AND frecency <> 0
     ${conditions}
     ORDER BY frecency DESC
     LIMIT 1`;
  return query;
}

const SQL_HOST_QUERY = hostQuery();

const SQL_TYPED_HOST_QUERY = hostQuery("AND typed = 1");

function bookmarkedHostQuery(conditions = "") {
  let query =
    `/* do not warn (bug NA): not worth to index on (typed, frecency) */
     SELECT :query_type, host || '/', IFNULL(prefix, '') || host || '/',
            NULL, (
              SELECT foreign_count > 0 FROM moz_places
              WHERE rev_host = get_unreversed_host(host || '.') || '.'
                 OR rev_host = get_unreversed_host(host || '.') || '.www.'
            ) AS bookmarked, NULL, NULL, NULL, NULL, NULL, NULL, frecency
     FROM moz_hosts
     WHERE host BETWEEN :searchString AND :searchString || X'FFFF'
     AND bookmarked
     AND frecency <> 0
     ${conditions}
     ORDER BY frecency DESC
     LIMIT 1`;
  return query;
}

const SQL_BOOKMARKED_HOST_QUERY = bookmarkedHostQuery();

const SQL_BOOKMARKED_TYPED_HOST_QUERY = bookmarkedHostQuery("AND typed = 1");

function urlQuery(conditions = "") {
  let query =
    `/* do not warn (bug no): cannot use an index */
     SELECT :query_type, h.url, NULL,
            NULL, foreign_count > 0 AS bookmarked, NULL, NULL, NULL, NULL, NULL, NULL, h.frecency
     FROM moz_places h
     WHERE h.frecency <> 0
     ${conditions}
     AND AUTOCOMPLETE_MATCH(:searchString, h.url,
     h.title, '',
     h.visit_count, h.typed, 0, 0,
     :matchBehavior, :searchBehavior)
     ORDER BY h.frecency DESC, h.id DESC
     LIMIT 1`;
  return query;
}

const SQL_URL_QUERY = urlQuery();

const SQL_TYPED_URL_QUERY = urlQuery("AND typed = 1");

// TODO (bug 1045924): use foreign_count once available.
const SQL_BOOKMARKED_URL_QUERY = urlQuery("AND bookmarked");

const SQL_BOOKMARKED_TYPED_URL_QUERY = urlQuery("AND bookmarked AND typed = 1");

////////////////////////////////////////////////////////////////////////////////
//// Getters

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
                                  "resource://gre/modules/TelemetryStopwatch.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesSearchAutocompleteProvider",
                                  "resource://gre/modules/PlacesSearchAutocompleteProvider.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "textURIService",
                                   "@mozilla.org/intl/texttosuburi;1",
                                   "nsITextToSubURI");

/**
 * Storage object for switch-to-tab entries.
 * This takes care of caching and registering open pages, that will be reused
 * by switch-to-tab queries.  It has an internal cache, so that the Sqlite
 * store is lazy initialized only on first use.
 * It has a simple API:
 *   initDatabase(conn): initializes the temporary Sqlite entities to store data
 *   add(uri): adds a given nsIURI to the store
 *   delete(uri): removes a given nsIURI from the store
 *   shutdown(): stops storing data to Sqlite
 */
XPCOMUtils.defineLazyGetter(this, "SwitchToTabStorage", () => Object.seal({
  _conn: null,
  // Temporary queue used while the database connection is not available.
  _queue: new Set(),
  initDatabase: Task.async(function* (conn) {
    // To reduce IO use an in-memory table for switch-to-tab tracking.
    // Note: this should be kept up-to-date with the definition in
    //       nsPlacesTables.h.
    yield conn.execute(
      `CREATE TEMP TABLE moz_openpages_temp (
         url TEXT PRIMARY KEY,
         open_count INTEGER
       )`);

    // Note: this should be kept up-to-date with the definition in
    //       nsPlacesTriggers.h.
    yield conn.execute(
      `CREATE TEMPORARY TRIGGER moz_openpages_temp_afterupdate_trigger
       AFTER UPDATE OF open_count ON moz_openpages_temp FOR EACH ROW
       WHEN NEW.open_count = 0
       BEGIN
         DELETE FROM moz_openpages_temp
         WHERE url = NEW.url;
       END`);

    this._conn = conn;

    // Populate the table with the current cache contents...
    this._queue.forEach(this.add, this);
    // ...then clear it to avoid double additions.
    this._queue.clear();
  }),

  add: function (uri) {
    if (!this._conn) {
      this._queue.add(uri);
      return;
    }
    this._conn.executeCached(
      `INSERT OR REPLACE INTO moz_openpages_temp (url, open_count)
         VALUES ( :url, IFNULL( (SELECT open_count + 1
                                  FROM moz_openpages_temp
                                  WHERE url = :url),
                                  1
                              )
                )`
    , { url: uri.spec });
  },

  delete: function (uri) {
    if (!this._conn) {
      this._queue.delete(uri);
      return;
    }
    this._conn.executeCached(
      `UPDATE moz_openpages_temp
       SET open_count = open_count - 1
       WHERE url = :url`
    , { url: uri.spec });
  },

  shutdown: function () {
    this._conn = null;
    this._queue.clear();
  }
}));

/**
 * This helper keeps track of preferences and keeps their values up-to-date.
 */
XPCOMUtils.defineLazyGetter(this, "Prefs", () => {
  let prefs = new Preferences(PREF_BRANCH);

  function loadPrefs() {
    store.enabled = prefs.get(...PREF_ENABLED);
    store.autofill = prefs.get(...PREF_AUTOFILL);
    store.autofillTyped = prefs.get(...PREF_AUTOFILL_TYPED);
    store.autofillSearchEngines = prefs.get(...PREF_AUTOFILL_SEARCHENGINES);
    store.delay = prefs.get(...PREF_DELAY);
    store.matchBehavior = prefs.get(...PREF_BEHAVIOR);
    store.filterJavaScript = prefs.get(...PREF_FILTER_JS);
    store.maxRichResults = prefs.get(...PREF_MAXRESULTS);
    store.restrictHistoryToken = prefs.get(...PREF_RESTRICT_HISTORY);
    store.restrictBookmarkToken = prefs.get(...PREF_RESTRICT_BOOKMARKS);
    store.restrictTypedToken = prefs.get(...PREF_RESTRICT_TYPED);
    store.restrictTagToken = prefs.get(...PREF_RESTRICT_TAG);
    store.restrictOpenPageToken = prefs.get(...PREF_RESTRICT_SWITCHTAB);
    store.matchTitleToken = prefs.get(...PREF_MATCH_TITLE);
    store.matchURLToken = prefs.get(...PREF_MATCH_URL);
    store.defaultBehavior = prefs.get(...PREF_DEFAULT_BEHAVIOR);
    // Further restrictions to apply for "empty searches" (i.e. searches for "").
    store.emptySearchDefaultBehavior = store.defaultBehavior |
                                       prefs.get(...PREF_EMPTY_BEHAVIOR);

    // Validate matchBehavior; default to MATCH_BOUNDARY_ANYWHERE.
    if (store.matchBehavior != MATCH_ANYWHERE &&
        store.matchBehavior != MATCH_BOUNDARY &&
        store.matchBehavior != MATCH_BEGINNING) {
      store.matchBehavior = MATCH_BOUNDARY_ANYWHERE;
    }

    store.tokenToBehaviorMap = new Map([
      [ store.restrictHistoryToken, "history" ],
      [ store.restrictBookmarkToken, "bookmark" ],
      [ store.restrictTagToken, "tag" ],
      [ store.restrictOpenPageToken, "openpage" ],
      [ store.matchTitleToken, "title" ],
      [ store.matchURLToken, "url" ],
      [ store.restrictTypedToken, "typed" ]
    ]);
  }

  let store = {
    observe: function (subject, topic, data) {
      loadPrefs();
    },
    QueryInterface: XPCOMUtils.generateQI([ Ci.nsIObserver ])
  };
  loadPrefs();
  prefs.observe("", store);

  return Object.seal(store);
});

////////////////////////////////////////////////////////////////////////////////
//// Helper functions

/**
 * Used to unescape encoded URI strings and drop information that we do not
 * care about.
 *
 * @param spec
 *        The text to unescape and modify.
 * @return the modified spec.
 */
function fixupSearchText(spec)
  textURIService.unEscapeURIForUI("UTF-8", stripPrefix(spec));

/**
 * Generates the tokens used in searching from a given string.
 *
 * @param searchString
 *        The string to generate tokens from.
 * @return an array of tokens.
 * @note Calling split on an empty string will return an array containing one
 *       empty string.  We don't want that, as it'll break our logic, so return
 *       an empty array then.
 */
function getUnfilteredSearchTokens(searchString)
  searchString.length ? searchString.split(" ") : [];

/**
 * Strip prefixes from the URI that we don't care about for searching.
 *
 * @param spec
 *        The text to modify.
 * @return the modified spec.
 */
function stripPrefix(spec)
{
  ["http://", "https://", "ftp://"].some(scheme => {
    // Strip protocol if not directly followed by a space
    if (spec.startsWith(scheme) && spec[scheme.length] != " ") {
      spec = spec.slice(scheme.length);
      return true;
    }
    return false;
  });

  // Strip www. if not directly followed by a space
  if (spec.startsWith("www.") && spec[4] != " ") {
    spec = spec.slice(4);
  }
  return spec;
}

/**
 * Strip http and trailing separators from a spec.
 *
 * @param spec
 *        The text to modify.
 * @return the modified spec.
 */
function stripHttpAndTrim(spec) {
  if (spec.startsWith("http://")) {
    spec = spec.slice(7);
  }
  if (spec.endsWith("?")) {
    spec = spec.slice(0, -1);
  }
  if (spec.endsWith("/")) {
    spec = spec.slice(0, -1);
  }
  return spec;
}

/**
 * Make a moz-action: URL for a given action and set of parameters.
 *
 * @param action
 *        Name of the action
 * @param params
 *        Object, whose keys are parameter names and values are the
 *        corresponding parameter values.
 * @return String representation of the built moz-action: URL
 */
function makeActionURL(action, params) {
  let url = "moz-action:" + action + "," + JSON.stringify(params);
  // Make a nsIURI out of this to ensure it's encoded properly.
  return NetUtil.newURI(url).spec;
}


////////////////////////////////////////////////////////////////////////////////
//// Search Class
//// Manages a single instance of an autocomplete search.

function Search(searchString, searchParam, autocompleteListener,
                resultListener, autocompleteSearch) {
  // We want to store the original string for case sensitive searches.
  this._originalSearchString = searchString;
  this._trimmedOriginalSearchString = searchString.trim();
  this._searchString = fixupSearchText(this._trimmedOriginalSearchString.toLowerCase());

  this._matchBehavior = Prefs.matchBehavior;
  // Set the default behavior for this search.
  this._behavior = this._searchString ? Prefs.defaultBehavior
                                      : Prefs.emptySearchDefaultBehavior;
  this._enableActions = searchParam.split(" ").indexOf("enable-actions") != -1;

  this._searchTokens =
    this.filterTokens(getUnfilteredSearchTokens(this._searchString));
  // The protocol and the host are lowercased by nsIURI, so it's fine to
  // lowercase the typed prefix, to add it back to the results later.
  this._strippedPrefix = this._trimmedOriginalSearchString.slice(
    0, this._trimmedOriginalSearchString.length - this._searchString.length
  ).toLowerCase();
  // The URIs in the database are fixed-up, so we can match on a lowercased
  // host, but the path must be matched in a case sensitive way.
  let pathIndex =
    this._trimmedOriginalSearchString.indexOf("/", this._strippedPrefix.length);
  this._autofillUrlSearchString = fixupSearchText(
    this._trimmedOriginalSearchString.slice(0, pathIndex).toLowerCase() +
    this._trimmedOriginalSearchString.slice(pathIndex)
  );

  this._listener = autocompleteListener;
  this._autocompleteSearch = autocompleteSearch;

  // Create a new result to add eventual matches.  Note we need a result
  // regardless having matches.
  let result = Cc["@mozilla.org/autocomplete/simple-result;1"]
                 .createInstance(Ci.nsIAutoCompleteSimpleResult);
  result.setSearchString(searchString);
  result.setListener(resultListener);
  // Will be set later, if needed.
  result.setDefaultIndex(-1);
  this._result = result;

  // These are used to avoid adding duplicate entries to the results.
  this._usedURLs = new Set();
  this._usedPlaceIds = new Set();
}

Search.prototype = {
  /**
   * Enables the desired AutoComplete behavior.
   *
   * @param type
   *        The behavior type to set.
   */
  setBehavior: function (type) {
    this._behavior |=
      Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type.toUpperCase()];
  },

  /**
   * Determines if the specified AutoComplete behavior is set.
   *
   * @param aType
   *        The behavior type to test for.
   * @return true if the behavior is set, false otherwise.
   */
  hasBehavior: function (type) {
    return this._behavior &
           Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type.toUpperCase()];
  },

  /**
   * Used to delay the most complex queries, to save IO while the user is
   * typing.
   */
  _sleepDeferred: null,
  _sleep: function (aTimeMs) {
    // Reuse a single instance to try shaving off some usless work before
    // the first query.
    if (!this._sleepTimer)
      this._sleepTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._sleepDeferred = Promise.defer();
    this._sleepTimer.initWithCallback(() => this._sleepDeferred.resolve(),
                                      aTimeMs, Ci.nsITimer.TYPE_ONE_SHOT);
    return this._sleepDeferred.promise;
  },

  /**
   * Given an array of tokens, this function determines which query should be
   * ran.  It also removes any special search tokens.
   *
   * @param tokens
   *        An array of search tokens.
   * @return the filtered list of tokens to search with.
   */
  filterTokens: function (tokens) {
    // Set the proper behavior while filtering tokens.
    for (let i = tokens.length - 1; i >= 0; i--) {
      let behavior = Prefs.tokenToBehaviorMap.get(tokens[i]);
      // Don't remove the token if it didn't match, or if it's an action but
      // actions are not enabled.
      if (behavior && (behavior != "openpage" || this._enableActions)) {
        this.setBehavior(behavior);
        tokens.splice(i, 1);
      }
    }

    // Set the right JavaScript behavior based on our preference.  Note that the
    // preference is whether or not we should filter JavaScript, and the
    // behavior is if we should search it or not.
    if (!Prefs.filterJavaScript) {
      this.setBehavior("javascript");
    }

    return tokens;
  },

  /**
   * Cancels this search.
   * After invoking this method, we won't run any more searches or heuristics,
   * and no new matches may be added to the current result.
   */
  cancel: function () {
    if (this._sleepTimer)
      this._sleepTimer.cancel();
    if (this._sleepDeferred) {
      this._sleepDeferred.resolve();
      this._sleepDeferred = null;
    }
    this.pending = false;
  },

  /**
   * Whether this search is active.
   */
  pending: true,

  /**
   * Execute the search and populate results.
   * @param conn
   *        The Sqlite connection.
   */
  execute: Task.async(function* (conn) {
    // A search might be canceled before it starts.
    if (!this.pending)
      return;

    TelemetryStopwatch.start(TELEMETRY_1ST_RESULT);
    if (this._searchString)
      TelemetryStopwatch.start(TELEMETRY_6_FIRST_RESULTS);

    // Since we call the synchronous parseSubmissionURL function later, we must
    // wait for the initialization of PlacesSearchAutocompleteProvider first.
    yield PlacesSearchAutocompleteProvider.ensureInitialized();

    // For any given search, we run many queries/heuristics:
    // 1) by alias (as defined in SearchService)
    // 2) inline completion from search engine resultDomains
    // 3) inline completion for hosts (this._hostQuery) or urls (this._urlQuery)
    // 4) directly typed in url (ie, can be navigated to as-is)
    // 5) submission for the current search engine
    // 6) keywords (this._keywordQuery)
    // 7) adaptive learning (this._adaptiveQuery)
    // 8) open pages not supported by history (this._switchToTabQuery)
    // 9) query based on match behavior
    //
    // (6) only gets ran if we get any filtered tokens, since if there are no
    // tokens, there is nothing to match. This is the *first* query we check if
    // we want to run, but it gets queued to be run later.
    //
    // (1), (4), (5) only get run if actions are enabled. When actions are
    // enabled, the first result is always a special result (resulting from one
    // of the queries between (1) and (6) inclusive). As such, the UI is
    // expected to auto-select the first result when actions are enabled. If the
    // first result is an inline completion result, that will also be the
    // default result and therefore be autofilled (this also happens if actions
    // are not enabled).

    // Get the final query, based on the tokens found in the search string.
    let queries = [ this._adaptiveQuery,
                    this._switchToTabQuery,
                    this._searchQuery ];

    // When actions are enabled, we run a series of heuristics to determine what
    // the first result should be - which is always a special result.
    // |hasFirstResult| is used to keep track of whether we've obtained such a
    // result yet, so we can skip further heuristics and not add any additional
    // special results.
    let hasFirstResult = false;

    if (this._searchTokens.length > 0 &&
        PlacesUtils.bookmarks.getURIForKeyword(this._searchTokens[0])) {
      // This may be a keyword of a bookmark.
      queries.unshift(this._keywordQuery);
      hasFirstResult = true;
    }

    if (this._enableActions && !hasFirstResult) {
      // If it's not a bookmarked keyword, then it may be a search engine
      // with an alias - which works like a keyword.
      hasFirstResult = yield this._matchSearchEngineAlias();
    }
    let shouldAutofill = this._shouldAutofill;
    if (this.pending && !hasFirstResult && shouldAutofill) {
      // Or it may look like a URL we know about from search engines.
      hasFirstResult = yield this._matchSearchEngineUrl();
    }

    if (this.pending && !hasFirstResult && shouldAutofill) {
      // It may also look like a URL we know from the database.
      // Here we can only try to predict whether the URL autofill query is
      // likely to return a result.  If the prediction ends up being wrong,
      // later we will need to make up for the lack of a special first result.
      hasFirstResult = yield this._matchKnownUrl(conn, queries);
    }

    if (this.pending && this._enableActions && !hasFirstResult) {
      // If we don't have a result that matches what we know about, then
      // we use a fallback for things we don't know about.
      yield this._matchHeuristicFallback();
    }

    // IMPORTANT: No other first result heuristics should run after
    // _matchHeuristicFallback().

    yield this._sleep(Prefs.delay);
    if (!this.pending)
      return;

    for (let [query, params] of queries) {
      let hasResult = yield conn.executeCached(query, params, this._onResultRow.bind(this));

      if (this.pending && params.query_type == QUERYTYPE_AUTOFILL_URL &&
          !hasResult) {
        // If we predicted that our URL autofill query might have gotten a
        // result, but it didn't, then we need to recover.
        yield this._matchHeuristicFallback();
      }

      if (!this.pending)
        return;
    }

    // If we do not have enough results, and our match type is
    // MATCH_BOUNDARY_ANYWHERE, search again with MATCH_ANYWHERE to get more
    // results.
    if (this._matchBehavior == MATCH_BOUNDARY_ANYWHERE &&
        this._result.matchCount < Prefs.maxRichResults) {
      this._matchBehavior = MATCH_ANYWHERE;
      for (let [query, params] of [ this._adaptiveQuery,
                                    this._searchQuery ]) {
        yield conn.executeCached(query, params, this._onResultRow.bind(this));
        if (!this.pending)
          return;
      }
    }
  }),

  _matchKnownUrl: function* (conn, queries) {
    // Hosts have no "/" in them.
    let lastSlashIndex = this._searchString.lastIndexOf("/");
    // Search only URLs if there's a slash in the search string...
    if (lastSlashIndex != -1) {
      // ...but not if it's exactly at the end of the search string.
      if (lastSlashIndex < this._searchString.length - 1) {
        // We don't want to execute this query right away because it needs to
        // search the entire DB without an index, but we need to know if we have
        // a result as it will influence other heuristics. So we guess by
        // assuming that if we get a result from a *host* query and it *looks*
        // like a URL, then we'll probably have a result.
        let gotResult = false;
        let [ query, params ] = this._urlPredictQuery;
        yield conn.executeCached(query, params, row => {
          gotResult = true;
          queries.unshift(this._urlQuery);
        });
        return gotResult;
      }

      return false;
    }

    let gotResult = false;
    let [ query, params ] = this._hostQuery;
    yield conn.executeCached(query, params, row => {
      gotResult = true;
      this._onResultRow(row);
    });

    return gotResult;
  },

  _matchSearchEngineUrl: function* () {
    if (!Prefs.autofillSearchEngines)
      return false;

    let match = yield PlacesSearchAutocompleteProvider.findMatchByToken(
                                                           this._searchString);
    if (!match)
      return false;

    // The match doesn't contain a 'scheme://www.' prefix, but since we have
    // stripped it from the search string, here we could still be matching
    // 'https://www.g' to 'google.com'.
    // There are a couple cases where we don't want to match though:
    //
    //  * If the protocol differs we should not match. For example if the user
    //    searched https we should not return http.
    try {
      let prefixURI = NetUtil.newURI(this._strippedPrefix);
      let finalURI = NetUtil.newURI(match.url);
      if (prefixURI.scheme != finalURI.scheme)
        return false;
    } catch (e) {}

    //  * If the user typed "www." but the final url doesn't have it, we
    //    should not match as well, the two urls may point to different pages.
    if (this._strippedPrefix.endsWith("www.") &&
        !stripHttpAndTrim(match.url).startsWith("www."))
      return false;

    let value = this._strippedPrefix + match.token;

    // In any case, we should never arrive here with a value that doesn't
    // match the search string.  If this happens there is some case we
    // are not handling properly yet.
    if (!value.startsWith(this._originalSearchString)) {
      Components.utils.reportError(`Trying to inline complete in-the-middle
                                    ${this._originalSearchString} to ${value}`);
      return false;
    }

    this._result.setDefaultIndex(0);
    this._addMatch({
      value: value,
      comment: match.engineName,
      icon: match.iconUrl,
      style: "priority-search",
      finalCompleteValue: match.url,
      frecency: FRECENCY_SEARCHENGINES_DEFAULT
    });
    return true;
  },

  _matchSearchEngineAlias: function* () {
    if (this._searchTokens.length < 2)
      return false;

    let match = yield PlacesSearchAutocompleteProvider.findMatchByAlias(
                                                         this._searchTokens[0]);
    if (!match)
      return false;

    let query = this._searchTokens.slice(1).join(" ");

    yield this._addSearchEngineMatch(match, query);
    return true;
  },

  _matchCurrentSearchEngine: function* () {
    let match = yield PlacesSearchAutocompleteProvider.getDefaultMatch();
    if (!match)
      return;

    let query = this._originalSearchString;

    yield this._addSearchEngineMatch(match, query);
  },

  _addSearchEngineMatch: function* (match, query) {
    let value = makeActionURL("searchengine", {
      engineName: match.engineName,
      input: this._originalSearchString,
      searchQuery: query,
    });

    this._addMatch({
      value: value,
      comment: match.engineName,
      icon: match.iconUrl,
      style: "action searchengine",
      finalCompleteValue: this._trimmedOriginalSearchString,
      frecency: FRECENCY_SEARCHENGINES_DEFAULT,
    });
  },

  // These are separated out so we can run them in two distinct cases:
  // (1) We didn't match on anything that we know about
  // (2) Our predictive query for URL autofill thought we may get a result,
  //     but we didn't.
  _matchHeuristicFallback: function* () {
    // We may not have auto-filled, but this may still look like a URL.
    let hasFirstResult = yield this._matchUnknownUrl();
    // However, even if the input is a valid URL, we may not want to use
    // it as such. This can happen if the host would require whitelisting,
    // but isn't in the whitelist.

    if (this.pending && !hasFirstResult) {
      // When all else fails, we search using the current search engine.
      yield this._matchCurrentSearchEngine();
    }
  },

  // TODO (bug 1054814): Use visited URLs to inform which scheme to use, if the
  // scheme isn't specificed.
  _matchUnknownUrl: function* () {
    let flags = Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
                Ci.nsIURIFixup.FIXUP_FLAG_REQUIRE_WHITELISTED_HOST;
    let fixupInfo = null;
    try {
      fixupInfo = Services.uriFixup.getFixupURIInfo(this._originalSearchString,
                                                    flags);
    } catch (e) {
      return false;
    }

    let uri = fixupInfo.preferredURI;
    // Check the host, as "http:///" is a valid nsIURI, but not useful to us.
    // But, some schemes are expected to have no host. So we check just against
    // schemes we know should have a host. This allows new schemes to be
    // implemented without us accidentally blocking access to them.
    let hostExpected = new Set(["http", "https", "ftp", "chrome", "resource"]);
    if (!uri || (hostExpected.has(uri.scheme) && !uri.host))
      return false;

    let value = makeActionURL("visiturl", {
      url: uri.spec,
      input: this._originalSearchString,
    });

    let match = {
      value: value,
      comment: uri.spec,
      style: "action visiturl",
      finalCompleteValue: this._originalSearchString,
      frecency: 0,
    };

    try {
      let favicon = yield PlacesUtils.promiseFaviconLinkUrl(uri);
      if (favicon)
        match.icon = favicon.spec;
    } catch (e) {
      // It's possible we don't have a favicon for this - and that's ok.
    };

    this._addMatch(match);
    return true;
  },

  _onResultRow: function (row) {
    TelemetryStopwatch.finish(TELEMETRY_1ST_RESULT);
    let queryType = row.getResultByIndex(QUERYINDEX_QUERYTYPE);
    let match;
    switch (queryType) {
      case QUERYTYPE_AUTOFILL_HOST:
        this._result.setDefaultIndex(0);
        // Fall through.
      case QUERYTYPE_AUTOFILL_PREDICTURL:
        match = this._processHostRow(row);
        break;
      case QUERYTYPE_AUTOFILL_URL:
        this._result.setDefaultIndex(0);
        match = this._processUrlRow(row);
        break;
      case QUERYTYPE_FILTERED:
      case QUERYTYPE_KEYWORD:
        match = this._processRow(row);
        break;
    }
    this._addMatch(match);
    // If the search has been canceled by the user or by _addMatch reaching the
    // maximum number of results, we can stop the underlying Sqlite query.
    if (!this.pending)
      throw StopIteration;
  },

  _maybeRestyleSearchMatch: function (match) {
    // Return if the URL does not represent a search result.
    let parseResult =
      PlacesSearchAutocompleteProvider.parseSubmissionURL(match.value);
    if (!parseResult) {
      return;
    }

    // Do not apply the special style if the user is doing a search from the
    // location bar but the entered terms match an irrelevant portion of the
    // URL. For example, "https://www.google.com/search?q=terms&client=firefox"
    // when searching for "Firefox".
    let terms = parseResult.terms.toLowerCase();
    if (this._searchTokens.length > 0 &&
        this._searchTokens.every(token => terms.indexOf(token) == -1)) {
      return;
    }

    // Use the special separator that the binding will use to style the item.
    match.style = "search " + match.style;
    match.comment = parseResult.terms + TITLE_SEARCH_ENGINE_SEPARATOR +
                    parseResult.engineName;
  },

  _addMatch: function (match) {
    // A search could be canceled between a query start and its completion,
    // in such a case ensure we won't notify any result for it.
    if (!this.pending)
      return;

    let notifyResults = false;

    // Must check both id and url, cause keywords dinamically modify the url.
    let urlMapKey = stripHttpAndTrim(match.value);
    if ((!match.placeId || !this._usedPlaceIds.has(match.placeId)) &&
        !this._usedURLs.has(urlMapKey)) {
      // Add this to our internal tracker to ensure duplicates do not end up in
      // the result.
      // Not all entries have a place id, thus we fallback to the url for them.
      // We cannot use only the url since keywords entries are modified to
      // include the search string, and would be returned multiple times.  Ids
      // are faster too.
      if (match.placeId)
        this._usedPlaceIds.add(match.placeId);
      this._usedURLs.add(urlMapKey);

      if (!match.style) {
        match.style = "favicon";
      }

      // Restyle past searches, unless they are bookmarks or special results.
      if (match.style == "favicon") {
        this._maybeRestyleSearchMatch(match);
      }

      this._result.appendMatch(match.value,
                               match.comment,
                               match.icon || PlacesUtils.favicons.defaultFavicon.spec,
                               match.style,
                               match.finalCompleteValue);
      notifyResults = true;
    }

    if (this._result.matchCount == 6)
      TelemetryStopwatch.finish(TELEMETRY_6_FIRST_RESULTS);

    if (this._result.matchCount == Prefs.maxRichResults) {
      // We have enough results, so stop running our search.
      // We don't need to notify results in this case, cause the main promise
      // chain will do that for us when finishSearch is invoked.
      this.cancel();
    } else if (notifyResults) {
      // Notify about results if we've gotten them.
      this.notifyResults(true);
    }
  },

  _processHostRow: function (row) {
    let match = {};
    let trimmedHost = row.getResultByIndex(QUERYINDEX_URL);
    let untrimmedHost = row.getResultByIndex(QUERYINDEX_TITLE);
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    // If the untrimmed value doesn't preserve the user's input just
    // ignore it and complete to the found host.
    if (untrimmedHost &&
        !untrimmedHost.toLowerCase().contains(this._trimmedOriginalSearchString.toLowerCase())) {
      untrimmedHost = null;
    }

    match.value = this._strippedPrefix + trimmedHost;
    // Remove the trailing slash.
    match.comment = stripHttpAndTrim(trimmedHost);
    match.finalCompleteValue = untrimmedHost;

    try {
      let iconURI = NetUtil.newURI(untrimmedHost);
      iconURI.path = "/favicon.ico";
      match.icon = PlacesUtils.favicons.getFaviconLinkForIcon(iconURI).spec;
    } catch (e) {
      // This can fail, which is ok.
    }

    // Although this has a frecency, this query is executed before any other
    // queries that would result in frecency matches.
    match.frecency = frecency;
    match.style = "autofill";
    return match;
  },

  _processUrlRow: function (row) {
    let match = {};
    let value = row.getResultByIndex(QUERYINDEX_URL);
    let url = fixupSearchText(value);
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    let prefix = value.slice(0, value.length - stripPrefix(value).length);

    // We must complete the URL up to the next separator (which is /, ? or #).
    let separatorIndex = url.slice(this._searchString.length)
                            .search(/[\/\?\#]/);
    if (separatorIndex != -1) {
      separatorIndex += this._searchString.length;
      if (url[separatorIndex] == "/") {
        separatorIndex++; // Include the "/" separator
      }
      url = url.slice(0, separatorIndex);
    }

    // If the untrimmed value doesn't preserve the user's input just
    // ignore it and complete to the found url.
    let untrimmedURL = prefix + url;
    if (untrimmedURL &&
        !untrimmedURL.toLowerCase().contains(this._trimmedOriginalSearchString.toLowerCase())) {
      untrimmedURL = null;
     }

    match.value = this._strippedPrefix + url;
    match.comment = url;
    match.finalCompleteValue = untrimmedURL;
    // Although this has a frecency, this query is executed before any other
    // queries that would result in frecency matches.
    match.frecency = frecency;
    match.style = "autofill";
    return match;
  },

  _processRow: function (row) {
    let match = {};
    match.placeId = row.getResultByIndex(QUERYINDEX_PLACEID);
    let queryType = row.getResultByIndex(QUERYINDEX_QUERYTYPE);
    let escapedURL = row.getResultByIndex(QUERYINDEX_URL);
    let openPageCount = row.getResultByIndex(QUERYINDEX_SWITCHTAB) || 0;
    let historyTitle = row.getResultByIndex(QUERYINDEX_TITLE) || "";
    let iconurl = row.getResultByIndex(QUERYINDEX_ICONURL) || "";
    let bookmarked = row.getResultByIndex(QUERYINDEX_BOOKMARKED);
    let bookmarkTitle = bookmarked ?
      row.getResultByIndex(QUERYINDEX_BOOKMARKTITLE) : null;
    let tags = row.getResultByIndex(QUERYINDEX_TAGS) || "";
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    // If actions are enabled and the page is open, add only the switch-to-tab
    // result.  Otherwise, add the normal result.
    let url = escapedURL;
    let action = null;
    if (this._enableActions && openPageCount > 0) {
      url = makeActionURL("switchtab", {url: escapedURL});
      action = "switchtab";
    }

    // Always prefer the bookmark title unless it is empty
    let title = bookmarkTitle || historyTitle;

    if (queryType == QUERYTYPE_KEYWORD) {
      if (this._enableActions) {
        match.style = "keyword";
        url = makeActionURL("keyword", {
          url: escapedURL,
          input: this._originalSearchString,
        });
        action = "keyword";
      } else {
        // If we do not have a title, then we must have a keyword, so let the UI
        // know it is a keyword.  Otherwise, we found an exact page match, so just
        // show the page like a regular result.  Because the page title is likely
        // going to be more specific than the bookmark title (keyword title).
        if (!historyTitle) {
          match.style = "keyword"
        }
        else {
          title = historyTitle;
        }
      }
    }

    // We will always prefer to show tags if we have them.
    let showTags = !!tags;

    // However, we'll act as if a page is not bookmarked or tagged if the user
    // only wants only history and not bookmarks or tags.
    if (this.hasBehavior("history") &&
        !(this.hasBehavior("bookmark") || this.hasBehavior("tag"))) {
      showTags = false;
      match.style = "favicon";
    }

    // If we have tags and should show them, we need to add them to the title.
    if (showTags) {
      title += TITLE_TAGS_SEPARATOR + tags;
    }

    // We have to determine the right style to display.  Tags show the tag icon,
    // bookmarks get the bookmark icon, and keywords get the keyword icon.  If
    // the result does not fall into any of those, it just gets the favicon.
    if (!match.style) {
      // It is possible that we already have a style set (from a keyword
      // search or because of the user's preferences), so only set it if we
      // haven't already done so.
      if (showTags) {
        match.style = "tag";
      }
      else if (bookmarked) {
        match.style = "bookmark";
      }
    }

    if (action)
      match.style = "action " + action;

    match.value = url;
    match.comment = title;
    if (iconurl) {
      match.icon = PlacesUtils.favicons
                              .getFaviconLinkForIcon(NetUtil.newURI(iconurl)).spec;
    }
    match.frecency = frecency;

    return match;
  },

  /**
   * Obtains the search query to be used based on the previously set search
   * behaviors (accessed by this.hasBehavior).
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _searchQuery() {
    // We use more optimized queries for restricted searches, so we will always
    // return the most restrictive one to the least restrictive one if more than
    // one token is found.
    // Note: "openpages" behavior is supported by the default query.
    //       _switchToTabQuery instead returns only pages not supported by
    //       history and it is always executed.
    let query = this.hasBehavior("tag") ? SQL_TAGS_QUERY :
                this.hasBehavior("bookmark") ? SQL_BOOKMARK_QUERY :
                this.hasBehavior("typed") ? SQL_TYPED_QUERY :
                this.hasBehavior("history") ? SQL_HISTORY_QUERY :
                SQL_DEFAULT_QUERY;

    return [
      query,
      {
        parent: PlacesUtils.tagsFolderId,
        query_type: QUERYTYPE_FILTERED,
        matchBehavior: this._matchBehavior,
        searchBehavior: this._behavior,
        // We only want to search the tokens that we are left with - not the
        // original search string.
        searchString: this._searchTokens.join(" "),
        // Limit the query to the the maximum number of desired results.
        // This way we can avoid doing more work than needed.
        maxResults: Prefs.maxRichResults
      }
    ];
  },

  /**
   * Obtains the query to search for keywords.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _keywordQuery() {
    // The keyword is the first word in the search string, with the parameters
    // following it.
    let searchString = this._trimmedOriginalSearchString;
    let queryString = "";
    let queryIndex = searchString.indexOf(" ");
    if (queryIndex != -1) {
      queryString = searchString.substring(queryIndex + 1);
    }
    // We need to escape the parameters as if they were the query in a URL
    queryString = encodeURIComponent(queryString).replace("%20", "+", "g");

    // The first word could be a keyword, so that's what we'll search.
    let keyword = this._searchTokens[0];

    return [
      SQL_KEYWORD_QUERY,
      {
        keyword: keyword,
        query_string: queryString,
        query_type: QUERYTYPE_KEYWORD
      }
    ];
  },

  /**
   * Obtains the query to search for switch-to-tab entries.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _switchToTabQuery() [
    SQL_SWITCHTAB_QUERY,
    {
      query_type: QUERYTYPE_FILTERED,
      matchBehavior: this._matchBehavior,
      searchBehavior: this._behavior,
      // We only want to search the tokens that we are left with - not the
      // original search string.
      searchString: this._searchTokens.join(" "),
      maxResults: Prefs.maxRichResults
    }
  ],

  /**
   * Obtains the query to search for adaptive results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _adaptiveQuery() [
    SQL_ADAPTIVE_QUERY,
    {
      parent: PlacesUtils.tagsFolderId,
      search_string: this._searchString,
      query_type: QUERYTYPE_FILTERED,
      matchBehavior: this._matchBehavior,
      searchBehavior: this._behavior
    }
  ],

  /**
   * Whether we should try to autoFill.
   */
  get _shouldAutofill() {
    // First of all, check for the autoFill pref.
    if (!Prefs.autofill)
      return false;

    if (!this._searchTokens.length == 1)
      return false;

    // Then, we should not try to autofill if the behavior is not the default.
    // TODO (bug 751709): Ideally we should have a more fine-grained behavior
    // here, but for now it's enough to just check for default behavior.
    if (Prefs.defaultBehavior != DEFAULT_BEHAVIOR) {
      // autoFill can only cope with history or bookmarks entries
      // (typed or not).
      if (!this.hasBehavior("typed") &&
          !this.hasBehavior("history") &&
          !this.hasBehavior("bookmark"))
        return false;

      // autoFill doesn't search titles or tags.
      if (this.hasBehavior("title") || this.hasBehavior("tags"))
        return false;
    }

    // Don't try to autofill if the search term includes any whitespace.
    // This may confuse completeDefaultIndex cause the AUTOCOMPLETE_MATCH
    // tokenizer ends up trimming the search string and returning a value
    // that doesn't match it, or is even shorter.
    if (/\s/.test(this._originalSearchString))
      return false;

    if (this._searchString.length == 0)
      return false;

    return true;
  },

  /**
   * Obtains the query to search for autoFill host results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _hostQuery() {
    let typed = Prefs.autofillTyped || this.hasBehavior("typed");
    let bookmarked =  this.hasBehavior("bookmark");

    return [
      bookmarked ? typed ? SQL_BOOKMARKED_TYPED_HOST_QUERY
                         : SQL_BOOKMARKED_HOST_QUERY
                 : typed ? SQL_TYPED_HOST_QUERY
                         : SQL_HOST_QUERY,
      {
        query_type: QUERYTYPE_AUTOFILL_HOST,
        searchString: this._searchString.toLowerCase()
      }
    ];
  },

  /**
   * Obtains a query to predict whether this._urlQuery is likely to return a
   * result. We do by extracting what should be a host out of the input and
   * performing a host query based on that.
   */
  get _urlPredictQuery() {
    // We expect this to be a full URL, not just a host. We want to extract the
    // host and use that as a guess for whether we'll get a result from a URL
    // query.
    let slashIndex = this._searchString.indexOf("/");

    let host = this._searchString.substring(0, slashIndex);
    host = host.toLowerCase();

    return [
      SQL_HOST_QUERY,
      {
        query_type: QUERYTYPE_AUTOFILL_PREDICTURL,
        searchString: host
      }
    ];
  },

  /**
   * Obtains the query to search for autoFill url results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _urlQuery()  {
    let typed = Prefs.autofillTyped || this.hasBehavior("typed");
    let bookmarked =  this.hasBehavior("bookmark");

    return [
      bookmarked ? typed ? SQL_BOOKMARKED_TYPED_URL_QUERY
                         : SQL_BOOKMARKED_URL_QUERY
                 : typed ? SQL_TYPED_URL_QUERY
                         : SQL_URL_QUERY,
      {
        query_type: QUERYTYPE_AUTOFILL_URL,
        searchString: this._autofillUrlSearchString,
        matchBehavior: MATCH_BEGINNING_CASE_SENSITIVE,
        searchBehavior: Ci.mozIPlacesAutoComplete.BEHAVIOR_URL
      }
    ];
  },

 /**
   * Notifies the listener about results.
   *
   * @param searchOngoing
   *        Indicates whether the search is ongoing.
   */
  notifyResults: function (searchOngoing) {
    let result = this._result;
    let resultCode = result.matchCount ? "RESULT_SUCCESS" : "RESULT_NOMATCH";
    if (searchOngoing) {
      resultCode += "_ONGOING";
    }
    result.setSearchResult(Ci.nsIAutoCompleteResult[resultCode]);
    this._listener.onSearchResult(this._autocompleteSearch, result);
  },
}

////////////////////////////////////////////////////////////////////////////////
//// UnifiedComplete class
//// component @mozilla.org/autocomplete/search;1?name=unifiedcomplete

function UnifiedComplete() {
  Services.obs.addObserver(this, TOPIC_SHUTDOWN, true);
}

UnifiedComplete.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function (subject, topic, data) {
    if (topic === TOPIC_SHUTDOWN) {
      this.ensureShutdown();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Database handling

  /**
   * Promise resolved when the database initialization has completed, or null
   * if it has never been requested.
   */
  _promiseDatabase: null,

  /**
   * Gets a Sqlite database handle.
   *
   * @return {Promise}
   * @resolves to the Sqlite database handle (according to Sqlite.jsm).
   * @rejects javascript exception.
   */
  getDatabaseHandle: function () {
    if (Prefs.enabled && !this._promiseDatabase) {
      this._promiseDatabase = Task.spawn(function* () {
        let conn = yield Sqlite.cloneStorageConnection({
          connection: PlacesUtils.history.DBConnection,
          readOnly: true
        });

        // Autocomplete often fallbacks to a table scan due to lack of text
        // indices.  A larger cache helps reducing IO and improving performance.
        // The value used here is larger than the default Storage value defined
        // as MAX_CACHE_SIZE_BYTES in storage/src/mozStorageConnection.cpp.
        yield conn.execute("PRAGMA cache_size = -6144"); // 6MiB

        yield SwitchToTabStorage.initDatabase(conn);

        return conn;
      }.bind(this)).then(null, ex => { dump("Couldn't get database handle: " + ex + "\n");
                                       Cu.reportError(ex); });
    }
    return this._promiseDatabase;
  },

  /**
   * Used to stop running queries and close the database handle.
   */
  ensureShutdown: function () {
    if (this._promiseDatabase) {
      Task.spawn(function* () {
        let conn = yield this.getDatabaseHandle();
        SwitchToTabStorage.shutdown();
        yield conn.close()
      }.bind(this)).then(null, Cu.reportError);
      this._promiseDatabase = null;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// mozIPlacesAutoComplete

  registerOpenPage: function PAC_registerOpenPage(uri) {
    SwitchToTabStorage.add(uri);
  },

  unregisterOpenPage: function PAC_unregisterOpenPage(uri) {
    SwitchToTabStorage.delete(uri);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSearch

  startSearch: function (searchString, searchParam, previousResult, listener) {
    // Stop the search in case the controller has not taken care of it.
    if (this._currentSearch) {
      this.stopSearch();
    }

    // Note: We don't use previousResult to make sure ordering of results are
    //       consistent.  See bug 412730 for more details.

    this._currentSearch = new Search(searchString, searchParam, listener,
                                     this, this);

    // If we are not enabled, we need to return now.  Notice we need an empty
    // result regardless, so we still create the Search object.
    if (!Prefs.enabled) {
      this.finishSearch(true);
      return;
    }

    let search = this._currentSearch;
    this.getDatabaseHandle().then(conn => search.execute(conn))
                            .then(null, ex => {
                              dump(`Query failed: ${ex}\n`);
                              Cu.reportError(ex);
                            })
                            .then(() => {
                              if (search == this._currentSearch) {
                                this.finishSearch(true);
                              }
                            });
  },

  stopSearch: function () {
    if (this._currentSearch) {
      this._currentSearch.cancel();
    }
    // Don't notify since we are canceling this search.  This also means we
    // won't fire onSearchComplete for this search.
    this.finishSearch();
  },

  /**
   * Properly cleans up when searching is completed.
   *
   * @param notify [optional]
   *        Indicates if we should notify the AutoComplete listener about our
   *        results or not.
   */
  finishSearch: function (notify=false) {
    TelemetryStopwatch.cancel(TELEMETRY_1ST_RESULT);
    TelemetryStopwatch.cancel(TELEMETRY_6_FIRST_RESULTS);
    // Clear state now to avoid race conditions, see below.
    let search = this._currentSearch;
    delete this._currentSearch;

    if (!notify)
      return;

    // There is a possible race condition here.
    // When a search completes it calls finishSearch that notifies results
    // here.  When the controller gets the last result it fires
    // onSearchComplete.
    // If onSearchComplete immediately starts a new search it will set a new
    // _currentSearch, and on return the execution will continue here, after
    // notifyResults.
    // Thus, ensure that notifyResults is the last call in this method,
    // otherwise you might be touching the wrong search.
    search.notifyResults(false);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSimpleResultListener

  onValueRemoved: function (result, spec, removeFromDB) {
    if (removeFromDB) {
      PlacesUtils.history.removePage(NetUtil.newURI(spec));
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIAutoCompleteSearchDescriptor

  get searchType() Ci.nsIAutoCompleteSearchDescriptor.SEARCH_TYPE_IMMEDIATE,

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  classID: Components.ID("f964a319-397a-4d21-8be6-5cdd1ee3e3ae"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(UnifiedComplete),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIAutoCompleteSearch,
    Ci.nsIAutoCompleteSimpleResultListener,
    Ci.nsIAutoCompleteSearchDescriptor,
    Ci.mozIPlacesAutoComplete,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([UnifiedComplete]);
