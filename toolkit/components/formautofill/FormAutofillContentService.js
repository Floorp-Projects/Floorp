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

/**
 * Handles requestAutocomplete for a DOM Form element.
 */
function FormHandler(aForm, aWindow) {
  this.form = aForm;
  this.window = aWindow;
}

FormHandler.prototype = {
  /**
   * DOM Form element to which this object is attached.
   */
  form: null,

  /**
   * nsIDOMWindow to which this object is attached.
   */
  window: null,

  /**
   * Handles requestAutocomplete and generates the DOM events when finished.
   */
  handleRequestAutocomplete: Task.async(function* () {
    // Start processing the request asynchronously.  At the end, the "reason"
    // variable will contain the outcome of the operation, where an empty
    // string indicates that an unexpected exception occurred.
    let reason = "";
    try {
      let data = this.collectFormElements();

      let ui = yield FormAutofill.integration.createRequestAutocompleteUI(data);
      let result = yield ui.show();

      // At present, we only have cancellation and success cases, since we
      // don't do any validation or precondition check.
      reason = result.canceled ? "cancel" : "success";
    } catch (ex) {
      Cu.reportError(ex);
    }

    // The type of event depends on whether this is a success condition.
    let event = (reason == "success")
                ? new this.window.Event("autocomplete", { bubbles: true })
                : new this.window.AutocompleteErrorEvent("autocompleteerror",
                                                         { bubbles: true,
                                                           reason: reason });
    yield this.waitForTick();
    this.form.dispatchEvent(event);
  }),

  /**
   * Collects information from the form about fields that can be autofilled, and
   * returns an object that can be used to build RequestAutocompleteUI.
   */
  collectFormElements: function () {
    let autofillData = {
      sections: [],
    };

    for (let element of this.form.elements) {
      // Query the interface and exclude elements that cannot be autocompleted.
      if (!(element instanceof Ci.nsIDOMHTMLInputElement)) {
        continue;
      }

      // Exclude elements to which no autocomplete field has been assigned.
      let info = element.getAutocompleteInfo();
      if (!info.fieldName || ["on", "off"].indexOf(info.fieldName) != -1) {
        continue;
      }

      // The first level is the custom section.
      let section = autofillData.sections
                                .find(s => s.name == info.section);
      if (!section) {
        section = {
          name: info.section,
          addressSections: [],
        };
        autofillData.sections.push(section);
      }

      // The second level is the address section.
      let addressSection = section.addressSections
                                  .find(s => s.addressType == info.addressType);
      if (!addressSection) {
        addressSection = {
          addressType: info.addressType,
          fields: [],
        };
        section.addressSections.push(addressSection);
      }

      // The third level contains all the fields within the section.
      let field = {
        fieldName: info.fieldName,
        contactType: info.contactType,
      };
      addressSection.fields.push(field);
    }

    return autofillData;
  },

  /**
   * Waits for one tick of the event loop before resolving the returned promise.
   */
  waitForTick: function () {
    return new Promise(function (resolve) {
      Services.tm.currentThread.dispatch(resolve, Ci.nsIThread.DISPATCH_NORMAL);
    });
  },
};

/**
 * Implements a service used by DOM content to request Form Autofill, in
 * particular when the requestAutocomplete method of Form objects is invoked.
 */
function FormAutofillContentService() {
}

FormAutofillContentService.prototype = {
  classID: Components.ID("{ed9c2c3c-3f86-4ae5-8e31-10f71b0f19e6}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormAutofillContentService]),

  // nsIFormAutofillContentService
  requestAutocomplete: function (aForm, aWindow) {
    new FormHandler(aForm, aWindow).handleRequestAutocomplete()
                                   .catch(Cu.reportError);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FormAutofillContentService]);
