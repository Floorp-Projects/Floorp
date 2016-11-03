/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "ExtensionSearchHandler" ];

// Used to keep track of the active input session.
let gActiveKeyword = null;

// Used to keep track of who has control over the active suggestion callback
// so others can be ignored. The callback ID should increment whenever the input
// changes or the input session ends.
let gCurrentCallbackID = 0;

// The callback used to provided suggestions to the urlbar from the extension.
let gSuggestionsCallback = null;

// Maps keywords to KeywordInfo instances.
let gKeywordMap = new Map();

// Stores information associated to a registered keyword.
class KeywordInfo {
  constructor(extension) {
    this.extension = extension;

    // The extension name is used as the default description
    // for the heuristic result.
    this.description = extension.name;
  }
}

var ExtensionSearchHandler = Object.freeze({
  MSG_INPUT_STARTED: "webext-omnibox-input-started",
  MSG_INPUT_CHANGED: "webext-omnibox-input-changed",
  MSG_INPUT_ENTERED: "webext-omnibox-input-entered",
  MSG_INPUT_CANCELLED: "webext-omnibox-input-cancelled",

  /**
   * Registers a keyword.
   *
   * @param {string} keyword The keyword to register.
   * @param {Extension} extension The extension registering the keyword.
   */
  registerKeyword(keyword, extension) {
    if (gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is already registered: ${keyword}`);
    }
    gKeywordMap.set(keyword, new KeywordInfo(extension));
  },

  /**
   * Unregisters a keyword.
   *
   * @param {string} keyword The keyword to unregister.
   */
  unregisterKeyword(keyword) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: ${keyword}`);
    }
    gActiveKeyword = null;
    gKeywordMap.delete(keyword);
  },

  /**
   * Checks if a keyword is registered.
   * @param {string} keyword The word to check.
   * @return {boolean} true if the word is a registered keyword.
   */
  isKeywordRegistered(keyword) {
    return gKeywordMap.has(keyword);
  },

  /**
   * @param {string} keyword The keyword to look up.
   * @return {string} the description to use for the heuristic result.
   */
  getDescription(keyword) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: ${keyword}`);
    }
    return gKeywordMap.get(keyword).description;
  },

  /**
   * @return {boolean} true if there is an input session is currently active.
   */
  hasActiveInputSession() {
    return gActiveKeyword != null;
  },

  /**
   * Sets the default suggestion for the registered keyword. The suggestion's
   * description will be used for the comment in the heuristic result.
   *
   * @param {string} keyword The keyword.
   * @param {string} description The description to use for the heuristic result.
   */
  setDefaultSuggestion(keyword, {description}) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: ${keyword}`);
    }
    gKeywordMap.get(keyword).description = description;
  },

  /**
   * Adds suggestions for the registered keyword. This function will throw if
   * the keyword provided is not registered or active, or if the callback ID
   * provided is no longer equal to the active callback ID.
   *
   * @param {string} keyword The keyword.
   * @param {integer} id The ID of the suggestion callback.
   * @param {Array<Object>} suggestions An array of suggestions to provide to the urlbar.
   * @return {boolean} true if a valid callback ID was provided; false otherwise.
   */
  addSuggestions(keyword, id, suggestions) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: ${keyword}`);
    }

    if (keyword != gActiveKeyword) {
      throw new Error("A different input session is already ongoing");
    }

    if (id != gCurrentCallbackID) {
      return false;
    }

    gSuggestionsCallback(suggestions);
    return true;
  },

  /**
   * Called when the input in the urlbar begins with `<keyword><space>`.
   *
   * If the keyword is inactive, MSG_INPUT_STARTED is emitted and the
   * keyword is marked as active. If the keyword is followed by any text,
   * MSG_INPUT_CHANGED is fired with the current callback ID that can be
   * used to provide suggestions to the urlbar while the callback ID is active.
   * The callback is invalidated when either the input changes or the urlbar blurs.
   *
   * @param {string} keyword The keyword to handle.
   * @param {string} text The search text in the urlbar.
   * @param {Function} callback The callback used to provide search suggestions.
   */
  handleSearch(keyword, text, callback) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: ${keyword}`);
    }

    if (gActiveKeyword && keyword != gActiveKeyword) {
      throw new Error("A different input session is already ongoing");
    }

    if (!callback) {
      throw new Error("A callback must be provided");
    }

    let {extension} = gKeywordMap.get(keyword);

    // Only emit MSG_INPUT_STARTED when the session first becomes active.
    // This is different from Chrome's behavior, which fires MSG_INPUT_STARTED
    // when MSG_INPUT_CHANGED first fires, but this is a bug in Chrome according
    // to https://crbug.com/258911.
    if (!gActiveKeyword) {
      extension.emit(this.MSG_INPUT_STARTED);
    }

    // The search text in the urlbar currently starts with <keyword><space>, and
    // we only want the text that follows.
    text = text.substring(keyword.length + 1);

    // Only emit MSG_INPUT_CHANGED if the session is already an active
    // session or there is text to process.
    if (gActiveKeyword || text.length) {
      // Increment the callback ID before emitting MSG_INPUT_CHANGED so
      // the active callback ID is passed to the extension.
      gCurrentCallbackID++;
      gSuggestionsCallback = callback;
      extension.emit(this.MSG_INPUT_CHANGED, text, gCurrentCallbackID);
    }

    // Set the active keyword so we only emit MSG_INPUT_STARTED when the
    // input session starts.
    if (!gActiveKeyword) {
      gActiveKeyword = keyword;
    }
  },

  /**
   * Called when the user submits a suggestion that was added by
   * an extension. MSG_INPUT_ENTERED is emitted to the extension with
   * the keyword, the current search string, and info about how the
   * the search should be handled. In addition, the active keyword and
   * active callback are reset to null.
   *
   * @param {string} keyword The keyword associated to the suggestion.
   * @param {string} text The search text in the urlbar.
   * @param {string} where Where the page should be opened.
   */
  handleInputEntered(keyword, text, where) {
    let dispositionMap = {
      current: "currentTab",
      tab: "newForegroundTab",
      tabshifted: "newBackgroundTab",
    }
    let {extension} = gKeywordMap.get(keyword);
    let disposition = dispositionMap[where] || dispositionMap.current;

    // The search text in the urlbar currently starts with <keyword><space>, and
    // we only want the text that follows.
    text = text.substring(keyword.length + 1);

    gCurrentCallbackID++;
    gActiveKeyword = null;
    extension.emit(this.MSG_INPUT_ENTERED, text, disposition);
  },

  /**
   * If the user has ended the keyword input session without accepting the input,
   * MSG_INPUT_CANCELLED is emitted, and the active keyword and active callback
   * are reset to null.
   */
  handleInputCancelled() {
    if (!gActiveKeyword) {
      throw new Error("There is no active input session to handle");
    }
    let {extension} = gKeywordMap.get(gActiveKeyword);
    gCurrentCallbackID++;
    gActiveKeyword = null;
    extension.emit(this.MSG_INPUT_CANCELLED);
  }
});
