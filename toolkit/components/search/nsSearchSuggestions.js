/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const SEARCH_RESPONSE_SUGGESTION_JSON = "application/x-suggestions+json";

const BROWSER_SUGGEST_PREF = "browser.search.suggest.enabled";
const XPCOM_SHUTDOWN_TOPIC              = "xpcom-shutdown";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const HTTP_OK                    = 200;
const HTTP_INTERNAL_SERVER_ERROR = 500;
const HTTP_BAD_GATEWAY           = 502;
const HTTP_SERVICE_UNAVAILABLE   = 503;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/nsFormAutoCompleteResult.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/**
 * SuggestAutoComplete is a base class that implements nsIAutoCompleteSearch
 * and can collect results for a given search by using the search URL supplied
 * by the subclass. We do it this way since the AutoCompleteController in
 * Mozilla requires a unique XPCOM Service for every search provider, even if
 * the logic for two providers is identical.
 * @constructor
 */
function SuggestAutoComplete() {
  this._init();
}
SuggestAutoComplete.prototype = {

  _init: function() {
    this._addObservers();
    this._suggestEnabled = Services.prefs.getBoolPref(BROWSER_SUGGEST_PREF);
  },

  get _suggestionLabel() {
    delete this._suggestionLabel;
    let bundle = Services.strings.createBundle("chrome://global/locale/search/search.properties");
    return this._suggestionLabel = bundle.GetStringFromName("suggestion_label");
  },

  /**
   * Search suggestions will be shown if this._suggestEnabled is true.
   */
  _suggestEnabled: null,

  /*************************************************************************
   * Server request backoff implementation fields below
   * These allow us to throttle requests if the server is getting hammered.
   **************************************************************************/

  /**
   * This is an array that contains the timestamps (in unixtime) of
   * the last few backoff-triggering errors.
   */
  _serverErrorLog: [],

  /**
   * If we receive this number of backoff errors within the amount of time
   * specified by _serverErrorPeriod, then we initiate backoff.
   */
  _maxErrorsBeforeBackoff: 3,

  /**
   * If we receive enough consecutive errors (where "enough" is defined by
   * _maxErrorsBeforeBackoff above) within this time period,
   * we trigger the backoff behavior.
   */
  _serverErrorPeriod: 600000,  // 10 minutes in milliseconds

  /**
   * If we get another backoff error immediately after timeout, we increase the
   * backoff to (2 x old period) + this value.
   */
  _serverErrorTimeoutIncrement: 600000,  // 10 minutes in milliseconds

  /**
   * The current amount of time to wait before trying a server request
   * after receiving a backoff error.
   */
  _serverErrorTimeout: 0,

  /**
   * Time (in unixtime) after which we're allowed to try requesting again.
   */
  _nextRequestTime: 0,

  /**
   * The last engine we requested against (so that we can tell if the
   * user switched engines).
   */
  _serverErrorEngine: null,

  /**
   * The XMLHttpRequest object.
   * @private
   */
  _request: null,

  /**
   * The object implementing nsIAutoCompleteObserver that we notify when
   * we have found results
   * @private
   */
  _listener: null,

  /**
   * If this is true, we'll integrate form history results with the
   * suggest results.
   */
  _includeFormHistory: true,

  /**
   * True if a request for remote suggestions was sent. This is used to
   * differentiate between the "_request is null because the request has
   * already returned a result" and "_request is null because no request was
   * sent" cases.
   */
  _sentSuggestRequest: false,

  /**
   * This is the callback for the suggest timeout timer.
   */
  notify: function SAC_notify(timer) {
    // FIXME: bug 387341
    // Need to break the cycle between us and the timer.
    this._formHistoryTimer = null;

    // If this._listener is null, we've already sent out suggest results, so
    // nothing left to do here.
    if (!this._listener)
      return;

    // Otherwise, the XMLHTTPRequest for suggest results is taking too long,
    // so send out the form history results and cancel the request.
    this._listener.onSearchResult(this, this._formHistoryResult);
    this._reset();
  },

  /**
   * This determines how long (in ms) we should wait before giving up on
   * the suggestions and just showing local form history results.
   */
  _suggestionTimeout: 500,

  /**
   * This is the callback for that the form history service uses to
   * send us results.
   */
  onSearchResult: function SAC_onSearchResult(search, result) {
    this._formHistoryResult = result;

    if (this._request) {
      // We still have a pending request, wait a bit to give it a chance to
      // finish.
      this._formHistoryTimer = Cc["@mozilla.org/timer;1"].
                               createInstance(Ci.nsITimer);
      this._formHistoryTimer.initWithCallback(this, this._suggestionTimeout,
                                              Ci.nsITimer.TYPE_ONE_SHOT);
    } else if (!this._sentSuggestRequest) {
      // We didn't send a request, so just send back the form history results.
      this._listener.onSearchResult(this, this._formHistoryResult);
      this._reset();
    }
  },

  /**
   * This is the URI that the last suggest request was sent to.
   */
  _suggestURI: null,

  /**
   * Autocomplete results from the form history service get stored here.
   */
  _formHistoryResult: null,

  /**
   * This holds the suggest server timeout timer, if applicable.
   */
  _formHistoryTimer: null,

  /**
   * This clears all the per-request state.
   */
  _reset: function SAC_reset() {
    // Don't let go of our listener and form history result if the timer is
    // still pending, the timer will call _reset() when it fires.
    if (!this._formHistoryTimer) {
      this._listener = null;
      this._formHistoryResult = null;
    }
    this._request = null;
  },

  /**
   * This sends an autocompletion request to the form history service,
   * which will call onSearchResults with the results of the query.
   */
  _startHistorySearch: function SAC_SHSearch(searchString, searchParam) {
    var formHistory =
      Cc["@mozilla.org/autocomplete/search;1?name=form-history"].
      createInstance(Ci.nsIAutoCompleteSearch);
    formHistory.startSearch(searchString, searchParam, this._formHistoryResult, this);
  },

  /**
   * Makes a note of the fact that we've received a backoff-triggering
   * response, so that we can adjust the backoff behavior appropriately.
   */
  _noteServerError: function SAC__noteServeError() {
    var currentTime = Date.now();

    this._serverErrorLog.push(currentTime);
    if (this._serverErrorLog.length > this._maxErrorsBeforeBackoff)
      this._serverErrorLog.shift();

    if ((this._serverErrorLog.length == this._maxErrorsBeforeBackoff) &&
        ((currentTime - this._serverErrorLog[0]) < this._serverErrorPeriod)) {
      // increase timeout, and then don't request until timeout is over
      this._serverErrorTimeout = (this._serverErrorTimeout * 2) +
                                 this._serverErrorTimeoutIncrement;
      this._nextRequestTime = currentTime + this._serverErrorTimeout;
    }
  },

  /**
   * Resets the backoff behavior; called when we get a successful response.
   */
  _clearServerErrors: function SAC__clearServerErrors() {
    this._serverErrorLog = [];
    this._serverErrorTimeout = 0;
    this._nextRequestTime = 0;
  },

  /**
   * This checks whether we should send a server request (i.e. we're not
   * in a error-triggered backoff period.
   *
   * @private
   */
  _okToRequest: function SAC__okToRequest() {
    return Date.now() > this._nextRequestTime;
  },

  /**
   * This checks to see if the new search engine is different
   * from the previous one, and if so clears any error state that might
   * have accumulated for the old engine.
   *
   * @param engine The engine that the suggestion request would be sent to.
   * @private
   */
  _checkForEngineSwitch: function SAC__checkForEngineSwitch(engine) {
    if (engine == this._serverErrorEngine)
      return;

    // must've switched search providers, clear old errors
    this._serverErrorEngine = engine;
    this._clearServerErrors();
  },

  /**
   * This returns true if the status code of the HTTP response
   * represents a backoff-triggering error.
   *
   * @param status  The status code from the HTTP response
   * @private
   */
  _isBackoffError: function SAC__isBackoffError(status) {
    return ((status == HTTP_INTERNAL_SERVER_ERROR) ||
            (status == HTTP_BAD_GATEWAY) ||
            (status == HTTP_SERVICE_UNAVAILABLE));
  },

  /**
   * Called when the 'readyState' of the XMLHttpRequest changes. We only care
   * about state 4 (COMPLETED) - handle the response data.
   * @private
   */
  onReadyStateChange: function() {
    // xxx use the real const here
    if (!this._request || this._request.readyState != 4)
      return;

    try {
      var status = this._request.status;
    } catch (e) {
      // The XML HttpRequest can throw NS_ERROR_NOT_AVAILABLE.
      return;
    }

    if (this._isBackoffError(status)) {
      this._noteServerError();
      return;
    }

    var responseText = this._request.responseText;
    if (status != HTTP_OK || responseText == "")
      return;

    this._clearServerErrors();

    var serverResults = JSON.parse(responseText);
    var searchString = serverResults[0] || "";
    var results = serverResults[1] || [];

    var comments = [];  // "comments" column values for suggestions
    var historyResults = [];
    var historyComments = [];

    // If form history is enabled and has results, add them to the list.
    if (this._includeFormHistory && this._formHistoryResult &&
        (this._formHistoryResult.searchResult ==
         Ci.nsIAutoCompleteResult.RESULT_SUCCESS)) {
      for (var i = 0; i < this._formHistoryResult.matchCount; ++i) {
        var term = this._formHistoryResult.getValueAt(i);

        // we don't want things to appear in both history and suggestions
        var dupIndex = results.indexOf(term);
        if (dupIndex != -1)
          results.splice(dupIndex, 1);

        historyResults.push(term);
        historyComments.push("");
      }
    }

    // fill out the comment column for the suggestions
    for (var i = 0; i < results.length; ++i)
      comments.push("");

    // if we have any suggestions, put a label at the top
    if (comments.length > 0)
      comments[0] = this._suggestionLabel;

    // now put the history results above the suggestions
    var finalResults = historyResults.concat(results);
    var finalComments = historyComments.concat(comments);

    // Notify the FE of our new results
    this.onResultsReady(searchString, finalResults, finalComments,
                        this._formHistoryResult);

    // Reset our state for next time.
    this._reset();
  },

  /**
   * Notifies the front end of new results.
   * @param searchString  the user's query string
   * @param results       an array of results to the search
   * @param comments      an array of metadata corresponding to the results
   * @private
   */
  onResultsReady: function(searchString, results, comments,
                           formHistoryResult) {
    if (this._listener) {
      var result = new FormAutoCompleteResult(
          searchString,
          Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
          0,
          "",
          results,
          results,
          comments,
          formHistoryResult);

      this._listener.onSearchResult(this, result);

      // Null out listener to make sure we don't notify it twice, in case our
      // timer callback still hasn't run.
      this._listener = null;
    }
  },

  /**
   * Initiates the search result gathering process. Part of
   * nsIAutoCompleteSearch implementation.
   *
   * @param searchString    the user's query string
   * @param searchParam     unused, "an extra parameter"; even though
   *                        this parameter and the next are unused, pass
   *                        them through in case the form history
   *                        service wants them
   * @param previousResult  unused, a client-cached store of the previous
   *                        generated resultset for faster searching.
   * @param listener        object implementing nsIAutoCompleteObserver which
   *                        we notify when results are ready.
   */
  startSearch: function(searchString, searchParam, previousResult, listener) {
    // Don't reuse a previous form history result when it no longer applies.
    if (!previousResult)
      this._formHistoryResult = null;

    var formHistorySearchParam = searchParam.split("|")[0];

    // Receive the information about the privacy mode of the window to which
    // this search box belongs.  The front-end's search.xml bindings passes this
    // information in the searchParam parameter.  The alternative would have
    // been to modify nsIAutoCompleteSearch to add an argument to startSearch
    // and patch all of autocomplete to be aware of this, but the searchParam
    // argument is already an opaque argument, so this solution is hopefully
    // less hackish (although still gross.)
    var privacyMode = (searchParam.split("|")[1] == "private");

    // Start search immediately if possible, otherwise once the search
    // service is initialized
    if (Services.search.isInitialized) {
      this._triggerSearch(searchString, formHistorySearchParam, listener, privacyMode);
      return;
    }

    Services.search.init((function startSearch_cb(aResult) {
      if (!Components.isSuccessCode(aResult)) {
        Cu.reportError("Could not initialize search service, bailing out: " + aResult);
        return;
      }
      this._triggerSearch(searchString, formHistorySearchParam, listener, privacyMode);
    }).bind(this));
  },

  /**
   * Actual implementation of search.
   */
  _triggerSearch: function(searchString, searchParam, listener, privacyMode) {
    // If there's an existing request, stop it. There is no smart filtering
    // here as there is when looking through history/form data because the
    // result set returned by the server is different for every typed value -
    // "ocean breathes" does not return a subset of the results returned for
    // "ocean", for example. This does nothing if there is no current request.
    this.stopSearch();

    this._listener = listener;

    var engine = Services.search.currentEngine;

    this._checkForEngineSwitch(engine);

    if (!searchString ||
        !this._suggestEnabled ||
        !engine.supportsResponseType(SEARCH_RESPONSE_SUGGESTION_JSON) ||
        !this._okToRequest()) {
      // We have an empty search string (user pressed down arrow to see
      // history), or search suggestions are disabled, or the current engine
      // has no suggest functionality, or we're in backoff mode; so just use
      // local history.
      this._sentSuggestRequest = false;
      this._startHistorySearch(searchString, searchParam);
      return;
    }

    // Actually do the search
    this._request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                    createInstance(Ci.nsIXMLHttpRequest);
    var submission = engine.getSubmission(searchString,
                                          SEARCH_RESPONSE_SUGGESTION_JSON);
    this._suggestURI = submission.uri;
    var method = (submission.postData ? "POST" : "GET");
    this._request.open(method, this._suggestURI.spec, true);
    this._request.channel.notificationCallbacks = new AuthPromptOverride();
    if (this._request.channel instanceof Ci.nsIPrivateBrowsingChannel) {
      this._request.channel.setPrivate(privacyMode);
    }

    var self = this;
    function onReadyStateChange() {
      self.onReadyStateChange();
    }
    this._request.onreadystatechange = onReadyStateChange;
    this._request.send(submission.postData);

    if (this._includeFormHistory) {
      this._sentSuggestRequest = true;
      this._startHistorySearch(searchString, searchParam);
    }
  },

  /**
   * Ends the search result gathering process. Part of nsIAutoCompleteSearch
   * implementation.
   */
  stopSearch: function() {
    if (this._request) {
      this._request.abort();
      this._reset();
    }
  },

  /**
   * nsIObserver
   */
  observe: function SAC_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        this._suggestEnabled = Services.prefs.getBoolPref(BROWSER_SUGGEST_PREF);
        break;
      case XPCOM_SHUTDOWN_TOPIC:
        this._removeObservers();
        break;
    }
  },

  _addObservers: function SAC_addObservers() {
    Services.prefs.addObserver(BROWSER_SUGGEST_PREF, this, false);

    Services.obs.addObserver(this, XPCOM_SHUTDOWN_TOPIC, false);
  },

  _removeObservers: function SAC_removeObservers() {
    Services.prefs.removeObserver(BROWSER_SUGGEST_PREF, this);

    Services.obs.removeObserver(this, XPCOM_SHUTDOWN_TOPIC);
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteSearch,
                                         Ci.nsIAutoCompleteObserver])
};

function AuthPromptOverride() {
}
AuthPromptOverride.prototype = {
  // nsIAuthPromptProvider
  getAuthPrompt: function (reason, iid) {
    // Return a no-op nsIAuthPrompt2 implementation.
    return {
      promptAuth: function () {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      asyncPromptAuth: function () {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      }
    };
  },

  // nsIInterfaceRequestor
  getInterface: function SSLL_getInterface(iid) {
    return this.QueryInterface(iid);
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAuthPromptProvider,
                                         Ci.nsIInterfaceRequestor])
};
/**
 * SearchSuggestAutoComplete is a service implementation that handles suggest
 * results specific to web searches.
 * @constructor
 */
function SearchSuggestAutoComplete() {
  // This calls _init() in the parent class (SuggestAutoComplete) via the
  // prototype, below.
  this._init();
}
SearchSuggestAutoComplete.prototype = {
  classID: Components.ID("{aa892eb4-ffbf-477d-9f9a-06c995ae9f27}"),
  __proto__: SuggestAutoComplete.prototype,
  serviceURL: ""
};

var component = [SearchSuggestAutoComplete];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
