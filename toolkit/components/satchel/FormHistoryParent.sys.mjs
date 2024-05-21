/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FirefoxRelay: "resource://gre/modules/FirefoxRelay.sys.mjs",
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "PREFERENCE_PREFIX_WEIGHT",
  "browser.formfill.prefixWeight"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "PREFERENCE_BOUNDARY_WEIGHT",
  "browser.formfill.boundaryWeight"
);

export class FormHistoryParent extends JSWindowActorParent {
  receiveMessage({ name, data }) {
    switch (name) {
      case "FormHistory:FormSubmitEntries":
        this.#onFormSubmitEntries(data);
        break;

      case "FormHistory:AutoCompleteSearchAsync":
        return this.#onAutoCompleteSearch(data);

      case "FormHistory:RemoveEntry":
        this.#onRemoveEntry(data);
        break;
    }

    return undefined;
  }

  #onFormSubmitEntries(entries) {
    const changes = entries.map(entry => ({
      op: "bump",
      fieldname: entry.name,
      value: entry.value,
    }));

    lazy.FormHistory.update(changes);
  }

  get formOrigin() {
    return lazy.LoginHelper.getLoginOrigin(
      this.manager.documentPrincipal?.originNoSuffix
    );
  }

  async #onAutoCompleteSearch({ searchString, params, scenarioName }) {
    searchString = searchString.trim().toLowerCase();

    let formHistoryPromise;
    if (
      FormHistoryParent.canSearchIncrementally(
        searchString,
        this.previousSearchString
      )
    ) {
      formHistoryPromise = Promise.resolve(
        FormHistoryParent.incrementalSearch(
          searchString,
          this.previousSearchString,
          this.previousSearchResult
        )
      );
    } else {
      formHistoryPromise = lazy.FormHistory.getAutoCompleteResults(
        searchString,
        params
      );
    }

    const relayPromise = lazy.FirefoxRelay.autocompleteItemsAsync({
      formOrigin: this.formOrigin,
      scenarioName,
      hasInput: !!searchString.length,
    });
    const [formHistoryEntries, externalEntries] = await Promise.all([
      formHistoryPromise,
      relayPromise,
    ]);

    this.previousSearchString = searchString;
    this.previousSearchResult = formHistoryEntries;

    return { formHistoryEntries, externalEntries };
  }

  #onRemoveEntry({ inputName, value, guid }) {
    lazy.FormHistory.update({
      op: "remove",
      fieldname: inputName,
      value,
      guid,
    });
  }

  async searchAutoCompleteEntries(searchString, data) {
    const { inputName, scenarioName } = data;
    const params = {
      fieldname: inputName,
    };
    return this.#onAutoCompleteSearch({ searchString, params, scenarioName });
  }

  static canSearchIncrementally(searchString, previousSearchString) {
    previousSearchString ||= "";
    return (
      previousSearchString.length > 1 &&
      searchString.includes(previousSearchString)
    );
  }

  static incrementalSearch(
    searchString,
    previousSearchString,
    previousSearchResult
  ) {
    const searchTokens = searchString.split(/\s+/);
    // We have a list of results for a shorter search string, so just
    // filter them further based on the new search string and add to a new array.
    let filteredEntries = [];
    for (const entry of previousSearchResult) {
      // Remove results that do not contain the token
      // XXX bug 394604 -- .toLowerCase can be wrong for some intl chars
      if (searchTokens.some(tok => !entry.textLowerCase.includes(tok))) {
        continue;
      }
      FormHistoryParent.calculateScore(entry, searchString, searchTokens);
      filteredEntries.push(entry);
    }
    filteredEntries.sort((a, b) => b.totalScore - a.totalScore);
    return filteredEntries;
  }

  /*
   * calculateScore
   *
   * entry    -- an nsIAutoCompleteResult entry
   * searchString -- current value of the input (lowercase)
   * searchTokens -- array of tokens of the search string
   *
   * Returns: an int
   */
  static calculateScore(entry, searchString, searchTokens) {
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
    boundaryCalc = boundaryCalc * lazy.PREFERENCE_BOUNDARY_WEIGHT;
    // now add more weight if we have a traditional prefix match and
    // multiply boundary bonuses by boundary weight
    if (entry.textLowerCase.startsWith(searchString)) {
      boundaryCalc += lazy.PREFERENCE_PREFIX_WEIGHT;
    }
    entry.totalScore = Math.round(entry.frecency * Math.max(1, boundaryCalc));
  }

  previewFields(_result) {
    // Not implemented
  }

  autofillFields(_result) {
    // Not implemented
  }
}
