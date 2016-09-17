/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "AutoCompletePopup" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// nsITreeView implementation that feeds the autocomplete popup
// with the search data.
var AutoCompleteTreeView = {
  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITreeView,
                                         Ci.nsIAutoCompleteController]),

  // Private variables
  treeBox: null,
  results: [],

  // nsITreeView
  selection: null,

  get rowCount()                     { return this.results.length; },
  setTree: function(treeBox)         { this.treeBox = treeBox; },
  getCellText: function(idx, column) { return this.results[idx].value },
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
  hasNextSibling: function(idx, after) { return idx < this.results.length - 1 },
  toggleOpenState: function(idx)     { },
  getCellProperties: function(idx, column) { return this.results[idx].style || ""; },
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
    AutoCompletePopup.handleEnter(aIsPopupSelection);
  },

  stopSearch: function() {},

  // Internal JS-only API
  clearResults: function() {
    this.results = [];
  },

  setResults: function(results) {
    this.results = results;
  },
};

this.AutoCompletePopup = {
  MESSAGES: [
    "FormAutoComplete:SelectBy",
    "FormAutoComplete:GetSelectedIndex",
    "FormAutoComplete:SetSelectedIndex",
    "FormAutoComplete:MaybeOpenPopup",
    "FormAutoComplete:ClosePopup",
    "FormAutoComplete:Disconnect",
    "FormAutoComplete:RemoveEntry",
    "FormAutoComplete:Invalidate",
  ],

  init: function() {
    for (let msg of this.MESSAGES) {
      Services.mm.addMessageListener(msg, this);
    }
  },

  uninit: function() {
    for (let msg of this.MESSAGES) {
      Services.mm.removeMessageListener(msg, this);
    }
  },

  handleEvent: function(evt) {
    switch (evt.type) {
      case "popupshowing": {
        this.sendMessageToBrowser("FormAutoComplete:PopupOpened");
        break;
      }

      case "popuphidden": {
        this.sendMessageToBrowser("FormAutoComplete:PopupClosed");
        this.openedPopup = null;
        this.weakBrowser = null;
        evt.target.removeEventListener("popuphidden", this);
        evt.target.removeEventListener("popupshowing", this);
        break;
      }
    }
  },

  // Along with being called internally by the receiveMessage handler,
  // this function is also called directly by the login manager, which
  // uses a single message to fill in the autocomplete results. See
  // "RemoteLogins:autoCompleteLogins".
  showPopupWithResults: function({ browser, rect, dir, results }) {
    if (!results.length || this.openedPopup) {
      // We shouldn't ever be showing an empty popup, and if we
      // already have a popup open, the old one needs to close before
      // we consider opening a new one.
      return;
    }

    let window = browser.ownerDocument.defaultView;
    let tabbrowser = window.gBrowser;
    if (Services.focus.activeWindow != window ||
        tabbrowser.selectedBrowser != browser) {
      // We were sent a message from a window or tab that went into the
      // background, so we'll ignore it for now.
      return;
    }

    this.weakBrowser = Cu.getWeakReference(browser);
    this.openedPopup = browser.autoCompletePopup;
    this.openedPopup.hidden = false;
    // don't allow the popup to become overly narrow
    this.openedPopup.setAttribute("width", Math.max(100, rect.width));
    this.openedPopup.style.direction = dir;

    AutoCompleteTreeView.setResults(results);
    this.openedPopup.view = AutoCompleteTreeView;
    this.openedPopup.selectedIndex = -1;
    this.openedPopup.invalidate();

    if (results.length) {
      // Reset fields that were set from the last time the search popup was open
      this.openedPopup.mInput = null;
      this.openedPopup.showCommentColumn = false;
      this.openedPopup.showImageColumn = false;
      this.openedPopup.addEventListener("popuphidden", this);
      this.openedPopup.addEventListener("popupshowing", this);
      this.openedPopup.openPopupAtScreenRect("after_start", rect.left, rect.top,
                                             rect.width, rect.height, false,
                                             false);
    } else {
      this.closePopup();
    }
  },

  invalidate(results) {
    if (!this.openedPopup) {
      return;
    }

    if (!results.length) {
      this.closePopup();
    } else {
      AutoCompleteTreeView.setResults(results);
      // We need to re-set the view in order for the
      // tree to know the view has changed.
      this.openedPopup.view = AutoCompleteTreeView;
      this.openedPopup.invalidate();
    }
  },

  closePopup() {
    if (this.openedPopup) {
      // Note that hidePopup() closes the popup immediately,
      // so popuphiding or popuphidden events will be fired
      // and handled during this call.
      this.openedPopup.hidePopup();
    }
    AutoCompleteTreeView.clearResults();
  },

  removeLogin(login) {
    Services.logins.removeLogin(login);
  },

  receiveMessage: function(message) {
    if (!message.target.autoCompletePopup) {
      // Returning false to pacify ESLint, but this return value is
      // ignored by the messaging infrastructure.
      return false;
    }

    switch (message.name) {
      case "FormAutoComplete:SelectBy": {
        this.openedPopup.selectBy(message.data.reverse, message.data.page);
        break;
      }

      case "FormAutoComplete:GetSelectedIndex": {
        if (this.openedPopup) {
          return this.openedPopup.selectedIndex;
        }
        // If the popup was closed, then the selection
        // has not changed.
        return -1;
      }

      case "FormAutoComplete:SetSelectedIndex": {
        let { index } = message.data;
        if (this.openedPopup) {
          this.openedPopup.selectedIndex = index;
        }
        break;
      }

      case "FormAutoComplete:MaybeOpenPopup": {
        let { results, rect, dir } = message.data;
        this.showPopupWithResults({ browser: message.target, rect, dir,
                                    results });
        break;
      }

      case "FormAutoComplete:Invalidate": {
        let { results } = message.data;
        this.invalidate(results);
        break;
      }

      case "FormAutoComplete:ClosePopup": {
        this.closePopup();
        break;
      }

      case "FormAutoComplete:Disconnect": {
        // The controller stopped controlling the current input, so clear
        // any cached data.  This is necessary cause otherwise we'd clear data
        // only when starting a new search, but the next input could not support
        // autocomplete and it would end up inheriting the existing data.
        AutoCompleteTreeView.clearResults();
        break;
      }
    }
    // Returning false to pacify ESLint, but this return value is
    // ignored by the messaging infrastructure.
    return false;
  },

  /**
   * Despite its name, handleEnter is what is called when the
   * user clicks on one of the items in the popup.
   */
  handleEnter(aIsPopupSelection) {
    if (this.openedPopup) {
      this.sendMessageToBrowser("FormAutoComplete:HandleEnter", {
        selectedIndex: this.openedPopup.selectedIndex,
        isPopupSelection: aIsPopupSelection,
      });
    }
  },

  /**
   * If a browser exists that AutoCompletePopup knows about,
   * sends it a message. Otherwise, this is a no-op.
   *
   * @param {string} msgName
   *        The name of the message to send.
   * @param {object} data
   *        The optional data to send with the message.
   */
  sendMessageToBrowser(msgName, data) {
    let browser = this.weakBrowser ? this.weakBrowser.get()
                                   : null;
    if (browser) {
      browser.messageManager.sendAsyncMessage(msgName, data);
    }
  },

  stopSearch: function() {}
}
