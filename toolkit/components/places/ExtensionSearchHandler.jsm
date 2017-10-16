/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "ExtensionSearchHandler" ];

// Used to keep track of all of the registered keywords, where each keyword is
// mapped to a KeywordInfo instance.
let gKeywordMap = new Map();

// Used to keep track of the active input session.
let gActiveInputSession = null;

// Used to keep track of who has control over the active suggestion callback
// so older callbacks can be ignored. The callback ID should increment whenever
// the input changes or the input session ends.
let gCurrentCallbackID = 0;

// Handles keeping track of information associated to the registered keyword.
class KeywordInfo {
  constructor(extension, description) {
    this._extension = extension;
    this._description = description;
  }

  get description() {
    return this._description;
  }

  set description(desc) {
    this._description = desc;
  }

  get extension() {
    return this._extension;
  }
}

// Responsible for handling communication between the extension and the urlbar.
class InputSession {
  constructor(keyword, extension) {
    this._keyword = keyword;
    this._extension = extension;
    this._suggestionsCallback = null;
    this._searchFinishedCallback = null;
  }

  get keyword() {
    return this._keyword;
  }

  addSuggestions(suggestions) {
    if (this._suggestionsCallback) {
      this._suggestionsCallback(suggestions);
    }
  }

  start(eventName) {
    this._extension.emit(eventName);
  }

  update(eventName, text, suggestionsCallback, searchFinishedCallback) {
    // Check to see if an existing input session needs to be ended first.
    if (this._searchFinishedCallback) {
      this._searchFinishedCallback();
    }
    this._searchFinishedCallback = searchFinishedCallback;
    this._suggestionsCallback = suggestionsCallback;
    this._extension.emit(eventName, text, ++gCurrentCallbackID);
  }

  cancel(eventName) {
    if (this._searchFinishedCallback) {
      this._searchFinishedCallback();
      this._searchFinishedCallback = null;
      this._suggestionsCallback = null;
    }
    this._extension.emit(eventName);
  }

  end(eventName, text, disposition) {
    if (this._searchFinishedCallback) {
      this._searchFinishedCallback();
      this._searchFinishedCallback = null;
      this._suggestionsCallback = null;
    }
    this._extension.emit(eventName, text, disposition);
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
      throw new Error(`The keyword provided is already registered: "${keyword}"`);
    }
    gKeywordMap.set(keyword, new KeywordInfo(extension, extension.name));
  },

  /**
   * Unregisters a keyword.
   *
   * @param {string} keyword The keyword to unregister.
   */
  unregisterKeyword(keyword) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: "${keyword}"`);
    }
    gActiveInputSession = null;
    gKeywordMap.delete(keyword);
  },

  /**
   * Checks if a keyword is registered.
   *
   * @param {string} keyword The word to check.
   * @return {boolean} true if the word is a registered keyword.
   */
  isKeywordRegistered(keyword) {
    return gKeywordMap.has(keyword);
  },

  /**
   * @return {boolean} true if there is an active input session.
   */
  hasActiveInputSession() {
    return gActiveInputSession != null;
  },

  /**
   * @param {string} keyword The keyword to look up.
   * @return {string} the description to use for the heuristic result.
   */
  getDescription(keyword) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: "${keyword}"`);
    }
    return gKeywordMap.get(keyword).description;
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
      throw new Error(`The keyword provided is not registered: "${keyword}"`);
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
   */
  addSuggestions(keyword, id, suggestions) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: "${keyword}"`);
    }

    if (!gActiveInputSession || gActiveInputSession.keyword != keyword) {
      throw new Error(`The keyword provided is not apart of an active input session: "${keyword}"`);
    }

    if (id != gCurrentCallbackID) {
      throw new Error(`The callback is no longer active for the keyword provided: "${keyword}"`);
    }

    gActiveInputSession.addSuggestions(suggestions);
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
   * @return {Promise} promise that resolves when the current search is complete.
   */
  handleSearch(keyword, text, callback) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: "${keyword}"`);
    }

    if (gActiveInputSession && gActiveInputSession.keyword != keyword) {
      throw new Error("A different input session is already ongoing");
    }

    if (!text || !text.startsWith(`${keyword} `)) {
      throw new Error(`The text provided must start with: "${keyword} "`);
    }

    if (!callback) {
      throw new Error("A callback must be provided");
    }

    // The search text in the urlbar currently starts with <keyword><space>, and
    // we only want the text that follows.
    text = text.substring(keyword.length + 1);

    // We fire MSG_INPUT_STARTED once we have <keyword><space>, and only fire
    // MSG_INPUT_CHANGED when we have text to process. This is different from Chrome's
    // behavior, which always fires MSG_INPUT_STARTED right before MSG_INPUT_CHANGED
    // first fires, but this is a bug in Chrome according to https://crbug.com/258911.
    if (!gActiveInputSession) {
      gActiveInputSession = new InputSession(keyword, gKeywordMap.get(keyword).extension);
      gActiveInputSession.start(this.MSG_INPUT_STARTED);

      // Resolve early if there is no text to process. There can be text to process when
      // the input starts if the user copy/pastes the text into the urlbar.
      if (!text.length) {
        return Promise.resolve();
      }
    }

    return new Promise(resolve => {
      gActiveInputSession.update(this.MSG_INPUT_CHANGED, text, callback, resolve);
    });
  },

  /**
   * Called when the user clicks on a suggestion that was added by
   * an extension. MSG_INPUT_ENTERED is emitted to the extension with
   * the keyword, the current search string, and info about how the
   * the search should be handled. This ends the active input session.
   *
   * @param {string} keyword The keyword associated to the suggestion.
   * @param {string} text The search text in the urlbar.
   * @param {string} where How the page should be opened. Accepted values are:
   *    "current": open the page in the same tab.
   *    "tab": open the page in a new foreground tab.
   *    "tabshifted": open the page in a new background tab.
   */
  handleInputEntered(keyword, text, where) {
    if (!gKeywordMap.has(keyword)) {
      throw new Error(`The keyword provided is not registered: "${keyword}"`);
    }

    if (gActiveInputSession && gActiveInputSession.keyword != keyword) {
      throw new Error("A different input session is already ongoing");
    }

    if (!text || !text.startsWith(`${keyword} `)) {
      throw new Error(`The text provided must start with: "${keyword} "`);
    }

    let dispositionMap = {
      current: "currentTab",
      tab: "newForegroundTab",
      tabshifted: "newBackgroundTab",
    };
    let disposition = dispositionMap[where];

    if (!disposition) {
      throw new Error(`Invalid "where" argument: ${where}`);
    }

    // The search text in the urlbar currently starts with <keyword><space>, and
    // we only want to send the text that follows.
    text = text.substring(keyword.length + 1);

    gActiveInputSession.end(this.MSG_INPUT_ENTERED, text, disposition);
    gActiveInputSession = null;
  },

  /**
   * If the user has ended the keyword input session without accepting the input,
   * MSG_INPUT_CANCELLED is emitted and the input session is ended.
   */
  handleInputCancelled() {
    if (!gActiveInputSession) {
      throw new Error("There is no active input session");
    }
    gActiveInputSession.cancel(this.MSG_INPUT_CANCELLED);
    gActiveInputSession = null;
  }
});
