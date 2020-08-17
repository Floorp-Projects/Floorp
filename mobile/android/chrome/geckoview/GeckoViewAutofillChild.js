/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewChildModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewChildModule.jsm"
);
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.jsm",
  GeckoViewAutofill: "resource://gre/modules/GeckoViewAutofill.jsm",
});

class GeckoViewAutofillChild extends GeckoViewChildModule {
  onInit() {
    debug`onInit`;

    // Listen to Gecko's autofill commit events.
    content.windowRoot.addEventListener(
      "PasswordManager:onFormSubmit",
      aEvent => {
        const formLike = aEvent.detail.form;
        this._autofill.commitAutofill(formLike);
      }
    );

    const options = {
      mozSystemGroup: true,
      capture: false,
    };

    addEventListener("DOMFormHasPassword", this, options);
    addEventListener("DOMInputPasswordAdded", this, options);
    addEventListener("pagehide", this, options);
    addEventListener("pageshow", this, options);
    addEventListener("focusin", this, options);
    addEventListener("focusout", this, options);

    XPCOMUtils.defineLazyGetter(
      this,
      "_autofill",
      () => new GeckoViewAutofill(this.eventDispatcher)
    );
  }

  onEnable() {
    debug`onEnable`;
  }

  onDisable() {
    debug`onDisable`;
  }

  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;

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
        if (aEvent.composedTarget instanceof content.HTMLInputElement) {
          this._autofill.onFocus(aEvent.composedTarget);
        }
        break;
      }
      case "focusout": {
        if (aEvent.composedTarget instanceof content.HTMLInputElement) {
          this._autofill.onFocus(null);
        }
        break;
      }
      case "pagehide": {
        if (aEvent.target === content.document) {
          this._autofill.clearElements();
        }
        break;
      }
      case "pageshow": {
        if (aEvent.target === content.document && aEvent.persisted) {
          this._autofill.scanDocument(aEvent.target);
        }
        break;
      }
    }
  }
}

const { debug, warn } = GeckoViewAutofillChild.initLogging("GeckoViewAutofill");
const module = GeckoViewAutofillChild.create(this);
