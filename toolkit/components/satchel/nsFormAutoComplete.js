/* vim: set ts=4 sts=4 sw=4 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils", "resource://gre/modules/BrowserUtils.jsm");

function isAutocompleteDisabled(aField) {
  if (aField.autocomplete !== "") {
    return aField.autocomplete === "off";
  }

  return aField.form && aField.form.autocomplete === "off";
}

/**
 * An abstraction to talk with the FormHistory database over
 * the message layer. FormHistoryClient will take care of
 * figuring out the most appropriate message manager to use,
 * and what things to send.
 *
 * It is assumed that nsFormAutoComplete will only ever use
 * one instance at a time, and will not attempt to perform more
 * than one search request with the same instance at a time.
 * However, nsFormAutoComplete might call remove() any number of
 * times with the same instance of the client.
 *
 * @param {Object} clientInfo
 *        Info required to build the FormHistoryClient
 * @param {Node} clientInfo.formField
 *        A DOM node that we're requesting form history for.
 * @param {string} clientInfo.inputName
 *        The name of the input to do the FormHistory look-up with.
 *        If this is searchbar-history, then formField needs to be null,
 *        otherwise constructing will throw.
 */
function FormHistoryClient({ formField, inputName }) {
  if (formField && inputName != this.SEARCHBAR_ID) {
    let window = formField.ownerGlobal;
    let topDocShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDocShell)
                            .sameTypeRootTreeItem
                            .QueryInterface(Ci.nsIDocShell);
    this.mm = topDocShell.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIContentFrameMessageManager);
  } else {
    if (inputName == this.SEARCHBAR_ID && formField) {
      throw new Error("FormHistoryClient constructed with both a " +
                      "formField and an inputName. This is not " +
                      "supported, and only empty results will be " +
                      "returned.");
    }
    this.mm = Services.cpmm;
  }

  this.inputName = inputName;
  this.id = FormHistoryClient.nextRequestID++;
}

FormHistoryClient.prototype = {
  SEARCHBAR_ID: "searchbar-history",

  // It is assumed that nsFormAutoComplete only uses / cares about
  // one FormHistoryClient at a time, and won't attempt to have
  // multiple in-flight searches occurring with the same FormHistoryClient.
  // We use an ID number per instantiated FormHistoryClient to make
  // sure we only respond to messages that were meant for us.
  id: 0,
  callback: null,
  inputName: "",
  mm: null,

  /**
   * Query FormHistory for some results.
   *
   * @param {string} searchString
   *        The string to search FormHistory for. See
   *        FormHistory.getAutoCompleteResults.
   * @param {Object} params
   *        An Object with search properties. See
   *        FormHistory.getAutoCompleteResults.
   * @param {function} callback
   *        A callback function that will take a single
   *        argument (the found entries).
   */
  requestAutoCompleteResults(searchString, params, callback) {
    this.mm.sendAsyncMessage("FormHistory:AutoCompleteSearchAsync", {
      id: this.id,
      searchString,
      params,
    });

    this.mm.addMessageListener("FormHistory:AutoCompleteSearchResults", this);
    this.callback = callback;
  },

  /**
   * Cancel an in-flight results request. This ensures that the
   * callback that requestAutoCompleteResults was passed is never
   * called from this FormHistoryClient.
   */
  cancel() {
    this.clearListeners();
  },

  /**
   * Remove an item from FormHistory.
   *
   * @param {string} value
   *
   *        The value to remove for this particular
   *        field.
   */
  remove(value) {
    this.mm.sendAsyncMessage("FormHistory:RemoveEntry", {
      inputName: this.inputName,
      value,
    });
  },

  // Private methods

  receiveMessage(msg) {
    let { id, results } = msg.data;
    if (id != this.id) {
      return;
    }
    if (!this.callback) {
      Cu.reportError("FormHistoryClient received message with no callback");
      return;
    }
    this.callback(results);
    this.clearListeners();
  },

  clearListeners() {
    this.mm.removeMessageListener("FormHistory:AutoCompleteSearchResults", this);
    this.callback = null;
  },
};

FormHistoryClient.nextRequestID = 1;


function FormAutoComplete() {
  this.init();
}

/**
 * Implements the nsIFormAutoComplete interface in the main process.
 */
FormAutoComplete.prototype = {
  classID: Components.ID("{c11c21b2-71c9-4f87-a0f8-5e13f50495fd}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormAutoComplete, Ci.nsISupportsWeakReference]),

  _prefBranch: null,
  _debug: true, // mirrors browser.formfill.debug
  _enabled: true, // mirrors browser.formfill.enable preference
  _agedWeight: 2,
  _bucketSize: 1,
  _maxTimeGroupings: 25,
  _timeGroupingSize: 7 * 24 * 60 * 60 * 1000 * 1000,
  _expireDays: null,
  _boundaryWeight: 25,
  _prefixWeight: 5,

  // Only one query via FormHistoryClient is performed at a time, and the
  // most recent FormHistoryClient which will be stored in _pendingClient
  // while the query is being performed. It will be cleared when the query
  // finishes, is cancelled, or an error occurs. If a new query occurs while
  // one is already pending, the existing one is cancelled.
  _pendingClient: null,

  init() {
    // Preferences. Add observer so we get notified of changes.
    this._prefBranch = Services.prefs.getBranch("browser.formfill.");
    this._prefBranch.addObserver("", this.observer, true);
    this.observer._self = this;

    this._debug            = this._prefBranch.getBoolPref("debug");
    this._enabled          = this._prefBranch.getBoolPref("enable");
    this._agedWeight       = this._prefBranch.getIntPref("agedWeight");
    this._bucketSize       = this._prefBranch.getIntPref("bucketSize");
    this._maxTimeGroupings = this._prefBranch.getIntPref("maxTimeGroupings");
    this._timeGroupingSize = this._prefBranch.getIntPref("timeGroupingSize") * 1000 * 1000;
    this._expireDays       = this._prefBranch.getIntPref("expire_days");
  },

  observer: {
    _self: null,

    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIObserver,
      Ci.nsISupportsWeakReference,
    ]),

    observe(subject, topic, data) {
      let self = this._self;

      if (topic == "nsPref:changed") {
        let prefName = data;
        self.log("got change to " + prefName + " preference");

        switch (prefName) {
          case "agedWeight":
            self._agedWeight = self._prefBranch.getIntPref(prefName);
            break;
          case "debug":
            self._debug = self._prefBranch.getBoolPref(prefName);
            break;
          case "enable":
            self._enabled = self._prefBranch.getBoolPref(prefName);
            break;
          case "maxTimeGroupings":
            self._maxTimeGroupings = self._prefBranch.getIntPref(prefName);
            break;
          case "timeGroupingSize":
            self._timeGroupingSize = self._prefBranch.getIntPref(prefName) * 1000 * 1000;
            break;
          case "bucketSize":
            self._bucketSize = self._prefBranch.getIntPref(prefName);
            break;
          case "boundaryWeight":
            self._boundaryWeight = self._prefBranch.getIntPref(prefName);
            break;
          case "prefixWeight":
            self._prefixWeight = self._prefBranch.getIntPref(prefName);
            break;
          default:
            self.log("Oops! Pref not handled, change ignored.");
        }
      }
    },
  },

  // AutoCompleteE10S needs to be able to call autoCompleteSearchAsync without
  // going through IDL in order to pass a mock DOM object field.
  get wrappedJSObject() {
    return this;
  },

  /*
   * log
   *
   * Internal function for logging debug messages to the Error Console
   * window
   */
  log(message) {
    if (!this._debug) {
      return;
    }
    dump("FormAutoComplete: " + message + "\n");
    Services.console.logStringMessage("FormAutoComplete: " + message);
  },

  /*
   * autoCompleteSearchAsync
   *
   * aInputName    -- |name| attribute from the form input being autocompleted.
   * aUntrimmedSearchString -- current value of the input
   * aField -- nsIDOMHTMLInputElement being autocompleted (may be null if from chrome)
   * aPreviousResult -- previous search result, if any.
   * aDatalistResult -- results from list=datalist for aField.
   * aListener -- nsIFormAutoCompleteObserver that listens for the nsIAutoCompleteResult
   *              that may be returned asynchronously.
   */
  autoCompleteSearchAsync(aInputName,
                          aUntrimmedSearchString,
                          aField,
                          aPreviousResult,
                          aDatalistResult,
                          aListener) {
    function sortBytotalScore(a, b) {
      return b.totalScore - a.totalScore;
    }

    // Guard against void DOM strings filtering into this code.
    if (typeof aInputName === "object") {
      aInputName = "";
    }
    if (typeof aUntrimmedSearchString === "object") {
      aUntrimmedSearchString = "";
    }

    let client = new FormHistoryClient({ formField: aField, inputName: aInputName });

    // If we have datalist results, they become our "empty" result.
    let emptyResult = aDatalistResult || new FormAutoCompleteResult(client,
                                                                    [],
                                                                    aInputName,
                                                                    aUntrimmedSearchString,
                                                                    null);
    if (!this._enabled) {
      if (aListener) {
        aListener.onSearchCompletion(emptyResult);
      }
      return;
    }

    // don't allow form inputs (aField != null) to get results from search bar history
    if (aInputName == "searchbar-history" && aField) {
      this.log('autoCompleteSearch for input name "' + aInputName + '" is denied');
      if (aListener) {
        aListener.onSearchCompletion(emptyResult);
      }
      return;
    }

    if (aField && isAutocompleteDisabled(aField)) {
      this.log("autoCompleteSearch not allowed due to autcomplete=off");
      if (aListener) {
        aListener.onSearchCompletion(emptyResult);
      }
      return;
    }

    this.log("AutoCompleteSearch invoked. Search is: " + aUntrimmedSearchString);
    let searchString = aUntrimmedSearchString.trim().toLowerCase();

    // reuse previous results if:
    // a) length greater than one character (others searches are special cases) AND
    // b) the the new results will be a subset of the previous results
    if (aPreviousResult && aPreviousResult.searchString.trim().length > 1 &&
      searchString.includes(aPreviousResult.searchString.trim().toLowerCase())) {
      this.log("Using previous autocomplete result");
      let result = aPreviousResult;
      let wrappedResult = result.wrappedJSObject;
      wrappedResult.searchString = aUntrimmedSearchString;

      // Leaky abstraction alert: it would be great to be able to split
      // this code between nsInputListAutoComplete and here but because of
      // the way we abuse the formfill autocomplete API in e10s, we have
      // to deal with the <datalist> results here as well (and down below
      // in mergeResults).
      // If there were datalist results result is a FormAutoCompleteResult
      // as defined in nsFormAutoCompleteResult.jsm with the entire list
      // of results in wrappedResult._values and only the results from
      // form history in wrappedResult.entries.
      // First, grab the entire list of old results.
      let allResults = wrappedResult._labels;
      let datalistResults, datalistLabels;
      if (allResults) {
        // We have datalist results, extract them from the values array.
        // Both allResults and values arrays are in the form of:
        // |--wR.entries--|
        // <history entries><datalist entries>
        let oldLabels = allResults.slice(wrappedResult.entries.length);
        let oldValues = wrappedResult._values.slice(wrappedResult.entries.length);

        datalistLabels = [];
        datalistResults = [];
        for (let i = 0; i < oldLabels.length; ++i) {
          if (oldLabels[i].toLowerCase().includes(searchString)) {
            datalistLabels.push(oldLabels[i]);
            datalistResults.push(oldValues[i]);
          }
        }
      }

      let searchTokens = searchString.split(/\s+/);
      // We have a list of results for a shorter search string, so just
      // filter them further based on the new search string and add to a new array.
      let entries = wrappedResult.entries;
      let filteredEntries = [];
      for (let i = 0; i < entries.length; i++) {
        let entry = entries[i];
        // Remove results that do not contain the token
        // XXX bug 394604 -- .toLowerCase can be wrong for some intl chars
        if (searchTokens.some(tok => !entry.textLowerCase.includes(tok))) {
          continue;
        }
        this._calculateScore(entry, searchString, searchTokens);
        this.log("Reusing autocomplete entry '" + entry.text +
                 "' (" + entry.frecency + " / " + entry.totalScore + ")");
        filteredEntries.push(entry);
      }
      filteredEntries.sort(sortBytotalScore);
      wrappedResult.entries = filteredEntries;

      // If we had datalistResults, re-merge them back into the filtered
      // entries.
      if (datalistResults) {
        filteredEntries = filteredEntries.map(elt => elt.text);

        let comments = new Array(filteredEntries.length + datalistResults.length).fill("");
        comments[filteredEntries.length] = "separator";

        // History entries don't have labels (their labels would be read
        // from their values). Pad out the labels array so the datalist
        // results (which do have separate values and labels) line up.
        datalistLabels = new Array(filteredEntries.length).fill("").concat(datalistLabels);
        wrappedResult._values = filteredEntries.concat(datalistResults);
        wrappedResult._labels = datalistLabels;
        wrappedResult._comments = comments;
      }

      if (aListener) {
        aListener.onSearchCompletion(result);
      }
    } else {
      this.log("Creating new autocomplete search result.");

      // Start with an empty list.
      let result = aDatalistResult ?
        new FormAutoCompleteResult(client, [], aInputName, aUntrimmedSearchString, null) :
        emptyResult;

      let processEntry = (aEntries) => {
        if (aField && aField.maxLength > -1) {
          result.entries =
            aEntries.filter(el => el.text.length <= aField.maxLength);
        } else {
          result.entries = aEntries;
        }

        if (aDatalistResult && aDatalistResult.matchCount > 0) {
          result = this.mergeResults(result, aDatalistResult);
        }

        if (aListener) {
          aListener.onSearchCompletion(result);
        }
      };

      this.getAutoCompleteValues(client, aInputName, searchString, processEntry);
    }
  },

  mergeResults(historyResult, datalistResult) {
    let values = datalistResult.wrappedJSObject._values;
    let labels = datalistResult.wrappedJSObject._labels;
    let comments = new Array(values.length).fill("");

    // historyResult will be null if form autocomplete is disabled. We
    // still want the list values to display.
    let entries = historyResult.wrappedJSObject.entries;
    let historyResults = entries.map(entry => entry.text);
    let historyComments = new Array(entries.length).fill("");

    // now put the history results above the datalist suggestions
    let finalValues = historyResults.concat(values);
    let finalLabels = historyResults.concat(labels);
    let finalComments = historyComments.concat(comments);

    // This is ugly: there are two FormAutoCompleteResult classes in the
    // tree, one in a module and one in this file. Datalist results need to
    // use the one defined in the module but the rest of this file assumes
    // that we use the one defined here. To get around that, we explicitly
    // import the module here, out of the way of the other uses of
    // FormAutoCompleteResult.
    let {FormAutoCompleteResult} = Cu.import("resource://gre/modules/nsFormAutoCompleteResult.jsm",
                                             {});
    return new FormAutoCompleteResult(datalistResult.searchString,
                                      Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
                                      0,
                                      "",
                                      finalValues,
                                      finalLabels,
                                      finalComments,
                                      historyResult);
  },

  stopAutoCompleteSearch() {
    if (this._pendingClient) {
      this._pendingClient.cancel();
      this._pendingClient = null;
    }
  },

  /*
   * Get the values for an autocomplete list given a search string.
   *
   *  client - a FormHistoryClient instance to perform the search with
   *  fieldName - fieldname field within form history (the form input name)
   *  searchString - string to search for
   *  callback - called when the values are available. Passed an array of objects,
   *             containing properties for each result. The callback is only called
   *             when successful.
   */
  getAutoCompleteValues(client, fieldName, searchString, callback) {
    let params = {
      agedWeight:         this._agedWeight,
      bucketSize:         this._bucketSize,
      expiryDate:         1000 * (Date.now() - this._expireDays * 24 * 60 * 60 * 1000),
      fieldname:          fieldName,
      maxTimeGroupings:   this._maxTimeGroupings,
      timeGroupingSize:   this._timeGroupingSize,
      prefixWeight:       this._prefixWeight,
      boundaryWeight:     this._boundaryWeight,
    };

    this.stopAutoCompleteSearch();
    client.requestAutoCompleteResults(searchString, params, (entries) => {
      this._pendingClient = null;
      callback(entries);
    });
    this._pendingClient = client;
  },

  /*
   * _calculateScore
   *
   * entry    -- an nsIAutoCompleteResult entry
   * aSearchString -- current value of the input (lowercase)
   * searchTokens -- array of tokens of the search string
   *
   * Returns: an int
   */
  _calculateScore(entry, aSearchString, searchTokens) {
    let boundaryCalc = 0;
    // for each word, calculate word boundary weights
    for (let token of searchTokens) {
      if (entry.textLowerCase.startsWith(token)) {
        boundaryCalc++;
      }
      if (entry.textLowerCase.includes(" " + token)) {
        boundaryCalc++;
      }
    }
    boundaryCalc = boundaryCalc * this._boundaryWeight;
    // now add more weight if we have a traditional prefix match and
    // multiply boundary bonuses by boundary weight
    if (entry.textLowerCase.startsWith(aSearchString)) {
      boundaryCalc += this._prefixWeight;
    }
    entry.totalScore = Math.round(entry.frecency * Math.max(1, boundaryCalc));
  },

}; // end of FormAutoComplete implementation

// nsIAutoCompleteResult implementation
function FormAutoCompleteResult(client,
                                entries,
                                fieldName,
                                searchString,
                                messageManager) {
  this.client = client;
  this.entries = entries;
  this.fieldName = fieldName;
  this.searchString = searchString;
  this.messageManager = messageManager;
}

FormAutoCompleteResult.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteResult, Ci.nsISupportsWeakReference]),

  // private
  client: null,
  entries: null,
  fieldName: null,

  _checkIndexBounds(index) {
    if (index < 0 || index >= this.entries.length) {
      throw Components.Exception("Index out of range.", Cr.NS_ERROR_ILLEGAL_VALUE);
    }
  },

  // Allow autoCompleteSearch to get at the JS object so it can
  // modify some readonly properties for internal use.
  get wrappedJSObject() {
    return this;
  },

  // Interfaces from idl...
  searchString: "",
  errorDescription: "",
  get defaultIndex() {
    if (this.entries.length == 0) {
      return -1;
    }
    return 0;
  },
  get searchResult() {
    if (this.entries.length == 0) {
      return Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
    }
    return Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
  },
  get matchCount() {
    return this.entries.length;
  },

  getValueAt(index) {
    this._checkIndexBounds(index);
    return this.entries[index].text;
  },

  getLabelAt(index) {
    return this.getValueAt(index);
  },

  getCommentAt(index) {
    this._checkIndexBounds(index);
    return "";
  },

  getStyleAt(index) {
    this._checkIndexBounds(index);
    return "";
  },

  getImageAt(index) {
    this._checkIndexBounds(index);
    return "";
  },

  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  },

  removeValueAt(index, removeFromDB) {
    this._checkIndexBounds(index);

    let [removedEntry] = this.entries.splice(index, 1);

    if (removeFromDB) {
      this.client.remove(removedEntry.text);
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FormAutoComplete]);
