/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Handles the requestAutocomplete user interface.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "RequestAutocompleteUI",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

/**
 * Handles the requestAutocomplete user interface.
 */
this.RequestAutocompleteUI = function (aAutofillData) {
  this._autofillData = aAutofillData;
}

this.RequestAutocompleteUI.prototype = {
  _autofillData: null,

  show: Task.async(function* () {
    // Create a new promise and store the function that will resolve it.  This
    // will be called by the UI once the selection has been made.
    let resolveFn;
    let uiPromise = new Promise(resolve => resolveFn = resolve);

    // Wrap the callback function so that it survives XPCOM.
    let args = {
      resolveFn: resolveFn,
      autofillData: this._autofillData,
    };
    args.wrappedJSObject = args;

    // Open the window providing the function to call when it closes.
    Services.ww.openWindow(null,
                           "chrome://formautofill/content/requestAutocomplete.xhtml",
                           "Toolkit:RequestAutocomplete",
                           "chrome,dialog=no,resizable",
                           args);

    // Wait for the window to be closed and the operation confirmed.
    return yield uiPromise;
  }),
};
