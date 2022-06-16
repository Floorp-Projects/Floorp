/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CreditCardTelemetry"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { FormAutofillUtils } = ChromeUtils.import(
  "resource://autofill/FormAutofillUtils.jsm"
);

const { FIELD_STATES } = FormAutofillUtils;

const CreditCardTelemetry = {
  // Mapping of field name used in formautofill code to the field name
  // used in the telemetry.
  CC_FORM_V2_SUPPORTED_FIELDS: {
    "cc-name": "cc_name",
    "cc-number": "cc_number",
    "cc-type": "cc_type",
    "cc-exp": "cc_exp",
    "cc-exp-month": "cc_exp_month",
    "cc-exp-year": "cc_exp_year",
  },

  /**
   * Utility function to get an `extra` object of `cc_form_v2` event telemetry
   * with a default value that applies to all keys.
   *
   * @param {string} value The default value
   * @returns {Object} The extra object
   */
  _ccFormV2InitExtra(value) {
    let extra = {};
    for (const field of Object.values(this.CC_FORM_V2_SUPPORTED_FIELDS)) {
      extra[field] = value;
    }
    return extra;
  },

  /**
   * Utility function to set the value of the specified fieldName of `cc_form_v2`
   * extra object.
   *
   * @param {Object} extra The `extra` object to be set
   * @param {string} key Field name, all supported field names are listed in
   *                     `CC_FORM_V2_SUPPORTED_FIELDS`
   * @param {string} value
   */
  _ccFormV2SetExtra(extra, key, value) {
    extra[this.CC_FORM_V2_SUPPORTED_FIELDS[key]] = value;
  },

  /**
   * Utility function to record both `cc_form` and `cc_form_v2` events
   *
   * @param {string} method The method name.
   * @param {string} flowId Flow id.
   * @param {Object} ccFormExtra  The extra object passed to `cc_form` telemetry
   * @param {Object} ccFormV2Extra The extra object passed to `cc_form_v2` telemetry
   */
  _recordCCFormEvent(method, flowId, ccFormExtra, ccFormV2Extra) {
    Services.telemetry.recordEvent(
      "creditcard",
      method,
      "cc_form",
      flowId,
      ccFormExtra
    );

    Services.telemetry.recordEvent(
      "creditcard",
      method,
      "cc_form_v2",
      flowId,
      ccFormV2Extra
    );
  },

  /**
   * Called when a form is recognized as a credit card form.
   *
   * @param {string} flowId Flow id.
   * @param {Array<Object>} fieldDetails List of current field details
   */
  recordFormDetected(flowId, fieldDetails) {
    // Record which fields could be identified
    let ccFormV2Extra = this._ccFormV2InitExtra("false");
    let identified = new Set();
    fieldDetails.forEach(detail => {
      identified.add(detail.fieldName);
      this._ccFormV2SetExtra(ccFormV2Extra, detail.fieldName, "true");
    });

    let ccFormExtra = {
      cc_name_found: identified.has("cc-name") ? "true" : "false",
      cc_number_found: identified.has("cc-number") ? "true" : "false",
      cc_exp_found:
        identified.has("cc-exp") ||
        (identified.has("cc-exp-month") && identified.has("cc-exp-year"))
          ? "true"
          : "false",
    };

    this._recordCCFormEvent("detected", flowId, ccFormExtra, ccFormV2Extra);

    Services.telemetry.scalarAdd(
      "formautofill.creditCards.detected_sections_count",
      1
    );
  },

  /**
   * Called when the credit card autofill popup is shown.
   *
   * @param {string} flowId Flow id.
   * @param {string} fieldName Field that triggers the event.
   */
  recordPopupShown(flowId, fieldName) {
    if (!flowId) {
      return;
    }

    let ccFormExtra = null;
    let ccFormV2Extra = { field_name: fieldName };
    this._recordCCFormEvent("popup_shown", flowId, ccFormExtra, ccFormV2Extra);
  },

  /**
   * Called when a credit card form is autofilled.
   *
   * @param {string} flowId Flow id.
   * @param {Array<Object>} fieldDetails List of current field details
   * @param {Object} profile The profile to be autofilled
   */
  recordFormFilled(flowId, fieldDetails, profile) {
    // Calculate values for telemetry
    let ccFormExtra = {
      cc_name: "unavailable",
      cc_number: "unavailable",
      cc_exp: "unavailable",
    };

    let ccFormV2Extra = this._ccFormV2InitExtra("unavailable");

    for (let fieldDetail of fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      let state = profile[fieldDetail.fieldName] ? "filled" : "not_filled";
      if (
        fieldDetail.state == FIELD_STATES.NORMAL &&
        (HTMLSelectElement.isInstance(element) ||
          (HTMLInputElement.isInstance(element) && element.value.length))
      ) {
        state = "user_filled";
      }
      this._ccFormV2SetExtra(ccFormV2Extra, fieldDetail.fieldName, state);
      switch (fieldDetail.fieldName) {
        case "cc-name":
          ccFormExtra.cc_name = state;
          break;
        case "cc-number":
          ccFormExtra.cc_number = state;
          break;
        case "cc-exp":
        case "cc-exp-month":
        case "cc-exp-year":
          ccFormExtra.cc_exp = state;
          break;
      }
    }

    this._recordCCFormEvent("filled", flowId, ccFormExtra, ccFormV2Extra);
  },

  /**
   * Called when a credit card field is filled and then modifed by
   * the user.
   *
   * @param {string} flowId Flow id.
   * @param {string} fieldName Field that triggers the clear form event.
   */
  recordFilledModified(flowId, fieldName) {
    let ccFormExtra = { field_name: fieldName };
    let ccFormV2Extra = { field_name: fieldName };

    this._recordCCFormEvent(
      "filled_modified",
      flowId,
      ccFormExtra,
      ccFormV2Extra
    );
  },

  /**
   * Called when a credit card form is submitted
   *
   * @param {Objecy} records Credit card and address records filled in the form.
   * @param {Array<HTMLElements>} elements Elements in the form
   */
  recordFormSubmitted(records, elements) {
    records.creditCard.forEach(record => {
      let ccFormExtra = {
        // Fields which have been filled manually.
        fields_not_auto: "0",
        // Fields which have been autofilled.
        fields_auto: "0",
        // Fields which have been autofilled and then modified.
        fields_modified: "0",
      };

      let ccFormV2Extra = this._ccFormV2InitExtra("unavailable");

      if (record.guid !== null) {
        // If the `guid` is not null, it means we're editing an existing record.
        // In that case, all fields in the record are autofilled, and fields in
        // `untouchedFields` are unmodified.
        let totalCount = elements.length;
        let autofilledCount = Object.keys(record.record).length;
        let unmodifiedCount = record.untouchedFields.length;

        for (let fieldName of Object.keys(record.record)) {
          if (record.untouchedFields?.includes(fieldName)) {
            this._ccFormV2SetExtra(ccFormV2Extra, fieldName, "autofilled");
          } else {
            this._ccFormV2SetExtra(ccFormV2Extra, fieldName, "user filled");
          }
        }
        ccFormExtra.fields_not_auto = (totalCount - autofilledCount).toString();
        ccFormExtra.fields_auto = autofilledCount.toString();
        ccFormExtra.fields_modified = (
          autofilledCount - unmodifiedCount
        ).toString();
      } else {
        // If the `guid` is null, we're filling a new form.
        // In that case, all not-null fields are manually filled.

        ccFormExtra.fields_not_auto = Array.from(elements)
          .filter(element => !!element.value?.trim().length)
          .length.toString();
      }

      this._recordCCFormEvent(
        "submitted",
        record.flowId,
        ccFormExtra,
        ccFormV2Extra
      );
    });

    if (records.creditCard.length) {
      Services.telemetry.scalarAdd(
        "formautofill.creditCards.submitted_sections_count",
        records.creditCard.length
      );
    }
  },

  /**
   * Called when a credit card form is cleared.
   *
   * @param {string} flowId Flow id.
   * @param {string} fieldName Field that triggers the clear form event.
   */
  recordFormCleared(flowId, fieldName) {
    // Note that when a form is cleared, we also record `filled_modified` events
    // for all the fields that have been cleared.
    Services.telemetry.recordEvent(
      "creditcard",
      "cleared",
      "cc_form_v2",
      flowId,
      { field_name: fieldName }
    );
  },
};
