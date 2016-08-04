/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "AutoCompleteE10S" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/nsFormAutoCompleteResult.jsm");

// nsITreeView implementation that feeds the autocomplete popup
// with the search data.
var AutoCompleteE10SView = {
  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITreeView,
                                         Ci.nsIAutoCompleteController]),

  // Private variables
  treeBox: null,
  treeData: [],
  properties: [],

  // nsITreeView
  selection: null,

  get rowCount()                     { return this.treeData.length; },
  setTree: function(treeBox)         { this.treeBox = treeBox; },
  getCellText: function(idx, column) { return this.treeData[idx] },
  isContainer: function(idx)         { return false; },
  getCellValue: function(idx, column) { return false },
  isContainerOpen: function(idx)     { return false; },
  isContainerEmpty: function(idx)    { return false; },
  isSeparator: function(idx)         { return false; },
  isSorted: function()               { return false; },
  isEditable: function(idx, column)  { return false; },
  canDrop: function(idx, orientation, dt) { return false; },
  getLevel: function(idx)            { return 0; },
  getParentIndex: function(idx)      { return -1; },
  hasNextSibling: function(idx, after) { return idx < this.treeData.length - 1 },
  toggleOpenState: function(idx)     { },
  getCellProperties: function(idx, column) { return this.properties[idx] || ""; },
  getRowProperties: function(idx)    { return ""; },
  getImageSrc: function(idx, column) { return null; },
  getProgressMode : function(idx, column) { },
  cycleHeader: function(column) { },
  cycleCell: function(idx, column) { },
  selectionChanged: function() { },
  performAction: function(action) { },
  performActionOnCell: function(action, index, column) { },
  getColumnProperties: function(column) { return ""; },

  // nsIAutoCompleteController
  get matchCount() {
    return this.rowCount;
  },

  handleEnter: function(aIsPopupSelection) {
    AutoCompleteE10S.handleEnter(aIsPopupSelection);
  },

  stopSearch: function() {},

  // Internal JS-only API
  clearResults: function() {
    this.treeData = [];
    this.properties = [];
  },

  addResult: function(text, properties) {
    this.treeData.push(text);
    this.properties.push(properties);
  },
};

this.AutoCompleteE10S = {
  init: function() {
    let messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                         getService(Ci.nsIMessageListenerManager);
    messageManager.addMessageListener("FormAutoComplete:SelectBy", this);
    messageManager.addMessageListener("FormAutoComplete:GetSelectedIndex", this);
    messageManager.addMessageListener("FormAutoComplete:MaybeOpenPopup", this);
    messageManager.addMessageListener("FormAutoComplete:ClosePopup", this);
    messageManager.addMessageListener("FormAutoComplete:Disconnect", this);
    messageManager.addMessageListener("FormAutoComplete:RemoveEntry", this);
  },

  _initPopup: function(browserWindow, rect, direction) {
    if (this._popupCache) {
      this._popupCache.browserWindow.removeEventListener("unload", this);
    }
    browserWindow.addEventListener("unload", this);

    this._popupCache = { browserWindow, rect, direction };

    this.browser = browserWindow.gBrowser.selectedBrowser;
    this.popup = this.browser.autoCompletePopup;
    this.popup.hidden = false;
    // don't allow the popup to become overly narrow
    this.popup.setAttribute("width", Math.max(100, rect.width));
    this.popup.style.direction = direction;

    this.x = rect.left;
    this.y = rect.top;
    this.width = rect.width;
    this.height = rect.height;
  },

  handleEvent: function(evt) {
    if (evt.type === "unload") {
      this._uninitPopup();
    }
  },

  _uninitPopup: function() {
    this._popupCache = null;
    this.browser = null;
    this.popup = null;
  },

  _showPopup: function(results) {
    AutoCompleteE10SView.clearResults();

    let resultsArray = [];
    let count = results.matchCount;
    for (let i = 0; i < count; i++) {
      // The actual result for each match in the results object is the value.
      // We return that in order to submit the correct value. However, we have
      // to make sure we display the label corresponding to it in the popup.
      resultsArray.push(results.getValueAt(i));
      AutoCompleteE10SView.addResult(results.getLabelAt(i),
                                     results.getStyleAt(i));
    }

    this.popup.view = AutoCompleteE10SView;

    this.popup.selectedIndex = -1;
    this.popup.invalidate();

    if (count > 0) {
      // Reset fields that were set from the last time the search popup was open
      this.popup.mInput = null;
      this.popup.showCommentColumn = false;
      this.popup.showImageColumn = false;
      this.popup.openPopupAtScreenRect("after_start", this.x, this.y, this.width, this.height, false, false);
    } else {
      this.popup.closePopup();
    }

    this._resultCache = results;
    return resultsArray;
  },

  // This function is used by the login manager, which uses a single message
  // to fill in the autocomplete results. See
  // "RemoteLogins:autoCompleteLogins".
  showPopupWithResults: function(browserWindow, rect, results) {
    this._initPopup(browserWindow, rect);
    this._showPopup(results);
  },

  removeLogin(login) {
    Services.logins.removeLogin(login);

    // It's possible to race and have the deleted login no longer be in our
    // resultCache's logins, so we remove it from the database above and only
    // deal with our resultCache below.
    let idx = this._resultCache.logins.findIndex(cur => {
      return login.guid === cur.QueryInterface(Ci.nsILoginMetaInfo).guid
    });
    if (idx !== -1) {
      this.removeEntry(idx, false);
    }
  },

  removeEntry(index, updateDB = true) {
    this._resultCache.removeValueAt(index, updateDB);

    let selectedIndex = this.popup.selectedIndex;
    this.showPopupWithResults(this._popupCache.browserWindow,
                              this._popupCache.rect,
                              this._resultCache);

    // If we removed the last result, bump the selected index back once.
    if (selectedIndex >= this._resultCache.matchCount)
      selectedIndex--;
    this.popup.selectedIndex = selectedIndex;
  },

  // This function is called in response to AutoComplete requests from the
  // child (received via the message manager, see
  // "FormHistory:AutoCompleteSearchAsync").
  search: function(message) {
    let browserWindow = message.target.ownerDocument.defaultView;
    let rect = message.data;
    let direction = message.data.direction;

    this._initPopup(browserWindow, rect, direction);

    // NB: We use .wrappedJSObject here in order to pass our mock DOM object
    // without being rejected by XPConnect (which attempts to enforce that DOM
    // objects are implemented in C++.
    let formAutoComplete = Cc["@mozilla.org/satchel/form-autocomplete;1"]
                             .getService(Ci.nsIFormAutoComplete).wrappedJSObject;

    let values, labels;
    if (message.data.datalistResult) {
      // Create a full FormAutoCompleteResult from the mock one that we pass
      // over IPC.
      message.data.datalistResult =
        new FormAutoCompleteResult(message.data.untrimmedSearchString,
                                   Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
                                   0, "", message.data.datalistResult.values,
                                   message.data.datalistResult.labels,
                                   [], null);
    } else {
      message.data.datalistResult = null;
    }

    let previousResult = null;
    let previousSearchString = message.data.previousSearchString;
    let searchString = message.data.untrimmedSearchString.toLowerCase();
    if (previousSearchString && previousSearchString.length > 1 &&
        searchString.includes(previousSearchString)) {
      previousResult = this._resultCache;
    }
    formAutoComplete.autoCompleteSearchAsync(message.data.inputName,
                                             message.data.untrimmedSearchString,
                                             message.data.mockField,
                                             previousResult,
                                             message.data.datalistResult,
                                             { onSearchCompletion:
                                               this.onSearchComplete.bind(this) });
  },

  // The second half of search, this fills in the popup and returns the
  // results to the child.
  onSearchComplete: function(results) {
    let resultsArray = this._showPopup(results);

    this.browser.messageManager.sendAsyncMessage(
      "FormAutoComplete:AutoCompleteSearchAsyncResult",
      {results: resultsArray}
    );
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "FormAutoComplete:SelectBy":
        this.popup.selectBy(message.data.reverse, message.data.page);
        break;

      case "FormAutoComplete:GetSelectedIndex":
        return this.popup.selectedIndex;

      case "FormAutoComplete:RemoveEntry":
        this.removeEntry(message.data.index);
        break;

      case "FormAutoComplete:MaybeOpenPopup":
        if (AutoCompleteE10SView.treeData.length > 0 &&
            !this.popup.popupOpen) {
          // This happens when one of the arrow keys is pressed after a search
          // has already been completed. nsAutoCompleteController tries to
          // re-use its own cache of the results without re-doing the search.
          // Detect that and show the popup here.
          this.showPopupWithResults(this._popupCache.browserWindow,
                                    this._popupCache.rect,
                                    this._resultCache);
        }
        break;

      case "FormAutoComplete:ClosePopup":
        this.popup.closePopup();
        break;

      case "FormAutoComplete:Disconnect":
        // The controller stopped controlling the current input, so clear
        // any cached data.  This is necessary cause otherwise we'd clear data
        // only when starting a new search, but the next input could not support
        // autocomplete and it would end up inheriting the existing data.
        AutoCompleteE10SView.clearResults();
        break;
    }
    return undefined;
  },

  handleEnter: function(aIsPopupSelection) {
    this.browser.messageManager.sendAsyncMessage(
      "FormAutoComplete:HandleEnter",
      { selectedIndex: this.popup.selectedIndex,
        isPopupSelection: aIsPopupSelection }
    );
  },

  stopSearch: function() {}
}

this.AutoCompleteE10S.init();
