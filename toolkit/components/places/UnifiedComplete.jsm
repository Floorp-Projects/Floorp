/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint complexity: ["error", 53] */

"use strict";

// Constants

const MS_PER_DAY = 86400000; // 24 * 60 * 60 * 1000

// AutoComplete query type constants.
// Describes the various types of queries that we can process rows for.
const QUERYTYPE_FILTERED = 0;
const QUERYTYPE_AUTOFILL_ORIGIN = 1;
const QUERYTYPE_AUTOFILL_URL = 2;
const QUERYTYPE_ADAPTIVE = 3;

// The default frecency value used when inserting matches with unknown frecency.
const FRECENCY_DEFAULT = 1000;

// After this time, we'll give up waiting for the extension to return matches.
const MAXIMUM_ALLOWED_EXTENSION_TIME_MS = 3000;

// By default we add remote tabs that have been used less than this time ago.
// Any remaining remote tabs are added in queue if no other results are found.
const RECENT_REMOTE_TAB_THRESHOLD_MS = 259200000; // 72 hours.

// Regex used to match userContextId.
const REGEXP_USER_CONTEXT_ID = /(?:^| )user-context-id:(\d+)/;

// Regex used to match maxResults.
const REGEXP_MAX_RESULTS = /(?:^| )max-results:(\d+)/;

// Regex used to match one or more whitespace.
const REGEXP_SPACES = /\s+/;

// Regex used to strip prefixes from URLs.  See stripAnyPrefix().
const REGEXP_STRIP_PREFIX = /^[a-z]+:(?:\/){0,2}/i;

// The result is notified on a delay, to avoid rebuilding the panel at every match.
const NOTIFYRESULT_DELAY_MS = 16;

// Sqlite result row index constants.
const QUERYINDEX_QUERYTYPE = 0;
const QUERYINDEX_URL = 1;
const QUERYINDEX_TITLE = 2;
const QUERYINDEX_BOOKMARKED = 3;
const QUERYINDEX_BOOKMARKTITLE = 4;
const QUERYINDEX_TAGS = 5;
//    QUERYINDEX_VISITCOUNT    = 6;
//    QUERYINDEX_TYPED         = 7;
const QUERYINDEX_PLACEID = 8;
const QUERYINDEX_SWITCHTAB = 9;
const QUERYINDEX_FRECENCY = 10;

// This SQL query fragment provides the following:
//   - whether the entry is bookmarked (QUERYINDEX_BOOKMARKED)
//   - the bookmark title, if it is a bookmark (QUERYINDEX_BOOKMARKTITLE)
//   - the tags associated with a bookmarked entry (QUERYINDEX_TAGS)
const SQL_BOOKMARK_TAGS_FRAGMENT = `EXISTS(SELECT 1 FROM moz_bookmarks WHERE fk = h.id) AS bookmarked,
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
  let query = `SELECT :query_type, h.url, h.title, ${SQL_BOOKMARK_TAGS_FRAGMENT},
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

const SQL_SWITCHTAB_QUERY = `SELECT :query_type, t.url, t.url, NULL, NULL, NULL, NULL, NULL, NULL,
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

const SQL_ADAPTIVE_QUERY = `/* do not warn (bug 487789) */
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
   ORDER BY rank DESC, h.frecency DESC
   LIMIT :maxResults`;

// Result row indexes for originQuery()
const QUERYINDEX_ORIGIN_AUTOFILLED_VALUE = 1;
const QUERYINDEX_ORIGIN_URL = 2;
const QUERYINDEX_ORIGIN_FRECENCY = 3;

// `WITH` clause for the autofill queries.  autofill_frecency_threshold.value is
// the mean of all moz_origins.frecency values + stddevMultiplier * one standard
// deviation.  This is inlined directly in the SQL (as opposed to being a custom
// Sqlite function for example) in order to be as efficient as possible.
const SQL_AUTOFILL_WITH = `
  WITH
  frecency_stats(count, sum, squares) AS (
    SELECT
      CAST((SELECT IFNULL(value, 0.0) FROM moz_meta WHERE key = 'origin_frecency_count') AS REAL),
      CAST((SELECT IFNULL(value, 0.0) FROM moz_meta WHERE key = 'origin_frecency_sum') AS REAL),
      CAST((SELECT IFNULL(value, 0.0) FROM moz_meta WHERE key = 'origin_frecency_sum_of_squares') AS REAL)
  ),
  autofill_frecency_threshold(value) AS (
    SELECT
      CASE count
      WHEN 0 THEN 0.0
      WHEN 1 THEN sum
      ELSE (sum / count) + (:stddevMultiplier * sqrt((squares - ((sum * sum) / count)) / count))
      END
    FROM frecency_stats
  )
`;

const SQL_AUTOFILL_FRECENCY_THRESHOLD = `host_frecency >= (
  SELECT value FROM autofill_frecency_threshold
)`;

function originQuery({ select = "", where = "", having = "" }) {
  return `${SQL_AUTOFILL_WITH}
          SELECT :query_type,
                 fixed_up_host || '/',
                 IFNULL(:prefix, prefix) || moz_origins.host || '/',
                 frecency,
                 bookmarked,
                 id
          FROM (
            SELECT host,
                   host AS fixed_up_host,
                   TOTAL(frecency) AS host_frecency,
                   (
                     SELECT TOTAL(foreign_count) > 0
                     FROM moz_places
                     WHERE moz_places.origin_id = moz_origins.id
                   ) AS bookmarked
                   ${select}
            FROM moz_origins
            WHERE host BETWEEN :searchString AND :searchString || X'FFFF'
                  ${where}
            GROUP BY host
            HAVING ${having}
            UNION ALL
            SELECT host,
                   fixup_url(host) AS fixed_up_host,
                   TOTAL(frecency) AS host_frecency,
                   (
                     SELECT TOTAL(foreign_count) > 0
                     FROM moz_places
                     WHERE moz_places.origin_id = moz_origins.id
                   ) AS bookmarked
                   ${select}
            FROM moz_origins
            WHERE host BETWEEN 'www.' || :searchString AND 'www.' || :searchString || X'FFFF'
                  ${where}
            GROUP BY host
            HAVING ${having}
          ) AS grouped_hosts
          JOIN moz_origins ON moz_origins.host = grouped_hosts.host
          ORDER BY frecency DESC, id DESC
          LIMIT 1 `;
}

const QUERY_ORIGIN_HISTORY_BOOKMARK = originQuery({
  having: `bookmarked OR ${SQL_AUTOFILL_FRECENCY_THRESHOLD}`,
});

const QUERY_ORIGIN_PREFIX_HISTORY_BOOKMARK = originQuery({
  where: `AND prefix BETWEEN :prefix AND :prefix || X'FFFF'`,
  having: `bookmarked OR ${SQL_AUTOFILL_FRECENCY_THRESHOLD}`,
});

const QUERY_ORIGIN_HISTORY = originQuery({
  select: `, (
    SELECT TOTAL(visit_count) > 0
    FROM moz_places
    WHERE moz_places.origin_id = moz_origins.id
   ) AS visited`,
  having: `visited AND ${SQL_AUTOFILL_FRECENCY_THRESHOLD}`,
});

const QUERY_ORIGIN_PREFIX_HISTORY = originQuery({
  select: `, (
    SELECT TOTAL(visit_count) > 0
    FROM moz_places
    WHERE moz_places.origin_id = moz_origins.id
   ) AS visited`,
  where: `AND prefix BETWEEN :prefix AND :prefix || X'FFFF'`,
  having: `visited AND ${SQL_AUTOFILL_FRECENCY_THRESHOLD}`,
});

const QUERY_ORIGIN_BOOKMARK = originQuery({
  having: `bookmarked`,
});

const QUERY_ORIGIN_PREFIX_BOOKMARK = originQuery({
  where: `AND prefix BETWEEN :prefix AND :prefix || X'FFFF'`,
  having: `bookmarked`,
});

// Result row indexes for urlQuery()
const QUERYINDEX_URL_URL = 1;
const QUERYINDEX_URL_STRIPPED_URL = 2;
const QUERYINDEX_URL_FRECENCY = 3;

function urlQuery(where1, where2) {
  // We limit the search to places that are either bookmarked or have a frecency
  // over some small, arbitrary threshold (20) in order to avoid scanning as few
  // rows as possible.  Keep in mind that we run this query every time the user
  // types a key when the urlbar value looks like a URL with a path.
  return `/* do not warn (bug no): cannot use an index to sort */
          SELECT :query_type,
                 url,
                 :strippedURL,
                 frecency,
                 foreign_count > 0 AS bookmarked,
                 visit_count > 0 AS visited,
                 id
          FROM moz_places
          WHERE rev_host = :revHost
                ${where1}
          UNION ALL
          SELECT :query_type,
                 url,
                 :strippedURL,
                 frecency,
                 foreign_count > 0 AS bookmarked,
                 visit_count > 0 AS visited,
                 id
          FROM moz_places
          WHERE rev_host = :revHost || 'www.'
                ${where2}
          ORDER BY frecency DESC, id DESC
          LIMIT 1 `;
}

const QUERY_URL_HISTORY_BOOKMARK = urlQuery(
  `AND (bookmarked OR frecency > 20)
   AND strip_prefix_and_userinfo(url) BETWEEN :strippedURL AND :strippedURL || X'FFFF'`,
  `AND (bookmarked OR frecency > 20)
   AND strip_prefix_and_userinfo(url) BETWEEN 'www.' || :strippedURL AND 'www.' || :strippedURL || X'FFFF'`
);

const QUERY_URL_PREFIX_HISTORY_BOOKMARK = urlQuery(
  `AND (bookmarked OR frecency > 20)
   AND url BETWEEN :prefix || :strippedURL AND :prefix || :strippedURL || X'FFFF'`,
  `AND (bookmarked OR frecency > 20)
   AND url BETWEEN :prefix || 'www.' || :strippedURL AND :prefix || 'www.' || :strippedURL || X'FFFF'`
);

const QUERY_URL_HISTORY = urlQuery(
  `AND (visited OR NOT bookmarked)
   AND frecency > 20
   AND strip_prefix_and_userinfo(url) BETWEEN :strippedURL AND :strippedURL || X'FFFF'`,
  `AND (visited OR NOT bookmarked)
   AND frecency > 20
   AND strip_prefix_and_userinfo(url) BETWEEN 'www.' || :strippedURL AND 'www.' || :strippedURL || X'FFFF'`
);

const QUERY_URL_PREFIX_HISTORY = urlQuery(
  `AND (visited OR NOT bookmarked)
   AND frecency > 20
   AND url BETWEEN :prefix || :strippedURL AND :prefix || :strippedURL || X'FFFF'`,
  `AND (visited OR NOT bookmarked)
   AND frecency > 20
   AND url BETWEEN :prefix || 'www.' || :strippedURL AND :prefix || 'www.' || :strippedURL || X'FFFF'`
);

const QUERY_URL_BOOKMARK = urlQuery(
  `AND bookmarked
   AND strip_prefix_and_userinfo(url) BETWEEN :strippedURL AND :strippedURL || X'FFFF'`,
  `AND bookmarked
   AND strip_prefix_and_userinfo(url) BETWEEN 'www.' || :strippedURL AND 'www.' || :strippedURL || X'FFFF'`
);

const QUERY_URL_PREFIX_BOOKMARK = urlQuery(
  `AND bookmarked
   AND url BETWEEN :prefix || :strippedURL AND :prefix || :strippedURL || X'FFFF'`,
  `AND bookmarked
   AND url BETWEEN :prefix || 'www.' || :strippedURL AND :prefix || 'www.' || :strippedURL || X'FFFF'`
);

// Getters

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutPagesUtils: "resource://gre/modules/AboutPagesUtils.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  ExtensionSearchHandler: "resource://gre/modules/ExtensionSearchHandler.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  PlacesRemoteTabsAutocompleteProvider:
    "resource://gre/modules/PlacesRemoteTabsAutocompleteProvider.jsm",
  PlacesSearchAutocompleteProvider:
    "resource://gre/modules/PlacesSearchAutocompleteProvider.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ProfileAge: "resource://gre/modules/ProfileAge.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  Sqlite: "resource://gre/modules/Sqlite.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "syncUsernamePref",
  "services.sync.username"
);

function setTimeout(callback, ms) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(callback, ms, timer.TYPE_ONE_SHOT);
  return timer;
}

const kProtocolsWithIcons = [
  "chrome:",
  "moz-extension:",
  "about:",
  "http:",
  "https:",
  "ftp:",
];
function iconHelper(url) {
  if (typeof url == "string") {
    return kProtocolsWithIcons.some(p => url.startsWith(p))
      ? "page-icon:" + url
      : PlacesUtils.favicons.defaultFavicon.spec;
  }
  if (url && url instanceof URL && kProtocolsWithIcons.includes(url.protocol)) {
    return "page-icon:" + url.href;
  }
  return PlacesUtils.favicons.defaultFavicon.spec;
}

// Preloaded Sites related

function PreloadedSite(url, title) {
  this.uri = Services.io.newURI(url);
  this.title = title;
  this._matchTitle = title.toLowerCase();
  this._hasWWW = this.uri.host.startsWith("www.");
  this._hostWithoutWWW = this._hasWWW ? this.uri.host.slice(4) : this.uri.host;
}

/**
 * Storage object for Preloaded Sites.
 *   add(url, title): adds a site to storage
 *   populate(sites) : populates the  storage with array of [url,title]
 *   sites[]: resulting array of sites (PreloadedSite objects)
 */
XPCOMUtils.defineLazyGetter(this, "PreloadedSiteStorage", () =>
  Object.seal({
    sites: [],

    add(url, title) {
      let site = new PreloadedSite(url, title);
      this.sites.push(site);
    },

    populate(sites) {
      this.sites = [];
      for (let site of sites) {
        this.add(site[0], site[1]);
      }
    },
  })
);

XPCOMUtils.defineLazyGetter(this, "ProfileAgeCreatedPromise", async () => {
  let times = await ProfileAge();
  return times.created;
});

// Maps restriction character types to textual behaviors.
XPCOMUtils.defineLazyGetter(this, "typeToBehaviorMap", () => {
  return new Map([
    [UrlbarTokenizer.TYPE.RESTRICT_HISTORY, "history"],
    [UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK, "bookmark"],
    [UrlbarTokenizer.TYPE.RESTRICT_TAG, "tag"],
    [UrlbarTokenizer.TYPE.RESTRICT_OPENPAGE, "openpage"],
    [UrlbarTokenizer.TYPE.RESTRICT_SEARCH, "search"],
    [UrlbarTokenizer.TYPE.RESTRICT_TITLE, "title"],
    [UrlbarTokenizer.TYPE.RESTRICT_URL, "url"],
  ]);
});

XPCOMUtils.defineLazyGetter(this, "sourceToBehaviorMap", () => {
  return new Map([
    [UrlbarUtils.RESULT_SOURCE.HISTORY, "history"],
    [UrlbarUtils.RESULT_SOURCE.BOOKMARKS, "bookmark"],
    [UrlbarUtils.RESULT_SOURCE.TABS, "openpage"],
    [UrlbarUtils.RESULT_SOURCE.SEARCH, "search"],
  ]);
});

// Helper functions

/**
 * Strips the prefix from a URL and returns the prefix and the remainder of the
 * URL.  "Prefix" is defined to be the scheme and colon, plus, if present, two
 * slashes.  If the given string is not actually a URL, then an empty prefix and
 * the string itself is returned.
 *
 * @param  str
 *         The possible URL to strip.
 * @return If `str` is a URL, then [prefix, remainder].  Otherwise, ["", str].
 */
function stripAnyPrefix(str) {
  let match = REGEXP_STRIP_PREFIX.exec(str);
  if (!match) {
    return ["", str];
  }
  let prefix = match[0];
  if (prefix.length < str.length && str[prefix.length] == " ") {
    return ["", str];
  }
  return [prefix, str.substr(prefix.length)];
}

/**
 * Strips parts of a URL defined in `options`.
 *
 * @param {string} spec
 *        The text to modify.
 * @param {object} options
 * @param {boolean} options.stripHttp
 *        Whether to strip http.
 * @param {boolean} options.stripHttps
 *        Whether to strip https.
 * @param {boolean} options.stripWww
 *        Whether to strip `www.`.
 * @param {boolean} options.trimSlash
 *        Whether to trim the trailing slash.
 * @param {boolean} options.trimEmptyQuery
 *        Whether to trim a trailing `?`.
 * @param {boolean} options.trimEmptyHash
 *        Whether to trim a trailing `#`.
 * @returns {array} [modified, prefix, suffix]
 *          modified: {string} The modified spec.
 *          prefix: {string} The parts stripped from the prefix, if any.
 *          suffix: {string} The parts trimmed from the suffix, if any.
 */
function stripPrefixAndTrim(spec, options = {}) {
  let prefix = "";
  let suffix = "";
  if (options.stripHttp && spec.startsWith("http://")) {
    spec = spec.slice(7);
    prefix = "http://";
  } else if (options.stripHttps && spec.startsWith("https://")) {
    spec = spec.slice(8);
    prefix = "https://";
  }
  if (options.stripWww && spec.startsWith("www.")) {
    spec = spec.slice(4);
    prefix += "www.";
  }
  if (options.trimEmptyHash && spec.endsWith("#")) {
    spec = spec.slice(0, -1);
    suffix = "#" + suffix;
  }
  if (options.trimEmptyQuery && spec.endsWith("?")) {
    spec = spec.slice(0, -1);
    suffix = "?" + suffix;
  }
  if (options.trimSlash && spec.endsWith("/")) {
    spec = spec.slice(0, -1);
    suffix = "/" + suffix;
  }
  return [spec, prefix, suffix];
}

/**
 * Returns the key to be used for a match in a map for the purposes of removing
 * duplicate entries - any 2 matches that should be considered the same should
 * return the same key.  The type of the returned key depends on the type of the
 * match, so don't assume you can compare keys using ==.  Instead, use
 * ObjectUtils.deepEqual().
 *
 * @param   {object} match
 *          The match object.
 * @returns {value} Some opaque key object.  Use ObjectUtils.deepEqual() to
 *          compare keys.
 */
function makeKeyForMatch(match) {
  // For autofill entries, we need to have a key based on the finalCompleteValue
  // rather than the value field, because the latter may have been trimmed.
  let key, prefix;
  if (match.style && match.style.includes("autofill")) {
    [key, prefix] = stripPrefixAndTrim(match.finalCompleteValue, {
      stripHttp: true,
      stripHttps: true,
      stripWww: true,
      trimEmptyQuery: true,
      trimSlash: true,
    });

    return [key, prefix, null];
  }

  let action = PlacesUtils.parseActionUrl(match.value);
  if (!action) {
    [key, prefix] = stripPrefixAndTrim(match.value, {
      stripHttp: true,
      stripHttps: true,
      stripWww: true,
      trimSlash: true,
      trimEmptyQuery: true,
      trimEmptyHash: true,
    });
    return [key, prefix, null];
  }

  switch (action.type) {
    case "searchengine":
      // We want to exclude search suggestion matches that simply echo back the
      // query string in the heuristic result.  For example, if the user types
      // "@engine test", we want to exclude a "test" suggestion match.
      key = [
        action.type,
        action.params.engineName,
        (
          action.params.searchSuggestion || action.params.searchQuery
        ).toLocaleLowerCase(),
      ];
      break;
    default:
      [key, prefix] = stripPrefixAndTrim(action.params.url || match.value, {
        stripHttp: true,
        stripHttps: true,
        stripWww: true,
        trimEmptyQuery: true,
        trimSlash: true,
      });
      break;
  }

  return [key, prefix, action];
}

/**
 * Returns the portion of a string starting at the index where another string
 * ends.
 *
 * @param   {string} sourceStr
 *          The string to search within.
 * @param   {string} targetStr
 *          The string to search for.
 * @returns {string} The substring within sourceStr where targetStr ends, or the
 *          empty string if targetStr does not occur in sourceStr.
 */
function substringAfter(sourceStr, targetStr) {
  let index = sourceStr.indexOf(targetStr);
  return index < 0 ? "" : sourceStr.substr(index + targetStr.length);
}

/**
 * Makes a moz-action url for the given action and set of parameters.
 *
 * @param   type
 *          The action type.
 * @param   params
 *          A JS object of action params.
 * @returns A moz-action url as a string.
 */
function makeActionUrl(type, params) {
  let encodedParams = {};
  for (let key in params) {
    // Strip null or undefined.
    // Regardless, don't encode them or they would be converted to a string.
    if (params[key] === null || params[key] === undefined) {
      continue;
    }
    encodedParams[key] = encodeURIComponent(params[key]);
  }
  return `moz-action:${type},${JSON.stringify(encodedParams)}`;
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
 * @param {UrlbarQueryContext} [queryContext]
 *        The query context, undefined for legacy consumers.
 */
function Search(
  searchString,
  searchParam,
  autocompleteListener,
  autocompleteSearch,
  queryContext
) {
  // We want to store the original string for case sensitive searches.
  this._originalSearchString = searchString;
  this._trimmedOriginalSearchString = searchString.trim();
  let unescapedSearchString = Services.textToSubURI.unEscapeURIForUI(
    this._trimmedOriginalSearchString
  );
  let [prefix, suffix] = stripAnyPrefix(unescapedSearchString);
  this._searchString = suffix;
  this._strippedPrefix = prefix.toLowerCase();

  this._matchBehavior = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY;
  // Set the default behavior for this search.
  this._behavior = this._searchString
    ? UrlbarPrefs.get("defaultBehavior")
    : UrlbarPrefs.get("emptySearchDefaultBehavior");

  if (queryContext) {
    this._enableActions = true;
    this._inPrivateWindow = queryContext.isPrivate;
    this._disablePrivateActions =
      this._inPrivateWindow && !PrivateBrowsingUtils.permanentPrivateBrowsing;
    this._prohibitAutoFill = !queryContext.allowAutofill;
    this._maxResults = queryContext.maxResults;
    this._userContextId = queryContext.userContextId;
    this._currentPage = queryContext.currentPage;
  } else {
    let params = new Set(searchParam.split(" "));
    this._enableActions = params.has("enable-actions");
    this._disablePrivateActions = params.has("disable-private-actions");
    this._inPrivateWindow = params.has("private-window");
    this._prohibitAutoFill = params.has("prohibit-autofill");
    // Extract the max-results param.
    let maxResults = searchParam.match(REGEXP_MAX_RESULTS);
    this._maxResults = maxResults
      ? parseInt(maxResults[1])
      : UrlbarPrefs.get("maxRichResults");
    // Extract the user-context-id param.
    let userContextId = searchParam.match(REGEXP_USER_CONTEXT_ID);
    this._userContextId = userContextId
      ? parseInt(userContextId[1], 10)
      : Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;
  }

  // Use the original string here, not the stripped one, so the tokenizer can
  // properly recognize token types.
  let { tokens } = UrlbarTokenizer.tokenize({
    searchString: unescapedSearchString,
  });

  // This allows to handle leading or trailing restriction characters specially.
  this._leadingRestrictionToken = null;
  if (tokens.length) {
    if (
      UrlbarTokenizer.isRestrictionToken(tokens[0]) &&
      (tokens.length > 1 ||
        tokens[0].type == UrlbarTokenizer.TYPE.RESTRICT_SEARCH)
    ) {
      this._leadingRestrictionToken = tokens[0].value;
    }

    // Check if the first token has a strippable prefix and remove it, but don't
    // create an empty token.
    if (prefix && tokens[0].value.length > prefix.length) {
      tokens[0].value = tokens[0].value.substring(prefix.length);
    }
  }

  // The behavior can be set through:
  // 1. a specific restrictSource in the QueryContext
  // 2. typed restriction tokens
  if (
    queryContext &&
    queryContext.restrictSource &&
    sourceToBehaviorMap.has(queryContext.restrictSource)
  ) {
    this._searchTokens = tokens;
    this._behavior = 0;
    this.setBehavior("restrict");
    let behavior = sourceToBehaviorMap.get(queryContext.restrictSource);
    this.setBehavior(behavior);

    if (behavior == "search" && queryContext.engineName) {
      this._engineName = queryContext.engineName;
    }

    // When we are in restrict mode, all the tokens are valid for searching, so
    // there is no _heuristicToken.
    this._heuristicToken = null;
  } else {
    this._searchTokens = this.filterTokens(tokens);
    // The heuristic token is the first filtered search token, but only when it's
    // actually the first thing in the search string.  If a prefix or restriction
    // character occurs first, then the heurstic token is null.  We use the
    // heuristic token to help determine the heuristic result.  It may be a Places
    // keyword, a search engine alias, an extension keyword, or simply a URL or
    // part of the search string the user has typed.  We won't know until we
    // create the heuristic result.
    let firstToken = !!this._searchTokens.length && this._searchTokens[0].value;
    this._heuristicToken =
      firstToken && this._trimmedOriginalSearchString.startsWith(firstToken)
        ? firstToken
        : null;
  }

  // Set the right JavaScript behavior based on our preference.  Note that the
  // preference is whether or not we should filter JavaScript, and the
  // behavior is if we should search it or not.
  if (!UrlbarPrefs.get("filter.javascript")) {
    this.setBehavior("javascript");
  }

  this._keywordSubstitute = null;

  this._listener = autocompleteListener;
  this._autocompleteSearch = autocompleteSearch;

  // Create a new result to add eventual matches.  Note we need a result
  // regardless having matches.
  let result = Cc["@mozilla.org/autocomplete/simple-result;1"].createInstance(
    Ci.nsIAutoCompleteSimpleResult
  );
  result.setSearchString(searchString);
  // Will be set later, if needed.
  result.setDefaultIndex(-1);
  this._result = result;

  // Used to limit the number of adaptive results.
  this._adaptiveCount = 0;
  this._extraAdaptiveRows = [];

  // Used to limit the number of remote tab results.
  this._extraRemoteTabRows = [];

  // These are used to avoid adding duplicate entries to the results.
  this._usedURLs = [];
  this._usedPlaceIds = new Set();

  // Counters for the number of results per RESULT_GROUP.
  this._counts = Object.values(UrlbarUtils.RESULT_GROUP).reduce((o, p) => {
    o[p] = 0;
    return o;
  }, {});
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
    this._behavior |= Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type];
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

    if (
      this._disablePrivateActions &&
      behavior == Ci.mozIPlacesAutoComplete.BEHAVIOR_OPENPAGE
    ) {
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
    if (!this._sleepTimer) {
      this._sleepTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }
    return new Promise(resolve => {
      this._sleepResolve = resolve;
      this._sleepTimer.initWithCallback(
        resolve,
        aTimeMs,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
    });
  },

  /**
   * Given an array of tokens, this function determines which query should be
   * ran.  It also removes any special search tokens.
   *
   * @param tokens
   *        An array of search tokens.
   * @return A new, filtered array of tokens.
   */
  filterTokens(tokens) {
    let foundToken = false;
    // Set the proper behavior while filtering tokens.
    let filtered = [];
    for (let token of tokens) {
      if (!UrlbarTokenizer.isRestrictionToken(token)) {
        filtered.push(token);
        continue;
      }
      let behavior = typeToBehaviorMap.get(token.type);
      if (!behavior) {
        throw new Error(`Unknown token type ${token.type}`);
      }
      // Don't remove the token if it didn't match, or if it's an action but
      // actions are not enabled.
      if (behavior != "openpage" || this._enableActions) {
        // Don't use the suggest preferences if it is a token search and
        // set the restrict bit to 1 (to intersect the search results).
        if (!foundToken) {
          foundToken = true;
          // Do not take into account previous behavior (e.g.: history, bookmark)
          this._behavior = 0;
          this.setBehavior("restrict");
        }
        this.setBehavior(behavior);
        // We return tags only for bookmarks, thus when tags are enforced, we
        // must also set the bookmark behavior.
        if (behavior == "tag") {
          this.setBehavior("bookmark");
        }
      }
    }
    return filtered;
  },

  /**
   * Stop this search.
   * After invoking this method, we won't run any more searches or heuristics,
   * and no new matches may be added to the current result.
   */
  stop() {
    // Avoid multiple calls or re-entrance.
    if (!this.pending) {
      return;
    }
    if (this._notifyTimer) {
      this._notifyTimer.cancel();
    }
    this._notifyDelaysCount = 0;
    if (this._sleepTimer) {
      this._sleepTimer.cancel();
    }
    if (this._sleepResolve) {
      this._sleepResolve();
      this._sleepResolve = null;
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
    if (!this.pending) {
      return;
    }

    // Used by stop() to interrupt an eventual running statement.
    this.interrupt = () => {
      // Interrupt any ongoing statement to run the search sooner.
      if (!UrlbarProvidersManager.interruptLevel) {
        conn.interrupt();
      }
    };

    if (UrlbarPrefs.get("restyleSearches")) {
      // This explicit initialization is only necessary for
      // _maybeRestyleSearchMatch, because it calls the synchronous
      // parseSubmissionURL that can't wait for async initialization of
      // PlacesSearchAutocompleteProvider.
      await PlacesSearchAutocompleteProvider.ensureReady();
      if (!this.pending) {
        return;
      }
    }

    // For any given search, we run many queries/heuristics:
    // 1) by alias (as defined in SearchService)
    // 2) inline completion from search engine resultDomains
    // 3) inline completion for origins (this._originQuery) or urls (this._urlQuery)
    // 4) directly typed in url (ie, can be navigated to as-is)
    // 5) submission for the current search engine
    // 6) Places keywords
    // 7) adaptive learning (this._adaptiveQuery)
    // 8) open pages not supported by history (this._switchToTabQuery)
    // 9) query based on match behavior
    //
    // (6) only gets run if we get any filtered tokens, since if there are no
    // tokens, there is nothing to match.
    //
    // (1), (4), (5) only get run if actions are enabled. When actions are
    // enabled, the first result is always a special result (resulting from one
    // of the queries between (1) and (6) inclusive). As such, the UI is
    // expected to auto-select the first result when actions are enabled. If the
    // first result is an inline completion result, that will also be the
    // default result and therefore be autofilled (this also happens if actions
    // are not enabled).

    // Check for Preloaded Sites Expiry before Autofill
    await this._checkPreloadedSitesExpiry();

    // If the query is simply "@" and we have tokenAliasEngines then return
    // early. UrlbarProviderTokenAliasEngines will add engine results.
    let tokenAliasEngines = await PlacesSearchAutocompleteProvider.tokenAliasEngines();
    if (this._trimmedOriginalSearchString == "@" && tokenAliasEngines.length) {
      this._autocompleteSearch.finishSearch(true);
      return;
    }

    // Add the first heuristic result, if any.  Set _addingHeuristicResult
    // to true so that when the result is added, "heuristic" can be included in
    // its style.
    this._addingHeuristicResult = true;
    let hasHeuristic = await this._matchFirstHeuristicResult(conn);
    this._addingHeuristicResult = false;
    if (!this.pending) {
      return;
    }

    // We sleep a little between adding the heuristic result and matching
    // any other searches so we aren't kicking off potentially expensive
    // searches on every keystroke.
    // Though, if there's no heuristic result, we start searching immediately,
    // since autocomplete may be waiting for us.
    if (hasHeuristic) {
      await this._sleep(UrlbarPrefs.get("delay"));
      if (!this.pending) {
        return;
      }

      // If the heuristic result is an engine from a token alias, the search
      // restriction char, or we're in search-restriction mode, then we're done.
      // UrlbarProviderSearchSuggestions will handle suggestions, if any.
      let tokenAliasQuery =
        this._searchEngineAliasMatch &&
        this._searchEngineAliasMatch.isTokenAlias;
      let emptySearchRestriction =
        this._trimmedOriginalSearchString.length <= 3 &&
        this._leadingRestrictionToken == UrlbarTokenizer.RESTRICT.SEARCH &&
        /\s*\S?$/.test(this._trimmedOriginalSearchString);
      if (
        emptySearchRestriction ||
        tokenAliasQuery ||
        (this.hasBehavior("search") && this.hasBehavior("restrict"))
      ) {
        this._autocompleteSearch.finishSearch(true);
        return;
      }
    }

    // Only add extension suggestions if the first token is a registered keyword
    // and the search string has characters after the first token.
    let extensionsCompletePromise = Promise.resolve();
    if (
      this._heuristicToken &&
      ExtensionSearchHandler.isKeywordRegistered(this._heuristicToken) &&
      substringAfter(this._originalSearchString, this._heuristicToken) &&
      !this._searchEngineAliasMatch
    ) {
      // Do not await on this, since extensions cannot notify when they are done
      // adding results, it may take too long.
      extensionsCompletePromise = this._matchExtensionSuggestions();
    } else if (ExtensionSearchHandler.hasActiveInputSession()) {
      ExtensionSearchHandler.handleInputCancelled();
    }

    // Run the adaptive query first.
    await conn.executeCached(
      this._adaptiveQuery[0],
      this._adaptiveQuery[1],
      this._onResultRow.bind(this)
    );
    if (!this.pending) {
      return;
    }

    // Then fetch remote tabs.
    if (this._enableActions && this.hasBehavior("openpage")) {
      await this._matchRemoteTabs();
      if (!this.pending) {
        return;
      }
    }

    // Get the final query, based on the tokens found in the search string and
    // the keyword substitution, if any.
    let queries = [];
    // "openpage" behavior is supported by the default query.
    // _switchToTabQuery instead returns only pages not supported by history.
    if (this.hasBehavior("openpage")) {
      queries.push(this._switchToTabQuery);
    }
    queries.push(this._searchQuery);

    // Finally run all the remaining queries.
    for (let [query, params] of queries) {
      await conn.executeCached(query, params, this._onResultRow.bind(this));
      if (!this.pending) {
        return;
      }
    }

    // If we have some unused adaptive matches, add them now.
    while (
      this._extraAdaptiveRows.length &&
      this._result.matchCount < this._maxResults
    ) {
      this._addFilteredQueryMatch(this._extraAdaptiveRows.shift());
    }

    // If we have some unused remote tab matches, add them now.
    while (
      this._extraRemoteTabRows.length &&
      this._result.matchCount < this._maxResults
    ) {
      this._addMatch(this._extraRemoteTabRows.shift());
    }

    this._matchAboutPages();

    // If we do not have enough matches search again with MATCH_ANYWHERE, to
    // get more matches.
    let count =
      this._counts[UrlbarUtils.RESULT_GROUP.GENERAL] +
      this._counts[UrlbarUtils.RESULT_GROUP.HEURISTIC];
    if (count < this._maxResults) {
      this._matchBehavior = Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE;
      let queries = [this._adaptiveQuery, this._searchQuery];
      if (this.hasBehavior("openpage")) {
        queries.unshift(this._switchToTabQuery);
      }
      for (let [query, params] of queries) {
        await conn.executeCached(query, params, this._onResultRow.bind(this));
        if (!this.pending) {
          return;
        }
      }
    }

    this._matchPreloadedSites();

    // Ensure to fill any remaining space.
    await extensionsCompletePromise;
  },

  _shouldMatchAboutPages() {
    // Only autocomplete input that starts with 'about:' and has at least 1 more
    // character.
    return this._strippedPrefix == "about:" && this._searchString;
  },

  _matchAboutPages() {
    if (!this._shouldMatchAboutPages()) {
      return;
    }
    for (const url of AboutPagesUtils.visibleAboutUrls) {
      if (url.startsWith(`about:${this._searchString}`)) {
        this._addMatch({
          value: url,
          comment: url,
          frecency: FRECENCY_DEFAULT,
        });
      }
    }
  },

  _matchAboutPageForAutofill() {
    if (!this._shouldMatchAboutPages()) {
      return false;
    }
    for (const url of AboutPagesUtils.visibleAboutUrls) {
      if (url.startsWith(`about:${this._searchString.toLowerCase()}`)) {
        this._result.setDefaultIndex(0);
        this._addAutofillMatch(url.substr(6), url);
        return true;
      }
    }
    return false;
  },

  async _checkPreloadedSitesExpiry() {
    if (!UrlbarPrefs.get("usepreloadedtopurls.enabled")) {
      return;
    }
    let profileCreationDate = await ProfileAgeCreatedPromise;
    let daysSinceProfileCreation =
      (Date.now() - profileCreationDate) / MS_PER_DAY;
    if (
      daysSinceProfileCreation >
      UrlbarPrefs.get("usepreloadedtopurls.expire_days")
    ) {
      Services.prefs.setBoolPref(
        "browser.urlbar.usepreloadedtopurls.enabled",
        false
      );
    }
  },

  _matchPreloadedSites() {
    if (!UrlbarPrefs.get("usepreloadedtopurls.enabled")) {
      return;
    }

    if (!this._searchString) {
      // The user hasn't typed anything, or they've only typed a scheme.
      return;
    }

    for (let site of PreloadedSiteStorage.sites) {
      let url = site.uri.spec;
      if (
        (!this._strippedPrefix || url.startsWith(this._strippedPrefix)) &&
        (site.uri.host.includes(this._searchString) ||
          site._matchTitle.includes(this._searchString))
      ) {
        this._addMatch({
          value: url,
          comment: site.title,
          style: "preloaded-top-site",
          frecency: FRECENCY_DEFAULT - 1,
        });
      }
    }
  },

  _matchPreloadedSiteForAutofill() {
    if (!UrlbarPrefs.get("usepreloadedtopurls.enabled")) {
      return false;
    }

    let matchedSite = PreloadedSiteStorage.sites.find(site => {
      return (
        (!this._strippedPrefix ||
          site.uri.spec.startsWith(this._strippedPrefix)) &&
        (site.uri.host.startsWith(this._searchString) ||
          site.uri.host.startsWith("www." + this._searchString))
      );
    });
    if (!matchedSite) {
      return false;
    }

    this._result.setDefaultIndex(0);

    let url = matchedSite.uri.spec;
    let value = stripAnyPrefix(url)[1];
    value = value.substr(value.indexOf(this._searchString));

    this._addAutofillMatch(value, url, Infinity, ["preloaded-top-site"]);
    return true;
  },

  async _matchSearchEngineTokenAliasForAutofill() {
    // We need an "@engine" heuristic token.
    let token = this._heuristicToken;
    if (!token || token.length == 1 || !token.startsWith("@")) {
      return false;
    }

    // See if any engine has a token alias that starts with the heuristic token.
    let engines = await PlacesSearchAutocompleteProvider.tokenAliasEngines();
    for (let { engine, tokenAliases } of engines) {
      for (let alias of tokenAliases) {
        if (alias.startsWith(token.toLocaleLowerCase())) {
          // We found one.  The match we add here is a little special compared
          // to others.  It needs to be an autofill match and its `value` must
          // be the string that will be autofilled so that the controller will
          // autofill it.  But it also must be a searchengine action so that the
          // front end will style it as a search engine result.  The front end
          // uses `finalCompleteValue` as the URL for autofill results, so set
          // that to the moz-action URL.
          let aliasPreservingUserCase = token + alias.substr(token.length);
          let value = aliasPreservingUserCase + " ";
          this._result.setDefaultIndex(0);
          this._addMatch({
            value,
            finalCompleteValue: makeActionUrl("searchengine", {
              engineName: engine.name,
              alias: aliasPreservingUserCase,
              input: value,
              searchQuery: "",
            }),
            comment: engine.name,
            frecency: FRECENCY_DEFAULT,
            style: "autofill action searchengine",
            icon: engine.iconURI ? engine.iconURI.spec : null,
          });

          // Set _searchEngineAliasMatch with an empty query so that we don't
          // attempt to add any more matches.  When a token alias is autofilled,
          // the only match should be the one we just added.
          this._searchEngineAliasMatch = {
            engine,
            alias: aliasPreservingUserCase,
            query: "",
            isTokenAlias: true,
          };

          return true;
        }
      }
    }

    return false;
  },

  async _matchFirstHeuristicResult(conn) {
    // We always try to make the first result a special "heuristic" result.  The
    // heuristics below determine what type of result it will be, if any.

    if (this._heuristicToken) {
      // It may be a keyword registered by an extension.
      let matched = await this._matchExtensionHeuristicResult(
        this._heuristicToken
      );
      if (matched) {
        return true;
      }
    }

    if (this.pending && this._enableActions && this._heuristicToken) {
      // It may be a search engine with an alias - which works like a keyword.
      let matched = await this._matchSearchEngineAlias(this._heuristicToken);
      if (matched) {
        return true;
      }
    }

    if (this.pending && this._heuristicToken) {
      // It may be a Places keyword.
      let matched = await this._matchPlacesKeyword(this._heuristicToken);
      if (matched) {
        return true;
      }
    }

    let shouldAutofill = this._shouldAutofill;

    if (this.pending && shouldAutofill) {
      // It may also look like an about: link.
      let matched = await this._matchAboutPageForAutofill();
      if (matched) {
        return true;
      }
    }

    if (this.pending && shouldAutofill) {
      // It may also look like a URL we know from the database.
      let matched = await this._matchKnownUrl(conn);
      if (matched) {
        return true;
      }
    }

    if (this.pending && shouldAutofill) {
      // Or it may look like a search engine domain.
      let matched = await this._matchSearchEngineDomain();
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

    if (this.pending && shouldAutofill) {
      let matched = await this._matchSearchEngineTokenAliasForAutofill();
      if (matched) {
        return true;
      }
    }

    if (this.pending && this._searchTokens.length && this._enableActions) {
      // If we don't have a result that matches what we know about, then
      // we use a fallback for things we don't know about.

      // We may not have auto-filled, but this may still look like a URL.
      // However, even if the input is a valid URL, we may not want to use
      // it as such. This can happen if the host would require whitelisting,
      // but isn't in the whitelist.
      let matched = await this._matchUnknownUrl();
      if (matched) {
        // Since we can't tell if this is a real URL and whether the user wants
        // to visit or search for it, we provide an alternative searchengine
        // match if the string looks like an alphanumeric origin or an e-mail.
        let str = this._originalSearchString;
        try {
          new URL(str);
        } catch (ex) {
          if (
            UrlbarPrefs.get("keyword.enabled") &&
            (UrlbarTokenizer.looksLikeOrigin(str, {
              noIp: true,
              noPort: true,
            }) ||
              UrlbarTokenizer.REGEXP_COMMON_EMAIL.test(str))
          ) {
            this._addingHeuristicResult = false;
            await this._matchCurrentSearchEngine();
            this._addingHeuristicResult = true;
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

  async _matchKnownUrl(conn) {
    let gotResult = false;

    // If search string looks like an origin, try to autofill against origins.
    // Otherwise treat it as a possible URL.  When the string has only one slash
    // at the end, we still treat it as an URL.
    let query, params;
    if (
      UrlbarTokenizer.looksLikeOrigin(this._searchString, {
        ignoreWhitelist: true,
      })
    ) {
      [query, params] = this._originQuery;
    } else {
      [query, params] = this._urlQuery;
    }

    // _urlQuery doesn't always return a query.
    if (query) {
      await conn.executeCached(query, params, (row, cancel) => {
        gotResult = true;
        this._onResultRow(row, cancel);
      });
    }
    return gotResult;
  },

  _matchExtensionHeuristicResult(keyword) {
    if (
      ExtensionSearchHandler.isKeywordRegistered(keyword) &&
      substringAfter(this._originalSearchString, keyword)
    ) {
      let description = ExtensionSearchHandler.getDescription(keyword);
      this._addExtensionMatch(this._originalSearchString, description);
      return true;
    }
    return false;
  },

  async _matchPlacesKeyword(keyword) {
    let entry = await PlacesUtils.keywords.fetch(keyword);
    if (!entry) {
      return false;
    }

    let searchString = substringAfter(
      this._originalSearchString,
      keyword
    ).trim();

    let url = null;
    let postData = null;
    try {
      [url, postData] = await BrowserUtils.parseUrlAndPostData(
        entry.url.href,
        entry.postData,
        searchString
      );
    } catch (ex) {
      // It's not possible to bind a param to this keyword.
      return false;
    }

    let style = "keyword";
    let value = url;
    if (this._enableActions) {
      style = "action " + style;
      value = makeActionUrl("keyword", {
        url,
        keyword,
        input: this._originalSearchString,
        postData,
      });
    }

    let match = {
      value,
      // Don't use the url with replaced strings, since the icon doesn't change
      // but the string does, it may cause pointless icon flicker on typing.
      icon: iconHelper(entry.url),
      style,
      frecency: Infinity,
    };
    // If there is a query string, the title will be "host: queryString".
    if (this._searchTokens.length > 1) {
      match.comment = entry.url.host;
    }

    this._addMatch(match);
    if (!this._keywordSubstitute) {
      this._keywordSubstitute = entry.url.host;
    }
    return true;
  },

  async _matchSearchEngineDomain() {
    if (!UrlbarPrefs.get("autoFill.searchEngines")) {
      return false;
    }
    if (!this._searchString) {
      return false;
    }

    // PlacesSearchAutocompleteProvider only matches against engine domains.
    // Remove an eventual trailing slash from the search string (without the
    // prefix) and check if the resulting string is worth matching.
    // Later, we'll verify that the found result matches the original
    // searchString and eventually discard it.
    let searchStr = this._searchString;
    if (searchStr.indexOf("/") == searchStr.length - 1) {
      searchStr = searchStr.slice(0, -1);
    }
    // If the search string looks more like a url than a domain, bail out.
    if (
      !UrlbarTokenizer.looksLikeOrigin(searchStr, { ignoreWhitelist: true })
    ) {
      return false;
    }

    let engine = await PlacesSearchAutocompleteProvider.engineForDomainPrefix(
      searchStr
    );
    if (!engine) {
      return false;
    }
    let url = engine.searchForm;
    let domain = engine.getResultDomain();
    // Verify that the match we got is acceptable. Autofilling "example/" to
    // "example.com/" would not be good.
    if (
      (this._strippedPrefix && !url.startsWith(this._strippedPrefix)) ||
      !(domain + "/").includes(this._searchString)
    ) {
      return false;
    }

    // The value that's autofilled in the input is the prefix the user typed, if
    // any, plus the portion of the engine domain that the user typed.  Append a
    // trailing slash too, as is usual with autofill.
    let value =
      this._strippedPrefix + domain.substr(domain.indexOf(searchStr)) + "/";

    let finalCompleteValue = url;
    try {
      let fixupInfo = Services.uriFixup.getFixupURIInfo(url, 0);
      if (fixupInfo.fixedURI) {
        finalCompleteValue = fixupInfo.fixedURI.spec;
      }
    } catch (ex) {}

    this._result.setDefaultIndex(0);
    this._addMatch({
      value,
      finalCompleteValue,
      comment: engine.name,
      icon: engine.iconURI ? engine.iconURI.spec : null,
      style: "priority-search",
      frecency: Infinity,
    });
    return true;
  },

  async _matchSearchEngineAlias(alias) {
    let engine = await PlacesSearchAutocompleteProvider.engineForAlias(alias);
    if (!engine) {
      return false;
    }

    this._searchEngineAliasMatch = {
      engine,
      alias,
      query: substringAfter(this._originalSearchString, alias).trim(),
      isTokenAlias: alias.startsWith("@"),
    };
    this._addSearchEngineMatch(this._searchEngineAliasMatch);
    if (!this._keywordSubstitute) {
      this._keywordSubstitute = engine.getResultDomain();
    }
    return true;
  },

  async _matchCurrentSearchEngine() {
    let engine = this._engineName
      ? Services.search.getEngineByName(this._engineName)
      : await PlacesSearchAutocompleteProvider.currentEngine(
          this._inPrivateWindow
        );
    if (!engine || !this.pending) {
      return false;
    }
    // Strip a leading search restriction char, because we prepend it to text
    // when the search shortcut is used and it's not user typed. Don't strip
    // other restriction chars, so that it's possible to search for things
    // including one of those (e.g. "c#").
    let query = this._trimmedOriginalSearchString;
    if (this._leadingRestrictionToken === UrlbarTokenizer.RESTRICT.SEARCH) {
      query = substringAfter(query, this._leadingRestrictionToken).trim();
    }
    this._addSearchEngineMatch({ engine, query });
    return true;
  },

  _addExtensionMatch(content, comment) {
    let count =
      this._counts[UrlbarUtils.RESULT_GROUP.EXTENSION] +
      this._counts[UrlbarUtils.RESULT_GROUP.HEURISTIC];
    if (count >= UrlbarUtils.MAXIMUM_ALLOWED_EXTENSION_MATCHES) {
      return;
    }

    this._addMatch({
      value: makeActionUrl("extension", {
        content,
        keyword: this._heuristicToken,
      }),
      comment,
      icon: "chrome://browser/content/extension.svg",
      style: "action extension",
      frecency: Infinity,
      type: UrlbarUtils.RESULT_GROUP.EXTENSION,
    });
  },

  /**
   * Adds a search engine match.
   *
   * @param {nsISearchEngine} engine
   *        The search engine associated with the match.
   * @param {string} [query]
   *        The search query string.
   * @param {string} [alias]
   *        The search engine alias associated with the match, if any.
   * @param {bool} [historical]
   *        True if you're adding a suggestion match and the suggestion is from
   *        the user's local history (and not the search engine).
   */
  _addSearchEngineMatch({
    engine,
    query = "",
    alias = undefined,
    historical = false,
  }) {
    let actionURLParams = {
      engineName: engine.name,
      searchQuery: query,
    };

    if (alias && !query) {
      // `input` should have a trailing space so that when the user selects the
      // result, they can start typing their query without first having to enter
      // a space between the alias and query.
      actionURLParams.input = `${alias} `;
    } else {
      actionURLParams.input = this._originalSearchString;
    }

    let match = {
      comment: engine.name,
      icon: engine.iconURI ? engine.iconURI.spec : null,
      style: "action searchengine",
      frecency: FRECENCY_DEFAULT,
    };

    if (alias) {
      actionURLParams.alias = alias;
      match.style += " alias";
    }

    match.value = makeActionUrl("searchengine", actionURLParams);
    this._addMatch(match);
  },

  _matchExtensionSuggestions() {
    let data = {
      keyword: this._heuristicToken,
      text: this._originalSearchString,
      inPrivateWindow: this._inPrivateWindow,
    };
    let promise = ExtensionSearchHandler.handleSearch(data, suggestions => {
      for (let suggestion of suggestions) {
        let content = `${this._heuristicToken} ${suggestion.content}`;
        this._addExtensionMatch(content, suggestion.description);
      }
    });

    // Since the extension has no way to signal when it's done pushing
    // results, we add a timeout racing with the addition.
    let timeoutPromise = new Promise(resolve => {
      let timer = setTimeout(resolve, MAXIMUM_ALLOWED_EXTENSION_TIME_MS);
      // TODO Bug 1531268: Figure out why this cancel helps makes the tests
      // stable.
      promise.then(timer.cancel);
    });
    return Promise.race([timeoutPromise, promise]).catch(Cu.reportError);
  },

  async _matchRemoteTabs() {
    // Bail out early for non-sync users.
    if (!syncUsernamePref) {
      return;
    }
    let matches = await PlacesRemoteTabsAutocompleteProvider.getMatches(
      this._originalSearchString,
      this._maxResults
    );
    for (let { url, title, icon, deviceName, lastUsed } of matches) {
      // It's rare that Sync supplies the icon for the page (but if it does, it
      // is a string URL)
      if (!icon) {
        icon = iconHelper(url);
      } else {
        icon = PlacesUtils.favicons.getFaviconLinkForIcon(
          Services.io.newURI(icon)
        ).spec;
      }

      let match = {
        // We include the deviceName in the action URL so we can render it in
        // the URLBar.
        value: makeActionUrl("remotetab", { url, deviceName }),
        comment: title || url,
        style: "action remotetab",
        // we want frecency > FRECENCY_DEFAULT so it doesn't get pushed out
        // by "remote" matches.
        frecency: FRECENCY_DEFAULT + 1,
        icon,
      };
      if (lastUsed > Date.now() - RECENT_REMOTE_TAB_THRESHOLD_MS) {
        this._addMatch(match);
      } else {
        this._extraRemoteTabRows.push(match);
      }
    }
  },

  // TODO (bug 1054814): Use visited URLs to inform which scheme to use, if the
  // scheme isn't specificed.
  _matchUnknownUrl() {
    if (!this._searchString && this._strippedPrefix) {
      // The user just typed a stripped protocol, don't build a non-sense url
      // like http://http/ for it.
      return false;
    }
    // The user may have typed something like "word?" to run a search, we should
    // not convert that to a url.
    if (this.hasBehavior("search") && this.hasBehavior("restrict")) {
      return false;
    }
    let flags =
      Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
      Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    if (this._inPrivateWindow) {
      flags |= Ci.nsIURIFixup.FIXUP_FLAG_PRIVATE_CONTEXT;
    }
    let fixupInfo = null;
    let searchUrl = this._trimmedOriginalSearchString;
    try {
      fixupInfo = Services.uriFixup.getFixupURIInfo(searchUrl, flags);
    } catch (e) {
      if (
        e.result == Cr.NS_ERROR_MALFORMED_URI &&
        !UrlbarPrefs.get("keyword.enabled")
      ) {
        let value = makeActionUrl("visiturl", {
          url: searchUrl,
          input: searchUrl,
        });
        this._addMatch({
          value,
          comment: searchUrl,
          style: "action visiturl",
          frecency: Infinity,
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
    if (!fixupInfo.fixedURI || fixupInfo.keywordAsSent) {
      return false;
    }

    let uri = fixupInfo.fixedURI;
    // Check the host, as "http:///" is a valid nsIURI, but not useful to us.
    // But, some schemes are expected to have no host. So we check just against
    // schemes we know should have a host. This allows new schemes to be
    // implemented without us accidentally blocking access to them.
    let hostExpected = ["http", "https", "ftp", "chrome"].includes(uri.scheme);
    if (hostExpected && !uri.host) {
      return false;
    }

    // getFixupURIInfo() escaped the URI, so it may not be pretty.  Embed the
    // escaped URL in the action URI since that URL should be "canonical".  But
    // pass the pretty, unescaped URL as the match comment, since it's likely
    // to be displayed to the user, and in any case the front-end should not
    // rely on it being canonical.
    let escapedURL = uri.displaySpec;
    let displayURL = Services.textToSubURI.unEscapeURIForUI(escapedURL);

    let value = makeActionUrl("visiturl", {
      url: escapedURL,
      input: searchUrl,
    });

    let match = {
      value,
      comment: displayURL,
      style: "action visiturl",
      frecency: Infinity,
    };

    // We don't know if this url is in Places or not, and checking that would
    // be expensive. Thus we also don't know if we may have an icon.
    // If we'd just try to fetch the icon for the typed string, we'd cause icon
    // flicker, since the url keeps changing while the user types.
    // By default we won't provide an icon, but for the subset of urls with a
    // host we'll check for a typed slash and set favicon for the host part.
    if (
      hostExpected &&
      (searchUrl.endsWith("/") || uri.pathQueryRef.length > 1)
    ) {
      match.icon = `page-icon:${uri.prePath}/`;
    }

    this._addMatch(match);
    return true;
  },

  _onResultRow(row, cancel) {
    let queryType = row.getResultByIndex(QUERYINDEX_QUERYTYPE);
    switch (queryType) {
      case QUERYTYPE_AUTOFILL_ORIGIN:
        this._result.setDefaultIndex(0);
        this._addOriginAutofillMatch(row);
        break;
      case QUERYTYPE_AUTOFILL_URL:
        this._result.setDefaultIndex(0);
        this._addURLAutofillMatch(row);
        break;
      case QUERYTYPE_ADAPTIVE:
        this._addAdaptiveQueryMatch(row);
        break;
      case QUERYTYPE_FILTERED:
        this._addFilteredQueryMatch(row);
        break;
    }
    // If the search has been canceled by the user or by _addMatch, or we
    // fetched enough results, we can stop the underlying Sqlite query.
    let count =
      this._counts[UrlbarUtils.RESULT_GROUP.GENERAL] +
      this._counts[UrlbarUtils.RESULT_GROUP.HEURISTIC];
    if (!this.pending || count >= this._maxResults) {
      cancel();
    }
  },

  _maybeRestyleSearchMatch(match) {
    // Return if the URL does not represent a search result.
    let parseResult = PlacesSearchAutocompleteProvider.parseSubmissionURL(
      match.value
    );
    if (!parseResult) {
      return;
    }

    // Here we check that the user typed all or part of the search string in the
    // search history result.
    let terms = parseResult.terms.toLowerCase();
    if (
      this._searchTokens.length &&
      this._searchTokens.every(token => !terms.includes(token.value))
    ) {
      return;
    }

    // The URL for the search suggestion formed by the user's typed query.
    let [typedSuggestionUrl] = UrlbarUtils.getSearchQueryUrl(
      parseResult.engine,
      this._searchTokens.map(t => t.value).join(" ")
    );

    let historyParams = new URL(match.value).searchParams;
    let typedParams = new URL(typedSuggestionUrl).searchParams;

    // Checking the two URLs have the same query parameters with the same
    // values, or a subset value in the case of the query parameter.
    // Parameter order doesn't matter.
    if (
      Array.from(historyParams).length != Array.from(typedParams).length ||
      !Array.from(historyParams.entries()).every(
        ([key, value]) =>
          // We want to match all non-search-string GET parameters exactly, to avoid
          // restyling non-first pages of search results, or image results as web
          // results.
          // We let termsParameterName through because we already checked that the
          // typed query is a subset of the search history query above with
          // this._searchTokens.every(...).
          key == parseResult.termsParameterName ||
          value === typedParams.get(key)
      )
    ) {
      return;
    }

    // Turn the match into a searchengine action with a favicon.
    match.value = makeActionUrl("searchengine", {
      engineName: parseResult.engine.name,
      input: parseResult.terms,
      searchSuggestion: parseResult.terms,
      searchQuery: parseResult.terms,
      isSearchHistory: true,
    });
    match.comment = parseResult.engine.name;
    match.icon = match.icon || match.iconUrl;
    match.style = "action searchengine favicon suggestion";
  },

  _addMatch(match) {
    if (typeof match.frecency != "number") {
      throw new Error("Frecency not provided");
    }

    if (this._addingHeuristicResult) {
      match.type = UrlbarUtils.RESULT_GROUP.HEURISTIC;
    } else if (typeof match.type != "string") {
      match.type = UrlbarUtils.RESULT_GROUP.GENERAL;
    }

    // A search could be canceled between a query start and its completion,
    // in such a case ensure we won't notify any result for it.
    if (!this.pending) {
      return;
    }

    match.style = match.style || "favicon";

    // Restyle past searches, unless they are bookmarks or special results.
    if (UrlbarPrefs.get("restyleSearches") && match.style == "favicon") {
      this._maybeRestyleSearchMatch(match);
    }

    if (this._addingHeuristicResult) {
      match.style += " heuristic";
    }

    match.icon = match.icon || "";
    match.finalCompleteValue = match.finalCompleteValue || "";

    let { index, replace } = this._getInsertIndexForMatch(match);
    if (index == -1) {
      return;
    }
    if (replace) {
      // Replacing an existing match from the previous search.
      this._result.removeMatchAt(index);
    }
    this._result.insertMatchAt(
      index,
      match.value,
      match.comment,
      match.icon,
      match.style,
      match.finalCompleteValue
    );
    this._counts[match.type]++;

    this.notifyResult(true, match.type == UrlbarUtils.RESULT_GROUP.HEURISTIC);
  },

  // Ranks a URL prefix from 3 - 0 with the following preferences:
  // https:// > https://www. > http:// > http://www.
  // Higher is better.
  // Returns -1 if the prefix does not match any of the above.
  _getPrefixRank(prefix) {
    return ["http://www.", "http://", "https://www.", "https://"].indexOf(
      prefix
    );
  },

  /**
   * Check for duplicates and either discard the duplicate or replace the
   * original match, in case the new one is more specific. For example,
   * a Remote Tab wins over History, and a Switch to Tab wins over a Remote Tab.
   * We must check both id and url for duplication, because keywords may change
   * the url by replacing the %s placeholder.
   * @param match
   * @returns {object} matchPosition
   * @returns {number} matchPosition.index
   *   The index the match should take in the results. Return -1 if the match
   *   should be discarded.
   * @returns {boolean} matchPosition.replace
   *   True if the match should replace the result already at
   *   matchPosition.index.
   *
   */
  _getInsertIndexForMatch(match) {
    let [urlMapKey, prefix, action] = makeKeyForMatch(match);
    if (
      (match.placeId && this._usedPlaceIds.has(match.placeId)) ||
      this._usedURLs.some(e => ObjectUtils.deepEqual(e.key, urlMapKey))
    ) {
      let isDupe = true;
      if (action && ["switchtab", "remotetab"].includes(action.type)) {
        // The new entry is a switch/remote tab entry, look for the duplicate
        // among current matches.
        for (let i = 0; i < this._usedURLs.length; ++i) {
          let {
            key: matchKey,
            action: matchAction,
            type: matchType,
          } = this._usedURLs[i];
          if (ObjectUtils.deepEqual(matchKey, urlMapKey)) {
            isDupe = true;
            // Don't replace the match if the existing one is heuristic and the
            // new one is a switchtab, instead also add the switchtab match.
            if (
              matchType == UrlbarUtils.RESULT_GROUP.HEURISTIC &&
              action.type == "switchtab"
            ) {
              isDupe = false;
              // Since we allow to insert a dupe in this case, we must continue
              // checking the next matches to be sure we won't insert more than
              // one dupe. For this same reason we must reset isDupe = true for
              // each found dupe.
              continue;
            }
            if (!matchAction || action.type == "switchtab") {
              this._usedURLs[i] = {
                key: urlMapKey,
                action,
                type: match.type,
                prefix,
                comment: match.comment,
              };
              return { index: i, replace: true };
            }
            break; // Found the duplicate, no reason to continue.
          }
        }
      } else {
        // Dedupe with this flow:
        // 1. If the two URLs are the same, dedupe whichever is not the
        //    heuristic result.
        // 2. If they both contain www. or both do not contain it, prefer https.
        // 3. If they differ by www.:
        //    3a. If the page titles are different, keep both. This is a guard
        //        against deduping when www.site.com and site.com have different
        //        content.
        //    3b. Otherwise, dedupe based on the priorities in _getPrefixRank.
        let prefixRank = this._getPrefixRank(prefix);
        for (let i = 0; i < this._usedURLs.length; ++i) {
          if (!this._usedURLs[i]) {
            // This is true when the result at [i] is a searchengine result.
            continue;
          }

          let {
            key: existingKey,
            prefix: existingPrefix,
            type: existingType,
            comment: existingComment,
          } = this._usedURLs[i];

          let existingPrefixRank = this._getPrefixRank(existingPrefix);
          if (ObjectUtils.deepEqual(existingKey, urlMapKey)) {
            isDupe = true;

            if (prefix == existingPrefix) {
              // The URLs are identical. Throw out the new result, unless it's
              // the heuristic.
              if (match.type != UrlbarUtils.RESULT_GROUP.HEURISTIC) {
                break; // Replace match.
              } else {
                this._usedURLs[i] = {
                  key: urlMapKey,
                  action,
                  type: match.type,
                  prefix,
                  comment: match.comment,
                };
                return { index: i, replace: true };
              }
            }

            if (prefix.endsWith("www.") == existingPrefix.endsWith("www.")) {
              // The results differ only by protocol.

              if (match.type == UrlbarUtils.RESULT_GROUP.HEURISTIC) {
                isDupe = false;
                continue;
              }

              if (prefixRank <= existingPrefixRank) {
                break; // Replace match.
              } else if (existingType != UrlbarUtils.RESULT_GROUP.HEURISTIC) {
                this._usedURLs[i] = {
                  key: urlMapKey,
                  action,
                  type: match.type,
                  prefix,
                  comment: match.comment,
                };
                return { index: i, replace: true };
              } else {
                isDupe = false;
                continue;
              }
            } else {
              // If either result is the heuristic, this will be true and we
              // will keep both results.
              if (match.comment != existingComment) {
                isDupe = false;
                continue;
              }

              if (prefixRank <= existingPrefixRank) {
                break; // Replace match.
              } else {
                this._usedURLs[i] = {
                  key: urlMapKey,
                  action,
                  type: match.type,
                  prefix,
                  comment: match.comment,
                };
                return { index: i, replace: true };
              }
            }
          }
        }
      }

      // Discard the duplicate.
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
    if (match.placeId) {
      this._usedPlaceIds.add(match.placeId);
    }

    let index = 0;
    // The buckets change depending on the context, that is currently decided by
    // the first added match (the heuristic one).
    if (!this._buckets) {
      // Convert the buckets to readable objects with a count property.
      let buckets =
        match.type == UrlbarUtils.RESULT_GROUP.HEURISTIC &&
        match.style.includes("searchengine")
          ? UrlbarPrefs.get("matchBucketsSearch")
          : UrlbarPrefs.get("matchBuckets");
      // - available is the number of available slots in the bucket
      // - insertIndex is the index of the first available slot in the bucket
      // - count is the number of matches in the bucket, note that it also
      //   account for matches from the previous search, while available and
      //   insertIndex don't.
      this._buckets = buckets.map(([type, available]) => ({
        type,
        available,
        insertIndex: 0,
        count: 0,
      }));
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
    this._usedURLs[index] = {
      key: urlMapKey,
      action,
      type: match.type,
      prefix,
      comment: match.comment || "",
    };
    return { index, replace };
  },

  _addOriginAutofillMatch(row) {
    this._addAutofillMatch(
      row.getResultByIndex(QUERYINDEX_ORIGIN_AUTOFILLED_VALUE),
      row.getResultByIndex(QUERYINDEX_ORIGIN_URL),
      row.getResultByIndex(QUERYINDEX_ORIGIN_FRECENCY)
    );
  },

  _addURLAutofillMatch(row) {
    let url = row.getResultByIndex(QUERYINDEX_URL_URL);
    let strippedURL = row.getResultByIndex(QUERYINDEX_URL_STRIPPED_URL);
    // We autofill urls to-the-next-slash.
    // http://mozilla.org/foo/bar/baz will be autofilled to:
    //  - http://mozilla.org/f[oo/]
    //  - http://mozilla.org/foo/b[ar/]
    //  - http://mozilla.org/foo/bar/b[az]
    let value;
    let strippedURLIndex = url.indexOf(strippedURL);
    let strippedPrefix = url.substr(0, strippedURLIndex);
    let nextSlashIndex = url.indexOf(
      "/",
      strippedURLIndex + strippedURL.length - 1
    );
    if (nextSlashIndex == -1) {
      value = url.substr(strippedURLIndex);
    } else {
      value = url.substring(strippedURLIndex, nextSlashIndex + 1);
    }

    this._addAutofillMatch(
      value,
      strippedPrefix + value,
      row.getResultByIndex(QUERYINDEX_URL_FRECENCY)
    );
  },

  _addAutofillMatch(
    autofilledValue,
    finalCompleteValue,
    frecency = Infinity,
    extraStyles = []
  ) {
    // The match's comment is only for display.  Set it to finalCompleteValue,
    // the actual URL that will be visited when the user chooses the match, so
    // that the user knows exactly where the match will take them.  To make it
    // look a little nicer, remove "http://", and if the user typed a host
    // without a trailing slash, remove any trailing slash, too.
    let [comment] = stripPrefixAndTrim(finalCompleteValue, {
      stripHttp: true,
      trimEmptyQuery: true,
      trimSlash: !this._searchString.includes("/"),
    });

    this._addMatch({
      value: this._strippedPrefix + autofilledValue,
      finalCompleteValue,
      comment,
      frecency,
      style: ["autofill"].concat(extraStyles).join(" "),
      icon: iconHelper(finalCompleteValue),
    });
  },

  // This is the same as _addFilteredQueryMatch, but it only returns a few
  // results, caching the others. If at the end we don't find other results, we
  // can add these.
  _addAdaptiveQueryMatch(row) {
    // Allow one quarter of the results to be adaptive results.
    // Note: ideally adaptive results should have their own provider and the
    // results muxer should decide what to show.  But that's too complex to
    // support in the current code, so that's left for a future refactoring.
    if (this._adaptiveCount < Math.ceil(this._maxResults / 4)) {
      this._addFilteredQueryMatch(row);
    } else {
      this._extraAdaptiveRows.push(row);
    }
    this._adaptiveCount++;
  },

  _addFilteredQueryMatch(row) {
    let placeId = row.getResultByIndex(QUERYINDEX_PLACEID);
    let url = row.getResultByIndex(QUERYINDEX_URL);
    let openPageCount = row.getResultByIndex(QUERYINDEX_SWITCHTAB) || 0;
    let historyTitle = row.getResultByIndex(QUERYINDEX_TITLE) || "";
    let bookmarked = row.getResultByIndex(QUERYINDEX_BOOKMARKED);
    let bookmarkTitle = bookmarked
      ? row.getResultByIndex(QUERYINDEX_BOOKMARKTITLE)
      : null;
    let tags = row.getResultByIndex(QUERYINDEX_TAGS) || "";
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    let match = {
      placeId,
      value: url,
      comment: bookmarkTitle || historyTitle,
      icon: iconHelper(url),
      frecency: frecency || FRECENCY_DEFAULT,
    };

    if (
      this._enableActions &&
      openPageCount > 0 &&
      this.hasBehavior("openpage")
    ) {
      if (this._currentPage == match.value) {
        // Don't suggest switching to the current tab.
        return;
      }
      // Actions are enabled and the page is open.  Add a switch-to-tab result.
      match.value = makeActionUrl("switchtab", { url: match.value });
      match.style = "action switchtab";
    } else if (
      this.hasBehavior("history") &&
      !this.hasBehavior("bookmark") &&
      !tags
    ) {
      // The consumer wants only history and not bookmarks and there are no
      // tags.  We'll act as if the page is not bookmarked.
      match.style = "favicon";
    } else if (tags) {
      // Store the tags in the title.  It's up to the consumer to extract them.
      match.comment += UrlbarUtils.TITLE_TAGS_SEPARATOR + tags;
      // If we're not suggesting bookmarks, then this shouldn't display as one.
      match.style = this.hasBehavior("bookmark") ? "bookmark-tag" : "tag";
    } else if (bookmarked) {
      match.style = "bookmark";
    }

    this._addMatch(match);
  },

  /**
   * @return a string consisting of the search query to be used based on the
   * previously set urlbar suggestion preferences.
   */
  get _suggestionPrefQuery() {
    if (
      !this.hasBehavior("restrict") &&
      this.hasBehavior("history") &&
      this.hasBehavior("bookmark")
    ) {
      return defaultQuery();
    }
    let conditions = [];
    if (this.hasBehavior("history")) {
      // Enforce ignoring the visit_count index, since the frecency one is much
      // faster in this case.  ANALYZE helps the query planner to figure out the
      // faster path, but it may not have up-to-date information yet.
      conditions.push("+h.visit_count > 0");
    }
    if (this.hasBehavior("bookmark")) {
      conditions.push("bookmarked");
    }
    if (this.hasBehavior("tag")) {
      conditions.push("tags NOTNULL");
    }

    return conditions.length
      ? defaultQuery("AND " + conditions.join(" AND "))
      : defaultQuery();
  },

  /**
   * Get the search string with the keyword substitution applied.
   * If the user-provided string starts with a keyword that gave a heuristic
   * result, it can provide a substitute string (e.g. the domain that keyword
   * will search) so that the history/bookmark results we show will correspond
   * to the keyword search rather than searching for the literal keyword.
   */
  get _keywordSubstitutedSearchString() {
    let tokens = this._searchTokens.map(t => t.value);
    if (this._keywordSubstitute) {
      tokens = [this._keywordSubstitute, ...tokens.slice(1)];
    }
    return tokens.join(" ");
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
        searchString: this._keywordSubstitutedSearchString,
        userContextId: this._userContextId,
        // Limit the query to the the maximum number of desired results.
        // This way we can avoid doing more work than needed.
        maxResults: this._maxResults,
      },
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
        searchString: this._keywordSubstitutedSearchString,
        userContextId: this._userContextId,
        maxResults: this._maxResults,
      },
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
        query_type: QUERYTYPE_ADAPTIVE,
        matchBehavior: this._matchBehavior,
        searchBehavior: this._behavior,
        userContextId: this._userContextId,
        maxResults: this._maxResults,
      },
    ];
  },

  /**
   * Whether we should try to autoFill.
   */
  get _shouldAutofill() {
    // First of all, check for the autoFill pref.
    if (!UrlbarPrefs.get("autoFill")) {
      return false;
    }

    if (this._searchTokens.length != 1) {
      return false;
    }

    // autoFill can only cope with history, bookmarks, and about: entries.
    if (!this.hasBehavior("history") && !this.hasBehavior("bookmark")) {
      return false;
    }

    // autoFill doesn't search titles or tags.
    if (this.hasBehavior("title") || this.hasBehavior("tag")) {
      return false;
    }

    // Don't try to autofill if the search term includes any whitespace.
    // This may confuse completeDefaultIndex cause the AUTOCOMPLETE_MATCH
    // tokenizer ends up trimming the search string and returning a value
    // that doesn't match it, or is even shorter.
    if (REGEXP_SPACES.test(this._originalSearchString)) {
      return false;
    }

    if (!this._searchString.length) {
      return false;
    }

    if (this._prohibitAutoFill) {
      return false;
    }

    return true;
  },

  /**
   * Obtains the query to search for autofill origin results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _originQuery() {
    // At this point, _searchString is not a URL with a path; it does not
    // contain a slash, except for possibly at the very end.  If there is
    // trailing slash, remove it when searching here to match the rest of the
    // string because it may be an origin.
    let searchStr = this._searchString.endsWith("/")
      ? this._searchString.slice(0, -1)
      : this._searchString;

    let opts = {
      query_type: QUERYTYPE_AUTOFILL_ORIGIN,
      searchString: searchStr.toLowerCase(),
      stddevMultiplier: UrlbarPrefs.get("autoFill.stddevMultiplier"),
    };
    if (this._strippedPrefix) {
      opts.prefix = this._strippedPrefix;
    }

    if (this.hasBehavior("history") && this.hasBehavior("bookmark")) {
      return [
        this._strippedPrefix
          ? QUERY_ORIGIN_PREFIX_HISTORY_BOOKMARK
          : QUERY_ORIGIN_HISTORY_BOOKMARK,
        opts,
      ];
    }
    if (this.hasBehavior("history")) {
      return [
        this._strippedPrefix
          ? QUERY_ORIGIN_PREFIX_HISTORY
          : QUERY_ORIGIN_HISTORY,
        opts,
      ];
    }
    if (this.hasBehavior("bookmark")) {
      return [
        this._strippedPrefix
          ? QUERY_ORIGIN_PREFIX_BOOKMARK
          : QUERY_ORIGIN_BOOKMARK,
        opts,
      ];
    }
    throw new Error("Either history or bookmark behavior expected");
  },

  /**
   * Obtains the query to search for autoFill url results.
   *
   * @return an array consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  get _urlQuery() {
    // Try to get the host from the search string.  The host is the part of the
    // URL up to either the path slash, port colon, or query "?".  If the search
    // string doesn't look like it begins with a host, then return; it doesn't
    // make sense to do a URL query with it.
    if (!this._urlQueryHostRegexp) {
      this._urlQueryHostRegexp = /^[^/:?]+/;
    }
    let hostMatch = this._urlQueryHostRegexp.exec(this._searchString);
    if (!hostMatch) {
      return [null, null];
    }

    let host = hostMatch[0].toLowerCase();
    let revHost =
      host
        .split("")
        .reverse()
        .join("") + ".";

    // Build a string that's the URL stripped of its prefix, i.e., the host plus
    // everything after the host.  Use _trimmedOriginalSearchString instead of
    // this._searchString because this._searchString has had unEscapeURIForUI()
    // called on it.  It's therefore not necessarily the literal URL.
    let strippedURL = this._trimmedOriginalSearchString;
    if (this._strippedPrefix) {
      strippedURL = strippedURL.substr(this._strippedPrefix.length);
    }
    strippedURL = host + strippedURL.substr(host.length);

    let opts = {
      query_type: QUERYTYPE_AUTOFILL_URL,
      revHost,
      strippedURL,
    };
    if (this._strippedPrefix) {
      opts.prefix = this._strippedPrefix;
    }

    if (this.hasBehavior("history") && this.hasBehavior("bookmark")) {
      return [
        this._strippedPrefix
          ? QUERY_URL_PREFIX_HISTORY_BOOKMARK
          : QUERY_URL_HISTORY_BOOKMARK,
        opts,
      ];
    }
    if (this.hasBehavior("history")) {
      return [
        this._strippedPrefix ? QUERY_URL_PREFIX_HISTORY : QUERY_URL_HISTORY,
        opts,
      ];
    }
    if (this.hasBehavior("bookmark")) {
      return [
        this._strippedPrefix ? QUERY_URL_PREFIX_BOOKMARK : QUERY_URL_BOOKMARK,
        opts,
      ];
    }
    throw new Error("Either history or bookmark behavior expected");
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
      if (!this.pending) {
        return;
      }
      this._notifyDelaysCount = 0;
      let resultCode = this._result.matchCount
        ? "RESULT_SUCCESS"
        : "RESULT_NOMATCH";
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
  if (UrlbarPrefs.get("usepreloadedtopurls.enabled")) {
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
    if (!this._promiseDatabase) {
      this._promiseDatabase = (async () => {
        let conn = await PlacesUtils.promiseLargeCacheDBConnection();

        // We don't catch exceptions here as it is too late to block shutdown.
        Sqlite.shutdown.addBlocker("Places UnifiedComplete.js closing", () => {
          // Break a possible cycle through the
          // previous result, the controller and
          // ourselves.
          this._currentSearch = null;
        });

        await UrlbarProviderOpenTabs.promiseDb();
        return conn;
      })().catch(ex => {
        dump("Couldn't get database handle: " + ex + "\n");
        Cu.reportError(ex);
      });
    }
    return this._promiseDatabase;
  },

  // mozIPlacesAutoComplete

  populatePreloadedSiteStorage(json) {
    PreloadedSiteStorage.populate(json);
  },

  /**
   * This is a wrapper around startSearch, with a better interface towards
   * Quantum Bar. Long term this provider should be migrated to new separate
   * providers and this won't be necessary
   * @param {UrlbarQueryContext} queryContext
   *        The context for the current search.
   * @param {Function} onAutocompleteResult
   *        A callback to notify each result to.
   */
  startQuery(queryContext, onAutocompleteResult) {
    let deferred = PromiseUtils.defer();
    let listener = {
      onSearchResult(_, result) {
        let done =
          [
            Ci.nsIAutoCompleteResult.RESULT_IGNORED,
            Ci.nsIAutoCompleteResult.RESULT_FAILURE,
            Ci.nsIAutoCompleteResult.RESULT_NOMATCH,
            Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
          ].includes(result.searchResult) || result.errorDescription;
        onAutocompleteResult(result);
        if (done) {
          deferred.resolve();
        }
      },
    };
    this.startSearch(
      queryContext.searchString,
      "",
      null,
      listener,
      queryContext
    );
    this._deferred = deferred;
    return this._deferred.promise;
  },

  // nsIAutoCompleteSearch

  startSearch(
    searchString,
    searchParam,
    acPreviousResult,
    listener,
    queryContext
  ) {
    // Stop the search in case the controller has not taken care of it.
    if (this._currentSearch) {
      this.stopSearch();
    }

    let search = (this._currentSearch = new Search(
      searchString,
      searchParam,
      listener,
      this,
      queryContext
    ));
    this.getDatabaseHandle()
      .then(conn => search.execute(conn))
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
    if (this._deferred) {
      this._deferred.resolve();
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
    // Clear state now to avoid race conditions, see below.
    let search = this._currentSearch;
    if (!search) {
      return;
    }
    this._lastLowResultsSearchSuggestion =
      search._lastLowResultsSearchSuggestion;

    if (!notify || !search.pending) {
      return;
    }

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

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAutoCompleteSearch,
    Ci.nsIAutoCompleteSearchDescriptor,
    Ci.mozIPlacesAutoComplete,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
};

var EXPORTED_SYMBOLS = ["UnifiedComplete"];
