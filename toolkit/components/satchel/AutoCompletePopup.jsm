/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AutoCompletePopup"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

// AutoCompleteResultView is an abstraction around a list of results
// we got back up from browser-content.js. It implements enough of
// nsIAutoCompleteController and nsIAutoCompleteInput to make the
// richlistbox popup work.
var AutoCompleteResultView = {
  // nsISupports
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAutoCompleteController,
    Ci.nsIAutoCompleteInput,
  ]),

  // Private variables
  results: [],

  // nsIAutoCompleteController
  get matchCount() {
    return this.results.length;
  },

  getValueAt(index) {
    return this.results[index].value;
  },

  getLabelAt(index) {
    // Unused by richlist autocomplete - see getCommentAt.
    return "";
  },

  getCommentAt(index) {
    // The richlist autocomplete popup uses comment for its main
    // display of an item, which is why we're returning the label
    // here instead.
    return this.results[index].label;
  },

  getStyleAt(index) {
    return this.results[index].style;
  },

  getImageAt(index) {
    return this.results[index].image;
  },

  handleEnter(aIsPopupSelection) {
    AutoCompletePopup.handleEnter(aIsPopupSelection);
  },

  stopSearch() {},

  searchString: "",

  // nsIAutoCompleteInput
  get controller() {
    return this;
  },

  get popup() {
    return null;
  },

  _focus() {
    AutoCompletePopup.requestFocus();
  },

  // Internal JS-only API
  clearResults() {
    this.results = [];
  },

  setResults(results) {
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

  init() {
    for (let msg of this.MESSAGES) {
      Services.mm.addMessageListener(msg, this);
    }
    Services.obs.addObserver(this, "message-manager-disconnect");
  },

  uninit() {
    for (let msg of this.MESSAGES) {
      Services.mm.removeMessageListener(msg, this);
    }
    Services.obs.removeObserver(this, "message-manager-disconnect");
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "message-manager-disconnect": {
        if (this.openedPopup) {
          this.openedPopup.closePopup();
        }
        break;
      }
    }
  },

  handleEvent(evt) {
    switch (evt.type) {
      case "popupshowing": {
        this.sendMessageToBrowser("FormAutoComplete:PopupOpened");
        break;
      }

      case "popuphidden": {
        let selectedIndex = this.openedPopup.selectedIndex;
        let selectedRowStyle = selectedIndex != -1 ?
          AutoCompleteResultView.getStyleAt(selectedIndex) : "";
        this.sendMessageToBrowser("FormAutoComplete:PopupClosed",
                                  { selectedRowStyle });
        AutoCompleteResultView.clearResults();
        // adjustHeight clears the height from the popup so that
        // we don't have a big shrink effect if we closed with a
        // large list, and then open on a small one.
        this.openedPopup.adjustHeight();
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
  showPopupWithResults({ browser, rect, dir, results }) {
    if (!results.length || this.openedPopup) {
      // We shouldn't ever be showing an empty popup, and if we
      // already have a popup open, the old one needs to close before
      // we consider opening a new one.
      return;
    }

    let window = browser.ownerGlobal;
    // Also check window top in case this is a sidebar.
    if (Services.focus.activeWindow !== window.top &&
        Services.focus.focusedWindow.top !== window.top) {
      // We were sent a message from a window or tab that went into the
      // background, so we'll ignore it for now.
      return;
    }

    let firstResultStyle = results[0].style;
    this.weakBrowser = Cu.getWeakReference(browser);
    this.openedPopup = browser.autoCompletePopup;
    // the layout varies according to different result type
    this.openedPopup.setAttribute("firstresultstyle", firstResultStyle);
    this.openedPopup.hidden = false;
    // don't allow the popup to become overly narrow
    this.openedPopup.setAttribute("width", Math.max(100, rect.width));
    this.openedPopup.style.direction = dir;

    AutoCompleteResultView.setResults(results);
    this.openedPopup.view = AutoCompleteResultView;
    this.openedPopup.selectedIndex = -1;

    if (results.length) {
      // Reset fields that were set from the last time the search popup was open
      this.openedPopup.mInput = AutoCompleteResultView;
      // Temporarily increase the maxRows as we don't want to show
      // the scrollbar in form autofill popup.
      if (firstResultStyle == "autofill-profile") {
        this.openedPopup._normalMaxRows = this.openedPopup.maxRows;
        this.openedPopup.mInput.maxRows = 100;
      }
      this.openedPopup.addEventListener("popuphidden", this);
      this.openedPopup.addEventListener("popupshowing", this);
      this.openedPopup.openPopupAtScreenRect("after_start", rect.left, rect.top,
                                             rect.width, rect.height, false,
                                             false);
      this.openedPopup.invalidate();
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
      AutoCompleteResultView.setResults(results);
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
  },

  removeLogin(login) {
    Services.logins.removeLogin(login);
  },

  receiveMessage(message) {
    if (!message.target.autoCompletePopup) {
      // Returning false to pacify ESLint, but this return value is
      // ignored by the messaging infrastructure.
      return false;
    }

    switch (message.name) {
      case "FormAutoComplete:SelectBy": {
        if (this.openedPopup) {
          this.openedPopup.selectBy(message.data.reverse, message.data.page);
        }
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
        this.showPopupWithResults({
          browser: message.target,
          rect,
          dir,
          results,
        });
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
        AutoCompleteResultView.clearResults();
        break;
      }
    }
    // Returning false to pacify ESLint, but this return value is
    // ignored by the messaging infrastructure.
    return false;
  },

  /**
   * Despite its name, this handleEnter is only called when the user clicks on
   * one of the items in the popup since the popup is rendered in the parent process.
   * The real controller's handleEnter is called directly in the content process
   * for other methods of completing a selection (e.g. using the tab or enter
   * keys) since the field with focus is in that process.
   * @param {boolean} aIsPopupSelection
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
    let browser = this.weakBrowser ?
      this.weakBrowser.get() :
      null;
    if (!browser) {
      return;
    }

    if (browser.messageManager) {
      browser.messageManager.sendAsyncMessage(msgName, data);
    } else {
      Cu.reportError(`AutoCompletePopup: No messageManager for message "${msgName}"`);
    }
  },

  stopSearch() {},

  /**
   * Sends a message to the browser requesting that the input
   * that the AutoCompletePopup is open for be focused.
   */
  requestFocus() {
    if (this.openedPopup) {
      this.sendMessageToBrowser("FormAutoComplete:Focus");
    }
  },
};
