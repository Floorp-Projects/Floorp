/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["FormAutofillChild"];

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofill",
  "resource://autofill/FormAutofill.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillContent",
  "resource://autofill/FormAutofillContent.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillUtils",
  "resource://autofill/FormAutofillUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AutoCompleteChild",
  "resource://gre/actors/AutoCompleteChild.jsm"
);

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

    AutoCompleteChild.addPopupStateListener(this);
  }

  didDestroy() {
    AutoCompleteChild.removePopupStateListener(this);
  }

  popupStateChanged(messageName, data, target) {
    let docShell;
    try {
      docShell = this.docShell;
    } catch (ex) {
      AutoCompleteChild.removePopupStateListener(this);
      return;
    }

    if (!FormAutofill.isAutofillEnabled) {
      return;
    }

    const { chromeEventHandler } = docShell;

    switch (messageName) {
      case "FormAutoComplete:PopupClosed": {
        FormAutofillContent.onPopupClosed(data.selectedRowStyle);
        Services.tm.dispatchToMainThread(() => {
          chromeEventHandler.removeEventListener(
            "keydown",
            FormAutofillContent._onKeyDown,
            true
          );
        });

        break;
      }
      case "FormAutoComplete:PopupOpened": {
        FormAutofillContent.onPopupOpened();
        chromeEventHandler.addEventListener(
          "keydown",
          FormAutofillContent._onKeyDown,
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

    setTimeout(() => {
      FormAutofillContent.identifyAutofillFields(this._nextHandleElement);
      this._hasPendingTask = false;
      this._nextHandleElement = null;
      // This is for testing purpose only which sends a notification to indicate that the
      // form has been identified, and ready to open popup.
      this.sendAsyncMessage("FormAutofill:FieldsIdentified");
      FormAutofillContent.updateActiveInput();
    });
  }

  handleEvent(evt) {
    if (!evt.isTrusted) {
      return;
    }

    switch (evt.type) {
      case "focusin": {
        if (FormAutofill.isAutofillEnabled) {
          this.onFocusIn(evt);
        }
        break;
      }
      case "DOMFormBeforeSubmit": {
        if (FormAutofill.isAutofillEnabled) {
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
    FormAutofillContent.updateActiveInput();

    let element = evt.target;
    if (!FormAutofillUtils.isFieldEligibleForAutofill(element)) {
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

    if (!FormAutofill.isAutofillEnabled) {
      return;
    }

    FormAutofillContent.formSubmitted(formElement);
  }

  receiveMessage(message) {
    if (!FormAutofill.isAutofillEnabled) {
      return;
    }

    const doc = this.document;

    switch (message.name) {
      case "FormAutofill:PreviewProfile": {
        FormAutofillContent.previewProfile(doc);
        break;
      }
      case "FormAutofill:ClearForm": {
        FormAutofillContent.clearForm();
        break;
      }
      case "FormAutofill:FillForm": {
        FormAutofillContent.activeHandler.autofillFormFields(message.data);
        break;
      }
    }
  }
}
