/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of "requestAutocomplete.xhtml".
 */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const RequestAutocompleteDialog = {
  resolveFn: null,
  autofillData: null,

  onLoad() {
    Task.spawn(function* () {
      let args = window.arguments[0].wrappedJSObject;
      this.resolveFn = args.resolveFn;
      this.autofillData = args.autofillData;

      window.sizeToContent();

      Services.obs.notifyObservers(window,
                                   "formautofill-window-initialized");
    }.bind(this)).catch(Cu.reportError);
  },

  onAccept() {
    // TODO: Replace with autofill storage module (bug 1018304).
    const dummyDB = {
      "": {
        "name": "Mozzy La",
        "street-address": "331 E Evelyn Ave",
        "address-level2": "Mountain View",
        "address-level1": "CA",
        "country": "US",
        "postal-code": "94041",
        "email": "email@example.org",
      }
    };

    let result = { fields: [] };
    for (let section of this.autofillData.sections) {
      for (let addressSection of section.addressSections) {
        let addressType = addressSection.addressType;
        if (!(addressType in dummyDB)) {
          continue;
        }

        for (let field of addressSection.fields) {
          let fieldName = field.fieldName;
          if (!(fieldName in dummyDB[addressType])) {
            continue;
          }

          result.fields.push({
            section: section.name,
            addressType,
            contactType: field.contactType,
            fieldName: field.fieldName,
            value: dummyDB[addressType][fieldName],
          });
        }
      }
    }

    window.close();
    this.resolveFn(result);
  },

  onCancel() {
    window.close();
    this.resolveFn({ canceled: true });
  },
};
