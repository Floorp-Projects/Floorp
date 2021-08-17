/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Defines a handler object to represent forms that autofill can handle.
 */

"use strict";

var EXPORTED_SYMBOLS = ["FormAutofillHandler"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { FormAutofill } = ChromeUtils.import(
  "resource://autofill/FormAutofill.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillUtils",
  "resource://autofill/FormAutofillUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillHeuristics",
  "resource://autofill/FormAutofillHeuristics.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormLikeFactory",
  "resource://gre/modules/FormLikeFactory.jsm"
);

const formFillController = Cc[
  "@mozilla.org/satchel/form-fill-controller;1"
].getService(Ci.nsIFormFillController);

XPCOMUtils.defineLazyGetter(this, "reauthPasswordPromptMessage", () => {
  const brandShortName = FormAutofillUtils.brandBundle.GetStringFromName(
    "brandShortName"
  );
  // The string name for Mac is changed because the value needed updating.
  const platform = AppConstants.platform.replace("macosx", "macos");
  return FormAutofillUtils.stringBundle.formatStringFromName(
    `useCreditCardPasswordPrompt.${platform}`,
    [brandShortName]
  );
});

XPCOMUtils.defineLazyModuleGetters(this, {
  CreditCard: "resource://gre/modules/CreditCard.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  gUUIDGenerator: ["@mozilla.org/uuid-generator;1", "nsIUUIDGenerator"],
});

this.log = null;
FormAutofill.defineLazyLogGetter(this, EXPORTED_SYMBOLS[0]);

const { FIELD_STATES } = FormAutofillUtils;

class FormAutofillSection {
  constructor(fieldDetails, winUtils) {
    this.fieldDetails = fieldDetails;
    this.filledRecordGUID = null;
    this.winUtils = winUtils;

    /**
     * Enum for form autofill MANUALLY_MANAGED_STATES values
     */
    this._FIELD_STATE_ENUM = {
      // not themed
      [FIELD_STATES.NORMAL]: null,
      // highlighted
      [FIELD_STATES.AUTO_FILLED]: "autofill",
      // highlighted && grey color text
      [FIELD_STATES.PREVIEW]: "-moz-autofill-preview",
    };

    if (!this.isValidSection()) {
      this.fieldDetails = [];
      log.debug(
        `Ignoring ${this.constructor.name} related fields since it is an invalid section`
      );
    }

    this._cacheValue = {
      allFieldNames: null,
      matchingSelectOption: null,
    };
  }

  /*
   * Examine the section is a valid section or not based on its fieldDetails or
   * other information. This method must be overrided.
   *
   * @returns {boolean} True for a valid section, otherwise false
   *
   */
  isValidSection() {
    throw new TypeError("isValidSection method must be overrided");
  }

  /*
   * Examine the section is an enabled section type or not based on its
   * preferences. This method must be overrided.
   *
   * @returns {boolean} True for an enabled section type, otherwise false
   *
   */
  isEnabled() {
    throw new TypeError("isEnabled method must be overrided");
  }

  /*
   * Examine the section is createable for storing the profile. This method
   * must be overrided.
   *
   * @param {Object} record The record for examining createable
   * @returns {boolean} True for the record is createable, otherwise false
   *
   */
  isRecordCreatable(record) {
    throw new TypeError("isRecordCreatable method must be overrided");
  }

  /**
   * Override this method if the profile is needed to apply some transformers.
   *
   * @param {Object} profile
   *        A profile should be converted based on the specific requirement.
   */
  applyTransformers(profile) {}

  /**
   * Override this method if the profile is needed to be customized for
   * previewing values.
   *
   * @param {Object} profile
   *        A profile for pre-processing before previewing values.
   */
  preparePreviewProfile(profile) {}

  /**
   * Override this method if the profile is needed to be customized for filling
   * values.
   *
   * @param {Object} profile
   *        A profile for pre-processing before filling values.
   * @returns {boolean} Whether the profile should be filled.
   */
  async prepareFillingProfile(profile) {
    return true;
  }

  /*
   * Override this methid if any data for `createRecord` is needed to be
   * normailized before submitting the record.
   *
   * @param {Object} profile
   *        A record for normalization.
   */
  normalizeCreatingRecord(data) {}

  /*
   * Override this method if there is any field value needs to compute for a
   * specific case. Return the original value in the default case.
   * @param {String} value
   *        The original field value.
   * @param {Object} fieldDetail
   *        A fieldDetail of the related element.
   * @param {HTMLElement} element
   *        A element for checking converting value.
   *
   * @returns {String}
   *          A string of the converted value.
   */
  computeFillingValue(value, fieldName, element) {
    return value;
  }

  set focusedInput(element) {
    this._focusedDetail = this.getFieldDetailByElement(element);
  }

  getFieldDetailByElement(element) {
    return this.fieldDetails.find(
      detail => detail.elementWeakRef.get() == element
    );
  }

  get allFieldNames() {
    if (!this._cacheValue.allFieldNames) {
      this._cacheValue.allFieldNames = this.fieldDetails.map(
        record => record.fieldName
      );
    }
    return this._cacheValue.allFieldNames;
  }

  getFieldDetailByName(fieldName) {
    return this.fieldDetails.find(detail => detail.fieldName == fieldName);
  }

  matchSelectOptions(profile) {
    if (!this._cacheValue.matchingSelectOption) {
      this._cacheValue.matchingSelectOption = new WeakMap();
    }

    for (let fieldName in profile) {
      let fieldDetail = this.getFieldDetailByName(fieldName);
      if (!fieldDetail) {
        continue;
      }

      let element = fieldDetail.elementWeakRef.get();
      if (ChromeUtils.getClassName(element) !== "HTMLSelectElement") {
        continue;
      }

      let cache = this._cacheValue.matchingSelectOption.get(element) || {};
      let value = profile[fieldName];
      if (cache[value] && cache[value].get()) {
        continue;
      }

      let option = FormAutofillUtils.findSelectOption(
        element,
        profile,
        fieldName
      );
      if (option) {
        cache[value] = Cu.getWeakReference(option);
        this._cacheValue.matchingSelectOption.set(element, cache);
      } else {
        if (cache[value]) {
          delete cache[value];
          this._cacheValue.matchingSelectOption.set(element, cache);
        }
        // Delete the field so the phishing hint won't treat it as a "also fill"
        // field.
        delete profile[fieldName];
      }
    }
  }

  adaptFieldMaxLength(profile) {
    for (let key in profile) {
      let detail = this.getFieldDetailByName(key);
      if (!detail) {
        continue;
      }

      let element = detail.elementWeakRef.get();
      if (!element) {
        continue;
      }

      let maxLength = element.maxLength;
      if (
        maxLength === undefined ||
        maxLength < 0 ||
        profile[key].toString().length <= maxLength
      ) {
        continue;
      }

      if (maxLength) {
        switch (typeof profile[key]) {
          case "string":
            // If this is an expiration field and our previous
            // adaptations haven't resulted in a string that is
            // short enough to satisfy the field length, and the
            // field is constrained to a length of 5, then we
            // assume it is intended to hold an expiration of the
            // form "MM/YY".
            if (key == "cc-exp" && maxLength == 5) {
              const month2Digits = (
                "0" + profile["cc-exp-month"].toString()
              ).slice(-2);
              const year2Digits = profile["cc-exp-year"].toString().slice(-2);
              profile[key] = `${month2Digits}/${year2Digits}`;
            } else {
              profile[key] = profile[key].substr(0, maxLength);
            }
            break;
          case "number":
            // There's no way to truncate a number smaller than a
            // single digit.
            if (maxLength < 1) {
              maxLength = 1;
            }
            // The only numbers we store are expiration month/year,
            // and if they truncate, we want the final digits, not
            // the initial ones.
            profile[key] = profile[key] % Math.pow(10, maxLength);
            break;
          default:
            log.warn(
              "adaptFieldMaxLength: Don't know how to truncate",
              typeof profile[key],
              profile[key]
            );
        }
      } else {
        delete profile[key];
      }
    }
  }

  getAdaptedProfiles(originalProfiles) {
    for (let profile of originalProfiles) {
      this.applyTransformers(profile);
    }
    return originalProfiles;
  }

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {Object} profile
   *        A profile to be filled in.
   * @returns {boolean}
   *          True if successful, false if failed
   */
  async autofillFields(profile) {
    let focusedDetail = this._focusedDetail;
    if (!focusedDetail) {
      throw new Error("No fieldDetail for the focused input.");
    }

    if (!(await this.prepareFillingProfile(profile))) {
      log.debug("profile cannot be filled", profile);
      return false;
    }
    log.debug("profile in autofillFields:", profile);

    let focusedInput = focusedDetail.elementWeakRef.get();

    this.filledRecordGUID = profile.guid;
    for (let fieldDetail of this.fieldDetails) {
      // Avoid filling field value in the following cases:
      // 1. a non-empty input field for an unfocused input
      // 2. the invalid value set
      // 3. value already chosen in select element

      let element = fieldDetail.elementWeakRef.get();
      if (!element) {
        continue;
      }

      element.previewValue = "";
      // Bug 1687679: Since profile appears to be presentation ready data, we need to utilize the "x-formatted" field
      // that is generated when presentation ready data doesn't fit into the autofilling element.
      // For example, autofilling expiration month into an input element will not work as expected if
      // the month is less than 10, since the input is expected a zero-padded string.
      // See Bug 1722941 for follow up.
      let value =
        profile[`${fieldDetail.fieldName}-formatted`] ||
        profile[fieldDetail.fieldName];

      if (ChromeUtils.getClassName(element) === "HTMLInputElement" && value) {
        // For the focused input element, it will be filled with a valid value
        // anyway.
        // For the others, the fields should be only filled when their values
        // are empty or are the result of an earlier auto-fill.
        if (
          element == focusedInput ||
          (element != focusedInput && !element.value) ||
          fieldDetail.state == FIELD_STATES.AUTO_FILLED
        ) {
          element.focus({ preventScroll: true });
          element.setUserInput(value);
          this._changeFieldState(fieldDetail, FIELD_STATES.AUTO_FILLED);
        }
      } else if (ChromeUtils.getClassName(element) === "HTMLSelectElement") {
        let cache = this._cacheValue.matchingSelectOption.get(element) || {};
        let option = cache[value] && cache[value].get();
        if (!option) {
          continue;
        }
        // Do not change value or dispatch events if the option is already selected.
        // Use case for multiple select is not considered here.
        if (!option.selected) {
          option.selected = true;
          element.focus({ preventScroll: true });
          element.dispatchEvent(
            new element.ownerGlobal.Event("input", { bubbles: true })
          );
          element.dispatchEvent(
            new element.ownerGlobal.Event("change", { bubbles: true })
          );
        }
        // Autofill highlight appears regardless if value is changed or not
        this._changeFieldState(fieldDetail, FIELD_STATES.AUTO_FILLED);
      }
    }
    focusedInput.focus({ preventScroll: true });
    return true;
  }

  /**
   * Populates result to the preview layers with given profile.
   *
   * @param {Object} profile
   *        A profile to be previewed with
   */
  previewFormFields(profile) {
    log.debug("preview profile: ", profile);

    this.preparePreviewProfile(profile);

    for (let fieldDetail of this.fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      let value = profile[fieldDetail.fieldName] || "";

      // Skip the field that is null
      if (!element) {
        continue;
      }

      if (ChromeUtils.getClassName(element) === "HTMLSelectElement") {
        // Unlike text input, select element is always previewed even if
        // the option is already selected.
        if (value) {
          let cache = this._cacheValue.matchingSelectOption.get(element) || {};
          let option = cache[value] && cache[value].get();
          if (option) {
            value = option.text || "";
          } else {
            value = "";
          }
        }
      } else if (element.value) {
        // Skip the field if it already has text entered.
        continue;
      }
      element.previewValue = value;
      this._changeFieldState(
        fieldDetail,
        value ? FIELD_STATES.PREVIEW : FIELD_STATES.NORMAL
      );
    }
  }

  /**
   * Clear preview text and background highlight of all fields.
   */
  clearPreviewedFormFields() {
    log.debug("clear previewed fields in:", this.form);

    for (let fieldDetail of this.fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      if (!element) {
        log.warn(fieldDetail.fieldName, "is unreachable");
        continue;
      }

      element.previewValue = "";

      // We keep the state if this field has
      // already been auto-filled.
      if (fieldDetail.state == FIELD_STATES.AUTO_FILLED) {
        continue;
      }

      this._changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
    }
  }

  /**
   * Clear value and highlight style of all filled fields.
   */
  clearPopulatedForm() {
    for (let fieldDetail of this.fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      if (!element) {
        log.warn(fieldDetail.fieldName, "is unreachable");
        continue;
      }

      // Only reset value for input element.
      if (
        fieldDetail.state == FIELD_STATES.AUTO_FILLED &&
        ChromeUtils.getClassName(element) === "HTMLInputElement"
      ) {
        element.setUserInput("");
      }
    }
  }

  /**
   * Change the state of a field to correspond with different presentations.
   *
   * @param {Object} fieldDetail
   *        A fieldDetail of which its element is about to update the state.
   * @param {string} nextState
   *        Used to determine the next state
   */
  _changeFieldState(fieldDetail, nextState) {
    let element = fieldDetail.elementWeakRef.get();

    if (!element) {
      log.warn(fieldDetail.fieldName, "is unreachable while changing state");
      return;
    }
    if (!(nextState in this._FIELD_STATE_ENUM)) {
      log.warn(
        fieldDetail.fieldName,
        "is trying to change to an invalid state"
      );
      return;
    }
    if (fieldDetail.state == nextState) {
      return;
    }

    let nextStateValue = null;
    for (let [state, mmStateValue] of Object.entries(this._FIELD_STATE_ENUM)) {
      // The NORMAL state is simply the absence of other manually
      // managed states so we never need to add or remove it.
      if (!mmStateValue) {
        continue;
      }

      if (state == nextState) {
        nextStateValue = mmStateValue;
      } else {
        this.winUtils.removeManuallyManagedState(element, mmStateValue);
      }
    }

    if (nextStateValue) {
      this.winUtils.addManuallyManagedState(element, nextStateValue);
    }

    if (nextState == FIELD_STATES.AUTO_FILLED) {
      element.addEventListener("input", this, { mozSystemGroup: true });
    }

    fieldDetail.state = nextState;
  }

  resetFieldStates() {
    for (let fieldDetail of this.fieldDetails) {
      const element = fieldDetail.elementWeakRef.get();
      element.removeEventListener("input", this, { mozSystemGroup: true });
      this._changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
    }
    this.filledRecordGUID = null;
  }

  isFilled() {
    return !!this.filledRecordGUID;
  }

  /**
   * Return the record that is converted from `fieldDetails` and only valid
   * form record is included.
   *
   * @returns {Object|null}
   *          A record object consists of three properties:
   *            - guid: The id of the previously-filled profile or null if omitted.
   *            - record: A valid record converted from details with trimmed result.
   *            - untouchedFields: Fields that aren't touched after autofilling.
   *          Return `null` for any uncreatable or invalid record.
   */
  createRecord() {
    let details = this.fieldDetails;
    if (!this.isEnabled() || !details || !details.length) {
      return null;
    }

    let data = {
      guid: this.filledRecordGUID,
      record: {},
      untouchedFields: [],
    };
    if (this.flowId) {
      data.flowId = this.flowId;
    }

    details.forEach(detail => {
      let element = detail.elementWeakRef.get();
      // Remove the unnecessary spaces
      let value = element && element.value.trim();
      value = this.computeFillingValue(value, detail, element);

      if (!value || value.length > FormAutofillUtils.MAX_FIELD_VALUE_LENGTH) {
        // Keep the property and preserve more information for updating
        data.record[detail.fieldName] = "";
        return;
      }

      data.record[detail.fieldName] = value;

      if (detail.state == FIELD_STATES.AUTO_FILLED) {
        data.untouchedFields.push(detail.fieldName);
      }
    });

    this.normalizeCreatingRecord(data);

    if (!this.isRecordCreatable(data.record)) {
      return null;
    }

    return data;
  }

  handleEvent(event) {
    switch (event.type) {
      case "input": {
        if (!event.isTrusted) {
          return;
        }
        const target = event.target;
        const targetFieldDetail = this.getFieldDetailByElement(target);
        const isCreditCardField = FormAutofillUtils.isCreditCardField(
          targetFieldDetail.fieldName
        );

        // If the user manually blanks a credit card field, then
        // we want the popup to be activated.
        if (
          ChromeUtils.getClassName(target) !== "HTMLSelectElement" &&
          isCreditCardField &&
          target.value === ""
        ) {
          formFillController.showPopup();
        }

        if (targetFieldDetail.state == FIELD_STATES.NORMAL) {
          return;
        }

        this._changeFieldState(targetFieldDetail, FIELD_STATES.NORMAL);

        if (isCreditCardField) {
          Services.telemetry.recordEvent(
            "creditcard",
            "filled_modified",
            "cc_form",
            this.flowId,
            {
              field_name: targetFieldDetail.fieldName,
            }
          );
        }

        let isAutofilled = false;
        let dimFieldDetails = [];
        for (const fieldDetail of this.fieldDetails) {
          const element = fieldDetail.elementWeakRef.get();

          if (ChromeUtils.getClassName(element) === "HTMLSelectElement") {
            // Dim fields are those we don't attempt to revert their value
            // when clear the target set, such as <select>.
            dimFieldDetails.push(fieldDetail);
          } else {
            isAutofilled |= fieldDetail.state == FIELD_STATES.AUTO_FILLED;
          }
        }
        if (!isAutofilled) {
          // Restore the dim fields to initial state as well once we knew
          // that user had intention to clear the filled form manually.
          for (const fieldDetail of dimFieldDetails) {
            this._changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
          }
          this.filledRecordGUID = null;
        }
        break;
      }
    }
  }
}

class FormAutofillAddressSection extends FormAutofillSection {
  constructor(fieldDetails, winUtils) {
    super(fieldDetails, winUtils);

    this._cacheValue.oneLineStreetAddress = null;
  }

  isValidSection() {
    return (
      this.fieldDetails.length >= FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD
    );
  }

  isEnabled() {
    return FormAutofill.isAutofillAddressesEnabled;
  }

  isRecordCreatable(record) {
    if (
      record.country &&
      !FormAutofill.supportedCountries.includes(record.country)
    ) {
      // We don't want to save data in the wrong fields due to not having proper
      // heuristic regexes in countries we don't yet support.
      log.warn("isRecordCreatable: Country not supported:", record.country);
      return false;
    }

    let hasName = 0;
    let length = 0;
    for (let key of Object.keys(record)) {
      if (!record[key]) {
        continue;
      }
      if (FormAutofillUtils.getCategoryFromFieldName(key) == "name") {
        hasName = 1;
        continue;
      }
      length++;
    }
    return length + hasName >= FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD;
  }

  _getOneLineStreetAddress(address) {
    if (!this._cacheValue.oneLineStreetAddress) {
      this._cacheValue.oneLineStreetAddress = {};
    }
    if (!this._cacheValue.oneLineStreetAddress[address]) {
      this._cacheValue.oneLineStreetAddress[
        address
      ] = FormAutofillUtils.toOneLineAddress(address);
    }
    return this._cacheValue.oneLineStreetAddress[address];
  }

  addressTransformer(profile) {
    if (profile["street-address"]) {
      // "-moz-street-address-one-line" is used by the labels in
      // ProfileAutoCompleteResult.
      profile["-moz-street-address-one-line"] = this._getOneLineStreetAddress(
        profile["street-address"]
      );
      let streetAddressDetail = this.getFieldDetailByName("street-address");
      if (
        streetAddressDetail &&
        ChromeUtils.getClassName(streetAddressDetail.elementWeakRef.get()) ===
          "HTMLInputElement"
      ) {
        profile["street-address"] = profile["-moz-street-address-one-line"];
      }

      let waitForConcat = [];
      for (let f of ["address-line3", "address-line2", "address-line1"]) {
        waitForConcat.unshift(profile[f]);
        if (this.getFieldDetailByName(f)) {
          if (waitForConcat.length > 1) {
            profile[f] = FormAutofillUtils.toOneLineAddress(waitForConcat);
          }
          waitForConcat = [];
        }
      }
    }
  }

  /**
   * Replace tel with tel-national if tel violates the input element's
   * restriction.
   * @param {Object} profile
   *        A profile to be converted.
   */
  telTransformer(profile) {
    if (!profile.tel || !profile["tel-national"]) {
      return;
    }

    let detail = this.getFieldDetailByName("tel");
    if (!detail) {
      return;
    }

    let element = detail.elementWeakRef.get();
    let _pattern;
    let testPattern = str => {
      if (!_pattern) {
        // The pattern has to match the entire value.
        _pattern = new RegExp("^(?:" + element.pattern + ")$", "u");
      }
      return _pattern.test(str);
    };
    if (element.pattern) {
      if (testPattern(profile.tel)) {
        return;
      }
    } else if (element.maxLength) {
      if (
        detail._reason == "autocomplete" &&
        profile.tel.length <= element.maxLength
      ) {
        return;
      }
    }

    if (detail._reason != "autocomplete") {
      // Since we only target people living in US and using en-US websites in
      // MVP, it makes more sense to fill `tel-national` instead of `tel`
      // if the field is identified by heuristics and no other clues to
      // determine which one is better.
      // TODO: [Bug 1407545] This should be improved once more countries are
      // supported.
      profile.tel = profile["tel-national"];
    } else if (element.pattern) {
      if (testPattern(profile["tel-national"])) {
        profile.tel = profile["tel-national"];
      }
    } else if (element.maxLength) {
      if (profile["tel-national"].length <= element.maxLength) {
        profile.tel = profile["tel-national"];
      }
    }
  }

  /*
   * Apply all address related transformers.
   *
   * @param {Object} profile
   *        A profile for adjusting address related value.
   * @override
   */
  applyTransformers(profile) {
    this.addressTransformer(profile);
    this.telTransformer(profile);
    this.matchSelectOptions(profile);
    this.adaptFieldMaxLength(profile);
  }

  computeFillingValue(value, fieldDetail, element) {
    // Try to abbreviate the value of select element.
    if (
      fieldDetail.fieldName == "address-level1" &&
      ChromeUtils.getClassName(element) === "HTMLSelectElement"
    ) {
      // Don't save the record when the option value is empty *OR* there
      // are multiple options being selected. The empty option is usually
      // assumed to be default along with a meaningless text to users.
      if (!value || element.selectedOptions.length != 1) {
        // Keep the property and preserve more information for address updating
        value = "";
      } else {
        let text = element.selectedOptions[0].text.trim();
        value =
          FormAutofillUtils.getAbbreviatedSubregionName([value, text]) || text;
      }
    }
    return value;
  }

  normalizeCreatingRecord(address) {
    if (!address) {
      return;
    }

    // Normalize Country
    if (address.record.country) {
      let detail = this.getFieldDetailByName("country");
      // Try identifying country field aggressively if it doesn't come from
      // @autocomplete.
      if (detail._reason != "autocomplete") {
        let countryCode = FormAutofillUtils.identifyCountryCode(
          address.record.country
        );
        if (countryCode) {
          address.record.country = countryCode;
        }
      }
    }

    // Normalize Tel
    FormAutofillUtils.compressTel(address.record);
    if (address.record.tel) {
      let allTelComponentsAreUntouched = Object.keys(address.record)
        .filter(
          field => FormAutofillUtils.getCategoryFromFieldName(field) == "tel"
        )
        .every(field => address.untouchedFields.includes(field));
      if (allTelComponentsAreUntouched) {
        // No need to verify it if none of related fields are modified after autofilling.
        if (!address.untouchedFields.includes("tel")) {
          address.untouchedFields.push("tel");
        }
      } else {
        let strippedNumber = address.record.tel.replace(/[\s\(\)-]/g, "");

        // Remove "tel" if it contains invalid characters or the length of its
        // number part isn't between 5 and 15.
        // (The maximum length of a valid number in E.164 format is 15 digits
        //  according to https://en.wikipedia.org/wiki/E.164 )
        if (!/^(\+?)[\da-zA-Z]{5,15}$/.test(strippedNumber)) {
          address.record.tel = "";
        }
      }
    }
  }
}

class FormAutofillCreditCardSection extends FormAutofillSection {
  /**
   * Credit Card Section Constructor
   *
   * @param {Object} fieldDetails
   *        The fieldDetail objects for the fields in this section
   * @param {Object} winUtils
   *        A WindowUtils reference for the Window the section appears in
   * @param {Object} handler
   *        The FormAutofillHandler responsible for this section
   */
  constructor(fieldDetails, winUtils, handler) {
    super(fieldDetails, winUtils);

    this.handler = handler;

    // Identifier used to correlate events relating to the same form
    this.flowId = gUUIDGenerator.generateUUID().toString();
    log.debug("Creating new credit card section with flowId =", this.flowId);

    if (!this.isValidSection()) {
      return;
    }

    // Record which fields could be identified
    let identified = new Set();
    fieldDetails.forEach(detail => identified.add(detail.fieldName));
    Services.telemetry.recordEvent(
      "creditcard",
      "detected",
      "cc_form",
      this.flowId,
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

    // Check whether the section is in an <iframe>; and, if so,
    // watch for the <iframe> to pagehide.
    if (handler.window.location != handler.window.parent?.location) {
      log.debug(
        "Credit card form is in an iframe -- watching for pagehide",
        fieldDetails
      );
      handler.window.addEventListener(
        "pagehide",
        this._handlePageHide.bind(this)
      );
    }
  }

  _handlePageHide(event) {
    this.handler.window.removeEventListener(
      "pagehide",
      this._handlePageHide.bind(this)
    );
    log.debug("Credit card subframe is pagehideing", this.handler.form);
    this.handler.onFormSubmitted();
  }

  isValidSection() {
    let ccNumberReason = "";
    let hasCCNumber = false;
    let hasExpiryDate = false;
    let hasCCName = false;

    for (let detail of this.fieldDetails) {
      switch (detail.fieldName) {
        case "cc-number":
          hasCCNumber = true;
          ccNumberReason = detail._reason;
          break;
        case "cc-name":
        case "cc-given-name":
        case "cc-additional-name":
        case "cc-family-name":
          hasCCName = true;
          break;
        case "cc-exp":
        case "cc-exp-month":
        case "cc-exp-year":
          hasExpiryDate = true;
          break;
      }
    }

    return (
      hasCCNumber &&
      (ccNumberReason == "autocomplete" || hasExpiryDate || hasCCName)
    );
  }

  isEnabled() {
    return FormAutofill.isAutofillCreditCardsEnabled;
  }

  isRecordCreatable(record) {
    return (
      record["cc-number"] && FormAutofillUtils.isCCNumber(record["cc-number"])
    );
  }

  creditCardExpDateTransformer(profile) {
    if (!profile["cc-exp"]) {
      return;
    }

    let detail = this.getFieldDetailByName("cc-exp");
    if (!detail) {
      return;
    }

    let element = detail.elementWeakRef.get();
    if (element.tagName != "INPUT" || !element.placeholder) {
      return;
    }

    let result,
      ccExpMonth = profile["cc-exp-month"],
      ccExpYear = profile["cc-exp-year"],
      placeholder = element.placeholder;

    // Bug 1687681: This is a short term fix to other locales having
    // different characters to represent year.
    // For example, FR locales may use "A" instead of "Y" to represent year
    // This approach will not scale well and should be investigated in a follow up bug.
    let monthChars = "m";
    let yearChars = "ya";

    let monthFirstCheck = new RegExp(
      "(?:\\b|^)((?:[" +
        monthChars +
        "]{2}){1,2})\\s*([\\-/])\\s*((?:[" +
        yearChars +
        "]{2}){1,2})(?:\\b|$)",
      "i"
    );

    // If the month first check finds a result, where placeholder is "mm - yyyy",
    // the result will be structured as such: ["mm - yyyy", "mm", "-", "yyyy"]
    result = monthFirstCheck.exec(placeholder);
    if (result) {
      profile["cc-exp"] =
        ccExpMonth.toString().padStart(result[1].length, "0") +
        result[2] +
        ccExpYear.toString().substr(-1 * result[3].length);
      return;
    }

    let yearFirstCheck = new RegExp(
      "(?:\\b|^)((?:[" +
      yearChars +
      "]{2}){1,2})\\s*([\\-/])\\s*((?:[" + // either one or two counts of 'yy' or 'aa' sequence
        monthChars +
        "]){1,2})(?:\\b|$)",
      "i" // either one or two counts of a 'm' sequence
    );

    // If the year first check finds a result, where placeholder is "yyyy mm",
    // the result will be structured as such: ["yyyy mm", "yyyy", " ", "mm"]
    result = yearFirstCheck.exec(placeholder);

    if (result) {
      profile["cc-exp"] =
        ccExpYear.toString().substr(-1 * result[1].length) +
        result[2] +
        ccExpMonth.toString().padStart(result[3].length, "0");
    }
  }

  creditCardExpMonthTransformer(profile) {
    if (!profile["cc-exp-month"]) {
      return;
    }

    let detail = this.getFieldDetailByName("cc-exp-month");
    if (!detail) {
      return;
    }

    let element = detail.elementWeakRef.get();

    // If the expiration month element is an input,
    // then we examine any placeholder to see if we should format the expiration month
    // as a zero padded string in order to autofill correctly.
    if (element.tagName === "INPUT") {
      let placeholder = element.placeholder;

      // Checks for 'MM' placeholder and converts the month to a two digit string.
      let result = /(?<!.)mm(?!.)/i.test(placeholder);
      if (result) {
        profile["cc-exp-month-formatted"] = profile["cc-exp-month"]
          .toString()
          .padStart(2, "0");
      }
    }
  }

  async _decrypt(cipherText, reauth) {
    // Get the window for the form field.
    let window;
    for (let fieldDetail of this.fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      if (element) {
        window = element.ownerGlobal;
        break;
      }
    }
    if (!window) {
      return null;
    }

    let actor = window.windowGlobalChild.getActor("FormAutofill");
    return actor.sendQuery("FormAutofill:GetDecryptedString", {
      cipherText,
      reauth,
    });
  }

  /*
   * Apply all credit card related transformers.
   *
   * @param {Object} profile
   *        A profile for adjusting credit card related value.
   * @override
   */
  applyTransformers(profile) {
    this.matchSelectOptions(profile);
    this.creditCardExpDateTransformer(profile);
    this.creditCardExpMonthTransformer(profile);
    this.adaptFieldMaxLength(profile);
  }

  computeFillingValue(value, fieldDetail, element) {
    if (
      fieldDetail.fieldName != "cc-type" ||
      ChromeUtils.getClassName(element) !== "HTMLSelectElement"
    ) {
      return value;
    }

    if (CreditCard.isValidNetwork(value)) {
      return value;
    }

    // Don't save the record when the option value is empty *OR* there
    // are multiple options being selected. The empty option is usually
    // assumed to be default along with a meaningless text to users.
    if (value && element.selectedOptions.length == 1) {
      let selectedOption = element.selectedOptions[0];
      let networkType =
        CreditCard.getNetworkFromName(selectedOption.text) ??
        CreditCard.getNetworkFromName(selectedOption.value);
      if (networkType) {
        return networkType;
      }
    }
    // If we couldn't match the value to any network, we'll
    // strip this field when submitting.
    return value;
  }

  /**
   * Customize for previewing prorifle.
   *
   * @param {Object} profile
   *        A profile for pre-processing before previewing values.
   * @override
   */
  preparePreviewProfile(profile) {
    // Always show the decrypted credit card number when Master Password is
    // disabled.
    if (profile["cc-number-decrypted"]) {
      profile["cc-number"] = profile["cc-number-decrypted"];
    }
  }

  /**
   * Customize for filling prorifle.
   *
   * @param {Object} profile
   *        A profile for pre-processing before filling values.
   * @returns {boolean} Whether the profile should be filled.
   * @override
   */
  async prepareFillingProfile(profile) {
    // Prompt the OS login dialog to get the decrypted credit
    // card number.
    if (profile["cc-number-encrypted"]) {
      let decrypted = await this._decrypt(
        profile["cc-number-encrypted"],
        reauthPasswordPromptMessage
      );

      if (!decrypted) {
        // Early return if the decrypted is empty or undefined
        return false;
      }

      profile["cc-number"] = decrypted;
    }
    return true;
  }

  async autofillFields(profile) {
    if (!(await super.autofillFields(profile))) {
      return false;
    }

    // Calculate values for telemetry
    let extra = {
      cc_name: "unavailable",
      cc_number: "unavailable",
      cc_exp: "unavailable",
    };

    for (let fieldDetail of this.fieldDetails) {
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
      this.flowId,
      extra
    );
    return true;
  }
}

/**
 * Handles profile autofill for a DOM Form element.
 */
class FormAutofillHandler {
  /**
   * Initialize the form from `FormLike` object to handle the section or form
   * operations.
   * @param {FormLike} form Form that need to be auto filled
   * @param {function} onFormSubmitted Function that can be invoked
   *                   to simulate form submission. Function is passed
   *                   three arguments: (1) a FormLike for the form being
   *                   submitted, (2) the corresponding Window, and (3) the
   *                   responsible FormAutofillHandler.
   */
  constructor(form, onFormSubmitted = () => {}) {
    this._updateForm(form);

    /**
     * The window to which this form belongs
     */
    this.window = this.form.rootElement.ownerGlobal;

    /**
     * A WindowUtils reference of which Window the form belongs
     */
    this.winUtils = this.window.windowUtils;

    /**
     * Time in milliseconds since epoch when a user started filling in the form.
     */
    this.timeStartedFillingMS = null;

    /**
     * This function is used if the form handler (or one of its sections)
     * determines that it needs to act as if the form had been submitted.
     */
    this.onFormSubmitted = () => {
      onFormSubmitted(this.form, this.window, this);
    };
  }

  set focusedInput(element) {
    let section = this._sectionCache.get(element);
    if (!section) {
      section = this.sections.find(s => s.getFieldDetailByElement(element));
      this._sectionCache.set(element, section);
    }

    this._focusedSection = section;

    if (section) {
      section.focusedInput = element;
    }
  }

  get activeSection() {
    return this._focusedSection;
  }

  /**
   * Check the form is necessary to be updated. This function should be able to
   * detect any changes including all control elements in the form.
   * @param {HTMLElement} element The element supposed to be in the form.
   * @returns {boolean} FormAutofillHandler.form is updated or not.
   */
  updateFormIfNeeded(element) {
    // When the following condition happens, FormAutofillHandler.form should be
    // updated:
    // * The count of form controls is changed.
    // * When the element can not be found in the current form.
    //
    // However, we should improve the function to detect the element changes.
    // e.g. a tel field is changed from type="hidden" to type="tel".

    let _formLike;
    let getFormLike = () => {
      if (!_formLike) {
        _formLike = FormLikeFactory.createFromField(element);
      }
      return _formLike;
    };

    let currentForm = element.form;
    if (!currentForm) {
      currentForm = getFormLike();
    }

    if (currentForm.elements.length != this.form.elements.length) {
      log.debug("The count of form elements is changed.");
      this._updateForm(getFormLike());
      return true;
    }

    if (!this.form.elements.includes(element)) {
      log.debug("The element can not be found in the current form.");
      this._updateForm(getFormLike());
      return true;
    }

    return false;
  }

  /**
   * Update the form with a new FormLike, and the related fields should be
   * updated or clear to ensure the data consistency.
   * @param {FormLike} form a new FormLike to replace the original one.
   */
  _updateForm(form) {
    /**
     * DOM Form element to which this object is attached.
     */
    this.form = form;

    /**
     * Array of collected data about relevant form fields.  Each item is an object
     * storing the identifying details of the field and a reference to the
     * originally associated element from the form.
     *
     * The "section", "addressType", "contactType", and "fieldName" values are
     * used to identify the exact field when the serializable data is received
     * from the backend.  There cannot be multiple fields which have
     * the same exact combination of these values.
     *
     * A direct reference to the associated element cannot be sent to the user
     * interface because processing may be done in the parent process.
     */
    this.fieldDetails = null;

    this.sections = [];
    this._sectionCache = new WeakMap();
  }

  /**
   * Set fieldDetails from the form about fields that can be autofilled.
   *
   * @param {boolean} allowDuplicates
   *        true to remain any duplicated field details otherwise to remove the
   *        duplicated ones.
   * @returns {Array} The valid address and credit card details.
   */
  collectFormFields(allowDuplicates = false) {
    let sections = FormAutofillHeuristics.getFormInfo(
      this.form,
      allowDuplicates
    );
    let allValidDetails = [];
    for (let { fieldDetails, type } of sections) {
      let section;
      if (type == FormAutofillUtils.SECTION_TYPES.ADDRESS) {
        section = new FormAutofillAddressSection(fieldDetails, this.winUtils);
      } else if (type == FormAutofillUtils.SECTION_TYPES.CREDIT_CARD) {
        section = new FormAutofillCreditCardSection(
          fieldDetails,
          this.winUtils,
          this
        );
      } else {
        throw new Error("Unknown field type.");
      }
      this.sections.push(section);
      allValidDetails.push(...section.fieldDetails);
    }

    for (let detail of allValidDetails) {
      let input = detail.elementWeakRef.get();
      if (!input) {
        continue;
      }
      input.addEventListener("input", this, { mozSystemGroup: true });
    }

    this.fieldDetails = allValidDetails;
    return allValidDetails;
  }

  _hasFilledSection() {
    return this.sections.some(section => section.isFilled());
  }

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {Object} profile
   *        A profile to be filled in.
   */
  async autofillFormFields(profile) {
    let noFilledSectionsPreviously = !this._hasFilledSection();
    await this.activeSection.autofillFields(profile);

    const onChangeHandler = e => {
      if (!e.isTrusted) {
        return;
      }
      if (e.type == "reset") {
        for (let section of this.sections) {
          section.resetFieldStates();
        }
      }
      // Unregister listeners once no field is in AUTO_FILLED state.
      if (!this._hasFilledSection()) {
        this.form.rootElement.removeEventListener("input", onChangeHandler, {
          mozSystemGroup: true,
        });
        this.form.rootElement.removeEventListener("reset", onChangeHandler, {
          mozSystemGroup: true,
        });
      }
    };

    if (noFilledSectionsPreviously) {
      // Handle the highlight style resetting caused by user's correction afterward.
      log.debug("register change handler for filled form:", this.form);
      this.form.rootElement.addEventListener("input", onChangeHandler, {
        mozSystemGroup: true,
      });
      this.form.rootElement.addEventListener("reset", onChangeHandler, {
        mozSystemGroup: true,
      });
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "input":
        if (!event.isTrusted) {
          return;
        }

        for (let detail of this.fieldDetails) {
          let input = detail.elementWeakRef.get();
          if (!input) {
            continue;
          }
          input.removeEventListener("input", this, { mozSystemGroup: true });
        }
        this.timeStartedFillingMS = Date.now();
        break;
    }
  }

  /**
   * Collect the filled sections within submitted form and convert all the valid
   * field data into multiple records.
   *
   * @returns {Object} records
   *          {Array.<Object>} records.address
   *          {Array.<Object>} records.creditCard
   */
  createRecords() {
    const records = {
      address: [],
      creditCard: [],
    };

    for (const section of this.sections) {
      const secRecord = section.createRecord();
      if (!secRecord) {
        continue;
      }
      if (section instanceof FormAutofillAddressSection) {
        records.address.push(secRecord);
      } else if (section instanceof FormAutofillCreditCardSection) {
        records.creditCard.push(secRecord);
      } else {
        throw new Error("Unknown section type");
      }
    }
    log.debug("Create records:", records);
    return records;
  }
}
