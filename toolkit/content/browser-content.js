/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* eslint no-unused-vars: ["error", {args: "none"}] */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { ActorManagerChild } = ChromeUtils.import(
  "resource://gre/modules/ActorManagerChild.jsm"
);

ActorManagerChild.attach(this);

ChromeUtils.defineModuleGetter(
  this,
  "AutoCompletePopup",
  "resource://gre/modules/AutoCompletePopupContent.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AutoScrollController",
  "resource://gre/modules/AutoScrollController.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "formFill",
  "@mozilla.org/satchel/form-fill-controller;1",
  "nsIFormFillController"
);

var global = this;

var AutoScrollListener = {
  handleEvent(event) {
    if (event.isTrusted & !event.defaultPrevented && event.button == 1) {
      if (!this._controller) {
        this._controller = new AutoScrollController(global);
      }
      this._controller.handleEvent(event);
    }
  },
};
Services.els.addSystemEventListener(
  global,
  "mousedown",
  AutoScrollListener,
  true
);

let AutoComplete = {
  _connected: false,

  init() {
    addEventListener("unload", this, { once: true });
    addEventListener("DOMContentLoaded", this, { once: true });
    // WebExtension browserAction is preloaded and does not receive DCL, wait
    // on pageshow so we can hookup the formfill controller.
    addEventListener("pageshow", this, { capture: true, once: true });

    XPCOMUtils.defineLazyProxy(
      this,
      "popup",
      () => new AutoCompletePopup(global),
      { QueryInterface: null }
    );
    this.init = null;
  },

  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded":
      case "pageshow":
        // We need to wait for a content viewer to be available
        // before we can attach our AutoCompletePopup handler,
        // since nsFormFillController assumes one will exist
        // when we call attachToBrowser.
        if (!this._connected) {
          formFill.attachToBrowser(docShell, this.popup);
          this._connected = true;
        }
        break;

      case "unload":
        if (this._connected) {
          formFill.detachFromBrowser(docShell);
          this._connected = false;
        }
        break;
    }
  },
};

AutoComplete.init();
