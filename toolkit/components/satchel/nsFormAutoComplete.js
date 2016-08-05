/* vim: set ts=4 sts=4 sw=4 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
                                  "resource://gre/modules/Deprecated.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");

function isAutocompleteDisabled(aField) {
    if (aField.autocomplete !== "") {
        return aField.autocomplete === "off";
    }

    return aField.form && aField.form.autocomplete === "off";
}

function FormAutoComplete() {
    this.init();
}

/**
 * FormAutoComplete
 *
 * Implements the nsIFormAutoComplete interface in the main process.
 */
FormAutoComplete.prototype = {
    classID          : Components.ID("{c11c21b2-71c9-4f87-a0f8-5e13f50495fd}"),
    QueryInterface   : XPCOMUtils.generateQI([Ci.nsIFormAutoComplete, Ci.nsISupportsWeakReference]),

    _prefBranch         : null,
    _debug              : true, // mirrors browser.formfill.debug
    _enabled            : true, // mirrors browser.formfill.enable preference
    _agedWeight         : 2,
    _bucketSize         : 1,
    _maxTimeGroupings   : 25,
    _timeGroupingSize   : 7 * 24 * 60 * 60 * 1000 * 1000,
    _expireDays         : null,
    _boundaryWeight     : 25,
    _prefixWeight       : 5,

    // Only one query is performed at a time, which will be stored in _pendingQuery
    // while the query is being performed. It will be cleared when the query finishes,
    // is cancelled, or an error occurs. If a new query occurs while one is already
    // pending, the existing one is cancelled. The pending query will be an
    // mozIStoragePendingStatement object.
    _pendingQuery       : null,

    init : function() {
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

    observer : {
        _self : null,

        QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                                Ci.nsISupportsWeakReference]),

        observe : function (subject, topic, data) {
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
        }
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
    log : function (message) {
        if (!this._debug)
            return;
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
    autoCompleteSearchAsync : function (aInputName,
                                        aUntrimmedSearchString,
                                        aField,
                                        aPreviousResult,
                                        aDatalistResult,
                                        aListener) {
        function sortBytotalScore (a, b) {
            return b.totalScore - a.totalScore;
        }

        // Guard against void DOM strings filtering into this code.
        if (typeof aInputName === "object") {
            aInputName = "";
        }
        if (typeof aUntrimmedSearchString === "object") {
            aUntrimmedSearchString = "";
        }

        // If we have datalist results, they become our "empty" result.
        let emptyResult = aDatalistResult ||
                          new FormAutoCompleteResult(FormHistory, [],
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
        if (aInputName == 'searchbar-history' && aField) {
            this.log('autoCompleteSearch for input name "' + aInputName + '" is denied');
            if (aListener) {
                aListener.onSearchCompletion(emptyResult);
            }
            return;
        }

        if (aField && isAutocompleteDisabled(aField)) {
            this.log('autoCompleteSearch not allowed due to autcomplete=off');
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
            searchString.indexOf(aPreviousResult.searchString.trim().toLowerCase()) >= 0) {
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
                if (searchTokens.some(tok => entry.textLowerCase.indexOf(tok) < 0))
                    continue;
                this._calculateScore(entry, searchString, searchTokens);
                this.log("Reusing autocomplete entry '" + entry.text +
                         "' (" + entry.frecency +" / " + entry.totalScore + ")");
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
                new FormAutoCompleteResult(FormHistory, [], aInputName, aUntrimmedSearchString, null) :
                emptyResult;

            let processEntry = (aEntries) => {
                if (aField && aField.maxLength > -1) {
                    result.entries =
                        aEntries.filter(function (el) { return el.text.length <= aField.maxLength; });
                } else {
                    result.entries = aEntries;
                }

                if (aDatalistResult && aDatalistResult.matchCount > 0) {
                    result = this.mergeResults(result, aDatalistResult);
                }

                if (aListener) {
                    aListener.onSearchCompletion(result);
                }
            }

            this.getAutoCompleteValues(aInputName, searchString, processEntry);
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

        // fill out the comment column for the suggestions
        // if we have any suggestions, put a label at the top
        if (values.length) {
            comments[0] = "separator";
        }

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
        let {FormAutoCompleteResult} = Cu.import("resource://gre/modules/nsFormAutoCompleteResult.jsm", {});
        return new FormAutoCompleteResult(datalistResult.searchString,
                                          Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
                                          0, "", finalValues, finalLabels,
                                          finalComments, historyResult);
    },

    stopAutoCompleteSearch : function () {
        if (this._pendingQuery) {
            this._pendingQuery.cancel();
            this._pendingQuery = null;
        }
    },

    /*
     * Get the values for an autocomplete list given a search string.
     *
     *  fieldName - fieldname field within form history (the form input name)
     *  searchString - string to search for
     *  callback - called when the values are available. Passed an array of objects,
     *             containing properties for each result. The callback is only called
     *             when successful.
     */
    getAutoCompleteValues : function (fieldName, searchString, callback) {
        let params = {
            agedWeight:         this._agedWeight,
            bucketSize:         this._bucketSize,
            expiryDate:         1000 * (Date.now() - this._expireDays * 24 * 60 * 60 * 1000),
            fieldname:          fieldName,
            maxTimeGroupings:   this._maxTimeGroupings,
            timeGroupingSize:   this._timeGroupingSize,
            prefixWeight:       this._prefixWeight,
            boundaryWeight:     this._boundaryWeight
        }

        this.stopAutoCompleteSearch();

        let results = [];
        let processResults = {
          handleResult: aResult => {
            results.push(aResult);
          },
          handleError: aError => {
            this.log("getAutocompleteValues failed: " + aError.message);
          },
          handleCompletion: aReason => {
            // Check that the current query is still the one we created. Our
            // query might have been canceled shortly before completing, in
            // that case we don't want to call the callback anymore.
            if (query == this._pendingQuery) {
              this._pendingQuery = null;
              if (!aReason) {
                callback(results);
              }
            }
          }
        };

        let query = FormHistory.getAutoCompleteResults(searchString, params, processResults);
        this._pendingQuery = query;
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
    _calculateScore : function (entry, aSearchString, searchTokens) {
        let boundaryCalc = 0;
        // for each word, calculate word boundary weights
        for (let token of searchTokens) {
            boundaryCalc += (entry.textLowerCase.indexOf(token) == 0);
            boundaryCalc += (entry.textLowerCase.indexOf(" " + token) >= 0);
        }
        boundaryCalc = boundaryCalc * this._boundaryWeight;
        // now add more weight if we have a traditional prefix match and
        // multiply boundary bonuses by boundary weight
        boundaryCalc += this._prefixWeight *
                        (entry.textLowerCase.
                         indexOf(aSearchString) == 0);
        entry.totalScore = Math.round(entry.frecency * Math.max(1, boundaryCalc));
    }

}; // end of FormAutoComplete implementation

/**
 * FormAutoCompleteChild
 *
 * Implements the nsIFormAutoComplete interface in a child content process,
 * and forwards the auto-complete requests to the parent process which
 * also implements a nsIFormAutoComplete interface and has
 * direct access to the FormHistory database.
 */
function FormAutoCompleteChild() {
  this.init();
}

FormAutoCompleteChild.prototype = {
    classID          : Components.ID("{c11c21b2-71c9-4f87-a0f8-5e13f50495fd}"),
    QueryInterface   : XPCOMUtils.generateQI([Ci.nsIFormAutoComplete, Ci.nsISupportsWeakReference]),

    _debug: false,
    _enabled: true,
    _pendingSearch: null,

    /*
     * init
     *
     * Initializes the content-process side of the FormAutoComplete component,
     * and add a listener for the message that the parent process sends when
     * a result is produced.
     */
    init: function() {
      this._debug    = Services.prefs.getBoolPref("browser.formfill.debug");
      this._enabled  = Services.prefs.getBoolPref("browser.formfill.enable");
      this.log("init");
    },

    /*
     * log
     *
     * Internal function for logging debug messages
     */
    log : function (message) {
      if (!this._debug)
        return;
      dump("FormAutoCompleteChild: " + message + "\n");
    },

    autoCompleteSearchAsync : function (aInputName, aUntrimmedSearchString,
                                        aField, aPreviousResult, aDatalistResult,
                                        aListener) {
      this.log("autoCompleteSearchAsync");

      if (this._pendingSearch) {
        this.stopAutoCompleteSearch();
      }

      let window = aField.ownerDocument.defaultView;

      let rect = BrowserUtils.getElementBoundingScreenRect(aField);
      let direction = window.getComputedStyle(aField).direction;
      let mockField = {};
      if (isAutocompleteDisabled(aField))
          mockField.autocomplete = "off";
      if (aField.maxLength > -1)
          mockField.maxLength = aField.maxLength;

      let datalistResult = aDatalistResult ?
        { values: aDatalistResult.wrappedJSObject._values,
          labels: aDatalistResult.wrappedJSObject._labels} :
        null;

      let topLevelDocshell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIDocShell)
                                   .sameTypeRootTreeItem
                                   .QueryInterface(Ci.nsIDocShell);

      let mm = topLevelDocshell.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIContentFrameMessageManager);

      mm.sendAsyncMessage("FormHistory:AutoCompleteSearchAsync", {
        inputName: aInputName,
        untrimmedSearchString: aUntrimmedSearchString,
        mockField: mockField,
        datalistResult: datalistResult,
        previousSearchString: aPreviousResult && aPreviousResult.searchString.trim().toLowerCase(),
        left: rect.left,
        top: rect.top,
        width: rect.width,
        height: rect.height,
        direction: direction,
      });

      let search = this._pendingSearch = {};
      let searchFinished = message => {
        mm.removeMessageListener("FormAutoComplete:AutoCompleteSearchAsyncResult", searchFinished);

        // Check whether stopAutoCompleteSearch() was called, i.e. the search
        // was cancelled, while waiting for a result.
        if (search != this._pendingSearch) {
          return;
        }
        this._pendingSearch = null;

        let result = new FormAutoCompleteResult(
          null,
          Array.from(message.data.results, res => ({ text: res })),
          null,
          aUntrimmedSearchString,
          mm
        );
        if (aListener) {
          aListener.onSearchCompletion(result);
        }
      }

      mm.addMessageListener("FormAutoComplete:AutoCompleteSearchAsyncResult", searchFinished);
      this.log("autoCompleteSearchAsync message was sent");
    },

    stopAutoCompleteSearch : function () {
      this.log("stopAutoCompleteSearch");
      this._pendingSearch = null;
    },

    stopControllingInput(aField) {
      let window = aField.ownerDocument.defaultView;
      let topLevelDocshell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIDocShell)
                                   .sameTypeRootTreeItem
                                   .QueryInterface(Ci.nsIDocShell);
      let mm = topLevelDocshell.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIContentFrameMessageManager);
      mm.sendAsyncMessage("FormAutoComplete:Disconnect");
    }
}; // end of FormAutoCompleteChild implementation

// nsIAutoCompleteResult implementation
function FormAutoCompleteResult(formHistory,
                                entries,
                                fieldName,
                                searchString,
                                messageManager) {
    this.formHistory = formHistory;
    this.entries = entries;
    this.fieldName = fieldName;
    this.searchString = searchString;
    this.messageManager = messageManager;
}

FormAutoCompleteResult.prototype = {
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIAutoCompleteResult,
                                            Ci.nsISupportsWeakReference]),

    // private
    formHistory : null,
    entries : null,
    fieldName : null,

    _checkIndexBounds : function (index) {
        if (index < 0 || index >= this.entries.length)
            throw Components.Exception("Index out of range.", Cr.NS_ERROR_ILLEGAL_VALUE);
    },

    // Allow autoCompleteSearch to get at the JS object so it can
    // modify some readonly properties for internal use.
    get wrappedJSObject() {
        return this;
    },

    // Interfaces from idl...
    searchString : "",
    errorDescription : "",
    get defaultIndex() {
        if (this.entries.length == 0)
            return -1;
        return 0;
    },
    get searchResult() {
        if (this.entries.length == 0)
            return Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
        return Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
    },
    get matchCount() {
        return this.entries.length;
    },

    getValueAt : function (index) {
        this._checkIndexBounds(index);
        return this.entries[index].text;
    },

    getLabelAt: function(index) {
        return this.getValueAt(index);
    },

    getCommentAt : function (index) {
        this._checkIndexBounds(index);
        return "";
    },

    getStyleAt : function (index) {
        this._checkIndexBounds(index);
        return "";
    },

    getImageAt : function (index) {
        this._checkIndexBounds(index);
        return "";
    },

    getFinalCompleteValueAt : function (index) {
        return this.getValueAt(index);
    },

    removeValueAt : function (index, removeFromDB) {
        this._checkIndexBounds(index);

        let [removedEntry] = this.entries.splice(index, 1);

        if (removeFromDB) {
            if (this.formHistory) {
                this.formHistory.update({ op: "remove",
                                          fieldname: this.fieldName,
                                          value: removedEntry.text });
            } else {
                this.messageManager.sendAsyncMessage("FormAutoComplete:RemoveEntry",
                                                     { index });
            }
        }
    }
};


if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT &&
    Services.prefs.getBoolPref("browser.tabs.remote.desktopbehavior", false)) {
  // Register the stub FormAutoComplete module in the child which will
  // forward messages to the parent through the process message manager.
  let component = [FormAutoCompleteChild];
  this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
} else {
  let component = [FormAutoComplete];
  this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
}
