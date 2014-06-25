/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module defines the default implementation of platform-specific functions
 * that can be overridden by the host application and by add-ons.
 *
 * This module should not be imported directly, but the "integration" getter of
 * the FormAutofill module should be used to get a reference to the currently
 * defined implementations of the methods.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "FormAutofillIntegration",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RequestAutocompleteUI",
                                  "resource://gre/modules/RequestAutocompleteUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

/**
 * This module defines the default implementation of platform-specific functions
 * that can be overridden by the host application and by add-ons.
 */
this.FormAutofillIntegration = {
  /**
   * Creates a new RequestAutocompleteUI object.
   *
   * @param aAutofillData
   *        Provides the initial data required to display the user interface.
   *        {
   *          sections: [{
   *            name: User-specified section name, or empty string.
   *            addressSections: [{
   *              addressType: "shipping", "billing", or empty string.
   *              fields: [{
   *                fieldName: Type of information requested, like "email".
   *                contactType: For example "work", "home", or empty string.
   *              }],
   *            }],
   *          }],
   *        }
   *
   * @return {Promise}
   * @resolves The newly created RequestAutocompleteUI object.
   * @rejects JavaScript exception.
   */
  createRequestAutocompleteUI: Task.async(function* (aAutofillData) {
    return new RequestAutocompleteUI(aAutofillData);
  }),
};
