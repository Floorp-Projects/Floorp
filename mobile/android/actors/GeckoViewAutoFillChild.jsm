/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.jsm",
  GeckoViewAutofill: "resource://gre/modules/GeckoViewAutofill.jsm",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
});

const EXPORTED_SYMBOLS = ["GeckoViewAutoFillChild"];

class GeckoViewAutoFillChild extends GeckoViewActorChild {
  constructor() {
    super();

    XPCOMUtils.defineLazyGetter(this, "_autofill", function() {
      return new GeckoViewAutofill(this.eventDispatcher);
    });
  }

  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;
    const { contentWindow } = this;

    switch (aEvent.type) {
      case "DOMFormHasPassword": {
        this._autofill.addElement(
          FormLikeFactory.createFromForm(aEvent.composedTarget)
        );
        break;
      }
      case "DOMInputPasswordAdded": {
        const input = aEvent.composedTarget;
        if (!input.form) {
          this._autofill.addElement(FormLikeFactory.createFromField(input));
        }
        break;
      }
      case "focusin": {
        if (aEvent.composedTarget instanceof contentWindow.HTMLInputElement) {
          this._autofill.onFocus(aEvent.composedTarget);
        }
        break;
      }
      case "focusout": {
        if (aEvent.composedTarget instanceof contentWindow.HTMLInputElement) {
          this._autofill.onFocus(null);
        }
        break;
      }
      case "pagehide": {
        if (aEvent.target === contentWindow.top.document) {
          this._autofill.clearElements();
        }
        break;
      }
      case "pageshow": {
        if (aEvent.target === contentWindow.top.document && aEvent.persisted) {
          this._autofill.scanDocument(aEvent.target);
        }
        break;
      }
      case "PasswordManager:ShowDoorhanger": {
        const { form: formLike } = aEvent.detail;
        this._autofill.commitAutofill(formLike);
        break;
      }
    }
  }
}

const { debug, warn } = GeckoViewAutoFillChild.initLogging("GeckoViewAutoFill");
