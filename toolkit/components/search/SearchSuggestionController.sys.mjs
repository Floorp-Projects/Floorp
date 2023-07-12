/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

const DEFAULT_FORM_HISTORY_PARAM = "searchbar-history";
const HTTP_OK = 200;
const BROWSER_SUGGEST_PREF = "browser.search.suggest.enabled";
const BROWSER_SUGGEST_PRIVATE_PREF = "browser.search.suggest.enabled.private";
const BROWSER_RICH_SUGGEST_PREF = "browser.urlbar.richSuggestions.featureGate";
const REMOTE_TIMEOUT_PREF = "browser.search.suggest.timeout";
const REMOTE_TIMEOUT_DEFAULT = 500; // maximum time (ms) to wait before giving up on a remote suggestions

const SEARCH_DATA_TRANSFERRED_SCALAR = "browser.search.data_transferred";
const SEARCH_TELEMETRY_KEY_PREFIX = "sggt";
const SEARCH_TELEMETRY_PRIVATE_BROWSING_KEY_SUFFIX = "pb";

const SEARCH_TELEMETRY_LATENCY = "SEARCH_SUGGESTIONS_LATENCY_MS";

/**
 * Generates an UUID.
 *
 * @returns {string}
 *   An UUID string, without leading or trailing braces.
 */
function uuid() {
  let uuid = Services.uuid.generateUUID().toString();
  return uuid.slice(1, uuid.length - 1);
}

/**
 * Represents a search suggestion.
 * TODO: Support other Google tail fields: `a`, `dc`, `i`, `q`, `ansa`,
 * `ansb`, `ansc`, `du`. See bug 1626897 comment 2.
 */
class SearchSuggestionEntry {
  /**
   * Creates an entry.
   *
   * @param {string} value
   *   The suggestion as a full-text string. Suitable for display directly to
   *   the user.
   * @param {object} options
   *   An object with the following properties:
   * @param {string} [options.matchPrefix]
   *   Represents the part of a tail suggestion that is already typed. For
   *   example, Google returns "…" as the match prefix to replace
   *   "what time is it in" in a tail suggestion for the query
   *   "what time is it in t".
   * @param {string} [options.tail]
   *   Represents the suggested part of a tail suggestion. For example, Google
   *   might return "toronto" as the tail for the query "what time is it in t".
   * @param {string} [options.icon]
   *   An icon representing the result in a data uri format.
   * @param {string} [options.description]
   *   A description of the result.
   * @param {boolean} [options.trending]
   *   Whether this is a trending suggestion.
   */
  constructor(value, { matchPrefix, tail, icon, description, trending } = {}) {
    this.#value = value;
    this.#matchPrefix = matchPrefix;
    this.#tail = tail;
    this.#trending = trending;
    this.#icon = icon;
    this.#description = description;
  }

  get value() {
    return this.#value;
  }

  get matchPrefix() {
    return this.#matchPrefix;
  }

  get tail() {
    return this.#tail;
  }

  get trending() {
    return this.#trending;
  }

  get icon() {
    return this.#icon;
  }

  get description() {
    return this.#description;
  }

  get tailOffsetIndex() {
    if (!this.#tail) {
      return -1;
    }

    let offsetIndex = this.#value.lastIndexOf(this.#tail);
    if (offsetIndex + this.#tail.length < this.#value.length) {
      // We might have a tail suggestion that starts with a word contained in
      // the full-text suggestion. e.g. "london sights in l" ... "london".
      let lastWordIndex = this.#value.lastIndexOf(" ");
      if (this.#tail.startsWith(this.#value.substring(lastWordIndex))) {
        offsetIndex = lastWordIndex;
      } else {
        // Something's gone wrong. Consumers should not show this result.
        offsetIndex = -1;
      }
    }

    return offsetIndex;
  }

  /**
   * Returns true if `otherEntry` is equivalent to this instance of
   * SearchSuggestionEntry.
   *
   * @param {SearchSuggestionEntry} otherEntry The entry to compare to.
   * @returns {boolean}
   */
  equals(otherEntry) {
    return otherEntry.value == this.value;
  }

  #value;
  #matchPrefix;
  #tail;
  #trending;
  #icon;
  #description;
}

// Maps each engine name to a unique firstPartyDomain, so that requests to
// different engines are isolated from each other and from normal browsing.
// This is the same for all the controllers.
var gFirstPartyDomains = new Map();

/**
 *
 * The SearchSuggestionController class fetches search suggestions from two
 * sources: a remote search engine and the user's previous searches stored
 * locally in their profile (also called "form history").
 *
 * The number of each suggestion type is configurable, and the controller will
 * fetch and return both types at the same time. Instances of the class are
 * reusable, but one instance should be used per input. The fetch() method is
 * the main entry point. After creating an instance of the class, fetch() can
 * be called many times to fetch suggestions.
 *
 */
export class SearchSuggestionController {
  /**
   * Constructor
   *
   * @param {string} [formHistoryParam]
   *   The form history type to use with this controller.
   */
  constructor(formHistoryParam = DEFAULT_FORM_HISTORY_PARAM) {
    this.formHistoryParam = formHistoryParam;
  }

  /**
   * The maximum length of a value to be stored in search history.
   *
   *  @type {number}
   */
  static SEARCH_HISTORY_MAX_VALUE_LENGTH = 255;

  /**
   * Maximum time (ms) to wait before giving up on remote suggestions
   *
   *  @type {number}
   */
  static REMOTE_TIMEOUT_DEFAULT = REMOTE_TIMEOUT_DEFAULT;

  /**
   * Determines whether the given engine offers search suggestions.
   *
   * @param {nsISearchEngine} engine - The search engine
   * @param {boolean} fetchTrending - Whether we should fetch trending suggestions.
   * @returns {boolean} True if the engine offers suggestions and false otherwise.
   */
  static engineOffersSuggestions(engine, fetchTrending) {
    return engine.supportsResponseType(
      fetchTrending
        ? lazy.SearchUtils.URL_TYPE.TRENDING_JSON
        : lazy.SearchUtils.URL_TYPE.SUGGEST_JSON
    );
  }

  /**
   * The maximum number of local form history results to return. This limit is
   * only enforced if remote results are also returned.
   *
   * @type {number}
   */
  maxLocalResults = 5;

  /**
   * The maximum number of remote search engine results to return.
   * We'll actually only display at most
   * maxRemoteResults - <displayed local results count> remote results.
   *
   * @type {number}
   */
  maxRemoteResults = 10;

  /**
   * The additional parameter used when searching form history.
   *
   * @type {string}
   */
  formHistoryParam = DEFAULT_FORM_HISTORY_PARAM;

  /**
   * The last form history result used to improve the performance of
   * subsequent searches. This shouldn't be used for any other purpose as it
   * is never cleared and therefore could be stale.
   *
   * @type {object|null}
   */
  formHistoryResult = null;

  /**
   * Gets the firstPartyDomains Map, useful for tests.
   *
   * @returns {Map} firstPartyDomains mapped by engine names.
   */
  get firstPartyDomains() {
    return gFirstPartyDomains;
  }

  /**
   * @typedef {object} FetchResult
   * @property {Array<SearchSuggestionEntry>} local
   *   Contains local search suggestions.
   * @property {Array<SearchSuggestionEntry>} remote
   *   Contains remote search suggestions.
   */

  /**
   * Fetch search suggestions from all of the providers. Fetches in progress
   * will be stopped and results from them will not be provided.
   *
   * @param {string} searchTerm - the term to provide suggestions for
   * @param {boolean} privateMode - whether the request is being made in the
   *                                context of private browsing.
   * @param {nsISearchEngine} engine - search engine for the suggestions.
   * @param {int} userContextId - the userContextId of the selected tab.
   * @param {boolean} restrictToEngine - whether to restrict local historical
   *   suggestions to the ones registered under the given engine.
   * @param {boolean} dedupeRemoteAndLocal - whether to remove remote
   *   suggestions that dupe local suggestions
   * @param {boolean} fetchTrending - Whether we should fetch trending suggestions.
   *
   * @returns {Promise<FetchResult>}
   */
  fetch(
    searchTerm,
    privateMode,
    engine,
    userContextId = 0,
    restrictToEngine = false,
    dedupeRemoteAndLocal = true,
    fetchTrending = false
  ) {
    // There is no smart filtering from previous results here (as there is when
    // looking through history/form data) because the result set returned by the
    // server is different for every typed value - e.g. "ocean breathes" does
    // not return a subset of the results returned for "ocean".

    this.stop();

    if (!Services.search.isInitialized) {
      throw new Error("Search not initialized yet (how did you get here?)");
    }
    if (typeof privateMode === "undefined") {
      throw new Error(
        "The privateMode argument is required to avoid unintentional privacy leaks"
      );
    }
    if (!engine.getSubmission) {
      throw new Error("Invalid search engine");
    }
    if (!this.maxLocalResults && !this.maxRemoteResults) {
      throw new Error("Zero results expected, what are you trying to do?");
    }
    if (this.maxLocalResults < 0 || this.maxRemoteResults < 0) {
      throw new Error("Number of requested results must be positive");
    }

    // Array of promises to resolve before returning results.
    let promises = [];
    let context = (this.#context = {
      awaitingLocalResults: false,
      dedupeRemoteAndLocal,
      engine,
      engineId: engine?.identifier || "other",
      fetchTrending,
      privateMode,
      request: null,
      restrictToEngine,
      searchString: searchTerm,
      telemetryHandled: false,
      timer: Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer),
      userContextId,
    });

    // Fetch local results from Form History, if requested.
    if (this.maxLocalResults && !fetchTrending) {
      context.awaitingLocalResults = true;
      promises.push(this.#fetchFormHistory(context));
    }
    // Fetch remote results from Search Service, if requested.
    if (
      (searchTerm || fetchTrending) &&
      this.suggestionsEnabled &&
      (!privateMode || this.suggestionsInPrivateBrowsingEnabled) &&
      this.maxRemoteResults &&
      SearchSuggestionController.engineOffersSuggestions(engine, fetchTrending)
    ) {
      promises.push(this.#fetchRemote(context));
    }

    function handleRejection(reason) {
      if (reason == "HTTP request aborted") {
        // Do nothing since this is normal.
        return null;
      }
      console.error("SearchSuggestionController rejection:", reason);
      return null;
    }
    return Promise.all(promises).then(
      results => this.#dedupeAndReturnResults(context, results),
      handleRejection
    );
  }

  /**
   * Stop pending fetches so no results are returned from them.
   *
   * Note: If there was no remote results fetched, the fetching cannot be
   * stopped and local results will still be returned because stopping relies
   * on aborting the XMLHTTPRequest to reject the promise for Promise.all.
   */
  stop() {
    if (this.#context) {
      this.#context.abort = true;
      this.#context.request?.abort();
    }
    this.#context = null;
  }

  #context;

  async #fetchFormHistory(context) {
    // We don't cache these results as we assume that the in-memory SQL cache is
    // good enough in performance.
    let params = {
      fieldname: this.formHistoryParam,
    };

    if (context.restrictToEngine) {
      params.source = context.engine.name;
    }

    let results = await lazy.FormHistory.getAutoCompleteResults(
      context.searchString,
      params
    );

    context.awaitingLocalResults = false;

    return { localResults: results };
  }

  /**
   * Records per-engine telemetry after a search has finished.
   *
   * @param {object} context
   *   The search context.
   */
  #reportTelemetryForEngine(context) {
    this.#reportBandwidthForEngine(context);

    // Stop the latency stopwatch.
    if (!context.telemetryHandled) {
      if (context.abort) {
        TelemetryStopwatch.cancelKeyed(
          SEARCH_TELEMETRY_LATENCY,
          context.engineId,
          context
        );
      } else {
        TelemetryStopwatch.finishKeyed(
          SEARCH_TELEMETRY_LATENCY,
          context.engineId,
          context
        );
      }
      context.telemetryHandled = true;
    }
  }

  /**
   * Report bandwidth used by search activities. It only reports when it matches
   * search provider information.
   *
   * @param {object} context
   *   The search context.
   * @param {boolean} context.abort
   *   If the request should be aborted.
   * @param {string} context.engineId
   *   The search engine identifier.
   * @param {object} context.request
   *   Request information
   * @param {boolean} context.privateMode
   *   Set to true if this is coming from a private browsing mode request.
   */
  #reportBandwidthForEngine(context) {
    if (context.abort || !context.request.channel) {
      return;
    }

    let channel = ChannelWrapper.get(context.request.channel);
    let bytesTransferred = channel.requestSize + channel.responseSize;
    if (bytesTransferred == 0) {
      return;
    }

    let telemetryKey = `${SEARCH_TELEMETRY_KEY_PREFIX}-${context.engineId}`;
    if (context.privateMode) {
      telemetryKey += `-${SEARCH_TELEMETRY_PRIVATE_BROWSING_KEY_SUFFIX}`;
    }

    Services.telemetry.keyedScalarAdd(
      SEARCH_DATA_TRANSFERRED_SCALAR,
      telemetryKey,
      bytesTransferred
    );
  }

  /**
   * Fetch suggestions from the search engine over the network.
   *
   * @param {object} context
   *   The search context.
   * @returns {Promise}
   *   Returns a promise that is resolved when the response is received, or
   *   rejected if there is an error.
   */
  #fetchRemote(context) {
    let deferredResponse = lazy.PromiseUtils.defer();
    let request = (context.request = new XMLHttpRequest());
    // Expect the response type to be JSON, so that the network layer will
    // decode it for us. This will also ignore incorrect Mime Types, as we are
    // dictating how we process it.
    request.responseType = "json";

    let submission = context.engine.getSubmission(
      context.searchString,
      context.searchString
        ? lazy.SearchUtils.URL_TYPE.SUGGEST_JSON
        : lazy.SearchUtils.URL_TYPE.TRENDING_JSON
    );
    let method = submission.postData ? "POST" : "GET";
    request.open(method, submission.uri.spec, true);
    // Don't set or store cookies or on-disk cache.
    request.channel.loadFlags =
      Ci.nsIChannel.LOAD_ANONYMOUS | Ci.nsIChannel.INHIBIT_PERSISTENT_CACHING;
    // Use a unique first-party domain for each engine, to isolate the
    // suggestions requests.
    if (!gFirstPartyDomains.has(context.engine.name)) {
      // Use the engine identifier, or an uuid when not available, because the
      // domain cannot contain invalid chars and the engine name may not be
      // suitable. When using an uuid the firstPartyDomain of the same engine
      // will differ across restarts, but that's acceptable for now.
      // TODO (Bug 1511339): use a persistent unique identifier per engine.
      gFirstPartyDomains.set(
        context.engine.name,
        `${context.engine.identifier || uuid()}.search.suggestions.mozilla`
      );
    }
    let firstPartyDomain = gFirstPartyDomains.get(context.engine.name);

    request.setOriginAttributes({
      userContextId: context.userContextId,
      privateBrowsingId: context.privateMode ? 1 : 0,
      firstPartyDomain,
    });

    request.mozBackgroundRequest = true; // suppress dialogs and fail silently

    context.timer.initWithCallback(
      () => {
        // Abort if we already got local results.
        if (
          request.readyState != 4 /* not complete */ &&
          !context.awaitingLocalResults
        ) {
          deferredResponse.resolve("HTTP request timeout");
        }
      },
      this.remoteTimeout,
      Ci.nsITimer.TYPE_ONE_SHOT
    );

    request.addEventListener("load", () => {
      context.timer.cancel();
      this.#reportTelemetryForEngine(context);
      if (!this.#context || context != this.#context || context.abort) {
        deferredResponse.resolve(
          "Got HTTP response after the request was cancelled"
        );
        return;
      }
      this.#onRemoteLoaded(context, deferredResponse);
    });

    request.addEventListener("error", evt => {
      this.#reportTelemetryForEngine(context);
      deferredResponse.resolve("HTTP error");
    });

    // Reject for an abort assuming it's always from .stop() in which case we
    // shouldn't return local or remote results for existing searches.
    request.addEventListener("abort", evt => {
      context.timer.cancel();
      this.#reportTelemetryForEngine(context);
      deferredResponse.reject("HTTP request aborted");
    });

    if (submission.postData) {
      request.sendInputStream(submission.postData);
    } else {
      request.send();
    }

    TelemetryStopwatch.startKeyed(
      SEARCH_TELEMETRY_LATENCY,
      context.engineId,
      context
    );

    return deferredResponse.promise;
  }

  /**
   * Called when the request completed successfully (thought the HTTP status
   * could be anything) so we can handle the response data.
   *
   * @param {object} context
   *   The search context.
   * @param {Promise} deferredResponse
   *   The promise to resolve when a response is received.
   * @private
   */
  #onRemoteLoaded(context, deferredResponse) {
    let status;
    try {
      status = context.request.status;
    } catch (e) {
      // The XMLHttpRequest can throw NS_ERROR_NOT_AVAILABLE.
      deferredResponse.resolve("Unknown HTTP status: " + e);
      return;
    }

    if (status != HTTP_OK) {
      deferredResponse.resolve(
        "Non-200 status or empty HTTP response: " + status
      );
      return;
    }

    let serverResults = context.request.response;

    try {
      if (
        !Array.isArray(serverResults) ||
        serverResults[0] == undefined ||
        (context.searchString.localeCompare(serverResults[0], undefined, {
          sensitivity: "base",
        }) &&
          // Some engines (e.g. Amazon) return a search string containing
          // escaped Unicode sequences. Try decoding the remote search string
          // and compare that with our typed search string.
          context.searchString.localeCompare(
            decodeURIComponent(
              JSON.parse('"' + serverResults[0].replace(/\"/g, '\\"') + '"')
            ),
            undefined,
            {
              sensitivity: "base",
            }
          ))
      ) {
        // something is wrong here so drop remote results
        deferredResponse.resolve(
          "Unexpected response, searchString does not match remote response"
        );
        return;
      }
    } catch (ex) {
      deferredResponse.resolve(
        `Failed to parse the remote response string: ${ex}`
      );
      return;
    }

    // Remove the search string from the server results since it is no longer
    // needed.
    let results = serverResults.slice(1) || [];
    deferredResponse.resolve({ result: results });
  }

  /**
   * @param {object} context
   *   The search context.
   * @param {Array} suggestResults - an array of result objects from different
   *   sources (local or remote).
   * @returns {object}
   */
  #dedupeAndReturnResults(context, suggestResults) {
    if (context.abort) {
      return null;
    }

    let results = {
      term: context.searchString,
      remote: [],
      local: [],
    };

    for (let resultData of suggestResults) {
      if (typeof resultData === "string") {
        // Failure message
        console.error(
          "SearchSuggestionController found an unexpected string value:",
          resultData
        );
      } else if (resultData.localResults) {
        results.formHistoryResults = resultData.localResults;
        results.local = resultData.localResults.map(
          s => new SearchSuggestionEntry(s.text)
        );
      } else if (resultData.result) {
        // Remote result
        let richSuggestionData = this.#getRichSuggestionData(resultData.result);
        let fullTextSuggestions = resultData.result[0];
        for (let i = 0; i < fullTextSuggestions.length; ++i) {
          results.remote.push(
            this.#newSearchSuggestionEntry(
              fullTextSuggestions[i],
              richSuggestionData?.[i],
              context.fetchTrending
            )
          );
        }
      }
    }

    // If we have remote results, cap the number of local results
    if (results.remote.length) {
      results.local = results.local.slice(0, this.maxLocalResults);
    }

    // We don't want things to appear in both history and suggestions so remove
    // entries from remote results that are already in local.
    if (
      results.remote.length &&
      results.local.length &&
      context.dedupeRemoteAndLocal
    ) {
      for (let i = 0; i < results.local.length; ++i) {
        let dupIndex = results.remote.findIndex(e =>
          e.equals(results.local[i])
        );
        if (dupIndex != -1) {
          results.remote.splice(dupIndex, 1);
        }
      }
    }

    // Trim the number of results to the maximum requested (now that we've pruned dupes).
    let maxRemoteCount = this.maxRemoteResults;
    if (context.dedupeRemoteAndLocal) {
      maxRemoteCount -= results.local.length;
    }
    results.remote = results.remote.slice(0, maxRemoteCount);

    return results;
  }

  /**
   * Returns rich suggestion data from a remote fetch, if available.
   *
   * @param {Array} remoteResultData
   *  The results.remote array returned by SearchSuggestionsController.fetch.
   * @returns {Array}
   *  An array of additional rich suggestion data. Each element should
   *  correspond to the array of text suggestions.
   */
  #getRichSuggestionData(remoteResultData) {
    if (!remoteResultData || !Array.isArray(remoteResultData)) {
      return undefined;
    }

    for (let entry of remoteResultData) {
      if (
        typeof entry == "object" &&
        entry.hasOwnProperty("google:suggestdetail")
      ) {
        let richData = entry["google:suggestdetail"];
        if (
          Array.isArray(richData) &&
          richData.length == remoteResultData[0].length
        ) {
          return richData;
        }
      }
    }
    return undefined;
  }

  /**
   * Given a text suggestion and rich suggestion data, returns a
   * SearchSuggestionEntry.
   *
   * @param {string} suggestion
   *   A suggestion string.
   * @param {object} richSuggestionData
   *   Rich suggestion data returned by the engine. In Google's case, this is
   *   the corresponding entry at "google:suggestdetail".
   * @param {boolean} trending
   *   Whether the suggestion is a trending suggestion.
   * @returns {SearchSuggestionEntry}
   */
  #newSearchSuggestionEntry(suggestion, richSuggestionData, trending) {
    if (richSuggestionData && (!trending || this.richSuggestionsEnabled)) {
      // We have valid rich suggestions.
      let args = {
        matchPrefix: richSuggestionData?.mp,
        tail: richSuggestionData?.t,
        trending,
      };

      if (this.richSuggestionsEnabled) {
        args.icon = richSuggestionData?.i;
        args.description = richSuggestionData?.a;
      }

      return new SearchSuggestionEntry(suggestion, args);
    }
    // Return a regular suggestion.
    return new SearchSuggestionEntry(suggestion, { trending });
  }
}

/**
 * The maximum time (ms) to wait before giving up on a remote suggestions.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  SearchSuggestionController.prototype,
  "remoteTimeout",
  REMOTE_TIMEOUT_PREF,
  REMOTE_TIMEOUT_DEFAULT
);

/**
 * Whether or not remote suggestions are turned on.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  SearchSuggestionController.prototype,
  "suggestionsEnabled",
  BROWSER_SUGGEST_PREF,
  true
);

/**
 * Whether or not remote suggestions are turned on in private browsing mode.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  SearchSuggestionController.prototype,
  "suggestionsInPrivateBrowsingEnabled",
  BROWSER_SUGGEST_PRIVATE_PREF,
  false
);

/**
 * Whether or not rich suggestions are turned on.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  SearchSuggestionController.prototype,
  "richSuggestionsEnabled",
  BROWSER_RICH_SUGGEST_PREF,
  false
);
