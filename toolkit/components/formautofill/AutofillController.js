/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function AutofillController() {}

AutofillController.prototype = {
  _dispatchAsync: function (fn) {
    Services.tm.currentThread.dispatch(fn, Ci.nsIThread.DISPATCH_NORMAL);
  },

  _dispatchDisabled: function (form, win, message) {
    Services.console.logStringMessage("requestAutocomplete disabled: " + message);
    let evt = new win.AutocompleteErrorEvent("autocompleteerror", { bubbles: true, reason: "disabled" });
    form.dispatchEvent(evt);
  },

  requestAutocomplete: function (form, win) {
    this._dispatchAsync(() => this._dispatchDisabled(form, win, "not implemented"));
  },

  classID: Components.ID("{ed9c2c3c-3f86-4ae5-8e31-10f71b0f19e6}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutofillController])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AutofillController]);
