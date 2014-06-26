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
   * @param aProperties
   *        Provides the initial properties for the newly created object.
   *
   * @return {Promise}
   * @resolves The newly created RequestAutocompleteUI object.
   * @rejects JavaScript exception.
   */
  createRequestAutocompleteUI: Task.async(function* (aProperties) {
    return {};
  }),
};
