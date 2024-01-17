/* vim: set ts=4 sts=4 sw=4 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GenericAutocompleteItem } from "resource://gre/modules/FillHelpers.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormScenarios: "resource://gre/modules/FormScenarios.sys.mjs",
});

const formFillController = Cc[
  "@mozilla.org/satchel/form-fill-controller;1"
].getService(Ci.nsIFormFillController);

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
   * @param {string} scenarioName
   *        Optional autocompletion scenario name.
   * @param {Function} callback
   *        A callback function that will take a single
   *        argument (the found entries).
   */
  requestAutoCompleteResults(searchString, params, scenarioName, callback) {
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
          scenarioName,
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
        scenarioName,
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

/**
 * This autocomplete result combines 3 arrays of entries, fixedEntries and
 * externalEntries.
 * Entries are Form History entries, they can be removed.
 * Fixed entries are "appended" to entries, they are used for datalist items,
 * search suggestions and extra items from integrations.
 * External entries are meant for integrations, like Firefox Relay.
 * Internally entries and fixed entries are kept separated so we can
 * reuse and filter them.
 *
 * @implements {nsIAutoCompleteResult}
 */
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
  #fixedEntries = [];
  externalEntries = [];

  set fixedEntries(value) {
    this.#fixedEntries = value;
    this.removeDuplicateHistoryEntries();
  }

  canSearchIncrementally(searchString) {
    const prevSearchString = this.searchString.trim();
    return (
      prevSearchString.length > 1 &&
      searchString.includes(prevSearchString.toLowerCase())
    );
  }

  incrementalSearch(searchString) {
    this.searchString = searchString;
    searchString = searchString.trim().toLowerCase();
    this.#fixedEntries = this.#fixedEntries.filter(item =>
      item.label.toLowerCase().includes(searchString)
    );

    const searchTokens = searchString.split(/\s+/);
    // We have a list of results for a shorter search string, so just
    // filter them further based on the new search string and add to a new array.
    let filteredEntries = [];
    for (const entry of this.entries) {
      // Remove results that do not contain the token
      // XXX bug 394604 -- .toLowerCase can be wrong for some intl chars
      if (searchTokens.some(tok => !entry.textLowerCase.includes(tok))) {
        continue;
      }
      this.#calculateScore(entry, searchString, searchTokens);
      filteredEntries.push(entry);
    }
    filteredEntries.sort((a, b) => b.totalScore - a.totalScore);
    this.entries = filteredEntries;
    this.removeDuplicateHistoryEntries();
  }

  /*
   * #calculateScore
   *
   * entry    -- an nsIAutoCompleteResult entry
   * aSearchString -- current value of the input (lowercase)
   * searchTokens -- array of tokens of the search string
   *
   * Returns: an int
   */
  #calculateScore(entry, aSearchString, searchTokens) {
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

  /**
   * Remove items from history list that are already present in fixed list.
   * We do this rather than the opposite ( i.e. remove items from fixed list)
   * to reflect the order that is specified in the fixed list.
   */
  removeDuplicateHistoryEntries() {
    this.entries = this.entries.filter(entry =>
      this.#fixedEntries.every(
        fixed => entry.text != (fixed.label || fixed.value)
      )
    );
  }

  getAt(index) {
    for (const group of [
      this.entries,
      this.#fixedEntries,
      this.externalEntries,
    ]) {
      if (index < group.length) {
        return group[index];
      }
      index -= group.length;
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
    return this.matchCount ? 0 : -1;
  }

  get searchResult() {
    return this.matchCount
      ? Ci.nsIAutoCompleteResult.RESULT_SUCCESS
      : Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
  }

  get matchCount() {
    return (
      this.entries.length +
      this.#fixedEntries.length +
      this.externalEntries.length
    );
  }

  getValueAt(index) {
    const item = this.getAt(index);
    return item.text || item.value;
  }

  getLabelAt(index) {
    const item = this.getAt(index);
    return item.text || item.label || item.value;
  }

  getCommentAt(index) {
    return this.getAt(index).comment ?? "";
  }

  getStyleAt(index) {
    const itemStyle = this.getAt(index).style;
    if (itemStyle) {
      return itemStyle;
    }

    if (index >= 0) {
      if (index < this.entries.length) {
        return "fromhistory";
      }

      if (index > 0 && index == this.entries.length) {
        return "datalist-first";
      }
    }
    return "";
  }

  getImageAt(_index) {
    return "";
  }

  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  }

  isRemovableAt(index) {
    return this.#isFormHistoryEntry(index) || this.getAt(index).removable;
  }

  removeValueAt(index) {
    if (this.#isFormHistoryEntry(index)) {
      const [removedEntry] = this.entries.splice(index, 1);
      this.client.remove(removedEntry.text, removedEntry.guid);
    }
  }

  #isFormHistoryEntry(index) {
    return index >= 0 && index < this.entries.length;
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
    Services.obs.addObserver(this, "autocomplete-will-enter-text");
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

  fillRequestId = 0;

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

    // If we have datalist results, they become our "empty" result.
    const result = new FormAutoCompleteResult(
      client,
      [],
      aInputName,
      aUntrimmedSearchString
    );

    if (aAddDataList) {
      result.fixedEntries = this.getDataListSuggestions(aField);
    }

    if (!this._enabled) {
      reportSearchResult(result);
      return;
    }

    // Don't allow form inputs (aField != null) to get results from
    // search bar history.
    if (aInputName == "searchbar-history" && aField) {
      this.log(`autoCompleteSearch for input name "${aInputName}" is denied`);
      reportSearchResult(result);
      return;
    }

    if (isAutocompleteDisabled(aField)) {
      this.log("autoCompleteSearch not allowed due to autcomplete=off");
      reportSearchResult(result);
      return;
    }

    const searchString = aUntrimmedSearchString.trim().toLowerCase();
    const prevResult = aPreviousResult?.wrappedJSObject;
    if (prevResult?.canSearchIncrementally(searchString)) {
      this.log("Using previous autocomplete result");
      prevResult.incrementalSearch(aUntrimmedSearchString);
      reportSearchResult(prevResult);
    } else {
      this.log("Creating new autocomplete search result.");
      this.getAutoCompleteValues(
        client,
        aInputName,
        searchString,
        lazy.FormScenarios.detect({ input: aField }).signUpForm
          ? "SignUpFormScenario"
          : "",
        ({ formHistoryEntries, externalEntries }) => {
          formHistoryEntries ??= [];
          externalEntries ??= [];

          if (aField?.maxLength > -1) {
            result.entries = formHistoryEntries.filter(
              el => el.text.length <= aField.maxLength
            );
          } else {
            result.entries = formHistoryEntries;
          }

          result.externalEntries.push(
            ...externalEntries.map(
              entry =>
                new GenericAutocompleteItem(
                  entry.image,
                  entry.title,
                  entry.subtitle,
                  entry.fillMessageName,
                  entry.fillMessageData
                )
            )
          );

          result.removeDuplicateHistoryEntries();
          reportSearchResult(result);
        }
      );
    }
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
      });
    }

    return items;
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
   *  scenarioName - Optional autocompletion scenario name.
   *  callback - called when the values are available. Passed an array of objects,
   *             containing properties for each result. The callback is only called
   *             when successful.
   */
  getAutoCompleteValues(
    client,
    fieldname,
    searchString,
    scenarioName,
    callback
  ) {
    this.stopAutoCompleteSearch();
    client.requestAutoCompleteResults(
      searchString,
      { fieldname },
      scenarioName,
      entries => {
        this.#pendingClient = null;
        callback(entries);
      }
    );
    this.#pendingClient = client;
  }

  async observe(subject, topic, data) {
    switch (topic) {
      case "autocomplete-will-enter-text": {
        await this.sendFillRequestToFormHistoryParent(subject, data);
        break;
      }
    }
  }

  async sendFillRequestToFormHistoryParent(input, comment) {
    if (!comment) {
      return;
    }

    if (!input || input != formFillController.controller?.input) {
      return;
    }

    const { fillMessageName, fillMessageData } = JSON.parse(comment ?? "{}");
    if (!fillMessageName) {
      return;
    }

    this.fillRequestId++;
    const fillRequestId = this.fillRequestId;
    const actor =
      input.focusedInput.ownerGlobal.windowGlobalChild.getActor("FormHistory");
    const value = await actor.sendQuery(fillMessageName, fillMessageData ?? {});

    // skip fill if another fill operation started during await
    if (fillRequestId != this.fillRequestId) {
      return;
    }

    if (typeof value !== "string") {
      return;
    }

    // If FormHistoryParent returned a string to fill, we must do it here because
    // nsAutoCompleteController.cpp already finished it's work before we finished await.
    input.textValue = value;
    input.selectTextRange(value.length, value.length);
  }
}
