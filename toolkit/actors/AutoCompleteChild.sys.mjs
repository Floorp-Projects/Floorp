/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-unused-vars: ["error", {args: "none"}] */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ContentDOMReference: "resource://gre/modules/ContentDOMReference.sys.mjs",
  LayoutUtils: "resource://gre/modules/LayoutUtils.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
});

let autoCompleteListeners = new Set();

export class AutoCompleteChild extends JSWindowActorChild {
  constructor() {
    super();

    this._input = null;
    this._popupOpen = false;
  }

  static addPopupStateListener(listener) {
    autoCompleteListeners.add(listener);
  }

  static removePopupStateListener(listener) {
    autoCompleteListeners.delete(listener);
  }

  receiveMessage(message) {
    switch (message.name) {
      case "FormAutoComplete:HandleEnter": {
        this.selectedIndex = message.data.selectedIndex;

        let controller = Cc[
          "@mozilla.org/autocomplete/controller;1"
        ].getService(Ci.nsIAutoCompleteController);
        controller.handleEnter(message.data.isPopupSelection);
        break;
      }

      case "FormAutoComplete:PopupClosed": {
        this._popupOpen = false;
        this.notifyListeners(message.name, message.data);
        break;
      }

      case "FormAutoComplete:PopupOpened": {
        this._popupOpen = true;
        this.notifyListeners(message.name, message.data);
        break;
      }

      case "FormAutoComplete:Focus": {
        // XXX See bug 1582722
        // Before bug 1573836, the messages here didn't match
        // ("FormAutoComplete:Focus" versus "FormAutoComplete:RequestFocus")
        // so this was never called. However this._input is actually a
        // nsIAutoCompleteInput, which doesn't have a focus() method, so it
        // wouldn't have worked anyway. So for now, I have just disabled this.
        /*
        if (this._input) {
          this._input.focus();
        }
        */
        break;
      }
    }
  }

  notifyListeners(messageName, data) {
    for (let listener of autoCompleteListeners) {
      try {
        listener.popupStateChanged(messageName, data, this.contentWindow);
      } catch (ex) {
        console.error(ex);
      }
    }
  }

  get input() {
    return this._input;
  }

  set selectedIndex(index) {
    this.sendAsyncMessage("FormAutoComplete:SetSelectedIndex", { index });
  }

  get selectedIndex() {
    // selectedIndex getter must be synchronous because we need the
    // correct value when the controller is in controller::HandleEnter.
    // We can't easily just let the parent inform us the new value every
    // time it changes because not every action that can change the
    // selectedIndex is trivial to catch (e.g. moving the mouse over the
    // list).
    let selectedIndexResult = Services.cpmm.sendSyncMessage(
      "FormAutoComplete:GetSelectedIndex",
      {
        browsingContext: this.browsingContext,
      }
    );

    if (
      selectedIndexResult.length != 1 ||
      !Number.isInteger(selectedIndexResult[0])
    ) {
      throw new Error("Invalid autocomplete selectedIndex");
    }
    return selectedIndexResult[0];
  }

  get popupOpen() {
    return this._popupOpen;
  }

  openAutocompletePopup(input, element) {
    if (this._popupOpen || !input || !element?.isConnected) {
      return;
    }

    let rect = lazy.LayoutUtils.getElementBoundingScreenRect(element);
    let window = element.ownerGlobal;
    let dir = window.getComputedStyle(element).direction;
    let results = this.getResultsFromController(input);
    let formOrigin = lazy.LoginHelper.getLoginOrigin(
      element.ownerDocument.documentURI
    );
    let inputElementIdentifier = lazy.ContentDOMReference.get(element);

    this.sendAsyncMessage("FormAutoComplete:MaybeOpenPopup", {
      results,
      rect,
      dir,
      inputElementIdentifier,
      formOrigin,
    });

    this._input = input;
  }

  closePopup() {
    // We set this here instead of just waiting for the
    // PopupClosed message to do it so that we don't end
    // up in a state where the content thinks that a popup
    // is open when it isn't (or soon won't be).
    this._popupOpen = false;
    this.sendAsyncMessage("FormAutoComplete:ClosePopup", {});
  }

  invalidate() {
    if (this._popupOpen) {
      let results = this.getResultsFromController(this._input);
      this.sendAsyncMessage("FormAutoComplete:Invalidate", { results });
    }
  }

  selectBy(reverse, page) {
    Services.cpmm.sendSyncMessage("FormAutoComplete:SelectBy", {
      browsingContext: this.browsingContext,
      reverse,
      page,
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

AutoCompleteChild.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIAutoCompletePopup",
]);
