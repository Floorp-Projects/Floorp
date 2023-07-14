/* vim: set ts=4 sts=4 sw=4 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is ugly: there are two FormAutoCompleteResult classes in the
// tree, one in a module and one in this file. Datalist results need to
// use the one defined in the module but the rest of this file assumes
// that we use the one defined here. To get around that, we explicitly
// import the module here, out of the way of the other uses of
// FormAutoCompleteResult.
import { FormAutoCompleteResult as DataListAutoCompleteResult } from "resource://gre/modules/FormAutoCompleteResult.sys.mjs";

function isAutocompleteDisabled(aField) {
  if (!aField) {
    return false;
  }

  if (aField.autocomplete !== "") {
    return aField.autocomplete === "off";
  }

  return aField.form?.autocomplete === "off";
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
 * @param {object} clientInfo
 *        Info required to build the FormHistoryClient
 * @param {Node} clientInfo.formField
 *        A DOM node that we're requesting form history for.
 * @param {string} clientInfo.inputName
 *        The name of the input to do the FormHistory look-up with.
 *        If this is searchbar-history, then formField needs to be null,
 *        otherwise constructing will throw.
 */
export class FormHistoryClient {
  constructor({ formField, inputName }) {
    if (formField) {
      if (inputName == this.SEARCHBAR_ID) {
        throw new Error(
          "FormHistoryClient constructed with both a  formField and an inputName. " +
            "This is not supported, and only empty results will be returned."
        );
      }
      const window = formField.ownerGlobal;
      this.windowGlobal = window.windowGlobalChild;
    }

    this.inputName = inputName;
    this.id = FormHistoryClient.nextRequestID++;
  }

  static nextRequestID = 1;
  SEARCHBAR_ID = "searchbar-history";
  cancelled = false;
  inputName = "";

  getActor() {
    return this.windowGlobal?.getActor("FormHistory");
  }

  /**
   * Query FormHistory for some results.
   *
   * @param {string} searchString
   *        The string to search FormHistory for. See
   *        FormHistory.getAutoCompleteResults.
   * @param {object} params
   *        An Object with search properties. See
   *        FormHistory.getAutoCompleteResults.
   * @param {Function} callback
   *        A callback function that will take a single
   *        argument (the found entries).
   */
  requestAutoCompleteResults(searchString, params, callback) {
    this.cancelled = false;

    // Use the actor if possible, otherwise for the searchbar,
    // use the more roundabout per-process message manager which has
    // no sendQuery method.
    const actor = this.getActor();
    if (actor) {
      actor
        .sendQuery("FormHistory:AutoCompleteSearchAsync", {
          searchString,
          params,
        })
        .then(
          results => this.handleAutoCompleteResults(results, callback),
          () => this.cancel()
        );
    } else {
      this.callback = callback;
      Services.cpmm.addMessageListener(
        "FormHistory:AutoCompleteSearchResults",
        this
      );
      Services.cpmm.sendAsyncMessage("FormHistory:AutoCompleteSearchAsync", {
        id: this.id,
        searchString,
        params,
      });
    }
  }

  handleAutoCompleteResults(results, callback) {
    if (this.cancelled) {
      return;
    }

    if (!callback) {
      console.error("FormHistoryClient received response with no callback");
      return;
    }

    callback(results);
    this.cancel();
  }

  /**
   * Cancel an in-flight results request. This ensures that the
   * callback that requestAutoCompleteResults was passed is never
   * called from this FormHistoryClient.
   */
  cancel() {
    if (this.callback) {
      Services.cpmm.removeMessageListener(
        "FormHistory:AutoCompleteSearchResults",
        this
      );
      this.callback = null;
    }
    this.cancelled = true;
  }

  /**
   * Remove an item from FormHistory.
   *
   * @param {string} value
   *
   *        The value to remove for this particular
   *        field.
   *
   * @param {string} guid
   *
   *        The guid for the item being removed.
   */
  remove(value, guid) {
    const actor = this.getActor() || Services.cpmm;
    actor.sendAsyncMessage("FormHistory:RemoveEntry", {
      inputName: this.inputName,
      value,
      guid,
    });
  }

  receiveMessage(msg) {
    const { id, results } = msg.data;
    if (id == this.id) {
      this.handleAutoCompleteResults(results, this.callback);
    }
  }
}

// nsIAutoCompleteResult implementation
export class FormAutoCompleteResult {
  constructor(client, entries, fieldName, searchString) {
    this.client = client;
    this.entries = entries;
    this.fieldName = fieldName;
    this.searchString = searchString;
  }

  QueryInterface = ChromeUtils.generateQI([
    "nsIAutoCompleteResult",
    "nsISupportsWeakReference",
  ]);

  // private
  client = null;
  entries = null;
  fieldName = null;

  getAt(index) {
    if (index >= 0 && index < this.entries.length) {
      return this.entries[index];
    }

    throw Components.Exception(
      "Index out of range.",
      Cr.NS_ERROR_ILLEGAL_VALUE
    );
  }

  // Allow autoCompleteSearch to get at the JS object so it can
  // modify some readonly properties for internal use.
  get wrappedJSObject() {
    return this;
  }

  // Interfaces from idl...
  searchString = "";
  errorDescription = "";

  get defaultIndex() {
    return this.entries.length ? 0 : -1;
  }

  get searchResult() {
    return this.entries.length
      ? Ci.nsIAutoCompleteResult.RESULT_SUCCESS
      : Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
  }

  get matchCount() {
    return this.entries.length;
  }

  getValueAt(index) {
    return this.getAt(index).text;
  }

  getLabelAt(index) {
    return this.getValueAt(index);
  }

  getCommentAt(_index) {
    return "";
  }

  getStyleAt(_index) {
    return "";
  }

  getImageAt(_index) {
    return "";
  }

  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  }

  isRemovableAt(index) {
    this.getAt(index);
    return true;
  }

  removeValueAt(index) {
    this.getAt(index);

    const [removedEntry] = this.entries.splice(index, 1);
    this.client.remove(removedEntry.text, removedEntry.guid);
  }
}

export class FormAutoComplete {
  constructor() {
    // Preferences. Add observer so we get notified of changes.
    this._prefBranch = Services.prefs.getBranch("browser.formfill.");
    this._prefBranch.addObserver("", this.observer, true);
    this.observer._self = this;

    this._debug = this._prefBranch.getBoolPref("debug");
    this._enabled = this._prefBranch.getBoolPref("enable");
  }

  classID = Components.ID("{c11c21b2-71c9-4f87-a0f8-5e13f50495fd}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIFormAutoComplete",
    "nsISupportsWeakReference",
  ]);

  // Only one query via FormHistoryClient is performed at a time, and the
  // most recent FormHistoryClient which will be stored in _pendingClient
  // while the query is being performed. It will be cleared when the query
  // finishes, is cancelled, or an error occurs. If a new query occurs while
  // one is already pending, the existing one is cancelled.
  #pendingClient = null;

  observer = {
    _self: null,

    QueryInterface: ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]),

    observe(_subject, topic, data) {
      const self = this._self;

      if (topic == "nsPref:changed") {
        const prefName = data;
        self.log(`got change to ${prefName} preference`);

        switch (prefName) {
          case "debug":
            self._debug = self._prefBranch.getBoolPref(prefName);
            break;
          case "enable":
            self._enabled = self._prefBranch.getBoolPref(prefName);
            break;
        }
      }
    },
  };

  // AutoCompleteE10S needs to be able to call autoCompleteSearchAsync without
  // going through IDL in order to pass a mock DOM object field.
  get wrappedJSObject() {
    return this;
  }

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
  }

  /*
   * autoCompleteSearchAsync
   *
   * aInputName -- |name| or |id| attribute value from the form input being
   *               autocompleted
   * aUntrimmedSearchString -- current value of the input
   * aField -- HTMLInputElement being autocompleted (may be null if from chrome)
   * aPreviousResult -- previous search result, if any.
   * aAddDataList -- add results from list=datalist for aField.
   * aListener -- nsIFormAutoCompleteObserver that listens for the nsIAutoCompleteResult
   *              that may be returned asynchronously.
   */
  autoCompleteSearchAsync(
    aInputName,
    aUntrimmedSearchString,
    aField,
    aPreviousResult,
    aAddDataList,
    aListener
  ) {
    // Guard against void DOM strings filtering into this code.
    if (typeof aInputName === "object") {
      aInputName = "";
    }
    if (typeof aUntrimmedSearchString === "object") {
      aUntrimmedSearchString = "";
    }

    const client = new FormHistoryClient({
      formField: aField,
      inputName: aInputName,
    });

    function reportSearchResult(result) {
      aListener?.onSearchCompletion(result);
    }

    const dataListResult = aAddDataList
      ? this.getDataListResult(aField, aUntrimmedSearchString)
      : null;

    // If we have datalist results, they become our "empty" result.
    const emptyResult =
      dataListResult ||
      new FormAutoCompleteResult(
        client,
        [],
        aInputName,
        aUntrimmedSearchString
      );

    if (!this._enabled) {
      reportSearchResult(emptyResult);
      return;
    }

    // Don't allow form inputs (aField != null) to get results from
    // search bar history.
    if (aInputName == "searchbar-history" && aField) {
      this.log(`autoCompleteSearch for input name "${aInputName}" is denied`);
      reportSearchResult(emptyResult);
      return;
    }

    if (isAutocompleteDisabled(aField)) {
      this.log("autoCompleteSearch not allowed due to autcomplete=off");
      reportSearchResult(emptyResult);
      return;
    }

    this.log(
      "AutoCompleteSearch invoked. Search is: " + aUntrimmedSearchString
    );
    const searchString = aUntrimmedSearchString.trim().toLowerCase();

    const prevSearchString = aPreviousResult?.searchString.trim();
    const reuseResult =
      prevSearchString?.length > 1 &&
      searchString.includes(prevSearchString.toLowerCase());
    if (reuseResult) {
      this.log("Using previous autocomplete result");
      const result = aPreviousResult;
      const wrappedResult = result.wrappedJSObject;
      wrappedResult.searchString = aUntrimmedSearchString;

      // Leaky abstraction alert: it would be great to be able to split
      // this code between nsInputListAutoComplete and here but because of
      // the way we abuse the formfill autocomplete API in e10s, we have
      // to deal with the <datalist> results here as well (and down below
      // in mergeResults).
      // If there were datalist results result is a FormAutoCompleteResult
      // as defined in FormAutoCompleteResult.jsm with the entire list
      // of results in wrappedResult._items and only the results from
      // form history in wrappedResult.entries.
      // First, grab the entire list of old results.
      const allResults = wrappedResult._items;
      const datalistItems = [];
      if (allResults) {
        // We have datalist results, extract them from the values array.
        // Both allResults and values arrays are in the form of:
        // |--wR.entries--|
        // <history entries><datalist entries>
        for (const oldItem of allResults.slice(wrappedResult.entries.length)) {
          if (oldItem.label.toLowerCase().includes(searchString)) {
            datalistItems.push({
              value: oldItem.value,
              label: oldItem.label,
              comment: "",
              removable: oldItem.removable,
            });
          }
        }
      }

      const searchTokens = searchString.split(/\s+/);
      // We have a list of results for a shorter search string, so just
      // filter them further based on the new search string and add to a new array.
      let filteredEntries = [];
      for (const entry of wrappedResult.entries) {
        // Remove results that do not contain the token
        // XXX bug 394604 -- .toLowerCase can be wrong for some intl chars
        if (searchTokens.some(tok => !entry.textLowerCase.includes(tok))) {
          continue;
        }
        this._calculateScore(entry, searchString, searchTokens);
        this.log(
          `Reusing autocomplete entry '${entry.text}' (${entry.frecency} / ${entry.totalScore})`
        );
        filteredEntries.push(entry);
      }
      filteredEntries.sort((a, b) => b.totalScore - a.totalScore);
      wrappedResult.entries = filteredEntries;

      // If we had datalistResults, re-merge them back into the filtered
      // entries.
      if (datalistItems.length) {
        filteredEntries = filteredEntries.map(elt => ({
          value: elt.text,
          // History entries don't have labels (their labels would be read
          // from their values).
          label: "",
          comment: "",
          removable: true,
        }));

        datalistItems[0].comment = "separator";

        wrappedResult._items = filteredEntries.concat(datalistItems);
      }

      reportSearchResult(result);
    } else {
      this.log("Creating new autocomplete search result.");

      // Start with an empty list.
      let result = dataListResult
        ? new FormAutoCompleteResult(
            client,
            [],
            aInputName,
            aUntrimmedSearchString
          )
        : emptyResult;

      this.getAutoCompleteValues(client, aInputName, searchString, aEntries => {
        if (aField?.maxLength > -1) {
          result.entries = aEntries.filter(
            el => el.text.length <= aField.maxLength
          );
        } else {
          result.entries = aEntries;
        }

        if (dataListResult?.matchCount > 0) {
          result = this.mergeResults(result, dataListResult);
        }

        reportSearchResult(result);
      });
    }
  }

  getDataListResult(aField, aUntrimmedSearchString) {
    const items = this.getDataListSuggestions(aField);

    return new DataListAutoCompleteResult(aUntrimmedSearchString, items, null);
  }

  getDataListSuggestions(aField) {
    const items = [];

    if (!aField?.list) {
      return items;
    }

    const upperFieldValue = aField.value.toUpperCase();

    for (const option of aField.list.options) {
      const label = option.label || option.text || option.value || "";

      if (!label.toUpperCase().includes(upperFieldValue)) {
        continue;
      }

      items.push({
        label,
        value: option.value,
        comment: "",
        removable: false,
      });
    }

    return items;
  }

  mergeResults(historyResult, datalistResult) {
    const items = datalistResult.wrappedJSObject._items;

    // historyResult will be null if form autocomplete is disabled. We
    // still want the list values to display.
    const entries = historyResult.wrappedJSObject.entries;
    const historyResults = entries.map(entry => ({
      value: entry.text,
      label: entry.text,
      comment: "",
      removable: true,
    }));

    const isInArray = (value, arr, key) =>
      arr.find(item => item[key].toUpperCase() === value.toUpperCase());

    // Remove items from history list that are already present in data list.
    // We do this rather than the opposite ( i.e. remove items from data list)
    // to reflect the order that is specified in the data list.
    const dedupedHistoryResults = historyResults.filter(
      historyRes => !isInArray(historyRes.value, items, "value")
    );

    // Now put the history results above the datalist suggestions.
    // Note that we don't need to worry about deduplication of elements inside
    // the datalist suggestions because the datalist is user-provided.
    const finalItems = dedupedHistoryResults.concat(items);

    historyResult.wrappedJSObject.entries =
      historyResult.wrappedJSObject.entries.filter(
        entry => !isInArray(entry.text, items, "value")
      );

    return new DataListAutoCompleteResult(
      datalistResult.searchString,
      finalItems,
      historyResult
    );
  }

  stopAutoCompleteSearch() {
    if (this.#pendingClient) {
      this.#pendingClient.cancel();
      this.#pendingClient = null;
    }
  }

  /*
   * Get the values for an autocomplete list given a search string.
   *
   *  client - a FormHistoryClient instance to perform the search with
   *  fieldname - fieldname field within form history (the form input name)
   *  searchString - string to search for
   *  callback - called when the values are available. Passed an array of objects,
   *             containing properties for each result. The callback is only called
   *             when successful.
   */
  getAutoCompleteValues(client, fieldname, searchString, callback) {
    this.stopAutoCompleteSearch();
    client.requestAutoCompleteResults(searchString, { fieldname }, entries => {
      this.#pendingClient = null;
      callback(entries);
    });
    this.#pendingClient = client;
  }

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
    for (const token of searchTokens) {
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
  }
}
