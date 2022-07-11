/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["FormAutofillChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AutoCompleteChild: "resource://gre/actors/AutoCompleteChild.jsm",
  FormAutofill: "resource://autofill/FormAutofill.jsm",
  FormAutofillContent: "resource://autofill/FormAutofillContent.jsm",
  FormAutofillUtils: "resource://autofill/FormAutofillUtils.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

/**
 * Handles content's interactions for the frame.
 */
class FormAutofillChild extends JSWindowActorChild {
  constructor() {
    super();

    this._nextHandleElement = null;
    this._alreadyDOMContentLoaded = false;
    this._hasDOMContentLoadedHandler = false;
    this._hasPendingTask = false;
    this.testListener = null;

    lazy.AutoCompleteChild.addPopupStateListener(this);
  }

  didDestroy() {
    lazy.AutoCompleteChild.removePopupStateListener(this);
  }

  popupStateChanged(messageName, data, target) {
    let docShell;
    try {
      docShell = this.docShell;
    } catch (ex) {
      lazy.AutoCompleteChild.removePopupStateListener(this);
      return;
    }

    if (!lazy.FormAutofill.isAutofillEnabled) {
      return;
    }

    const { chromeEventHandler } = docShell;

    switch (messageName) {
      case "FormAutoComplete:PopupClosed": {
        lazy.FormAutofillContent.onPopupClosed(data.selectedRowStyle);
        Services.tm.dispatchToMainThread(() => {
          chromeEventHandler.removeEventListener(
            "keydown",
            lazy.FormAutofillContent._onKeyDown,
            true
          );
        });

        break;
      }
      case "FormAutoComplete:PopupOpened": {
        lazy.FormAutofillContent.onPopupOpened();
        chromeEventHandler.addEventListener(
          "keydown",
          lazy.FormAutofillContent._onKeyDown,
          true
        );
        break;
      }
    }
  }

  _doIdentifyAutofillFields() {
    if (this._hasPendingTask) {
      return;
    }
    this._hasPendingTask = true;

    lazy.setTimeout(() => {
      lazy.FormAutofillContent.identifyAutofillFields(this._nextHandleElement);
      this._hasPendingTask = false;
      this._nextHandleElement = null;
      // This is for testing purpose only which sends a notification to indicate that the
      // form has been identified, and ready to open popup.
      this.sendAsyncMessage("FormAutofill:FieldsIdentified");
      lazy.FormAutofillContent.updateActiveInput();
    });
  }

  shouldIgnoreFormAutofillEvent(event) {
    let nodePrincipal = event.target.nodePrincipal;
    return (
      nodePrincipal.isSystemPrincipal ||
      nodePrincipal.isNullPrincipal ||
      nodePrincipal.schemeIs("about")
    );
  }

  handleEvent(evt) {
    if (!evt.isTrusted) {
      return;
    }

    if (this.shouldIgnoreFormAutofillEvent(evt)) {
      return;
    }

    switch (evt.type) {
      case "focusin": {
        if (lazy.FormAutofill.isAutofillEnabled) {
          this.onFocusIn(evt);
        }
        break;
      }
      case "DOMFormBeforeSubmit": {
        if (lazy.FormAutofill.isAutofillEnabled) {
          this.onDOMFormBeforeSubmit(evt);
        }
        break;
      }

      default: {
        throw new Error("Unexpected event type");
      }
    }
  }

  onFocusIn(evt) {
    lazy.FormAutofillContent.updateActiveInput();

    let element = evt.target;
    if (!lazy.FormAutofillUtils.isCreditCardOrAddressFieldType(element)) {
      return;
    }
    this._nextHandleElement = element;

    if (!this._alreadyDOMContentLoaded) {
      let doc = element.ownerDocument;
      if (doc.readyState === "loading") {
        if (!this._hasDOMContentLoadedHandler) {
          this._hasDOMContentLoadedHandler = true;
          doc.addEventListener(
            "DOMContentLoaded",
            () => this._doIdentifyAutofillFields(),
            { once: true }
          );
        }
        return;
      }
      this._alreadyDOMContentLoaded = true;
    }

    this._doIdentifyAutofillFields();
  }

  /**
   * Handle the DOMFormBeforeSubmit event.
   * @param {Event} evt
   */
  onDOMFormBeforeSubmit(evt) {
    let formElement = evt.target;

    if (!lazy.FormAutofill.isAutofillEnabled) {
      return;
    }

    lazy.FormAutofillContent.formSubmitted(formElement);
  }

  receiveMessage(message) {
    if (!lazy.FormAutofill.isAutofillEnabled) {
      return;
    }

    const doc = this.document;

    switch (message.name) {
      case "FormAutofill:PreviewProfile": {
        lazy.FormAutofillContent.previewProfile(doc);
        break;
      }
      case "FormAutofill:ClearForm": {
        lazy.FormAutofillContent.clearForm();
        break;
      }
      case "FormAutofill:FillForm": {
        lazy.FormAutofillContent.activeHandler.autofillFormFields(message.data);
        break;
      }
    }
  }
}
