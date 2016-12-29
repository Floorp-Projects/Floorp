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

  this.fieldDetails = [];
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
   * Array of collected data about relevant form fields.  Each item is an object
   * storing the identifying details of the field and a reference to the
   * originally associated element from the form.
   *
   * The "section", "addressType", "contactType", and "fieldName" values are
   * used to identify the exact field when the serializable data is received
   * from the requestAutocomplete user interface.  There cannot be multiple
   * fields which have the same exact combination of these values.
   *
   * A direct reference to the associated element cannot be sent to the user
   * interface because processing may be done in the parent process.
   */
  fieldDetails: null,

  /**
   * Handles requestAutocomplete and generates the DOM events when finished.
   */
  handleRequestAutocomplete: Task.async(function* () {
    // Start processing the request asynchronously.  At the end, the "reason"
    // variable will contain the outcome of the operation, where an empty
    // string indicates that an unexpected exception occurred.
    let reason = "";
    try {
      reason = yield this.promiseRequestAutocomplete();
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
   * Handles requestAutocomplete and returns the outcome when finished.
   *
   * @return {Promise}
   * @resolves The "reason" value indicating the outcome of the
   *           requestAutocomplete operation, including "success" if the
   *           operation completed successfully.
   */
  promiseRequestAutocomplete: Task.async(function* () {
    let data = this.collectFormFields();
    if (!data) {
      return "disabled";
    }

    // Access the frame message manager of the window starting the request.
    let rootDocShell = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIDocShell)
                                  .sameTypeRootTreeItem
                                  .QueryInterface(Ci.nsIDocShell);
    let frameMM = rootDocShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIContentFrameMessageManager);

    // We need to set up a temporary message listener for our result before we
    // send the request to the parent process.  At present, there is no check
    // for reentrancy (bug 1020459), thus it is possible that we'll receive a
    // message for a different request, but this will not be normally allowed.
    let promiseRequestAutocompleteResult = new Promise((resolve, reject) => {
      frameMM.addMessageListener("FormAutofill:RequestAutocompleteResult",
        function onResult(aMessage) {
          frameMM.removeMessageListener(
                        "FormAutofill:RequestAutocompleteResult", onResult);
          // Exceptions in the parent process are serialized and propagated in
          // the response message that we received.
          if ("exception" in aMessage.data) {
            reject(aMessage.data.exception);
          } else {
            resolve(aMessage.data);
          }
        });
    });

    // Send the message to the parent process, and wait for the result.  This
    // will throw an exception if one occurred in the parent process.
    frameMM.sendAsyncMessage("FormAutofill:RequestAutocomplete", data);
    let result = yield promiseRequestAutocompleteResult;
    if (result.canceled) {
      return "cancel";
    }

    this.autofillFormFields(result);

    return "success";
  }),

  /**
   * Returns information from the form about fields that can be autofilled, and
   * populates the fieldDetails array on this object accordingly.
   *
   * @returns Serializable data structure that can be sent to the user
   *          interface, or null if the operation failed because the constraints
   *          on the allowed fields were not honored.
   */
  collectFormFields: function() {
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

      // Store the association between the field metadata and the element.
      if (this.fieldDetails.some(f => f.section == info.section &&
                                      f.addressType == info.addressType &&
                                      f.contactType == info.contactType &&
                                      f.fieldName == info.fieldName)) {
        // A field with the same identifier already exists.
        return null;
      }
      this.fieldDetails.push({
        section: info.section,
        addressType: info.addressType,
        contactType: info.contactType,
        fieldName: info.fieldName,
        element: element,
      });

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
   * Processes form fields that can be autofilled, and populates them with the
   * data provided by RequestAutocompleteUI.
   *
   * @param aAutofillResult
   *        Data returned by the user interface.
   *        {
   *          fields: [
   *            section: Value originally provided to the user interface.
   *            addressType: Value originally provided to the user interface.
   *            contactType: Value originally provided to the user interface.
   *            fieldName: Value originally provided to the user interface.
   *            value: String with which the field should be updated.
   *          ],
   *        }
   */
  autofillFormFields: function(aAutofillResult) {
    for (let field of aAutofillResult.fields) {
      // Get the field details, if it was processed by the user interface.
      let fieldDetail = this.fieldDetails
                            .find(f => f.section == field.section &&
                                       f.addressType == field.addressType &&
                                       f.contactType == field.contactType &&
                                       f.fieldName == field.fieldName);
      if (!fieldDetail) {
        continue;
      }

      fieldDetail.element.value = field.value;
    }
  },

  /**
   * Waits for one tick of the event loop before resolving the returned promise.
   */
  waitForTick: function() {
    return new Promise(function(resolve) {
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
  requestAutocomplete: function(aForm, aWindow) {
    new FormHandler(aForm, aWindow).handleRequestAutocomplete()
                                   .catch(Cu.reportError);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FormAutofillContentService]);
