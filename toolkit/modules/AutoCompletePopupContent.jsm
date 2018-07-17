/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["AutoCompletePopup"];

/* eslint no-unused-vars: ["error", {args: "none"}] */

ChromeUtils.defineModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");

const MESSAGES = [
  "FormAutoComplete:HandleEnter",
  "FormAutoComplete:PopupClosed",
  "FormAutoComplete:PopupOpened",
  "FormAutoComplete:RequestFocus",
];


class AutoCompletePopup {
  constructor(mm) {
    this.mm = mm;

    for (let messageName of MESSAGES) {
      mm.addMessageListener(messageName, this);
    }

    this._input = null;
    this._popupOpen = false;
  }

  receiveMessage(message) {
    switch (message.name) {
      case "FormAutoComplete:HandleEnter": {
        this.selectedIndex = message.data.selectedIndex;

        let controller = Cc["@mozilla.org/autocomplete/controller;1"]
                           .getService(Ci.nsIAutoCompleteController);
        controller.handleEnter(message.data.isPopupSelection);
        break;
      }

      case "FormAutoComplete:PopupClosed": {
        this._popupOpen = false;
        break;
      }

      case "FormAutoComplete:PopupOpened": {
        this._popupOpen = true;
        break;
      }

      case "FormAutoComplete:RequestFocus": {
        if (this._input) {
          this._input.focus();
        }
        break;
      }
    }
  }

  get input() { return this._input; }
  get overrideValue() { return null; }
  set selectedIndex(index) {
    this.mm.sendAsyncMessage("FormAutoComplete:SetSelectedIndex", { index });
  }
  get selectedIndex() {
    // selectedIndex getter must be synchronous because we need the
    // correct value when the controller is in controller::HandleEnter.
    // We can't easily just let the parent inform us the new value every
    // time it changes because not every action that can change the
    // selectedIndex is trivial to catch (e.g. moving the mouse over the
    // list).
    return this.mm.sendSyncMessage("FormAutoComplete:GetSelectedIndex", {});
  }
  get popupOpen() {
    return this._popupOpen;
  }

  openAutocompletePopup(input, element) {
    if (this._popupOpen || !input) {
      return;
    }

    let rect = BrowserUtils.getElementBoundingScreenRect(element);
    let window = element.ownerGlobal;
    let dir = window.getComputedStyle(element).direction;
    let results = this.getResultsFromController(input);

    this.mm.sendAsyncMessage("FormAutoComplete:MaybeOpenPopup",
                             { results, rect, dir });
    this._input = input;
  }

  closePopup() {
    // We set this here instead of just waiting for the
    // PopupClosed message to do it so that we don't end
    // up in a state where the content thinks that a popup
    // is open when it isn't (or soon won't be).
    this._popupOpen = false;
    this.mm.sendAsyncMessage("FormAutoComplete:ClosePopup", {});
  }

  invalidate() {
    if (this._popupOpen) {
      let results = this.getResultsFromController(this._input);
      this.mm.sendAsyncMessage("FormAutoComplete:Invalidate", { results });
    }
  }

  selectBy(reverse, page) {
    this._index = this.mm.sendSyncMessage("FormAutoComplete:SelectBy", {
      reverse,
      page
    });
  }

  getResultsFromController(inputField) {
    let results = [];

    if (!inputField) {
      return results;
    }

    let controller = inputField.controller;
    if (!(controller instanceof Ci.nsIAutoCompleteController)) {
      return results;
    }

    for (let i = 0; i < controller.matchCount; ++i) {
      let result = {};
      result.value = controller.getValueAt(i);
      result.label = controller.getLabelAt(i);
      result.comment = controller.getCommentAt(i);
      result.style = controller.getStyleAt(i);
      result.image = controller.getImageAt(i);
      results.push(result);
    }

    return results;
  }
}
