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

// nsITreeView implementation that feeds the autocomplete popup
// with the search data.
let AutoCompleteE10SView = {
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
  getCellValue: function(idx, column){ return false },
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
  get matchCount() this.rowCount,

  handleEnter: function(aIsPopupSelection) {
    AutoCompleteE10S.handleEnter(aIsPopupSelection);
  },

  stopSearch: function(){},

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
    messageManager.addMessageListener("FormAutoComplete:ClosePopup", this);
  },

  _initPopup: function(browserWindow, rect) {
    this.browser = browserWindow.gBrowser.selectedBrowser;
    this.popup = this.browser.autoCompletePopup;
    this.popup.hidden = false;
    this.popup.setAttribute("width", rect.width);

    this.x = rect.left;
    this.y = rect.top + rect.height;
  },

  _showPopup: function(results) {
    AutoCompleteE10SView.clearResults();

    let resultsArray = [];
    let count = results.matchCount;
    for (let i = 0; i < count; i++) {
      let result = results.getValueAt(i);
      resultsArray.push(result);
      AutoCompleteE10SView.addResult(result, results.getStyleAt(i));
    }

    this.popup.view = AutoCompleteE10SView;

    this.popup.selectedIndex = -1;
    this.popup.invalidate();

    if (count > 0) {
      // Reset fields that were set from the last time the search popup was open
      this.popup.mInput = null;
      this.popup.showCommentColumn = false;
      this.popup.showImageColumn = false;
      this.popup.openPopupAtScreen(this.x, this.y, true);
    } else {
      this.popup.closePopup();
    }

    return resultsArray;
  },

  // This function is used by the login manager, which uses a single message
  // to fill in the autocomplete results. See
  // "RemoteLogins:autoCompleteLogins".
  showPopupWithResults: function(browserWindow, rect, results) {
    this._initPopup(browserWindow, rect);
    this._showPopup(results);
  },

  // This function is called in response to AutoComplete requests from the
  // child (received via the message manager, see
  // "FormHistory:AutoCompleteSearchAsync").
  search: function(message) {
    let browserWindow = message.target.ownerDocument.defaultView;
    let rect = message.data;

    this._initPopup(browserWindow, rect);

    let formAutoComplete = Cc["@mozilla.org/satchel/form-autocomplete;1"]
                             .getService(Ci.nsIFormAutoComplete);

    formAutoComplete.autoCompleteSearchAsync(message.data.inputName,
                                             message.data.untrimmedSearchString,
                                             null,
                                             null,
                                             this.onSearchComplete.bind(this));
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

      case "FormAutoComplete:ClosePopup":
        this.popup.closePopup();
        break;
    }
  },

  handleEnter: function(aIsPopupSelection) {
    this.browser.messageManager.sendAsyncMessage(
      "FormAutoComplete:HandleEnter",
      { selectedIndex: this.popup.selectedIndex,
        IsPopupSelection: aIsPopupSelection }
    );
  },

  stopSearch: function() {}
}

this.AutoCompleteE10S.init();
