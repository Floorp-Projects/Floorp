/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements a service used by DOM content to request Form Autofill, in
 * particular when the requestAutocomplete method of Form objects is invoked.
 *
 * See the nsIFormAutofillContentService documentation for details.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function FormAutofillContentService() {
}

FormAutofillContentService.prototype = {
  classID: Components.ID("{ed9c2c3c-3f86-4ae5-8e31-10f71b0f19e6}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormAutofillContentService]),

  // nsIFormAutofillContentService
  requestAutocomplete: function (aForm, aWindow) {
    Services.console.logStringMessage("requestAutocomplete not implemented.");

    // We will return "disabled" for now.
    let event = new aWindow.AutocompleteErrorEvent("autocompleteerror",
                                                   { bubbles: true,
                                                     reason: "disabled" });

    // Ensure the event is always dispatched on the next tick.
    Services.tm.currentThread.dispatch(() => aForm.dispatchEvent(event),
                                       Ci.nsIThread.DISPATCH_NORMAL);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FormAutofillContentService]);
