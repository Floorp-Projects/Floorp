/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CreditCardTelemetry"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillUtils",
  "resource://autofill/FormAutofillUtils.jsm"
);

const { FIELD_STATES } = FormAutofillUtils;

this.CreditCardTelemetry = {
  /**
   * cc_form event telemetry
   *
   * @param {string} flowId
   * @param {Object} fieldDetails
   *        The fieldDetail objects for the fields in this section
   */
  recordFormDetected(flowId, fieldDetails) {
    // Record which fields could be identified
    let identified = new Set();
    fieldDetails.forEach(detail => identified.add(detail.fieldName));

    Services.telemetry.recordEvent(
      "creditcard",
      "detected",
      "cc_form",
      flowId,
      {
        cc_name_found: identified.has("cc-name") ? "true" : "false",
        cc_number_found: identified.has("cc-number") ? "true" : "false",
        cc_exp_found:
          identified.has("cc-exp") ||
          (identified.has("cc-exp-month") && identified.has("cc-exp-year"))
            ? "true"
            : "false",
      }
    );
    Services.telemetry.scalarAdd(
      "formautofill.creditCards.detected_sections_count",
      1
    );
  },

  recordPopupShown(flowId) {
    if (!flowId) {
      return;
    }

    Services.telemetry.recordEvent(
      "creditcard",
      "popup_shown",
      "cc_form",
      flowId
    );
  },

  recordFormFilled(flowId, fieldDetails, profile) {
    // Calculate values for telemetry
    let extra = {
      cc_name: "unavailable",
      cc_number: "unavailable",
      cc_exp: "unavailable",
    };

    for (let fieldDetail of fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      let state = profile[fieldDetail.fieldName] ? "filled" : "not_filled";
      if (
        fieldDetail.state == FIELD_STATES.NORMAL &&
        (ChromeUtils.getClassName(element) == "HTMLSelectElement" ||
          (ChromeUtils.getClassName(element) == "HTMLInputElement" &&
            element.value.length))
      ) {
        state = "user_filled";
      }
      switch (fieldDetail.fieldName) {
        case "cc-name":
          extra.cc_name = state;
          break;
        case "cc-number":
          extra.cc_number = state;
          break;
        case "cc-exp":
        case "cc-exp-month":
        case "cc-exp-year":
          extra.cc_exp = state;
          break;
      }
    }

    Services.telemetry.recordEvent(
      "creditcard",
      "filled",
      "cc_form",
      flowId,
      extra
    );
  },

  recordFilledModified(flowId, fieldDetail) {
    Services.telemetry.recordEvent(
      "creditcard",
      "filled_modified",
      "cc_form",
      flowId,
      { field_name: fieldDetail.fieldName }
    );
  },

  recordFormSubmitted(records, elements) {
    records.creditCard.forEach(record => {
      let extra = {
        // Fields which have been filled manually.
        fields_not_auto: "0",
        // Fields which have been autofilled.
        fields_auto: "0",
        // Fields which have been autofilled and then modified.
        fields_modified: "0",
      };

      if (record.guid !== null) {
        // If the `guid` is not null, it means we're editing an existing record.
        // In that case, all fields in the record are autofilled, and fields in
        // `untouchedFields` are unmodified.
        let totalCount = elements.length;
        let autofilledCount = Object.keys(record.record).length;
        let unmodifiedCount = record.untouchedFields.length;

        extra.fields_not_auto = (totalCount - autofilledCount).toString();
        extra.fields_auto = autofilledCount.toString();
        extra.fields_modified = (autofilledCount - unmodifiedCount).toString();
      } else {
        // If the `guid` is null, we're filling a new form.
        // In that case, all not-null fields are manually filled.
        extra.fields_not_auto = Array.from(elements)
          .filter(element => !!element.value?.trim().length)
          .length.toString();
      }

      Services.telemetry.recordEvent(
        "creditcard",
        "submitted",
        "cc_form",
        record.flowId,
        extra
      );
    });

    if (records.creditCard.length) {
      Services.telemetry.scalarAdd(
        "formautofill.creditCards.submitted_sections_count",
        records.creditCard.length
      );
    }
  },
};
