/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Constants

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

const PREF_BRANCH = "browser.urlbar.";

// Prefs are defined as [pref name, default value].
const PREF_ENABLED =                [ "autocomplete.enabled",   true ];
const PREF_AUTOFILL =               [ "autoFill",               true ];
const PREF_AUTOFILL_TYPED =         [ "autoFill.typed",         true ];
const PREF_AUTOFILL_SEARCHENGINES = [ "autoFill.searchEngines", false ];
const PREF_RESTYLESEARCHES        = [ "restyleSearches",        false ];
const PREF_DELAY =                  [ "delay",                  50 ];
const PREF_BEHAVIOR =               [ "matchBehavior", MATCH_BOUNDARY_ANYWHERE ];
const PREF_FILTER_JS =              [ "filter.javascript",      true ];
const PREF_MAXRESULTS =             [ "maxRichResults",         25 ];
const PREF_RESTRICT_HISTORY =       [ "restrict.history",       "^" ];
const PREF_RESTRICT_BOOKMARKS =     [ "restrict.bookmark",      "*" ];
const PREF_RESTRICT_TYPED =         [ "restrict.typed",         "~" ];
const PREF_RESTRICT_TAG =           [ "restrict.tag",           "+" ];
const PREF_RESTRICT_SWITCHTAB =     [ "restrict.openpage",      "%" ];
const PREF_RESTRICT_SEARCHES =      [ "restrict.searces",       "$" ];
const PREF_MATCH_TITLE =            [ "match.title",            "#" ];
const PREF_MATCH_URL =              [ "match.url",              "@" ];

const PREF_SUGGEST_HISTORY =        [ "suggest.history",        true ];
const PREF_SUGGEST_BOOKMARK =       [ "suggest.bookmark",       true ];
const PREF_SUGGEST_OPENPAGE =       [ "suggest.openpage",       true ];
const PREF_SUGGEST_HISTORY_ONLYTYPED = [ "suggest.history.onlyTyped", false ];
const PREF_SUGGEST_SEARCHES =       [ "suggest.searches",       true ];

const PREF_MAX_CHARS_FOR_SUGGEST =  [ "maxCharsForSearchSuggestions", 20];

// Match type constants.
// These indicate what type of search function we should be using.
const MATCH_ANYWHERE = Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE;
const MATCH_BOUNDARY_ANYWHERE = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY_ANYWHERE;
const MATCH_BOUNDARY = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY;
const MATCH_BEGINNING = Ci.mozIPlacesAutoComplete.MATCH_BEGINNING;
const MATCH_BEGINNING_CASE_SENSITIVE = Ci.mozIPlacesAutoComplete.MATCH_BEGINNING_CASE_SENSITIVE;

// AutoComplete query type constants.
// Describes the various types of queries that we can process rows for.
const QUERYTYPE_FILTERED            = 0;
const QUERYTYPE_AUTOFILL_HOST       = 1;
const QUERYTYPE_AUTOFILL_URL        = 2;

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
const FRECENCY_DEFAULT = 1000;

// Remote matches are appended when local matches are below a given frecency
// threshold (FRECENCY_DEFAULT) as soon as they arrive.  However we'll
// always try to have at least MINIMUM_LOCAL_MATCHES local matches.
const MINIMUM_LOCAL_MATCHES = 5;

// A regex that matches "single word" hostnames for whitelisting purposes.
// The hostname will already have been checked for general validity, so we
// don't need to be exhaustive here, so allow dashes anywhere.
const REGEXP_SINGLEWORD_HOST = new RegExp("^[a-z0-9-]+$", "i");

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


function hostQuery(conditions = "") {
  let query =
    `/* do not warn (bug NA): not worth to index on (typed, frecency) */
     SELECT :query_type, host || '/', IFNULL(prefix, '') || host || '/',
            ( SELECT f.url FROM moz_favicons f
              JOIN moz_places h ON h.favicon_id = f.id
              WHERE rev_host = get_unreversed_host(host || '.') || '.'
                 OR rev_host = get_unreversed_host(host || '.') || '.www.'
            ) AS favicon_url,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, frecency
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
            ( SELECT f.url FROM moz_favicons f
              JOIN moz_places h ON h.favicon_id = f.id
              WHERE rev_host = get_unreversed_host(host || '.') || '.'
                 OR rev_host = get_unreversed_host(host || '.') || '.www.'
            ) AS favicon_url,
            ( SELECT foreign_count > 0 FROM moz_places
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
  return `/* do not warn (bug no): cannot use an index to sort */
          SELECT :query_type, h.url, NULL, f.url AS favicon_url,
            foreign_count > 0 AS bookmarked,
            NULL, NULL, NULL, NULL, NULL, NULL, h.frecency
          FROM moz_places h
          LEFT JOIN moz_favicons f ON h.favicon_id = f.id
          WHERE (rev_host = :revHost OR rev_host = :revHost || "www.")
          AND h.frecency <> 0
          AND fixup_url(h.url) BETWEEN :searchString AND :searchString || X'FFFF'
          ${conditions}
          ORDER BY h.frecency DESC, h.id DESC
          LIMIT 1`;
}

const SQL_URL_QUERY = urlQuery();

const SQL_TYPED_URL_QUERY = urlQuery("AND h.typed = 1");

// TODO (bug 1045924): use foreign_count once available.
const SQL_BOOKMARKED_URL_QUERY = urlQuery("AND bookmarked");

const SQL_BOOKMARKED_TYPED_URL_QUERY = urlQuery("AND bookmarked AND h.typed = 1");

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
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
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
  let types = ["History", "Bookmark", "Openpage", "Searches"];

  function syncEnabledPref() {
    loadSyncedPrefs();

    let suggestPrefs = [
      PREF_SUGGEST_HISTORY,
      PREF_SUGGEST_BOOKMARK,
      PREF_SUGGEST_OPENPAGE,
      PREF_SUGGEST_SEARCHES,
    ];

    if (store.enabled) {
      // If the autocomplete preference is active, set to default value all suggest
      // preferences only if all of them are false.
      if (types.every(type => store["suggest" + type] == false)) {
        for (let type of suggestPrefs) {
          prefs.set(...type);
        }
      }
    } else {
      // If the preference was deactivated, deactivate all suggest preferences.
      for (let type of suggestPrefs) {
        prefs.set(type[0], false);
      }
    }
  }

  function loadSyncedPrefs () {
    store.enabled = prefs.get(...PREF_ENABLED);
    store.suggestHistory = prefs.get(...PREF_SUGGEST_HISTORY);
    store.suggestBookmark = prefs.get(...PREF_SUGGEST_BOOKMARK);
    store.suggestOpenpage = prefs.get(...PREF_SUGGEST_OPENPAGE);
    store.suggestTyped = prefs.get(...PREF_SUGGEST_HISTORY_ONLYTYPED);
    store.suggestSearches = prefs.get(...PREF_SUGGEST_SEARCHES);
  }

  function loadPrefs(subject, topic, data) {
    if (data) {
      // Synchronize suggest.* prefs with autocomplete.enabled.
      if (data == PREF_BRANCH + PREF_ENABLED[0]) {
        syncEnabledPref();
      } else if (data.startsWith(PREF_BRANCH + "suggest.")) {
        loadSyncedPrefs();
        prefs.set(PREF_ENABLED[0], types.some(type => store["suggest" + type]));
      }
    }

    store.enabled = prefs.get(...PREF_ENABLED);
    store.autofill = prefs.get(...PREF_AUTOFILL);
    store.autofillTyped = prefs.get(...PREF_AUTOFILL_TYPED);
    store.autofillSearchEngines = prefs.get(...PREF_AUTOFILL_SEARCHENGINES);
    store.restyleSearches = prefs.get(...PREF_RESTYLESEARCHES);
    store.delay = prefs.get(...PREF_DELAY);
    store.matchBehavior = prefs.get(...PREF_BEHAVIOR);
    store.filterJavaScript = prefs.get(...PREF_FILTER_JS);
    store.maxRichResults = prefs.get(...PREF_MAXRESULTS);
    store.restrictHistoryToken = prefs.get(...PREF_RESTRICT_HISTORY);
    store.restrictBookmarkToken = prefs.get(...PREF_RESTRICT_BOOKMARKS);
    store.restrictTypedToken = prefs.get(...PREF_RESTRICT_TYPED);
    store.restrictTagToken = prefs.get(...PREF_RESTRICT_TAG);
    store.restrictOpenPageToken = prefs.get(...PREF_RESTRICT_SWITCHTAB);
    store.restrictSearchesToken = prefs.get(...PREF_RESTRICT_SEARCHES);
    store.matchTitleToken = prefs.get(...PREF_MATCH_TITLE);
    store.matchURLToken = prefs.get(...PREF_MATCH_URL);
    store.suggestHistory = prefs.get(...PREF_SUGGEST_HISTORY);
    store.suggestBookmark = prefs.get(...PREF_SUGGEST_BOOKMARK);
    store.suggestOpenpage = prefs.get(...PREF_SUGGEST_OPENPAGE);
    store.suggestTyped = prefs.get(...PREF_SUGGEST_HISTORY_ONLYTYPED);
    store.suggestSearches = prefs.get(...PREF_SUGGEST_SEARCHES);
    store.maxCharsForSearchSuggestions = prefs.get(...PREF_MAX_CHARS_FOR_SUGGEST);
    store.keywordEnabled = true;
    try {
      store.keywordEnabled = Services.prefs.getBoolPref("keyword.enabled");
    } catch (ex) {}

    // If history is not set, onlyTyped value should be ignored.
    if (!store.suggestHistory) {
      store.suggestTyped = false;
    }
    store.defaultBehavior = types.concat("Typed").reduce((memo, type) => {
      let prefValue = store["suggest" + type];
      return memo | (prefValue &&
                     Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type.toUpperCase()]);
    }, 0);

    // Further restrictions to apply for "empty searches" (i.e. searches for "").
    // The empty behavior is typed history, if history is enabled. Otherwise,
    // it is bookmarks, if they are enabled. If both history and bookmarks are disabled,
    // it defaults to open pages.
    store.emptySearchDefaultBehavior = Ci.mozIPlacesAutoComplete.BEHAVIOR_RESTRICT;
    if (store.suggestHistory) {
      store.emptySearchDefaultBehavior |= Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY |
                                          Ci.mozIPlacesAutoComplete.BEHAVIOR_TYPED;
    } else if (store.suggestBookmark) {
      store.emptySearchDefaultBehavior |= Ci.mozIPlacesAutoComplete.BEHAVIOR_BOOKMARK;
    } else {
      store.emptySearchDefaultBehavior |= Ci.mozIPlacesAutoComplete.BEHAVIOR_OPENPAGE;
    }

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
      [ store.restrictTypedToken, "typed" ],
      [ store.restrictSearchesToken, "searches" ],
    ]);
  }

  let store = {
    _ignoreNotifications: false,
    observe(subject, topic, data) {
      // Avoid re-entrancy when flipping linked preferences.
      if (this._ignoreNotifications)
        return;
      this._ignoreNotifications = true;
      loadPrefs(subject, topic, data);
      this._ignoreNotifications = false;
    },
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIObserver,
      Ci.nsISupportsWeakReference ])
  };

  // Synchronize suggest.* prefs with autocomplete.enabled at initialization
  syncEnabledPref();

  loadPrefs();
  prefs.observe("", store);
  Services.prefs.addObserver("keyword.enabled", store, true);

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

/**
 * Manages a single instance of an autocomplete search.
 *
 * The first three parameters all originate from the similarly named parameters
 * of nsIAutoCompleteSearch.startSearch().
 *
 * @param searchString
 *        The search string.
 * @param searchParam
 *        A space-delimited string of search parameters.  The following
 *        parameters are supported:
 *        * enable-actions: Include "actions", such as switch-to-tab and search
 *          engine aliases, in the results.
 *        * disable-private-actions: The search is taking place in a private
 *          window outside of permanent private-browsing mode.  The search
 *          should exclude privacy-sensitive results as appropriate.
 *        * private-window: The search is taking place in a private window,
 *          possibly in permanent private-browsing mode.  The search
 *          should exclude privacy-sensitive results as appropriate.
 * @param autocompleteListener
 *        An nsIAutoCompleteObserver.
 * @param resultListener
 *        An nsIAutoCompleteSimpleResultListener.
 * @param autocompleteSearch
 *        An nsIAutoCompleteSearch.
 * @param prohibitSearchSuggestions
 *        Whether search suggestions are allowed for this search.
 */
function Search(searchString, searchParam, autocompleteListener,
                resultListener, autocompleteSearch, prohibitSearchSuggestions) {
  // We want to store the original string for case sensitive searches.
  this._originalSearchString = searchString;
  this._trimmedOriginalSearchString = searchString.trim();
  this._searchString = fixupSearchText(this._trimmedOriginalSearchString.toLowerCase());

  this._matchBehavior = Prefs.matchBehavior;
  // Set the default behavior for this search.
  this._behavior = this._searchString ? Prefs.defaultBehavior
                                      : Prefs.emptySearchDefaultBehavior;

  let params = new Set(searchParam.split(" "));
  this._enableActions = params.has("enable-actions");
  this._disablePrivateActions = params.has("disable-private-actions");
  this._inPrivateWindow = params.has("private-window");
  this._prohibitAutoFill = params.has("prohibit-autofill");

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

  this._prohibitSearchSuggestions = prohibitSearchSuggestions;

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

  // Resolved when all the remote matches have been fetched.
  this._remoteMatchesPromises = [];

  // The index to insert remote matches at.
  this._remoteMatchesStartIndex = 0;

  // Counts the number of inserted local matches.
  this._localMatchesCount = 0;
  // Counts the number of inserted remote matches.
  this._remoteMatchesCount = 0;
}

Search.prototype = {
  /**
   * Enables the desired AutoComplete behavior.
   *
   * @param type
   *        The behavior type to set.
   */
  setBehavior: function (type) {
    type = type.toUpperCase();
    this._behavior |=
      Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type];

    // Setting the "typed" behavior should also set the "history" behavior.
    if (type == "TYPED") {
      this.setBehavior("history");
    }
  },

  /**
   * Determines if the specified AutoComplete behavior is set.
   *
   * @param aType
   *        The behavior type to test for.
   * @return true if the behavior is set, false otherwise.
   */
  hasBehavior: function (type) {
    let behavior = Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type.toUpperCase()];

    if (this._disablePrivateActions &&
        behavior == Ci.mozIPlacesAutoComplete.BEHAVIOR_OPENPAGE) {
      return false;
    }

    return this._behavior & behavior;
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
    this._sleepDeferred = PromiseUtils.defer();
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
    let foundToken = false;
    // Set the proper behavior while filtering tokens.
    for (let i = tokens.length - 1; i >= 0; i--) {
      let behavior = Prefs.tokenToBehaviorMap.get(tokens[i]);
      // Don't remove the token if it didn't match, or if it's an action but
      // actions are not enabled.
      if (behavior && (behavior != "openpage" || this._enableActions)) {
        // Don't use the suggest preferences if it is a token search and
        // set the restrict bit to 1 (to intersect the search results).
        if (!foundToken) {
          foundToken = true;
          // Do not take into account previous behavior (e.g.: history, bookmark)
          this._behavior = 0;
          this.setBehavior("restrict");
        }
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
   * Stop this search.
   * After invoking this method, we won't run any more searches or heuristics,
   * and no new matches may be added to the current result.
   */
  stop() {
    if (this._sleepTimer)
      this._sleepTimer.cancel();
    if (this._sleepDeferred) {
      this._sleepDeferred.resolve();
      this._sleepDeferred = null;
    }
    if (this._searchSuggestionController) {
      this._searchSuggestionController.stop();
      this._searchSuggestionController = null;
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

    TelemetryStopwatch.start(TELEMETRY_1ST_RESULT, this);
    if (this._searchString)
      TelemetryStopwatch.start(TELEMETRY_6_FIRST_RESULTS, this);

    // Since we call the synchronous parseSubmissionURL function later, we must
    // wait for the initialization of PlacesSearchAutocompleteProvider first.
    yield PlacesSearchAutocompleteProvider.ensureInitialized();
    if (!this.pending)
      return;

    // For any given search, we run many queries/heuristics:
    // 1) by alias (as defined in SearchService)
    // 2) inline completion from search engine resultDomains
    // 3) inline completion for hosts (this._hostQuery) or urls (this._urlQuery)
    // 4) directly typed in url (ie, can be navigated to as-is)
    // 5) submission for the current search engine
    // 6) Places keywords
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
    let queries = [ this._adaptiveQuery ];

    // "openpage" behavior is supported by the default query.
    // _switchToTabQuery instead returns only pages not supported by history.
    if (this.hasBehavior("openpage")) {
      queries.push(this._switchToTabQuery);
    }
    queries.push(this._searchQuery);

    // When actions are enabled, we run a series of heuristics to determine what
    // the first result should be - which is always a special result.
    // |hasFirstResult| is used to keep track of whether we've obtained such a
    // result yet, so we can skip further heuristics and not add any additional
    // special results.
    let hasFirstResult = false;

    if (this._searchTokens.length > 0) {
      // This may be a Places keyword.
      hasFirstResult = yield this._matchPlacesKeyword();
    }

    if (this.pending && this._enableActions && !hasFirstResult) {
      // If it's not a Places keyword, then it may be a search engine
      // with an alias - which works like a keyword.
      hasFirstResult = yield this._matchSearchEngineAlias();
    }

    let shouldAutofill = this._shouldAutofill;
    if (this.pending && !hasFirstResult && shouldAutofill) {
      // It may also look like a URL we know from the database.
      hasFirstResult = yield this._matchKnownUrl(conn);
    }

    if (this.pending && !hasFirstResult && shouldAutofill) {
      // Or it may look like a URL we know about from search engines.
      hasFirstResult = yield this._matchSearchEngineUrl();
    }

    if (this.pending && this._enableActions && !hasFirstResult) {
      // If we don't have a result that matches what we know about, then
      // we use a fallback for things we don't know about.

      // We may not have auto-filled, but this may still look like a URL.
      // However, even if the input is a valid URL, we may not want to use
      // it as such. This can happen if the host would require whitelisting,
      // but isn't in the whitelist.
      hasFirstResult = yield this._matchUnknownUrl();
    }

    if (this.pending && this._enableActions && !hasFirstResult) {
      // When all else fails, we search using the current search engine.
      hasFirstResult = yield this._matchCurrentSearchEngine();
    }

    // IMPORTANT: No other first result heuristics should run after this point.

    yield this._sleep(Prefs.delay);
    if (!this.pending)
      return;

    yield this._matchSearchSuggestions();
    if (!this.pending)
      return;

    for (let [query, params] of queries) {
      yield conn.executeCached(query, params, this._onResultRow.bind(this));
      if (!this.pending)
        return;
    }

    // If we do not have enough results, and our match type is
    // MATCH_BOUNDARY_ANYWHERE, search again with MATCH_ANYWHERE to get more
    // results.
    if (this._matchBehavior == MATCH_BOUNDARY_ANYWHERE &&
        this._localMatchesCount < Prefs.maxRichResults) {
      this._matchBehavior = MATCH_ANYWHERE;
      for (let [query, params] of [ this._adaptiveQuery,
                                    this._searchQuery ]) {
        yield conn.executeCached(query, params, this._onResultRow.bind(this));
        if (!this.pending)
          return;
      }
    }

    // Ensure to fill any remaining space.
    yield Promise.all(this._remoteMatchesPromises);
  }),

  *_matchSearchSuggestions() {
    // Limit the string sent for search suggestions to a maximum length.
    let searchString = this._searchTokens.join(" ")
                           .substr(0, Prefs.maxCharsForSearchSuggestions);
    // Avoid fetching suggestions if they are not required, private browsing
    // mode is enabled, or the search string may expose sensitive information.
    if (!this.hasBehavior("searches") || this._inPrivateWindow ||
        this._prohibitSearchSuggestionsFor(searchString)) {
      return;
    }

    this._searchSuggestionController =
      PlacesSearchAutocompleteProvider.getSuggestionController(
        searchString,
        this._inPrivateWindow,
        Prefs.maxRichResults
      );
    let promise = this._searchSuggestionController.fetchCompletePromise
      .then(() => {
        // The search has been canceled already.
        if (!this._searchSuggestionController)
          return;
        if (this._searchSuggestionController.resultsCount >= 0 &&
            this._searchSuggestionController.resultsCount < 2) {
          // The original string is used to properly compare with the next search.
          this._lastLowResultsSearchSuggestion = this._originalSearchString;
        }
        while (this.pending && this._remoteMatchesCount < Prefs.maxRichResults) {
          let [match, suggestion] = this._searchSuggestionController.consume();
          if (!suggestion)
            break;
          // Don't include the restrict token, if present.
          let searchString = this._searchTokens.join(" ");
          this._addSearchEngineMatch(match, searchString, suggestion);
        }
      });

    if (this.hasBehavior("restrict")) {
      // We're done if we're restricting to search suggestions.
      yield promise;
      this.stop();
    } else {
      this._remoteMatchesPromises.push(promise);
    }
  },

  _prohibitSearchSuggestionsFor(searchString) {
    if (this._prohibitSearchSuggestions)
      return true;

    // Suggestions for a single letter are unlikely to be useful.
    if (searchString.length < 2)
      return true;

    let tokens = searchString.split(/\s/);

    // The first token may be a whitelisted host.
    if (REGEXP_SINGLEWORD_HOST.test(tokens[0]) &&
        Services.uriFixup.isDomainWhitelisted(tokens[0], -1))
      return true;

    // Disallow fetching search suggestions for strings looking like URLs, to
    // avoid disclosing information about networks or passwords.
    return tokens.some(token => {
      return ["/", "@", ":", "."].some(c => token.includes(c));
    });
  },

  _matchKnownUrl: function* (conn) {
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
        let [ query, params ] = this._urlQuery;
        yield conn.executeCached(query, params, row => {
          gotResult = true;
          this._onResultRow(row);
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

  _matchPlacesKeyword: function* () {
    // The first word could be a keyword, so that's what we'll search.
    let keyword = this._searchTokens[0];
    let entry = yield PlacesUtils.keywords.fetch(this._searchTokens[0]);
    if (!entry)
      return false;

    // Build the url.
    let searchString = this._trimmedOriginalSearchString;
    let queryString = "";
    let queryIndex = searchString.indexOf(" ");
    if (queryIndex != -1) {
      queryString = searchString.substring(queryIndex + 1);
    }
    // We need to escape the parameters as if they were the query in a URL
    queryString = encodeURIComponent(queryString).replace(/%20/g, "+");
    let escapedURL = entry.url.href.replace("%s", queryString);

    let style = (this._enableActions ? "action " : "") + "keyword";
    let actionURL = makeActionURL("keyword", { url: escapedURL,
                                               input: this._originalSearchString });
    let value = this._enableActions ? actionURL : escapedURL;
    // The title will end up being "host: queryString"
    let comment = entry.url.host;

    this._addMatch({ value, comment, style, frecency: FRECENCY_DEFAULT });
    return true;
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
      frecency: FRECENCY_DEFAULT
    });
    return true;
  },

  _matchSearchEngineAlias: function* () {
    if (this._searchTokens.length < 2)
      return false;

    let alias = this._searchTokens[0];
    let match = yield PlacesSearchAutocompleteProvider.findMatchByAlias(alias);
    if (!match)
      return false;

    match.engineAlias = alias;
    let query = this._searchTokens.slice(1).join(" ");

    this._addSearchEngineMatch(match, query);
    return true;
  },

  _matchCurrentSearchEngine: function* () {
    let match = yield PlacesSearchAutocompleteProvider.getDefaultMatch();
    if (!match)
      return false;

    let query = this._originalSearchString;
    this._addSearchEngineMatch(match, query);
    return true;
  },

  _addSearchEngineMatch(match, query, suggestion) {
    let actionURLParams = {
      engineName: match.engineName,
      input: suggestion || this._originalSearchString,
      searchQuery: query,
    };
    if (suggestion)
      actionURLParams.searchSuggestion = suggestion;
    if (match.engineAlias) {
      actionURLParams.alias = match.engineAlias;
    }
    let value = makeActionURL("searchengine", actionURLParams);

    this._addMatch({
      value: value,
      comment: match.engineName,
      icon: match.iconUrl,
      style: "action searchengine",
      frecency: FRECENCY_DEFAULT,
      remote: !!suggestion
    });
  },

  // TODO (bug 1054814): Use visited URLs to inform which scheme to use, if the
  // scheme isn't specificed.
  _matchUnknownUrl: function* () {
    let flags = Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
                Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    let fixupInfo = null;
    try {
      fixupInfo = Services.uriFixup.getFixupURIInfo(this._originalSearchString,
                                                    flags);
    } catch (e) {
      return false;
    }

    // If the URI cannot be fixed or the preferred URI would do a keyword search,
    // that basically means this isn't useful to us. Note that
    // fixupInfo.keywordAsSent will never be true if the keyword.enabled pref
    // is false or there are no engines, so in that case we will always return
    // a "visit".
    if (!fixupInfo.fixedURI || fixupInfo.keywordAsSent)
      return false;

    let uri = fixupInfo.fixedURI;
    // Check the host, as "http:///" is a valid nsIURI, but not useful to us.
    // But, some schemes are expected to have no host. So we check just against
    // schemes we know should have a host. This allows new schemes to be
    // implemented without us accidentally blocking access to them.
    let hostExpected = new Set(["http", "https", "ftp", "chrome", "resource"]);
    if (hostExpected.has(uri.scheme) && !uri.host)
      return false;

    // If the result is something that looks like a single-worded hostname
    // we need to check the domain whitelist to treat it as such.
    // We also want to return a "visit" if keyword.enabled is false.
    if (uri.asciiHost &&
        Prefs.keywordEnabled &&
        REGEXP_SINGLEWORD_HOST.test(uri.asciiHost) &&
        !Services.uriFixup.isDomainWhitelisted(uri.asciiHost, -1)) {
      return false;
    }

    let value = makeActionURL("visiturl", {
      url: uri.spec,
      input: this._originalSearchString,
    });

    let match = {
      value: value,
      comment: uri.spec,
      style: "action visiturl",
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
    TelemetryStopwatch.finish(TELEMETRY_1ST_RESULT, this);
    let queryType = row.getResultByIndex(QUERYINDEX_QUERYTYPE);
    let match;
    switch (queryType) {
      case QUERYTYPE_AUTOFILL_HOST:
        this._result.setDefaultIndex(0);
        match = this._processHostRow(row);
        break;
      case QUERYTYPE_AUTOFILL_URL:
        this._result.setDefaultIndex(0);
        match = this._processUrlRow(row);
        break;
      case QUERYTYPE_FILTERED:
        match = this._processRow(row);
        break;
    }
    this._addMatch(match);
    // If the search has been canceled by the user or by _addMatch, or we
    // fetched enough results, we can stop the underlying Sqlite query.
    if (!this.pending || this._localMatchesCount == Prefs.maxRichResults)
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

  _addMatch(match) {
    // A search could be canceled between a query start and its completion,
    // in such a case ensure we won't notify any result for it.
    if (!this.pending)
      return;

    // Must check both id and url, cause keywords dynamically modify the url.
    let urlMapKey = stripHttpAndTrim(match.value);
    if ((match.placeId && this._usedPlaceIds.has(match.placeId)) ||
        this._usedURLs.has(urlMapKey)) {
      return;
    }

    // Add this to our internal tracker to ensure duplicates do not end up in
    // the result.
    // Not all entries have a place id, thus we fallback to the url for them.
    // We cannot use only the url since keywords entries are modified to
    // include the search string, and would be returned multiple times.  Ids
    // are faster too.
    if (match.placeId)
      this._usedPlaceIds.add(match.placeId);
    this._usedURLs.add(urlMapKey);

    match.style = match.style || "favicon";

    // Restyle past searches, unless they are bookmarks or special results.
    if (Prefs.restyleSearches && match.style == "favicon") {
      this._maybeRestyleSearchMatch(match);
    }

    match.icon = match.icon || PlacesUtils.favicons.defaultFavicon.spec;
    match.finalCompleteValue = match.finalCompleteValue || "";

    this._result.insertMatchAt(this._getInsertIndexForMatch(match),
                               match.value,
                               match.comment,
                               match.icon,
                               match.style,
                               match.finalCompleteValue);

    if (this._result.matchCount == 6)
      TelemetryStopwatch.finish(TELEMETRY_6_FIRST_RESULTS, this);

    this.notifyResults(true);
  },

  _getInsertIndexForMatch(match) {
    let index = 0;
    if (match.remote) {
      // Append after local matches.
      index = this._remoteMatchesStartIndex + this._remoteMatchesCount;
      this._remoteMatchesCount++;
    } else {
      // This is a local match.
      if (match.frecency > FRECENCY_DEFAULT ||
          this._localMatchesCount < MINIMUM_LOCAL_MATCHES) {
        // Append before remote matches.
        index = this._remoteMatchesStartIndex;
        this._remoteMatchesStartIndex++
      } else {
        // Append after remote matches.
        index = this._localMatchesCount + this._remoteMatchesCount;
      }
      this._localMatchesCount++;
    }
    return index;
  },

  _processHostRow: function (row) {
    let match = {};
    let trimmedHost = row.getResultByIndex(QUERYINDEX_URL);
    let untrimmedHost = row.getResultByIndex(QUERYINDEX_TITLE);
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);
    let faviconUrl = row.getResultByIndex(QUERYINDEX_ICONURL);

    // If the untrimmed value doesn't preserve the user's input just
    // ignore it and complete to the found host.
    if (untrimmedHost &&
        !untrimmedHost.toLowerCase().includes(this._trimmedOriginalSearchString.toLowerCase())) {
      untrimmedHost = null;
    }

    match.value = this._strippedPrefix + trimmedHost;
    // Remove the trailing slash.
    match.comment = stripHttpAndTrim(trimmedHost);
    match.finalCompleteValue = untrimmedHost;
    match.icon = faviconUrl;
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
    let faviconUrl = row.getResultByIndex(QUERYINDEX_ICONURL);

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
        !untrimmedURL.toLowerCase().includes(this._trimmedOriginalSearchString.toLowerCase())) {
      untrimmedURL = null;
     }

    match.value = this._strippedPrefix + url;
    match.comment = url;
    match.finalCompleteValue = untrimmedURL;
    match.icon = faviconUrl;
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
    if (this._enableActions && openPageCount > 0 && this.hasBehavior("openpage")) {
      url = makeActionURL("switchtab", {url: escapedURL});
      action = "switchtab";
    }

    // Always prefer the bookmark title unless it is empty
    let title = bookmarkTitle || historyTitle;

    // We will always prefer to show tags if we have them.
    let showTags = !!tags;

    // However, we'll act as if a page is not bookmarked if the user wants
    // only history and not bookmarks and there are no tags.
    if (this.hasBehavior("history") && !this.hasBehavior("bookmark") &&
        !showTags) {
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
        // If we're not suggesting bookmarks, then this shouldn't
        // display as one.
        match.style = this.hasBehavior("bookmark") ? "bookmark-tag" : "tag";
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
   * @return a string consisting of the search query to be used based on the
   * previously set urlbar suggestion preferences.
   */
  get _suggestionPrefQuery() {
    if (!this.hasBehavior("restrict") && this.hasBehavior("history") &&
        this.hasBehavior("bookmark")) {
      return this.hasBehavior("typed") ? defaultQuery("AND h.typed = 1")
                                       : defaultQuery();
    }
    let conditions = [];
    if (this.hasBehavior("history")) {
      // Enforce ignoring the visit_count index, since the frecency one is much
      // faster in this case.  ANALYZE helps the query planner to figure out the
      // faster path, but it may not have up-to-date information yet.
      conditions.push("+h.visit_count > 0");
    }
    if (this.hasBehavior("typed")) {
      conditions.push("h.typed = 1");
    }
    if (this.hasBehavior("bookmark")) {
      conditions.push("bookmarked");
    }
    if (this.hasBehavior("tag")) {
      conditions.push("tags NOTNULL");
    }

    return conditions.length ? defaultQuery("AND " + conditions.join(" AND "))
                             : defaultQuery();
  },

  /**
   * Obtains the search query to be used based on the previously set search
   * preferences (accessed by this.hasBehavior).
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _searchQuery() {
    let query = this._suggestionPrefQuery;

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

    // autoFill can only cope with history or bookmarks entries.
    if (!this.hasBehavior("history") &&
        !this.hasBehavior("bookmark"))
      return false;

    // autoFill doesn't search titles or tags.
    if (this.hasBehavior("title") || this.hasBehavior("tag"))
      return false;

    // Don't try to autofill if the search term includes any whitespace.
    // This may confuse completeDefaultIndex cause the AUTOCOMPLETE_MATCH
    // tokenizer ends up trimming the search string and returning a value
    // that doesn't match it, or is even shorter.
    if (/\s/.test(this._originalSearchString))
      return false;

    if (this._searchString.length == 0)
      return false;

    if (this._prohibitAutoFill)
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
    let bookmarked = this.hasBehavior("bookmark") && !this.hasBehavior("history");

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
   * Obtains the query to search for autoFill url results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _urlQuery() {
    // We expect this to be a full URL, not just a host. We want to extract the
    // host and use that as a guess for whether we'll get a result from a URL
    // query.
    let slashIndex = this._autofillUrlSearchString.indexOf("/");
    let revHost = this._autofillUrlSearchString.substring(0, slashIndex).toLowerCase()
                      .split("").reverse().join("") + ".";

    let typed = Prefs.autofillTyped || this.hasBehavior("typed");
    let bookmarked = this.hasBehavior("bookmark") && !this.hasBehavior("history");

    return [
      bookmarked ? typed ? SQL_BOOKMARKED_TYPED_URL_QUERY
                         : SQL_BOOKMARKED_URL_QUERY
                 : typed ? SQL_TYPED_URL_QUERY
                         : SQL_URL_QUERY,
      {
        query_type: QUERYTYPE_AUTOFILL_URL,
        searchString: this._autofillUrlSearchString,
        revHost
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
  // Make sure the preferences are initialized as soon as possible.
  // If the value of browser.urlbar.autocomplete.enabled is set to false,
  // then all the other suggest preferences for history, bookmarks and
  // open pages should be set to false.
  Prefs;
}

UnifiedComplete.prototype = {
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

        try {
           Sqlite.shutdown.addBlocker("Places UnifiedComplete.js clone closing",
                                      Task.async(function* () {
                                        SwitchToTabStorage.shutdown();
                                        yield conn.close();
                                      }));
        } catch (ex) {
          // It's too late to block shutdown, just close the connection.
          yield conn.close();
          throw ex;
        }

        // Autocomplete often fallbacks to a table scan due to lack of text
        // indices.  A larger cache helps reducing IO and improving performance.
        // The value used here is larger than the default Storage value defined
        // as MAX_CACHE_SIZE_BYTES in storage/mozStorageConnection.cpp.
        yield conn.execute("PRAGMA cache_size = -6144"); // 6MiB

        yield SwitchToTabStorage.initDatabase(conn);

        return conn;
      }.bind(this)).then(null, ex => { dump("Couldn't get database handle: " + ex + "\n");
                                       Cu.reportError(ex); });
    }
    return this._promiseDatabase;
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

    // If the previous search didn't fetch enough search suggestions, it's
    // unlikely a longer text would do.
    let prohibitSearchSuggestions =
      this._lastLowResultsSearchSuggestion &&
      searchString.length > this._lastLowResultsSearchSuggestion.length &&
      searchString.startsWith(this._lastLowResultsSearchSuggestion);

    this._currentSearch = new Search(searchString, searchParam, listener,
                                     this, this, prohibitSearchSuggestions);

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
      this._currentSearch.stop();
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
    TelemetryStopwatch.cancel(TELEMETRY_1ST_RESULT, this);
    TelemetryStopwatch.cancel(TELEMETRY_6_FIRST_RESULTS, this);
    // Clear state now to avoid race conditions, see below.
    let search = this._currentSearch;
    this._lastLowResultsSearchSuggestion = search._lastLowResultsSearchSuggestion;
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

  get searchType() {
    return Ci.nsIAutoCompleteSearchDescriptor.SEARCH_TYPE_IMMEDIATE;
  },

  get clearingAutoFillSearchesAgain() {
    return true;
  },

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
