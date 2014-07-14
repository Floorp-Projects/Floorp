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

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofill",
                                  "resource://gre/modules/FormAutofill.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

function FormAutofillContentService() {
}

FormAutofillContentService.prototype = {
  classID: Components.ID("{ed9c2c3c-3f86-4ae5-8e31-10f71b0f19e6}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormAutofillContentService]),

  // nsIFormAutofillContentService
  requestAutocomplete: function (aForm, aWindow) {
    Task.spawn(function* () {
      // Start processing the request asynchronously.  At the end, the "reason"
      // variable will contain the outcome of the operation, where an empty
      // string indicates that an unexpected exception occurred.
      let reason = "";
      try {
        let ui = yield FormAutofill.integration.createRequestAutocompleteUI({});
        let result = yield ui.show();

        // At present, we only have cancellation and success cases, since we
        // don't do any validation or precondition check.
        reason = result.canceled ? "cancel" : "success";
      } catch (ex) {
        Cu.reportError(ex);
      }

      // The type of event depends on whether this is a success condition.
      let event = (reason == "success")
                  ? new aWindow.Event("autocomplete", { bubbles: true })
                  : new aWindow.AutocompleteErrorEvent("autocompleteerror",
                                                       { bubbles: true,
                                                         reason: reason });

      // Ensure the event is always dispatched on the next tick.
      Services.tm.currentThread.dispatch(() => aForm.dispatchEvent(event),
                                         Ci.nsIThread.DISPATCH_NORMAL);
    }.bind(this)).catch(Cu.reportError);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FormAutofillContentService]);
