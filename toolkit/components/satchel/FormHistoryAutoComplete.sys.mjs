/* vim: set ts=4 sts=4 sw=4 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
export class FormHistoryAutoCompleteResult {
  constructor(input, entries, inputName, searchString) {
    this.input = input;
    this.entries = entries;
    this.inputName = inputName;
    this.searchString = searchString;
  }

  QueryInterface = ChromeUtils.generateQI([
    "nsIAutoCompleteResult",
    "nsISupportsWeakReference",
  ]);

  // private
  input = null;
  entries = null;
  inputName = null;
  #fixedEntries = [];
  externalEntries = [];

  set fixedEntries(value) {
    this.#fixedEntries = value;
    this.removeDuplicateHistoryEntries();
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
      const actor =
        this.input.ownerGlobal.windowGlobalChild.getActor("FormHistory");
      actor.sendAsyncMessage("FormHistory:RemoveEntry", {
        inputName: this.inputName,
        value: removedEntry.text,
        guid: removedEntry.guid,
      });
    }
  }

  #isFormHistoryEntry(index) {
    return index >= 0 && index < this.entries.length;
  }
}
