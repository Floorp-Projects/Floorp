/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Constants

const MS_PER_DAY = 86400000; // 24 * 60 * 60 * 1000

// Match type constants.
// These indicate what type of search function we should be using.
const {
  MATCH_ANYWHERE,
  MATCH_BOUNDARY_ANYWHERE,
  MATCH_BOUNDARY,
  MATCH_BEGINNING,
  MATCH_BEGINNING_CASE_SENSITIVE,
} = Ci.mozIPlacesAutoComplete;

// Values for browser.urlbar.insertMethod
const INSERTMETHOD = {
  APPEND: 0, // Just append new results.
  MERGE_RELATED: 1, // Merge previous and current results if search strings are related
  MERGE: 2 // Always merge previous and current results
};

// Prefs are defined as [pref name, default value].
const PREF_URLBAR_BRANCH = "browser.urlbar.";
const PREF_URLBAR_DEFAULTS = new Map([
  ["autocomplete.enabled", true],
  ["autoFill", true],
  ["autoFill.typed", true],
  ["autoFill.searchEngines", false],
  ["restyleSearches", false],
  ["delay", 50],
  ["matchBehavior", MATCH_BOUNDARY_ANYWHERE],
  ["filter.javascript", true],
  ["maxRichResults", 10],
  ["suggest.history", true],
  ["suggest.bookmark", true],
  ["suggest.openpage", true],
  ["suggest.history.onlyTyped", false],
  ["suggest.searches", false],
  ["maxCharsForSearchSuggestions", 20],
  ["maxHistoricalSearchSuggestions", 0],
  ["usepreloadedtopurls.enabled", true],
  ["usepreloadedtopurls.expire_days", 14],
  ["matchBuckets", "suggestion:4,general:Infinity"],
  ["matchBucketsSearch", ""],
  ["insertMethod", INSERTMETHOD.MERGE_RELATED],
]);
const PREF_OTHER_DEFAULTS = new Map([
  ["keyword.enabled", true],
]);

// AutoComplete query type constants.
// Describes the various types of queries that we can process rows for.
const QUERYTYPE_FILTERED            = 0;
const QUERYTYPE_AUTOFILL_HOST       = 1;
const QUERYTYPE_AUTOFILL_URL        = 2;

// This separator is used as an RTL-friendly way to split the title and tags.
// It can also be used by an nsIAutoCompleteResult consumer to re-split the
// "comment" back into the title and the tag.
const TITLE_TAGS_SEPARATOR = " \u2013 ";

// Telemetry probes.
const TELEMETRY_1ST_RESULT = "PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS";
const TELEMETRY_6_FIRST_RESULTS = "PLACES_AUTOCOMPLETE_6_FIRST_RESULTS_TIME_MS";
// The default frecency value used when inserting matches with unknown frecency.
const FRECENCY_DEFAULT = 1000;

// Extensions are allowed to add suggestions if they have registered a keyword
// with the omnibox API. This is the maximum number of suggestions an extension
// is allowed to add for a given search string.
// This value includes the heuristic result.
const MAXIMUM_ALLOWED_EXTENSION_MATCHES = 6;

// After this time, we'll give up waiting for the extension to return matches.
const MAXIMUM_ALLOWED_EXTENSION_TIME_MS = 3000;

// A regex that matches "single word" hostnames for whitelisting purposes.
// The hostname will already have been checked for general validity, so we
// don't need to be exhaustive here, so allow dashes anywhere.
const REGEXP_SINGLEWORD_HOST = new RegExp("^[a-z0-9-]+$", "i");

// Regex used to match userContextId.
const REGEXP_USER_CONTEXT_ID = /(?:^| )user-context-id:(\d+)/;

// Regex used to match one or more whitespace.
const REGEXP_SPACES = /\s+/;

// The result is notified on a delay, to avoid rebuilding the panel at every match.
const NOTIFYRESULT_DELAY_MS = 16;

// Sqlite result row index constants.
const QUERYINDEX_QUERYTYPE     = 0;
const QUERYINDEX_URL           = 1;
const QUERYINDEX_TITLE         = 2;
const QUERYINDEX_BOOKMARKED    = 3;
const QUERYINDEX_BOOKMARKTITLE = 4;
const QUERYINDEX_TAGS          = 5;
const QUERYINDEX_VISITCOUNT    = 6;
const QUERYINDEX_TYPED         = 7;
const QUERYINDEX_PLACEID       = 8;
const QUERYINDEX_SWITCHTAB     = 9;
const QUERYINDEX_FRECENCY      = 10;

// The special characters below can be typed into the urlbar to either restrict
// the search to visited history, bookmarked, tagged pages; or force a match on
// just the title text or url.
const TOKEN_TO_BEHAVIOR_MAP = new Map([
  ["^", "history"],
  ["*", "bookmark"],
  ["+", "tag"],
  ["%", "openpage"],
  ["~", "typed"],
  ["$", "searches"],
  ["#", "title"],
  ["@", "url"],
]);

const MATCHTYPE = {
  HEURISTIC: "heuristic",
  GENERAL: "general",
  SUGGESTION: "suggestion",
  EXTENSION: "extension"
};

// Buckets for match insertion.
// Every time a new match is returned, we go through each bucket in array order,
// and look for the first one having available space for the given match type.
// Each bucket is an array containing the following indices:
//   0: The match type of the acceptable entries.
//   1: available number of slots in this bucket.
// There are different matchBuckets definition for different contexts, currently
// a general one (matchBuckets) and a search one (matchBucketsSearch).
//
// First buckets. Anything with an Infinity frecency ends up here.
const DEFAULT_BUCKETS_BEFORE = [
  [MATCHTYPE.HEURISTIC, 1],
  [MATCHTYPE.EXTENSION, MAXIMUM_ALLOWED_EXTENSION_MATCHES - 1],
];
// => USER DEFINED BUCKETS WILL BE INSERTED HERE <=
//
// Catch-all buckets. Anything remaining ends up here.
const DEFAULT_BUCKETS_AFTER = [
  [MATCHTYPE.SUGGESTION, Infinity],
  [MATCHTYPE.GENERAL, Infinity],
];

// If a URL starts with one of these prefixes, then we don't provide search
// suggestions for it.
const DISALLOWED_URLLIKE_PREFIXES = [
  "http", "https", "ftp"
];

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
// NB: as a slight performance optimization, we only evaluate the "bookmarked"
// condition once, and avoid evaluating "btitle" and "tags" when it is false.
function defaultQuery(conditions = "") {
  let query =
    `SELECT :query_type, h.url, h.title, ${SQL_BOOKMARK_TAGS_FRAGMENT},
            h.visit_count, h.typed, h.id, t.open_count, h.frecency
     FROM moz_places h
     LEFT JOIN moz_openpages_temp t
            ON t.url = h.url
           AND t.userContextId = :userContextId
     WHERE h.frecency <> 0
       AND CASE WHEN bookmarked
         THEN
           AUTOCOMPLETE_MATCH(:searchString, h.url,
                              IFNULL(btitle, h.title), tags,
                              h.visit_count, h.typed,
                              1, t.open_count,
                              :matchBehavior, :searchBehavior)
         ELSE
           AUTOCOMPLETE_MATCH(:searchString, h.url,
                              h.title, '',
                              h.visit_count, h.typed,
                              0, t.open_count,
                              :matchBehavior, :searchBehavior)
         END
       ${conditions}
     ORDER BY h.frecency DESC, h.id DESC
     LIMIT :maxResults`;
  return query;
}

const SQL_SWITCHTAB_QUERY =
  `SELECT :query_type, t.url, t.url, NULL, NULL, NULL, NULL, NULL, NULL,
          t.open_count, NULL
   FROM moz_openpages_temp t
   LEFT JOIN moz_places h ON h.url_hash = hash(t.url) AND h.url = t.url
   WHERE h.id IS NULL
     AND t.userContextId = :userContextId
     AND AUTOCOMPLETE_MATCH(:searchString, t.url, t.url, NULL,
                            NULL, NULL, NULL, t.open_count,
                            :matchBehavior, :searchBehavior)
   ORDER BY t.ROWID DESC
   LIMIT :maxResults`;

const SQL_ADAPTIVE_QUERY =
  `/* do not warn (bug 487789) */
   SELECT :query_type, h.url, h.title, ${SQL_BOOKMARK_TAGS_FRAGMENT},
          h.visit_count, h.typed, h.id, t.open_count, h.frecency
   FROM (
     SELECT ROUND(MAX(use_count) * (1 + (input = :search_string)), 1) AS rank,
            place_id
     FROM moz_inputhistory
     WHERE input BETWEEN :search_string AND :search_string || X'FFFF'
     GROUP BY place_id
   ) AS i
   JOIN moz_places h ON h.id = i.place_id
   LEFT JOIN moz_openpages_temp t
          ON t.url = h.url
         AND t.userContextId = :userContextId
   WHERE AUTOCOMPLETE_MATCH(NULL, h.url,
                            IFNULL(btitle, h.title), tags,
                            h.visit_count, h.typed, bookmarked,
                            t.open_count,
                            :matchBehavior, :searchBehavior)
   ORDER BY rank DESC, h.frecency DESC`;


function hostQuery(conditions = "") {
  let query =
    `/* do not warn (bug NA): not worth to index on (typed, frecency) */
     SELECT :query_type, host || '/', IFNULL(prefix, 'http://') || host || '/',
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
     SELECT :query_type, host || '/', IFNULL(prefix, 'http://') || host || '/',
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
          SELECT :query_type, h.url, NULL,
            foreign_count > 0 AS bookmarked,
            NULL, NULL, NULL, NULL, NULL, NULL, h.frecency
          FROM moz_places h
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

// Getters

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

Cu.importGlobalProperties(["fetch"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  TelemetryStopwatch: "resource://gre/modules/TelemetryStopwatch.jsm",
  Sqlite: "resource://gre/modules/Sqlite.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  ExtensionSearchHandler: "resource://gre/modules/ExtensionSearchHandler.jsm",
  PlacesSearchAutocompleteProvider: "resource://gre/modules/PlacesSearchAutocompleteProvider.jsm",
  PlacesRemoteTabsAutocompleteProvider: "resource://gre/modules/PlacesRemoteTabsAutocompleteProvider.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  ProfileAge: "resource://gre/modules/ProfileAge.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(this, "syncUsernamePref",
                                      "services.sync.username");

function setTimeout(callback, ms) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(callback, ms, timer.TYPE_ONE_SHOT);
  return timer;
}

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
  _queue: new Map(),
  // Whether we are in the process of updating the temp table.
  _updatingLevel: 0,
  get updating() {
    return this._updatingLevel > 0;
  },
  async initDatabase(conn) {
    // To reduce IO use an in-memory table for switch-to-tab tracking.
    // Note: this should be kept up-to-date with the definition in
    //       nsPlacesTables.h.
    await conn.execute(
      `CREATE TEMP TABLE moz_openpages_temp (
         url TEXT,
         userContextId INTEGER,
         open_count INTEGER,
         PRIMARY KEY (url, userContextId)
       )`);

    // Note: this should be kept up-to-date with the definition in
    //       nsPlacesTriggers.h.
    await conn.execute(
      `CREATE TEMPORARY TRIGGER moz_openpages_temp_afterupdate_trigger
       AFTER UPDATE OF open_count ON moz_openpages_temp FOR EACH ROW
       WHEN NEW.open_count = 0
       BEGIN
         DELETE FROM moz_openpages_temp
         WHERE url = NEW.url
           AND userContextId = NEW.userContextId;
       END`);

    this._conn = conn;

    // Populate the table with the current cache contents...
    for (let [userContextId, uris] of this._queue) {
      for (let uri of uris) {
        this.add(uri, userContextId).catch(Cu.reportError);
      }
    }

    // ...then clear it to avoid double additions.
    this._queue.clear();
  },

  async add(uri, userContextId) {
    if (!this._conn) {
      if (!this._queue.has(userContextId)) {
        this._queue.set(userContextId, new Set());
      }
      this._queue.get(userContextId).add(uri);
      return;
    }
    try {
      this._updatingLevel++;
      await this._conn.executeCached(
        `INSERT OR REPLACE INTO moz_openpages_temp (url, userContextId, open_count)
          VALUES ( :url,
                    :userContextId,
                    IFNULL( ( SELECT open_count + 1
                              FROM moz_openpages_temp
                              WHERE url = :url
                              AND userContextId = :userContextId ),
                            1
                          )
                  )
        `, { url: uri.spec, userContextId });
    } finally {
      this._updatingLevel--;
    }
  },

  async delete(uri, userContextId) {
    if (!this._conn) {
      if (!this._queue.has(userContextId)) {
        throw new Error("Unknown userContextId!");
      }

      this._queue.get(userContextId).delete(uri);
      if (this._queue.get(userContextId).size == 0) {
        this._queue.delete(userContextId);
      }
      return;
    }
    try {
      this._updatingLevel++;
      await this._conn.executeCached(
        `UPDATE moz_openpages_temp
         SET open_count = open_count - 1
         WHERE url = :url
           AND userContextId = :userContextId
        `, { url: uri.spec, userContextId });
    } finally {
      this._updatingLevel--;
    }
  },

  shutdown() {
    this._conn = null;
    this._queue.clear();
  }
}));

/**
 * This helper keeps track of preferences and their updates.
 */
XPCOMUtils.defineLazyGetter(this, "Prefs", () => {
  let branch = Services.prefs.getBranch(PREF_URLBAR_BRANCH);
  let types = ["history", "bookmark", "openpage", "searches"];
  let prefTypes = new Map([["boolean", "Bool"], ["string", "Char"], ["number", "Int"]]);

  function readPref(pref) {
    let prefs = branch;
    let def = PREF_URLBAR_DEFAULTS.get(pref);
    if (def === undefined) {
      prefs = Services.prefs;
      def = PREF_OTHER_DEFAULTS.get(pref);
    }
    if (def === undefined)
      throw new Error("Trying to access an unknown pref " + pref);
    return prefs[`get${prefTypes.get(typeof def)}Pref`](pref, def);
  }

  function getPrefValue(pref) {
    switch (pref) {
      case "matchBuckets": {
        // Convert from pref char format to an array and add the default buckets.
        let val = readPref(pref);
        try {
          val = PlacesUtils.convertMatchBucketsStringToArray(val);
        } catch (ex) {
          val = PlacesUtils.convertMatchBucketsStringToArray(PREF_URLBAR_DEFAULTS.get(pref));
        }
        return [ ...DEFAULT_BUCKETS_BEFORE,
                ...val,
                ...DEFAULT_BUCKETS_AFTER ];
      }
      case "matchBucketsSearch": {
        // Convert from pref char format to an array and add the default buckets.
        let val = readPref(pref);
        if (val) {
          // Convert from pref char format to an array and add the default buckets.
          try {
            val = PlacesUtils.convertMatchBucketsStringToArray(val);
            return [ ...DEFAULT_BUCKETS_BEFORE,
                    ...val,
                    ...DEFAULT_BUCKETS_AFTER ];
          } catch (ex) { /* invalid format, will just return matchBuckets */ }
        }
        return store.get("matchBuckets");
      }
      case "suggest.history.onlyTyped": {
        // If history is not set, onlyTyped value should be ignored.
        return store.get("suggest.history") && readPref(pref);
      }
      case "defaultBehavior": {
        let val = 0;
        for (let type of [...types, "history.onlyTyped"]) {
          let behavior = type == "history.onlyTyped" ? "TYPED" : type.toUpperCase();
          val |= store.get("suggest." + type) &&
                      Ci.mozIPlacesAutoComplete["BEHAVIOR_" + behavior];
        }
        return val;
      }
      case "emptySearchDefaultBehavior": {
        // Further restrictions to apply for "empty searches" (searching for "").
        // The empty behavior is typed history, if history is enabled. Otherwise,
        // it is bookmarks, if they are enabled. If both history and bookmarks are
        // disabled, it defaults to open pages.
        let val = Ci.mozIPlacesAutoComplete.BEHAVIOR_RESTRICT;
        if (store.get("suggest.history")) {
          val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY |
                Ci.mozIPlacesAutoComplete.BEHAVIOR_TYPED;
        } else if (store.get("suggest.bookmark")) {
          val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_BOOKMARK;
        } else {
          val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_OPENPAGE;
        }
        return val;
      }
      case "matchBehavior": {
        // Validate matchBehavior; default to MATCH_BOUNDARY_ANYWHERE.
        let val = readPref(pref);
        if (![MATCH_ANYWHERE, MATCH_BOUNDARY, MATCH_BEGINNING].includes(val)) {
          val = MATCH_BOUNDARY_ANYWHERE;
        }
        return val;
      }
    }
    return readPref(pref);
  }

  // Used to keep some pref values linked.
  // TODO: remove autocomplete.enabled and rely only on suggest.* prefs once we
  // can drop legacy add-ons compatibility.
  let linkingPrefs = false;
  function updateLinkedPrefs(changedPref = "") {
    // Avoid re-entrance.
    if (linkingPrefs)
      return;
    linkingPrefs = true;
    try {
      if (changedPref.startsWith("suggest.")) {
        // A suggest pref changed, fix autocomplete.enabled.
        branch.setBoolPref("autocomplete.enabled",
                          types.some(type => store.get("suggest." + type)));
      } else if (store.get("autocomplete.enabled")) {
        // If autocomplete is enabled and all of the suggest.* prefs are disabled,
        // reset the suggest.* prefs to their default value.
        if (types.every(type => !store.get("suggest." + type))) {
          for (let type of types) {
            let def = PREF_URLBAR_DEFAULTS.get("suggest." + type);
            branch.setBoolPref("suggest." + type, def);
          }
        }
      } else {
        // If autocomplete is disabled, deactivate all suggest preferences.
        for (let type of types) {
          branch.setBoolPref("suggest." + type, false);
        }
      }
    } finally {
      linkingPrefs = false;
    }
  }

  let store = {
    _map: new Map(),
    get(pref) {
      if (!this._map.has(pref))
        this._map.set(pref, getPrefValue(pref));
      return this._map.get(pref);
    },
    observe(subject, topic, data) {
      let pref = data.replace(PREF_URLBAR_BRANCH, "");
      if (!PREF_URLBAR_DEFAULTS.has(pref) && !PREF_OTHER_DEFAULTS.has(pref))
        return;
      this._map.delete(pref);
      // Some prefs may influence others.
      if (pref == "matchBuckets") {
        this._map.delete("matchBucketsSearch");
      } else if (pref == "suggest.history") {
        this._map.delete("suggest.history.onlyTyped");
      }
      if (pref == "autocomplete.enabled" || pref.startsWith("suggest.")) {
        this._map.delete("defaultBehavior");
        this._map.delete("emptySearchDefaultBehavior");
        updateLinkedPrefs(pref);
      }
    },
    QueryInterface: ChromeUtils.generateQI([
      Ci.nsIObserver,
      Ci.nsISupportsWeakReference
    ])
  };
  Services.prefs.addObserver(PREF_URLBAR_BRANCH, store, true);
  Services.prefs.addObserver("keyword.enabled", store, true);

  // On startup we must check that some prefs are linked.
  updateLinkedPrefs();
  return store;
});

// Preloaded Sites related

function PreloadedSite(url, title) {
  this.uri = Services.io.newURI(url);
  this.title = title;
  this._matchTitle = title.toLowerCase();
  this._hasWWW = this.uri.host.startsWith("www.");
  this._hostWithoutWWW = this._hasWWW ? this.uri.host.slice(4)
                                      : this.uri.host;
}

/**
 * Storage object for Preloaded Sites.
 *   add(url, title): adds a site to storage
 *   populate(sites) : populates the  storage with array of [url,title]
 *   sites[]: resulting array of sites (PreloadedSite objects)
 */
XPCOMUtils.defineLazyGetter(this, "PreloadedSiteStorage", () => Object.seal({
  sites: [],

  add(url, title) {
    let site = new PreloadedSite(url, title);
    this.sites.push(site);
  },

  populate(sites) {
    for (let site of sites) {
      this.add(site[0], site[1]);
    }
  },
}));

XPCOMUtils.defineLazyGetter(this, "ProfileAgeCreatedPromise", () => {
  return (new ProfileAge(null, null)).created;
});

// Helper functions

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
function getUnfilteredSearchTokens(searchString) {
  return searchString.length ? searchString.split(REGEXP_SPACES) : [];
}

/**
 * Strip prefixes from the URI that we don't care about for searching.
 *
 * @param spec
 *        The text to modify.
 * @return the modified spec.
 */
function stripPrefix(spec) {
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
 * @param trimSlash
 *        Whether to trim the trailing slash.
 * @return the modified spec.
 */
function stripHttpAndTrim(spec, trimSlash = true) {
  if (spec.startsWith("http://")) {
    spec = spec.slice(7);
  }
  if (spec.endsWith("?")) {
    spec = spec.slice(0, -1);
  }
  if (trimSlash && spec.endsWith("/")) {
    spec = spec.slice(0, -1);
  }
  return spec;
}

/**
 * Returns the key to be used for a match in a map for the purposes of removing
 * duplicate entries - any 2 URLs that should be considered the same should
 * return the same key. For some moz-action URLs this will unwrap the params
 * and return a key based on the wrapped URL.
 */
function makeKeyForURL(match) {
  let url = match.value;
  let action = PlacesUtils.parseActionUrl(url);
  // At this stage we only consider moz-action URLs.
  if (!action || !("url" in action.params)) {
    // For autofill entries, we need to have a key based on the comment rather
    // than the value field, because the latter may have been trimmed.
    if (match.hasOwnProperty("style") && match.style.includes("autofill")) {
      url = match.comment;
    }
    return [stripHttpAndTrim(url), null];
  }
  return [stripHttpAndTrim(action.params.url), action];
}

/**
 * Returns whether the passed in string looks like a url.
 */
function looksLikeUrl(str, ignoreAlphanumericHosts = false) {
  // Single word not including special chars.
  return !REGEXP_SPACES.test(str) &&
         (["/", "@", ":", "["].some(c => str.includes(c)) ||
          (ignoreAlphanumericHosts ? /(.*\..*){3,}/.test(str) : str.includes(".")));
}

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
 *        * user-context-id: The userContextId of the selected tab.
 * @param autocompleteListener
 *        An nsIAutoCompleteObserver.
 * @param autocompleteSearch
 *        An nsIAutoCompleteSearch.
 * @param prohibitSearchSuggestions
 *        Whether search suggestions are allowed for this search.
 * @param [optional] previousResult
 *        The result object from the previous search. if available.
 */
function Search(searchString, searchParam, autocompleteListener,
                autocompleteSearch, prohibitSearchSuggestions, previousResult) {
  // We want to store the original string for case sensitive searches.
  this._originalSearchString = searchString;
  this._trimmedOriginalSearchString = searchString.trim();
  let strippedOriginalSearchString =
    stripPrefix(this._trimmedOriginalSearchString.toLowerCase());
  this._searchString =
    Services.textToSubURI.unEscapeURIForUI("UTF-8", strippedOriginalSearchString);

  // The protocol and the host are lowercased by nsIURI, so it's fine to
  // lowercase the typed prefix, to add it back to the results later.
  this._strippedPrefix = this._trimmedOriginalSearchString.slice(
    0, this._trimmedOriginalSearchString.length - strippedOriginalSearchString.length
  ).toLowerCase();

  this._matchBehavior = Prefs.get("matchBehavior");
  // Set the default behavior for this search.
  this._behavior = this._searchString ? Prefs.get("defaultBehavior")
                                      : Prefs.get("emptySearchDefaultBehavior");

  let params = new Set(searchParam.split(" "));
  this._enableActions = params.has("enable-actions");
  this._disablePrivateActions = params.has("disable-private-actions");
  this._inPrivateWindow = params.has("private-window");
  this._prohibitAutoFill = params.has("prohibit-autofill");

  let userContextId = searchParam.match(REGEXP_USER_CONTEXT_ID);
  this._userContextId = userContextId ?
                          parseInt(userContextId[1], 10) :
                          Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;

  this._searchTokens =
    this.filterTokens(getUnfilteredSearchTokens(this._searchString));

  this._prohibitSearchSuggestions = prohibitSearchSuggestions;

  this._listener = autocompleteListener;
  this._autocompleteSearch = autocompleteSearch;

  // Create a new result to add eventual matches.  Note we need a result
  // regardless having matches.
  let result = previousResult ||
               Cc["@mozilla.org/autocomplete/simple-result;1"]
                 .createInstance(Ci.nsIAutoCompleteSimpleResult);
  result.setSearchString(searchString);
  result.setListener({
    onValueRemoved(result, spec, removeFromDB) {
      if (removeFromDB) {
        PlacesUtils.history.remove(spec).catch(Cu.reportError);
      }
    },
    QueryInterface: ChromeUtils.generateQI([
      Ci.nsIAutoCompleteSimpleResultListener
    ])
  });
  // Will be set later, if needed.
  result.setDefaultIndex(-1);
  this._result = result;

  this._previousSearchMatchTypes = [];
  for (let i = 0; previousResult && i < previousResult.matchCount; ++i) {
    let style = previousResult.getStyleAt(i);
    if (style.includes("heuristic")) {
      this._previousSearchMatchTypes.push(MATCHTYPE.HEURISTIC);
    } else if (style.includes("suggestion")) {
      this._previousSearchMatchTypes.push(MATCHTYPE.SUGGESTION);
    } else if (style.includes("extension")) {
      this._previousSearchMatchTypes.push(MATCHTYPE.EXTENSION);
    } else {
      this._previousSearchMatchTypes.push(MATCHTYPE.GENERAL);
    }
  }

  // This is a replacement for this._result.matchCount, to be used when you need
  // to check how many "current" matches have been inserted.
  // Indeed this._result.matchCount may include matches from the previous search.
  this._currentMatchCount = 0;

  // These are used to avoid adding duplicate entries to the results.
  this._usedURLs = [];
  this._usedPlaceIds = new Set();

  // Counters for the number of matches per MATCHTYPE.
  this._counts = Object.values(MATCHTYPE)
                       .reduce((o, p) => { o[p] = 0; return o; }, {});

  this._searchStringHasWWW = this._strippedPrefix.endsWith("www.");
  this._searchStringWWW = this._searchStringHasWWW ? "www." : "";
  this._searchStringFromWWW = this._searchStringWWW + this._searchString;

  this._searchStringSchemeFound = this._strippedPrefix.match(/^(\w+):/i);
  this._searchStringScheme = this._searchStringSchemeFound ?
                             this._searchStringSchemeFound[1].toLowerCase() : "";
}

Search.prototype = {
  /**
   * Enables the desired AutoComplete behavior.
   *
   * @param type
   *        The behavior type to set.
   */
  setBehavior(type) {
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
  hasBehavior(type) {
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
  _sleepResolve: null,
  _sleep(aTimeMs) {
    // Reuse a single instance to try shaving off some usless work before
    // the first query.
    if (!this._sleepTimer)
      this._sleepTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    return new Promise(resolve => {
      this._sleepResolve = resolve;
      this._sleepTimer.initWithCallback(resolve, aTimeMs,
                                        Ci.nsITimer.TYPE_ONE_SHOT);
    });
  },

  /**
   * Given an array of tokens, this function determines which query should be
   * ran.  It also removes any special search tokens.
   *
   * @param tokens
   *        An array of search tokens.
   * @return the filtered list of tokens to search with.
   */
  filterTokens(tokens) {
    let foundToken = false;
    // Set the proper behavior while filtering tokens.
    for (let i = tokens.length - 1; i >= 0; i--) {
      let behavior = TOKEN_TO_BEHAVIOR_MAP.get(tokens[i]);
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
    if (!Prefs.get("filter.javascript")) {
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
    // Avoid multiple calls or re-entrance.
    if (!this.pending)
      return;
    if (this._notifyTimer)
      this._notifyTimer.cancel();
    this._notifyDelaysCount = 0;
    if (this._sleepTimer)
      this._sleepTimer.cancel();
    if (this._sleepResolve) {
      this._sleepResolve();
      this._sleepResolve = null;
    }
    if (this._searchSuggestionController) {
      this._searchSuggestionController.stop();
      this._searchSuggestionController = null;
    }
    if (typeof this.interrupt == "function") {
      this.interrupt();
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
  async execute(conn) {
    // A search might be canceled before it starts.
    if (!this.pending)
      return;

    // Used by stop() to interrupt an eventual running statement.
    this.interrupt = () => {
      // Interrupt any ongoing statement to run the search sooner.
      if (!SwitchToTabStorage.updating) {
        conn.interrupt();
      }
    };

    TelemetryStopwatch.start(TELEMETRY_1ST_RESULT, this);
    TelemetryStopwatch.start(TELEMETRY_6_FIRST_RESULTS, this);

    // Since we call the synchronous parseSubmissionURL function later, we must
    // wait for the initialization of PlacesSearchAutocompleteProvider first.
    await PlacesSearchAutocompleteProvider.ensureInitialized();
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
    let queries = [];
    // "openpage" behavior is supported by the default query.
    // _switchToTabQuery instead returns only pages not supported by history.
    if (this.hasBehavior("openpage")) {
      queries.push(this._switchToTabQuery);
    }
    queries.push(this._searchQuery);

    // Check for Preloaded Sites Expiry before Autofill
    await this._checkPreloadedSitesExpiry();

    // Add the first heuristic result, if any.  Set _addingHeuristicFirstMatch
    // to true so that when the result is added, "heuristic" can be included in
    // its style.
    this._addingHeuristicFirstMatch = true;
    let hasHeuristic = await this._matchFirstHeuristicResult(conn);
    this._addingHeuristicFirstMatch = false;
    this._cleanUpNonCurrentMatches(MATCHTYPE.HEURISTIC);
    if (!this.pending)
      return;

    // We sleep a little between adding the heuristicFirstMatch and matching
    // any other searches so we aren't kicking off potentially expensive
    // searches on every keystroke.
    // Though, if there's no heuristic result, we start searching immediately,
    // since autocomplete may be waiting for us.
    if (hasHeuristic) {
      await this._sleep(Prefs.get("delay"));
      if (!this.pending)
        return;
    }

    // Only add extension suggestions if the first token is a registered keyword
    // and the search string has characters after the first token.
    let extensionsCompletePromise = Promise.resolve();
    if (this._searchTokens.length > 0 &&
        ExtensionSearchHandler.isKeywordRegistered(this._searchTokens[0]) &&
        this._originalSearchString.length > this._searchTokens[0].length) {
      // Do not await on this, since extensions cannot notify when they are done
      // adding results, it may take too long.
      extensionsCompletePromise = this._matchExtensionSuggestions();
    } else if (ExtensionSearchHandler.hasActiveInputSession()) {
      ExtensionSearchHandler.handleInputCancelled();
    }

    let searchSuggestionsCompletePromise = Promise.resolve();
    if (this._enableActions && this._searchTokens.length > 0) {
      // Limit the string sent for search suggestions to a maximum length.
      let searchString = this._searchTokens.join(" ")
                             .substr(0, Prefs.get("maxCharsForSearchSuggestions"));
      // Avoid fetching suggestions if they are not required, private browsing
      // mode is enabled, or the search string may expose sensitive information.
      if (this.hasBehavior("searches") && !this._inPrivateWindow &&
          !this._prohibitSearchSuggestionsFor(searchString)) {
        searchSuggestionsCompletePromise = this._matchSearchSuggestions(searchString);
        if (this.hasBehavior("restrict")) {
          // Wait for the suggestions to be added.
          await searchSuggestionsCompletePromise;
          this._cleanUpNonCurrentMatches(MATCHTYPE.SUGGESTION);
          // We're done if we're restricting to search suggestions.
          // Notify the result completion then stop the search.
          this._autocompleteSearch.finishSearch(true);
          return;
        }
      }
    }
    // In any case, clear previous suggestions.
    searchSuggestionsCompletePromise.then(() => {
      this._cleanUpNonCurrentMatches(MATCHTYPE.SUGGESTION);
    });

    // Run the adaptive query first.
    await conn.executeCached(this._adaptiveQuery[0], this._adaptiveQuery[1],
                             this._onResultRow.bind(this));
    if (!this.pending)
      return;

    // Then fetch remote tabs.
    if (this._enableActions && this.hasBehavior("openpage")) {
      await this._matchRemoteTabs();
      if (!this.pending)
        return;
    }

    // Finally run all the other queries.
    for (let [query, params] of queries) {
      await conn.executeCached(query, params, this._onResultRow.bind(this));
      if (!this.pending)
        return;
    }

    // Ideally we should wait until MATCH_BOUNDARY_ANYWHERE, but that query
    // may be really slow and we may end up showing old results for too long.
    this._cleanUpNonCurrentMatches(MATCHTYPE.GENERAL);

    // If we do not have enough results, and our match type is
    // MATCH_BOUNDARY_ANYWHERE, search again with MATCH_ANYWHERE to get more
    // results.
    let count = this._counts[MATCHTYPE.GENERAL] + this._counts[MATCHTYPE.HEURISTIC];
    if (this._matchBehavior == MATCH_BOUNDARY_ANYWHERE &&
        count < Prefs.get("maxRichResults")) {
      this._matchBehavior = MATCH_ANYWHERE;
      for (let [query, params] of [ this._adaptiveQuery,
                                    this._searchQuery ]) {
        await conn.executeCached(query, params, this._onResultRow.bind(this));
        if (!this.pending)
          return;
      }
    }

    this._matchPreloadedSites();

    // Ensure to fill any remaining space.
    await searchSuggestionsCompletePromise;
    await extensionsCompletePromise;
  },

  async _checkPreloadedSitesExpiry() {
    if (!Prefs.get("usepreloadedtopurls.enabled"))
      return;
    let profileCreationDate = await ProfileAgeCreatedPromise;
    let daysSinceProfileCreation = (Date.now() - profileCreationDate) / MS_PER_DAY;
    if (daysSinceProfileCreation > Prefs.get("usepreloadedtopurls.expire_days"))
      Services.prefs.setBoolPref("browser.urlbar.usepreloadedtopurls.enabled", false);
  },

  _matchPreloadedSites() {
    if (!Prefs.get("usepreloadedtopurls.enabled"))
      return;

    // In case user typed just "https://" or "www." or "https://www."
    // - we do not put out the whole lot of sites
    if (!this._searchString)
      return;

    if (!(this._searchStringScheme === "" ||
          this._searchStringScheme === "https" ||
          this._searchStringScheme === "http"))
      return;

    let strictMatches = [];
    let looseMatches = [];

    for (let site of PreloadedSiteStorage.sites) {
      if (this._searchStringScheme && this._searchStringScheme !== site.uri.scheme)
        continue;
      let match = {
        value: site.uri.spec,
        comment: site.title,
        style: "preloaded-top-site",
        frecency: FRECENCY_DEFAULT - 1,
      };
      if (site.uri.host.includes(this._searchStringFromWWW) ||
          site._matchTitle.includes(this._searchStringFromWWW)) {
        strictMatches.push(match);
      } else if (site.uri.host.includes(this._searchString) ||
                 site._matchTitle.includes(this._searchString)) {
        looseMatches.push(match);
      }
    }
    for (let match of [...strictMatches, ...looseMatches]) {
      this._addMatch(match);
    }
  },

  _matchPreloadedSiteForAutofill() {
    if (!Prefs.get("usepreloadedtopurls.enabled"))
      return false;

    if (!(this._searchStringScheme === "" ||
          this._searchStringScheme === "https" ||
          this._searchStringScheme === "http"))
      return false;

    let searchStringSchemePrefix = this._searchStringScheme
                                   ? (this._searchStringScheme + "://")
                                   : "";

    // If search string has scheme - we'll match it strictly
    function matchScheme(site, search) {
      return !search._searchStringScheme ||
             search._searchStringScheme === site.uri.scheme;
    }

    // First we try to strict-match
    // If search string has "www."- we try to strict-match it along with "www."
    function matchStrict(site) {
      return site.uri.host.startsWith(this._searchStringFromWWW)
             && matchScheme(site, this);
    }
    let site = PreloadedSiteStorage.sites.find(matchStrict, this);
    if (site) {
      let match = {
        // We keep showing prefix that user typed, then what we match on
        value: searchStringSchemePrefix + site.uri.host + "/",
        style: "autofill preloaded-top-site",
        finalCompleteValue: site.uri.spec,
        frecency: Infinity
      };
      this._result.setDefaultIndex(0);
      this._addMatch(match);
      return true;
    }

    // If no strict result found - we try loose match
    // regardless of "www." in Preloaded-sites or search string
    function matchLoose(site) {
      return site._hostWithoutWWW.startsWith(this._searchString)
             && matchScheme(site, this);
    }
    site = PreloadedSiteStorage.sites.find(matchLoose, this);
    if (site) {
      let match = {
        // We keep showing prefix that user typed, then what we match on
        value: searchStringSchemePrefix + this._searchStringWWW +
               site._hostWithoutWWW + "/",
        style: "autofill preloaded-top-site",
        // On loose match, result should always have "www."
        finalCompleteValue: site.uri.scheme + "://www." +
                            site._hostWithoutWWW + "/",
        frecency: Infinity
      };
      this._result.setDefaultIndex(0);
      this._addMatch(match);
      return true;
    }

    return false;
  },

  async _matchFirstHeuristicResult(conn) {
    // We always try to make the first result a special "heuristic" result.  The
    // heuristics below determine what type of result it will be, if any.

    let hasSearchTerms = this._searchTokens.length > 0;

    if (hasSearchTerms) {
      // It may be a keyword registered by an extension.
      let matched = await this._matchExtensionHeuristicResult();
      if (matched) {
        return true;
      }
    }

    if (this._enableActions && hasSearchTerms) {
      // It may be a search engine with an alias - which works like a keyword.
      let matched = await this._matchSearchEngineAlias();
      if (matched) {
        return true;
      }
    }

    if (this.pending && hasSearchTerms) {
      // It may be a Places keyword.
      let matched = await this._matchPlacesKeyword();
      if (matched) {
        return true;
      }
    }

    let shouldAutofill = this._shouldAutofill;
    if (this.pending && shouldAutofill) {
      // It may also look like a URL we know from the database.
      let matched = await this._matchKnownUrl(conn);
      if (matched) {
        return true;
      }
    }

    if (this.pending && shouldAutofill) {
      // Or it may look like a URL we know about from search engines.
      let matched = await this._matchSearchEngineUrl();
      if (matched) {
        return true;
      }
    }

    if (this.pending && shouldAutofill) {
      let matched = this._matchPreloadedSiteForAutofill();
      if (matched) {
        return true;
      }
    }

    if (this.pending && hasSearchTerms && this._enableActions) {
      // If we don't have a result that matches what we know about, then
      // we use a fallback for things we don't know about.

      // We may not have auto-filled, but this may still look like a URL.
      // However, even if the input is a valid URL, we may not want to use
      // it as such. This can happen if the host would require whitelisting,
      // but isn't in the whitelist.
      let matched = await this._matchUnknownUrl();
      if (matched) {
        // Since we can't tell if this is a real URL and
        // whether the user wants to visit or search for it,
        // we always provide an alternative searchengine match.
        try {
          new URL(this._originalSearchString);
        } catch (ex) {
          if (Prefs.get("keyword.enabled") && !looksLikeUrl(this._originalSearchString, true)) {
            this._addingHeuristicFirstMatch = false;
            await this._matchCurrentSearchEngine();
            this._addingHeuristicFirstMatch = true;
          }
        }
        return true;
      }
    }

    if (this.pending && this._enableActions && this._originalSearchString) {
      // When all else fails, and the search string is non-empty, we search
      // using the current search engine.
      let matched = await this._matchCurrentSearchEngine();
      if (matched) {
        return true;
      }
    }

    return false;
  },

  _matchSearchSuggestions(searchString) {
    this._searchSuggestionController =
      PlacesSearchAutocompleteProvider.getSuggestionController(
        searchString,
        this._inPrivateWindow,
        Prefs.get("maxHistoricalSearchSuggestions"),
        Prefs.get("maxRichResults") - Prefs.get("maxHistoricalSearchSuggestions"),
        this._userContextId
      );
    return this._searchSuggestionController.fetchCompletePromise.then(() => {
      // The search has been canceled already.
      if (!this._searchSuggestionController)
        return;
      if (this._searchSuggestionController.resultsCount >= 0 &&
          this._searchSuggestionController.resultsCount < 2) {
        // The original string is used to properly compare with the next search.
        this._lastLowResultsSearchSuggestion = this._originalSearchString;
      }
      while (this.pending) {
        let result = this._searchSuggestionController.consume();
        if (!result)
          break;
        let { match, suggestion, historical } = result;
        if (!looksLikeUrl(suggestion)) {
          // Don't include the restrict token, if present.
          let searchString = this._searchTokens.join(" ");
          this._addSearchEngineMatch(match, searchString, suggestion, historical);
        }
      }
    }).catch(Cu.reportError);
  },

  _prohibitSearchSuggestionsFor(searchString) {
    if (this._prohibitSearchSuggestions)
      return true;

    // Suggestions for a single letter are unlikely to be useful.
    if (searchString.length < 2)
      return true;

    // The first token may be a whitelisted host.
    if (this._searchTokens.length == 1 &&
        REGEXP_SINGLEWORD_HOST.test(this._searchTokens[0]) &&
        Services.uriFixup.isDomainWhitelisted(this._searchTokens[0], -1)) {
      return true;
    }

    // Disallow fetching search suggestions for strings that start off looking
    // like urls.
    if (DISALLOWED_URLLIKE_PREFIXES.some(prefix => this._trimmedOriginalSearchString == prefix) ||
        DISALLOWED_URLLIKE_PREFIXES.some(prefix => this._trimmedOriginalSearchString.startsWith(prefix + ":"))) {
      return true;
    }

    // Disallow fetching search suggestions for strings looking like URLs, to
    // avoid disclosing information about networks or passwords.
    return this._searchTokens.some(looksLikeUrl);
  },

  async _matchKnownUrl(conn) {
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
        await conn.executeCached(query, params, (row, cancel) => {
          gotResult = true;
          this._onResultRow(row, cancel);
        });
        return gotResult;
      }
      return false;
    }

    let gotResult = false;
    let [ query, params ] = this._hostQuery;
    await conn.executeCached(query, params, (row, cancel) => {
      gotResult = true;
      this._onResultRow(row, cancel);
    });
    return gotResult;
  },

  _matchExtensionHeuristicResult() {
    if (ExtensionSearchHandler.isKeywordRegistered(this._searchTokens[0]) &&
        this._originalSearchString.length > this._searchTokens[0].length) {
      let description = ExtensionSearchHandler.getDescription(this._searchTokens[0]);
      this._addExtensionMatch(this._originalSearchString, description);
      return true;
    }
    return false;
  },

  async _matchPlacesKeyword() {
    // The first word could be a keyword, so that's what we'll search.
    let keyword = this._searchTokens[0];
    let entry = await PlacesUtils.keywords.fetch(this._searchTokens[0]);
    if (!entry)
      return false;

    let searchString = this._trimmedOriginalSearchString.substr(keyword.length + 1);

    let url = null, postData = null;
    try {
      [url, postData] =
        await BrowserUtils.parseUrlAndPostData(entry.url.href,
                                               entry.postData,
                                               searchString);
    } catch (ex) {
      // It's not possible to bind a param to this keyword.
      return false;
    }

    let style = "keyword";
    let value = url;
    if (this._enableActions) {
      style = "action " + style;
      value = PlacesUtils.mozActionURI("keyword", {
        url,
        input: this._originalSearchString,
        postData
      });
    }
    // The title will end up being "host: queryString"
    let comment = entry.url.host;

    this._addMatch({
      value,
      comment,
      // Don't use the url with replaced strings, since the icon doesn't change
      // but the string does, it may cause pointless icon flicker on typing.
      icon: "page-icon:" + entry.url.href,
      style,
      frecency: Infinity
    });
    return true;
  },

  async _matchSearchEngineUrl() {
    if (!Prefs.get("autoFill.searchEngines"))
      return false;

    let match = await PlacesSearchAutocompleteProvider.findMatchByToken(
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
      let prefixURI = Services.io.newURI(this._strippedPrefix + match.token);
      let finalURI = Services.io.newURI(match.url);
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
      Cu.reportError(`Trying to inline complete in-the-middle
                      ${this._originalSearchString} to ${value}`);
      return false;
    }

    this._result.setDefaultIndex(0);
    this._addMatch({
      value,
      comment: match.engineName,
      icon: match.iconUrl,
      style: "priority-search",
      finalCompleteValue: match.url,
      frecency: Infinity
    });
    return true;
  },

  async _matchSearchEngineAlias() {
    if (this._searchTokens.length < 1)
      return false;

    let alias = this._searchTokens[0];
    let match = await PlacesSearchAutocompleteProvider.findMatchByAlias(alias);
    if (!match)
      return false;

    match.engineAlias = alias;
    let query = this._trimmedOriginalSearchString.substr(alias.length + 1);

    this._addSearchEngineMatch(match, query);
    return true;
  },

  async _matchCurrentSearchEngine() {
    let match = await PlacesSearchAutocompleteProvider.getDefaultMatch();
    if (!match)
      return false;

    let query = this._originalSearchString;
    this._addSearchEngineMatch(match, query);
    return true;
  },

  _addExtensionMatch(content, comment) {
    let count = this._counts[MATCHTYPE.EXTENSION] + this._counts[MATCHTYPE.HEURISTIC];
    if (count >= MAXIMUM_ALLOWED_EXTENSION_MATCHES) {
      return;
    }

    this._addMatch({
      value: PlacesUtils.mozActionURI("extension", {
        content,
        keyword: this._searchTokens[0]
      }),
      comment,
      icon: "chrome://browser/content/extension.svg",
      style: "action extension",
      frecency: Infinity,
      type: MATCHTYPE.EXTENSION
    });
  },

  _addSearchEngineMatch(searchMatch, query, suggestion = "", historical = false) {
    let actionURLParams = {
      engineName: searchMatch.engineName,
      input: suggestion || this._originalSearchString,
      searchQuery: query,
    };
    if (suggestion)
      actionURLParams.searchSuggestion = suggestion;
    if (searchMatch.engineAlias) {
      actionURLParams.alias = searchMatch.engineAlias;
    }
    let value = PlacesUtils.mozActionURI("searchengine", actionURLParams);
    let match = {
      value,
      comment: searchMatch.engineName,
      icon: searchMatch.iconUrl,
      style: "action searchengine",
      frecency: FRECENCY_DEFAULT
    };
    if (suggestion) {
      match.style += " suggestion";
      match.type = MATCHTYPE.SUGGESTION;
    }

    this._addMatch(match);
  },

  _matchExtensionSuggestions() {
    let promise = ExtensionSearchHandler.handleSearch(this._searchTokens[0], this._originalSearchString,
      suggestions => {
        for (let suggestion of suggestions) {
          let content = `${this._searchTokens[0]} ${suggestion.content}`;
          this._addExtensionMatch(content, suggestion.description);
        }
      }
    );
    // Remove previous search matches sooner than the maximum timeout, otherwise
    // matches may appear stale for a long time.
    // This is necessary because WebExtensions don't have a method to notify
    // that they are done providing results, so they could be pending forever.
    setTimeout(() => this._cleanUpNonCurrentMatches(MATCHTYPE.EXTENSION), 100);

    // Since the extension has no way to signale when it's done pushing
    // results, we add a timeout racing with the addition.
    let timeoutPromise = new Promise(resolve => {
      setTimeout(resolve, MAXIMUM_ALLOWED_EXTENSION_TIME_MS);
    });
    return Promise.race([timeoutPromise, promise]).catch(Cu.reportError);
  },

  async _matchRemoteTabs() {
    // Bail out early for non-sync users.
    if (!syncUsernamePref) {
      return;
    }
    let matches = await PlacesRemoteTabsAutocompleteProvider.getMatches(this._originalSearchString);
    for (let {url, title, icon, deviceName} of matches) {
      // It's rare that Sync supplies the icon for the page (but if it does, it
      // is a string URL)
      if (!icon) {
        icon = "page-icon:" + url;
      } else {
        icon = PlacesUtils.favicons
                          .getFaviconLinkForIcon(Services.io.newURI(icon)).spec;
      }

      let match = {
        // We include the deviceName in the action URL so we can render it in
        // the URLBar.
        value: PlacesUtils.mozActionURI("remotetab", { url, deviceName }),
        comment: title || url,
        style: "action remotetab",
        // we want frecency > FRECENCY_DEFAULT so it doesn't get pushed out
        // by "remote" matches.
        frecency: FRECENCY_DEFAULT + 1,
        icon,
      };
      this._addMatch(match);
    }
  },

  // TODO (bug 1054814): Use visited URLs to inform which scheme to use, if the
  // scheme isn't specificed.
  _matchUnknownUrl() {
    let flags = Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
                Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    let fixupInfo = null;
    try {
      fixupInfo = Services.uriFixup.getFixupURIInfo(this._originalSearchString,
                                                    flags);
    } catch (e) {
      if (e.result == Cr.NS_ERROR_MALFORMED_URI && !Prefs.get("keyword.enabled")) {
        let value = PlacesUtils.mozActionURI("visiturl", {
          url: this._originalSearchString,
          input: this._originalSearchString,
        });
        this._addMatch({
          value,
          comment: this._originalSearchString,
          style: "action visiturl",
          frecency: Infinity
        });

        return true;
      }
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
    let hostExpected = ["http", "https", "ftp", "chrome"].includes(uri.scheme);
    if (hostExpected && !uri.host)
      return false;

    // getFixupURIInfo() escaped the URI, so it may not be pretty.  Embed the
    // escaped URL in the action URI since that URL should be "canonical".  But
    // pass the pretty, unescaped URL as the match comment, since it's likely
    // to be displayed to the user, and in any case the front-end should not
    // rely on it being canonical.
    let escapedURL = uri.displaySpec;
    let displayURL = Services.textToSubURI.unEscapeURIForUI("UTF-8", escapedURL);

    let value = PlacesUtils.mozActionURI("visiturl", {
      url: escapedURL,
      input: this._originalSearchString,
    });

    let match = {
      value,
      comment: displayURL,
      style: "action visiturl",
      frecency: Infinity
    };

    // We don't know if this url is in Places or not, and checking that would
    // be expensive. Thus we also don't know if we may have an icon.
    // If we'd just try to fetch the icon for the typed string, we'd cause icon
    // flicker, since the url keeps changing while the user types.
    // By default we won't provide an icon, but for the subset of urls with a
    // host we'll check for a typed slash and set favicon for the host part.
    if (hostExpected &&
        (this._trimmedOriginalSearchString.endsWith("/") || uri.pathQueryRef.length > 1)) {
      match.icon = `page-icon:${uri.prePath}/`;
    }

    this._addMatch(match);
    return true;
  },

  _onResultRow(row, cancel) {
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
    let count = this._counts[MATCHTYPE.GENERAL] + this._counts[MATCHTYPE.HEURISTIC];
    if (!this.pending || count >= Prefs.get("maxRichResults"))
      cancel();
  },

  _maybeRestyleSearchMatch(match) {
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
        this._searchTokens.every(token => !terms.includes(token))) {
      return;
    }

    // Turn the match into a searchengine action with a favicon.
    match.value = PlacesUtils.mozActionURI("searchengine", {
      engineName: parseResult.engineName,
      input: parseResult.terms,
      searchQuery: parseResult.terms,
    });
    match.comment = parseResult.engineName;
    match.icon = match.icon || match.iconUrl;
    match.style = "action searchengine favicon";
  },

  _addMatch(match) {
    if (typeof match.frecency != "number")
      throw new Error("Frecency not provided");

    if (this._addingHeuristicFirstMatch)
      match.type = MATCHTYPE.HEURISTIC;
    else if (typeof match.type != "string")
      match.type = MATCHTYPE.GENERAL;

    // A search could be canceled between a query start and its completion,
    // in such a case ensure we won't notify any result for it.
    if (!this.pending)
      return;

    // For autofill entries, the comment field must be a stripped version
    // of the final destination url, so that the user will definitely know
    // where he is going to end up. For example, if the user is visiting a
    // secure page, we'll leave the https on it, to let him know that.
    // This must happen before generating the dedupe key.
    if (match.hasOwnProperty("style") && match.style.includes("autofill")) {
      // We fallback to match.value, as that's what autocomplete does if
      // finalCompleteValue is null.
      // Trim only if the value looks like a domain, we want to retain the
      // trailing slash if we're completing a url to the next slash.
      match.comment = stripHttpAndTrim(match.finalCompleteValue || match.value,
                                       !this._searchString.includes("/"));
    }

    match.style = match.style || "favicon";

    // Restyle past searches, unless they are bookmarks or special results.
    if (Prefs.get("restyleSearches") && match.style == "favicon") {
      this._maybeRestyleSearchMatch(match);
    }

    if (this._addingHeuristicFirstMatch) {
      match.style += " heuristic";
    }

    match.icon = match.icon || "";
    match.finalCompleteValue = match.finalCompleteValue || "";

    let {index, replace} = this._getInsertIndexForMatch(match);
    if (index == -1)
      return;
    if (replace) { // Replacing an existing match from the previous search.
      this._result.removeMatchAt(index);
    }
    this._result.insertMatchAt(index,
                               match.value,
                               match.comment,
                               match.icon,
                               match.style,
                               match.finalCompleteValue);
    this._currentMatchCount++;
    this._counts[match.type]++;

    if (this._currentMatchCount == 1)
      TelemetryStopwatch.finish(TELEMETRY_1ST_RESULT, this);
    if (this._currentMatchCount == 6)
      TelemetryStopwatch.finish(TELEMETRY_6_FIRST_RESULTS, this);
    this.notifyResult(true, match.type == MATCHTYPE.HEURISTIC);
  },

  _getInsertIndexForMatch(match) {
    // Check for duplicates and either discard (by returning -1) the duplicate
    // or suggest to replace the original match, in case the new one is more
    // specific (for example a Remote Tab wins over History, and a Switch to Tab
    // wins over a Remote Tab).
    // Must check both id and url, cause keywords dynamically modify the url.
    // Note: this partially fixes Bug 1222435,  but not if the urls differ more
    // than just by "http://". We should still evaluate www and other schemes
    // equivalences.
    let [urlMapKey, action] = makeKeyForURL(match);
    if ((match.placeId && this._usedPlaceIds.has(match.placeId)) ||
        this._usedURLs.map(e => e.key).includes(urlMapKey)) {
      let isDupe = true;
      if (action && ["switchtab", "remotetab"].includes(action.type)) {
        // The new entry is a switch/remote tab entry, look for the duplicate
        // among current matches.
        for (let i = 0; i < this._usedURLs.length; ++i) {
          let {key: matchKey, action: matchAction, type: matchType} = this._usedURLs[i];
          if (matchKey == urlMapKey) {
            isDupe = true;
            // Don't replace the match if the existing one is heuristic and the
            // new one is a switchtab, instead also add the switchtab match.
            if (matchType == MATCHTYPE.HEURISTIC && action.type == "switchtab") {
              isDupe = false;
              // Since we allow to insert a dupe in this case, we must continue
              // checking the next matches to be sure we won't insert more than
              // one dupe. For this same reason we must reset isDupe = true for
              // each found dupe.
              continue;
            }
            if (!matchAction || action.type == "switchtab") {
              this._usedURLs[i] = {key: urlMapKey, action, type: match.type};
              return { index:  i, replace: true };
            }
            break; // Found the duplicate, no reason to continue.
          }
        }
      }
      if (isDupe) {
        return { index: -1, replace: false };
      }
    }

    // Add this to our internal tracker to ensure duplicates do not end up in
    // the result.
    // Not all entries have a place id, thus we fallback to the url for them.
    // We cannot use only the url since keywords entries are modified to
    // include the search string, and would be returned multiple times.  Ids
    // are faster too.
    if (match.placeId)
      this._usedPlaceIds.add(match.placeId);

    let index = 0;
    // The buckets change depending on the context, that is currently decided by
    // the first added match (the heuristic one).
    if (!this._buckets) {
      // Convert the buckets to readable objects with a count property.
      let buckets = match.type == MATCHTYPE.HEURISTIC &&
                    match.style.includes("searchengine") ? Prefs.get("matchBucketsSearch")
                                                         : Prefs.get("matchBuckets");
      // - available is the number of available slots in the bucket
      // - insertIndex is the index of the first available slot in the bucket
      // - count is the number of matches in the bucket, note that it also
      //   account for matches from the previous search, while available and
      //   insertIndex don't.
      this._buckets = buckets.map(([type, available]) => ({ type,
                                                            available,
                                                            insertIndex: 0,
                                                            count: 0
                                                          }));

      // If we have matches from the previous search, we want to replace them
      // in-place to reduce flickering.
      // This requires walking the previous matches and marking their existence
      // into the current buckets, so that, when we'll add the new matches to
      // the buckets, we can either append or replace a match.
      if (this._previousSearchMatchTypes.length > 0) {
        for (let type of this._previousSearchMatchTypes) {
          for (let bucket of this._buckets) {
            if (type == bucket.type && bucket.count < bucket.available) {
              bucket.count++;
              break;
            }
          }
        }
      }
    }

    let replace = 0;
    for (let bucket of this._buckets) {
      // Move to the next bucket if the match type is incompatible, or if there
      // is no available space or if the frecency is below the threshold.
      if (match.type != bucket.type || !bucket.available) {
        index += bucket.count;
        continue;
      }

      index += bucket.insertIndex;
      bucket.available--;
      if (bucket.insertIndex < bucket.count) {
        replace = true;
      } else {
        bucket.count++;
      }
      bucket.insertIndex++;
      break;
    }
    this._usedURLs[index] = {key: urlMapKey, action, type: match.type};
    return { index, replace };
  },

  /**
   * Removes matches from a previous search, that are no more returned by the
   * current search
   * @param type
   *        The MATCHTYPE to clean up.
   * @param [optional] notify
   *        Whether to notify a result change.
   */
  _cleanUpNonCurrentMatches(type, notify = true) {
    if (this._previousSearchMatchTypes.length == 0 || !this.pending)
      return;

    let index = 0;
    let changed = false;
    if (!this._buckets) {
      // No match arrived yet, so any match of the given type should be removed
      // from the top.
      while (this._previousSearchMatchTypes[0] == type) {
        this._previousSearchMatchTypes.shift();
        this._result.removeMatchAt(0);
        changed = true;
      }
    } else {
      for (let bucket of this._buckets) {
        if (bucket.type != type) {
          index += bucket.count;
          continue;
        }
        index += bucket.insertIndex;

        while (bucket.count > bucket.insertIndex) {
          this._result.removeMatchAt(index);
          changed = true;
          bucket.count--;
        }
      }
    }
    if (changed && notify) {
      this.notifyResult(true);
    }
  },

  /**
   * If in restrict mode, cleanups non current matches for all the empty types.
   */
  cleanUpRestrictNonCurrentMatches() {
    if (this.hasBehavior("restrict") && this._previousSearchMatchTypes.length > 0) {
      for (let type of new Set(this._previousSearchMatchTypes)) {
        if (this._counts[type] == 0) {
          // Don't notify, since we are about to notify completion.
          this._cleanUpNonCurrentMatches(type, false);
        }
      }
    }
  },

  _processHostRow(row) {
    let match = {};
    let strippedHost = row.getResultByIndex(QUERYINDEX_URL);
    let url = row.getResultByIndex(QUERYINDEX_TITLE);
    let unstrippedHost = stripHttpAndTrim(url, false);
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    // If the unfixup value doesn't preserve the user's input just
    // ignore it and complete to the found host.
    if (!unstrippedHost.toLowerCase().includes(this._trimmedOriginalSearchString.toLowerCase())) {
      unstrippedHost = null;
    }

    match.value = this._strippedPrefix + strippedHost;
    match.finalCompleteValue = unstrippedHost;

    match.icon = "page-icon:" + url;

    // Although this has a frecency, this query is executed before any other
    // queries that would result in frecency matches.
    match.frecency = frecency;
    match.style = "autofill";
    return match;
  },

  _processUrlRow(row) {
    let url = row.getResultByIndex(QUERYINDEX_URL);
    let strippedUrl = stripPrefix(url);
    let prefix = url.substr(0, url.length - strippedUrl.length);
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    // We must complete the URL up to the next separator (which is /, ? or #).
    let searchString = stripPrefix(this._trimmedOriginalSearchString);
    let separatorIndex = strippedUrl.slice(searchString.length)
                                    .search(/[\/\?\#]/);
    if (separatorIndex != -1) {
      separatorIndex += searchString.length;
      if (strippedUrl[separatorIndex] == "/") {
        separatorIndex++; // Include the "/" separator
      }
      strippedUrl = strippedUrl.slice(0, separatorIndex);
    }

    let match = {
      value: this._strippedPrefix + strippedUrl,
      // Although this has a frecency, this query is executed before any other
      // queries that would result in frecency matches.
      frecency,
      style: "autofill"
    };

    // Complete to the found url only if its untrimmed value preserves the
    // user's input.
    if (url.toLowerCase().includes(this._trimmedOriginalSearchString.toLowerCase())) {
      match.finalCompleteValue = prefix + strippedUrl;
    }

    match.icon = "page-icon:" + (match.finalCompleteValue || match.value);

    return match;
  },

  _processRow(row) {
    let match = {};
    match.placeId = row.getResultByIndex(QUERYINDEX_PLACEID);
    let escapedURL = row.getResultByIndex(QUERYINDEX_URL);
    let openPageCount = row.getResultByIndex(QUERYINDEX_SWITCHTAB) || 0;
    let historyTitle = row.getResultByIndex(QUERYINDEX_TITLE) || "";
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
      url = PlacesUtils.mozActionURI("switchtab", {url: escapedURL});
      action = "switchtab";
      if (frecency == null)
        frecency = FRECENCY_DEFAULT;
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
      } else if (bookmarked) {
        match.style = "bookmark";
      }
    }

    if (action)
      match.style = "action " + action;

    match.value = url;
    match.comment = title;
    match.icon = "page-icon:" + escapedURL;
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
        userContextId: this._userContextId,
        // Limit the query to the the maximum number of desired results.
        // This way we can avoid doing more work than needed.
        maxResults: Prefs.get("maxRichResults")
      }
    ];
  },

  /**
   * Obtains the query to search for switch-to-tab entries.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _switchToTabQuery() {
    return [
      SQL_SWITCHTAB_QUERY,
      {
        query_type: QUERYTYPE_FILTERED,
        matchBehavior: this._matchBehavior,
        searchBehavior: this._behavior,
        // We only want to search the tokens that we are left with - not the
        // original search string.
        searchString: this._searchTokens.join(" "),
        userContextId: this._userContextId,
        maxResults: Prefs.get("maxRichResults")
      }
    ];
  },

  /**
   * Obtains the query to search for adaptive results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _adaptiveQuery() {
    return [
      SQL_ADAPTIVE_QUERY,
      {
        parent: PlacesUtils.tagsFolderId,
        search_string: this._searchString,
        query_type: QUERYTYPE_FILTERED,
        matchBehavior: this._matchBehavior,
        searchBehavior: this._behavior,
        userContextId: this._userContextId,
      }
    ];
  },

  /**
   * Whether we should try to autoFill.
   */
  get _shouldAutofill() {
    // First of all, check for the autoFill pref.
    if (!Prefs.get("autoFill"))
      return false;

    if (this._searchTokens.length != 1)
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
    if (REGEXP_SPACES.test(this._originalSearchString))
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
    let typed = Prefs.get("autoFill.typed") || this.hasBehavior("typed");
    let bookmarked = this.hasBehavior("bookmark") && !this.hasBehavior("history");

    let query = [];
    if (bookmarked) {
      query.push(typed ? SQL_BOOKMARKED_TYPED_HOST_QUERY
                       : SQL_BOOKMARKED_HOST_QUERY);
    } else {
      query.push(typed ? SQL_TYPED_HOST_QUERY
                       : SQL_HOST_QUERY);
    }

    query.push({
      query_type: QUERYTYPE_AUTOFILL_HOST,
      searchString: this._searchString.toLowerCase()
    });

    return query;
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
    // The URIs in the database are fixed-up, so we can match on a lowercased
    // host, but the path must be matched in a case sensitive way.
    let pathIndex = this._trimmedOriginalSearchString.indexOf("/", this._strippedPrefix.length);
    let revHost = this._trimmedOriginalSearchString
                      .substring(this._strippedPrefix.length, pathIndex)
                      .toLowerCase().split("").reverse().join("") + ".";
    let searchString = stripPrefix(
      this._trimmedOriginalSearchString.slice(0, pathIndex).toLowerCase() +
      this._trimmedOriginalSearchString.slice(pathIndex)
    );

    let typed = Prefs.get("autoFill.typed") || this.hasBehavior("typed");
    let bookmarked = this.hasBehavior("bookmark") && !this.hasBehavior("history");

    let query = [];
    if (bookmarked) {
      query.push(typed ? SQL_BOOKMARKED_TYPED_URL_QUERY
                       : SQL_BOOKMARKED_URL_QUERY);
    } else {
      query.push(typed ? SQL_TYPED_URL_QUERY
                       : SQL_URL_QUERY);
    }

    query.push({
      query_type: QUERYTYPE_AUTOFILL_URL,
      searchString,
      revHost
    });

    return query;
  },

  // The result is notified to the search listener on a timer, to chunk multiple
  // match updates together and avoid rebuilding the popup at every new match.
  _notifyTimer: null,

  /**
   * Notifies the current result to the listener.
   *
   * @param searchOngoing
   *        Indicates whether the search result should be marked as ongoing.
   * @param skipDelay
   *        Whether to notify immediately.
   */
  _notifyDelaysCount: 0,
  notifyResult(searchOngoing, skipDelay = false) {
    let notify = () => {
      if (!this.pending)
        return;
      this._notifyDelaysCount = 0;
      let resultCode = this._currentMatchCount ? "RESULT_SUCCESS" : "RESULT_NOMATCH";
      if (searchOngoing) {
        resultCode += "_ONGOING";
      }
      let result = this._result;
      result.setSearchResult(Ci.nsIAutoCompleteResult[resultCode]);
      this._listener.onSearchResult(this._autocompleteSearch, result);
      if (!searchOngoing) {
        // Break possible cycles.
        this._listener = null;
        this._autocompleteSearch = null;
        this.stop();
      }
    };
    if (this._notifyTimer) {
      this._notifyTimer.cancel();
    }
    // In the worst case, we may get evenly spaced matches that would end up
    // delaying the UI by N_MATCHES * NOTIFYRESULT_DELAY_MS. Thus, we clamp the
    // number of times we may delay matches.
    if (skipDelay || this._notifyDelaysCount > 3) {
      notify();
    } else {
      this._notifyDelaysCount++;
      this._notifyTimer = setTimeout(notify, NOTIFYRESULT_DELAY_MS);
    }
  },
};

// UnifiedComplete class
// component @mozilla.org/autocomplete/search;1?name=unifiedcomplete

function UnifiedComplete() {
  // Make sure the preferences are initialized as soon as possible.
  // If the value of browser.urlbar.autocomplete.enabled is set to false,
  // then all the other suggest preferences for history, bookmarks and
  // open pages should be set to false.
  Prefs;

  if (Prefs.get("usepreloadedtopurls.enabled")) {
    // force initializing the profile age check
    // to ensure the off-main-thread-IO happens ASAP
    // and we don't have to wait for it when doing an autocomplete lookup
    ProfileAgeCreatedPromise;

    fetch("chrome://global/content/unifiedcomplete-top-urls.json")
      .then(response => response.json())
      .then(sites => PreloadedSiteStorage.populate(sites))
      .catch(ex => Cu.reportError(ex));
  }
}

UnifiedComplete.prototype = {
  // Database handling

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
  getDatabaseHandle() {
    if (Prefs.get("autocomplete.enabled") && !this._promiseDatabase) {
      this._promiseDatabase = (async () => {
        let conn = await Sqlite.cloneStorageConnection({
          connection: PlacesUtils.history.DBConnection,
          readOnly: true
        });

        try {
           Sqlite.shutdown.addBlocker("Places UnifiedComplete.js clone closing",
                                      async () => {
                                        // Break a possible cycle through the
                                        // previous result, the controller and
                                        // ourselves.
                                        this._currentSearch = null;
                                        SwitchToTabStorage.shutdown();
                                        await conn.close();
                                      });
        } catch (ex) {
          // It's too late to block shutdown, just close the connection.
          await conn.close();
          throw ex;
        }

        // Autocomplete often fallbacks to a table scan due to lack of text
        // indices.  A larger cache helps reducing IO and improving performance.
        // The value used here is larger than the default Storage value defined
        // as MAX_CACHE_SIZE_BYTES in storage/mozStorageConnection.cpp.
        await conn.execute("PRAGMA cache_size = -6144"); // 6MiB

        await SwitchToTabStorage.initDatabase(conn);

        return conn;
      })().catch(ex => {
        dump("Couldn't get database handle: " + ex + "\n");
        Cu.reportError(ex);
      });
    }
    return this._promiseDatabase;
  },

  // mozIPlacesAutoComplete

  registerOpenPage(uri, userContextId) {
    SwitchToTabStorage.add(uri, userContextId).catch(Cu.reportError);
  },

  unregisterOpenPage(uri, userContextId) {
    SwitchToTabStorage.delete(uri, userContextId).catch(Cu.reportError);
  },

  populatePreloadedSiteStorage(json) {
    PreloadedSiteStorage.populate(json);
  },

  // nsIAutoCompleteSearch

  startSearch(searchString, searchParam, acPreviousResult, listener) {
    // Stop the search in case the controller has not taken care of it.
    if (this._currentSearch) {
      this.stopSearch();
    }

    // If the previous search didn't fetch enough search suggestions, it's
    // unlikely a longer text would do.
    let prohibitSearchSuggestions =
      !!this._lastLowResultsSearchSuggestion &&
      searchString.length > this._lastLowResultsSearchSuggestion.length &&
      searchString.startsWith(this._lastLowResultsSearchSuggestion);

    // We don't directly reuse the controller provided previousResult because:
    //  * it is only populated when the new searchString is an extension of the
    //    previous one. We want to handle the backspace case too.
    //  * Bookmarks may be titled differently than history and we want to show
    //    the right title.  For example a "foo" titled page could be bookmarked
    //    as "foox", typing "foo" followed by "x" would show the history result
    //    from the previous search (See bug 412730).
    //  * Adaptive History means a result may appear even if the previous string
    //    didn't match it.
    // What we can do is reuse the previous result along with the bucketing
    // system to avoid flickering. Since we know where a new match should be
    // positioned, we  wait for a new match to arrive before replacing the
    // previous one. This may leave stale matches from the previous search that
    // would not be returned by the current one, thus once the current search is
    // complete, we remove those stale matches with _cleanUpNonCurrentMatches().
    let previousResult = null;
    let insertMethod = Prefs.get("insertMethod");
    if (this._currentSearch && insertMethod != INSERTMETHOD.APPEND) {
      let result = this._currentSearch._result;
      // Only reuse the previous result when one of the search strings is an
      // extension of the other one.  We could expand this to any case, but
      // that may leave exposed unrelated matches for a longer time.
      let previousSearchString = result.searchString;
      let stringsRelated = previousSearchString.length > 0 &&
                           searchString.length > 0 &&
                           (previousSearchString.includes(searchString) ||
                            searchString.includes(previousSearchString));
      if (insertMethod == INSERTMETHOD.MERGE || stringsRelated) {
        previousResult = result;
      }
    }

    this._currentSearch = new Search(searchString, searchParam, listener,
                                     this, prohibitSearchSuggestions,
                                     previousResult);

    // If we are not enabled, we need to return now.  Notice we need an empty
    // result regardless, so we still create the Search object.
    if (!Prefs.get("autocomplete.enabled")) {
      this.finishSearch(true);
      return;
    }

    let search = this._currentSearch;
    this.getDatabaseHandle().then(conn => search.execute(conn))
                            .catch(ex => {
                              dump(`Query failed: ${ex}\n`);
                              Cu.reportError(ex);
                            })
                            .then(() => {
                              if (search == this._currentSearch) {
                                this.finishSearch(true);
                              }
                            });
  },

  stopSearch() {
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
  finishSearch(notify = false) {
    TelemetryStopwatch.cancel(TELEMETRY_1ST_RESULT, this);
    TelemetryStopwatch.cancel(TELEMETRY_6_FIRST_RESULTS, this);
    // Clear state now to avoid race conditions, see below.
    let search = this._currentSearch;
    if (!search)
      return;
    this._lastLowResultsSearchSuggestion = search._lastLowResultsSearchSuggestion;

    if (!notify || !search.pending)
      return;

    // If we are in restrict mode and we reused the previous search results,
    // it's possible we didn't go through all the cleanup methods due to early
    // bailouts. Thus we could still have nonmatching results to remove.
    search.cleanUpRestrictNonCurrentMatches();

    // There is a possible race condition here.
    // When a search completes it calls finishSearch that notifies results
    // here.  When the controller gets the last result it fires
    // onSearchComplete.
    // If onSearchComplete immediately starts a new search it will set a new
    // _currentSearch, and on return the execution will continue here, after
    // notifyResult.
    // Thus, ensure that notifyResult is the last call in this method,
    // otherwise you might be touching the wrong search.
    search.notifyResult(false);
  },

  // nsIAutoCompleteSearchDescriptor

  get searchType() {
    return Ci.nsIAutoCompleteSearchDescriptor.SEARCH_TYPE_IMMEDIATE;
  },

  get clearingAutoFillSearchesAgain() {
    return true;
  },

  // nsISupports

  classID: Components.ID("f964a319-397a-4d21-8be6-5cdd1ee3e3ae"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(UnifiedComplete),

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAutoCompleteSearch,
    Ci.nsIAutoCompleteSearchDescriptor,
    Ci.mozIPlacesAutoComplete,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([UnifiedComplete]);
