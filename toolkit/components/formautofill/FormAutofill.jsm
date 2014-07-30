/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Main module handling references to objects living in the main process.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "FormAutofill",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillIntegration",
                                  "resource://gre/modules/FormAutofillIntegration.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

/**
 * Main module handling references to objects living in the main process.
 */
this.FormAutofill = {
  /**
   * Dynamically generated object implementing the FormAutofillIntegration
   * methods.  Platform-specific code and add-ons can override methods of this
   * object using the registerIntegration method.
   */
  get integration() {
    // This lazy getter is only called if registerIntegration was never called.
    this._refreshIntegrations();
    return this.integration;
  },

  /**
   * Registers new overrides for the FormAutofillIntegration methods.  Example:
   *
   *   FormAutofill.registerIntegration(base => ({
   *     createRequestAutocompleteUI: Task.async(function* () {
   *       yield base.createRequestAutocompleteUI.apply(this, arguments);
   *     }),
   *   }));
   *
   * @param aIntegrationFn
   *        Function returning an object defining the methods that should be
   *        overridden.  Its only parameter is an object that contains the base
   *        implementation of all the available methods.
   *
   * @note The integration function is called every time the list of registered
   *       integration functions changes.  Thus, it should not have any side
   *       effects or do any other initialization.
   */
  registerIntegration: function (aIntegrationFn) {
    this._integrationFns.add(aIntegrationFn);
    this._refreshIntegrations();
  },

  /**
   * Removes a previously registered FormAutofillIntegration override.
   *
   * Overrides don't usually need to be unregistered, unless they are added by a
   * restartless add-on, in which case they should be unregistered when the
   * add-on is disabled or uninstalled.
   *
   * @param aIntegrationFn
   *        This must be the same function object passed to registerIntegration.
   */
  unregisterIntegration: function (aIntegrationFn) {
    this._integrationFns.delete(aIntegrationFn);
    this._refreshIntegrations();
  },

  /**
   * Ordered list of registered functions defining integration overrides.
   */
  _integrationFns: new Set(),

  /**
   * Updates the "integration" getter with the object resulting from combining
   * all the registered integration overrides with the default implementation.
   */
  _refreshIntegrations: function () {
    delete this.integration;

    let combined = FormAutofillIntegration;
    for (let integrationFn of this._integrationFns) {
      try {
        // Obtain a new set of methods from the next integration function in the
        // list, specifying the current combined object as the base argument.
        let integration = integrationFn.call(null, combined);

        // Retrieve a list of property descriptors from the returned object, and
        // use them to build a new combined object whose prototype points to the
        // previous combined object.
        let descriptors = {};
        for (let name of Object.getOwnPropertyNames(integration)) {
          descriptors[name] = Object.getOwnPropertyDescriptor(integration, name);
        }
        combined = Object.create(combined, descriptors);
      } catch (ex) {
        // Any error will result in the current integration being skipped.
        Cu.reportError(ex);
      }
    }

    this.integration = combined;
  },

  /**
   * Processes a requestAutocomplete message asynchronously.
   *
   * @param aData
   *        Provided to FormAutofillIntegration.createRequestAutocompleteUI.
   *
   * @return {Promise}
   * @resolves Structured data received from the requestAutocomplete UI.
   */
  processRequestAutocomplete: Task.async(function* (aData) {
    let ui = yield FormAutofill.integration.createRequestAutocompleteUI(aData);
    return yield ui.show();
  }),
};
