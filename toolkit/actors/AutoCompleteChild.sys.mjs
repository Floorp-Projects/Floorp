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

const gFormFillController = Cc[
  "@mozilla.org/satchel/form-fill-controller;1"
].getService(Ci.nsIFormFillController);

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
      case "AutoComplete:HandleEnter": {
        this.selectedIndex = message.data.selectedIndex;

        let controller = Cc[
          "@mozilla.org/autocomplete/controller;1"
        ].getService(Ci.nsIAutoCompleteController);
        controller.handleEnter(message.data.isPopupSelection);
        break;
      }

      case "AutoComplete:PopupClosed": {
        this._popupOpen = false;
        this.notifyListeners(message.name, message.data);
        break;
      }

      case "AutoComplete:PopupOpened": {
        this._popupOpen = true;
        this.notifyListeners(message.name, message.data);
        break;
      }

      case "AutoComplete:Focus": {
        // XXX See bug 1582722
        // Before bug 1573836, the messages here didn't match
        // ("AutoComplete:Focus" versus "AutoComplete:RequestFocus")
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
    this.sendAsyncMessage("AutoComplete:SetSelectedIndex", { index });
  }

  get selectedIndex() {
    // selectedIndex getter must be synchronous because we need the
    // correct value when the controller is in controller::HandleEnter.
    // We can't easily just let the parent inform us the new value every
    // time it changes because not every action that can change the
    // selectedIndex is trivial to catch (e.g. moving the mouse over the
    // list).
    let selectedIndexResult = Services.cpmm.sendSyncMessage(
      "AutoComplete:GetSelectedIndex",
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

    this.sendAsyncMessage("AutoComplete:MaybeOpenPopup", {
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
    this.sendAsyncMessage("AutoComplete:ClosePopup", {});
  }

  invalidate() {
    if (this._popupOpen) {
      let results = this.getResultsFromController(this._input);
      this.sendAsyncMessage("AutoComplete:Invalidate", { results });
    }
  }

  selectBy(reverse, page) {
    Services.cpmm.sendSyncMessage("AutoComplete:SelectBy", {
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

  getNoRollupOnEmptySearch(input) {
    const providers = this.providersByInput(input);
    return Array.from(providers).find(p => p.actorName == "LoginManager");
  }

  // Store the input to interested autocomplete providers mapping
  #providersByInput = new WeakMap();

  // This functions returns the interested providers that have called
  // `markAsAutoCompletableField` for the given input and also the hard-coded
  // autocomplete providers based on input type.
  providersByInput(input) {
    const providers = new Set(this.#providersByInput.get(input));

    if (input.hasBeenTypePassword) {
      providers.add(
        input.ownerGlobal.windowGlobalChild.getActor("LoginManager")
      );
    } else {
      // The current design is that FormHisotry doesn't call `markAsAutoCompletable`
      // for every eligilbe input. Instead, when FormFillController receives a focus event,
      // it would control the <input> if the <input> is eligible to show form history.
      // Because of the design, we need to ask FormHistory whether to search for autocomplete entries
      // for every startSearch call
      providers.add(
        input.ownerGlobal.windowGlobalChild.getActor("FormHistory")
      );
    }
    return providers;
  }

  /**
   * This API should be used by an autocomplete entry provider to mark an input field
   * as eligible for autocomplete for its type.
   * When users click on an autocompletable input, we will search autocomplete entries
   * from all the providers that have called this API for the given <input>.
   *
   * An autocomplete provider should be a JSWindowActor and implements the following
   * functions:
   * - string actorName()
   * - bool shouldSearchForAutoComplete(element);
   * - jsval getAutoCompleteSearchOption(element);
   * - jsval searchResultToAutoCompleteResult(searchString, element, record);
   * See `FormAutofillChild` for example
   *
   * @param input - The HTML <input> element that is considered autocompletable by the
   *                given provider
   * @param provider - A module that provides autocomplete entries for a <input>, for example,
   *                   FormAutofill provides address or credit card autocomplete entries,
   *                   LoginManager provides logins entreis.
   */
  markAsAutoCompletableField(input, provider) {
    gFormFillController.markAsAutoCompletableField(input);

    let providers = this.#providersByInput.get(input);
    if (!providers) {
      providers = new Set();
      this.#providersByInput.set(input, providers);
    }
    providers.add(provider);
  }

  // Record the current ongoing search request. This is used by stopSearch
  // to prevent notifying the autocomplete controller after receiving search request
  // results that were issued prior to the call to stop the search.
  #ongoingSearches = new Set();

  async startSearch(searchString, input, listener) {
    // TODO: This should be removed once we implement triggering autocomplete
    // from the parent.
    this.lastProfileAutoCompleteFocusedInput = input;

    // For all the autocomplete entry providers that previsouly marked
    // this <input> as autocompletable, ask the provider whether we should
    // search for autocomplete entries in the parent. This is because the current
    // design doesn't rely on the provider constantly monitor the <input> and
    // then mark/unmark an input. The provider generally calls the
    // `markAsAutoCompletbleField` when it sees an <input> is eliglbe for autocomplete.
    // Here we ask the provider to exam the <input> more detailedly to see
    // whether we need to search for autocomplete entries at the time users
    // click on the <input>
    const providers = this.providersByInput(input);
    const data = Array.from(providers)
      .filter(p => p.shouldSearchForAutoComplete(input, searchString))
      .map(p => ({
        actorName: p.actorName,
        options: p.getAutoCompleteSearchOption(input, searchString),
      }));

    let result = [];

    // We don't return empty result when no provider requests seaching entries in the
    // parent because for some special cases, the autocomplete entries are coming
    // from the content. For example, <datalist>.
    if (data.length) {
      const promise = this.sendQuery("AutoComplete:StartSearch", {
        searchString,
        data,
      });
      this.#ongoingSearches.add(promise);
      result = await promise.catch(e => {
        this.#ongoingSearches.delete(promise);
      });
      result ||= [];

      // If the search is stopped, don't report back.
      if (!this.#ongoingSearches.delete(promise)) {
        return;
      }
    }

    for (const provider of providers) {
      // Search result could be empty. However, an autocomplete provider might
      // want to show an autocomplete popup when there is no search result. For example,
      // <datalist> for FormHisotry, insecure warning for LoginManager.
      const searchResult = result.find(r => r.actorName == provider.actorName);
      const acResult = provider.searchResultToAutoCompleteResult(
        searchString,
        input,
        searchResult
      );

      // We have not yet supported showing autocomplete entries from multiple providers,
      // Note: The prioty is defined in AutoCompleteParent.
      if (acResult) {
        this.lastProfileAutoCompleteResult = acResult;
        listener.onSearchCompletion(acResult);
        return;
      }
    }
    this.lastProfileAutoCompleteResult = null;
  }

  stopSearch() {
    this.lastProfileAutoCompleteResult = null;
    this.#ongoingSearches.clear();
  }

  selectEntry() {
    // we don't need to pass the selected index to the parent process because
    // the selected index is maintained in the parent.
    this.sendAsyncMessage("AutoComplete:SelectEntry");
  }
}

AutoCompleteChild.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIAutoCompletePopup",
]);
