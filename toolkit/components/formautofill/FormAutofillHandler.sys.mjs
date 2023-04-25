/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Defines a handler object to represent forms that autofill can handle.
 */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { FormAutofill } from "resource://autofill/FormAutofill.sys.mjs";
import { FormAutofillUtils } from "resource://autofill/FormAutofillUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/CreditCard.sys.mjs",
  FormAutofillHeuristics: "resource://autofill/FormAutofillHeuristics.sys.mjs",
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AutofillTelemetry: "resource://autofill/AutofillTelemetry.jsm",
});

const formFillController = Cc[
  "@mozilla.org/satchel/form-fill-controller;1"
].getService(Ci.nsIFormFillController);

XPCOMUtils.defineLazyGetter(lazy, "reauthPasswordPromptMessage", () => {
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

XPCOMUtils.defineLazyGetter(lazy, "log", () =>
  FormAutofill.defineLogGetter(lazy, "FormAutofillHandler")
);

const { FIELD_STATES } = FormAutofillUtils;

class FormAutofillSection {
  #focusedInput = null;

  constructor(fieldDetails, handler) {
    this.fieldDetails = fieldDetails;
    this.handler = handler;
    this.filledRecordGUID = null;

    if (!this.isValidSection()) {
      this.fieldDetails = [];
      lazy.log.debug(
        `Ignoring ${this.constructor.name} related fields since it is an invalid section`
      );
    }

    this._cacheValue = {
      allFieldNames: null,
      matchingSelectOption: null,
    };

    // Identifier used to correlate events relating to the same form
    this.flowId = Services.uuid.generateUUID().toString();
    lazy.log.debug(
      "Creating new credit card section with flowId =",
      this.flowId
    );
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
    throw new TypeError("isRecordCreatable method must be overridden");
  }

  /*
   * Override this method if any data for `createRecord` is needed to be
   * normalized before submitting the record.
   *
   * @param {Object} profile
   *        A record for normalization.
   */
  createNormalizedRecord(data) {}

  /**
   * Override this method if the profile is needed to apply some transformers.
   *
   * @param {object} profile
   *        A profile should be converted based on the specific requirement.
   */
  applyTransformers(profile) {}

  /**
   * Override this method if the profile is needed to be customized for
   * previewing values.
   *
   * @param {object} profile
   *        A profile for pre-processing before previewing values.
   */
  preparePreviewProfile(profile) {}

  /**
   * Override this method if the profile is needed to be customized for filling
   * values.
   *
   * @param {object} profile
   *        A profile for pre-processing before filling values.
   * @returns {boolean} Whether the profile should be filled.
   */
  async prepareFillingProfile(profile) {
    return true;
  }

  /**
   * Override this method if the profile is needed to be customized for filling
   * values.
   *
   * @param {object} fieldDetail A fieldDetail of the related element.
   * @param {object} profile The profile to fill.
   * @returns {string} The value to fill for the given field.
   */
  getFilledValueFromProfile(fieldDetail, profile) {
    return (
      profile[`${fieldDetail.fieldName}-formatted`] ||
      profile[fieldDetail.fieldName]
    );
  }

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
    this.#focusedInput = element;
  }

  getFieldDetailByElement(element) {
    return this.fieldDetails.find(
      detail => detail.elementWeakRef.get() == element
    );
  }

  getFieldDetailByName(fieldName) {
    return this.fieldDetails.find(detail => detail.fieldName == fieldName);
  }

  get allFieldNames() {
    if (!this._cacheValue.allFieldNames) {
      this._cacheValue.allFieldNames = this.fieldDetails.map(
        record => record.fieldName
      );
    }
    return this._cacheValue.allFieldNames;
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
      if (!HTMLSelectElement.isInstance(element)) {
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
            } else if (key == "cc-number") {
              // We want to show the last four digits of credit card so that
              // the masked credit card previews correctly and appears correctly
              // in the autocomplete menu
              profile[key] = profile[key].substr(
                profile[key].length - maxLength
              );
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
        }
      } else {
        delete profile[key];
        delete profile[`${key}-formatted`];
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
   * @param {object} profile
   *        A profile to be filled in.
   * @returns {boolean}
   *          True if successful, false if failed
   */
  async autofillFields(profile) {
    if (!this.#focusedInput) {
      throw new Error("No focused input.");
    }

    const focusedDetail = this.getFieldDetailByElement(this.#focusedInput);
    if (!focusedDetail) {
      throw new Error("No fieldDetail for the focused input.");
    }

    if (!(await this.prepareFillingProfile(profile))) {
      lazy.log.debug("profile cannot be filled");
      return false;
    }

    this.filledRecordGUID = profile.guid;
    for (const fieldDetail of this.fieldDetails) {
      // Avoid filling field value in the following cases:
      // 1. a non-empty input field for an unfocused input
      // 2. the invalid value set
      // 3. value already chosen in select element

      const element = fieldDetail.elementWeakRef.get();
      // Skip the field if it is null or readonly or disabled
      if (!FormAutofillUtils.isFieldAutofillable(element)) {
        continue;
      }

      element.previewValue = "";
      // Bug 1687679: Since profile appears to be presentation ready data, we need to utilize the "x-formatted" field
      // that is generated when presentation ready data doesn't fit into the autofilling element.
      // For example, autofilling expiration month into an input element will not work as expected if
      // the month is less than 10, since the input is expected a zero-padded string.
      // See Bug 1722941 for follow up.
      const value = this.getFilledValueFromProfile(fieldDetail, profile);

      if (HTMLInputElement.isInstance(element) && value) {
        // For the focused input element, it will be filled with a valid value
        // anyway.
        // For the others, the fields should be only filled when their values are empty
        // or their values are equal to the site prefill value
        // or are the result of an earlier auto-fill.
        if (
          element == this.#focusedInput ||
          (element != this.#focusedInput &&
            (!element.value || element.value == element.defaultValue)) ||
          this.handler.getFilledStateByElement(element) ==
            FIELD_STATES.AUTO_FILLED
        ) {
          element.focus({ preventScroll: true });
          element.setUserInput(value);
          this.handler.changeFieldState(fieldDetail, FIELD_STATES.AUTO_FILLED);
        }
      } else if (HTMLSelectElement.isInstance(element)) {
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
          // Set the value of the select element so that web event handlers can react accordingly
          element.value = option.value;
          element.dispatchEvent(
            new element.ownerGlobal.Event("input", { bubbles: true })
          );
          element.dispatchEvent(
            new element.ownerGlobal.Event("change", { bubbles: true })
          );
        }
        // Autofill highlight appears regardless if value is changed or not
        this.handler.changeFieldState(fieldDetail, FIELD_STATES.AUTO_FILLED);
      }
    }
    this.#focusedInput.focus({ preventScroll: true });

    lazy.AutofillTelemetry.recordFormInteractionEvent("filled", this, {
      profile,
    });

    return true;
  }

  /**
   * Populates result to the preview layers with given profile.
   *
   * @param {object} profile
   *        A profile to be previewed with
   */
  previewFormFields(profile) {
    this.preparePreviewProfile(profile);

    for (const fieldDetail of this.fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      // Skip the field if it is null or readonly or disabled
      if (!FormAutofillUtils.isFieldAutofillable(element)) {
        continue;
      }

      let value =
        profile[`${fieldDetail.fieldName}-formatted`] ||
        profile[fieldDetail.fieldName] ||
        "";
      if (HTMLSelectElement.isInstance(element)) {
        // Unlike text input, select element is always previewed even if
        // the option is already selected.
        if (value) {
          const cache =
            this._cacheValue.matchingSelectOption.get(element) ?? {};
          const option = cache[value]?.get();
          value = option?.text ?? "";
        }
      } else if (element.value && element.value != element.defaultValue) {
        // Skip the field if the user has already entered text and that text is not the site prefilled value.
        continue;
      }
      element.previewValue = value;
      this.handler.changeFieldState(
        fieldDetail,
        value ? FIELD_STATES.PREVIEW : FIELD_STATES.NORMAL
      );
    }
  }

  /**
   * Clear a previously autofilled field in this section
   */
  clearFilled(fieldDetail) {
    lazy.AutofillTelemetry.recordFormInteractionEvent("filled_modified", this, {
      fieldName: fieldDetail.fieldName,
    });

    let isAutofilled = false;
    const dimFieldDetails = [];
    for (const fieldDetail of this.fieldDetails) {
      const element = fieldDetail.elementWeakRef.get();

      if (HTMLSelectElement.isInstance(element)) {
        // Dim fields are those we don't attempt to revert their value
        // when clear the target set, such as <select>.
        dimFieldDetails.push(fieldDetail);
      } else {
        isAutofilled |=
          this.handler.getFilledStateByElement(element) ==
          FIELD_STATES.AUTO_FILLED;
      }
    }
    if (!isAutofilled) {
      // Restore the dim fields to initial state as well once we knew
      // that user had intention to clear the filled form manually.
      for (const fieldDetail of dimFieldDetails) {
        // If we can't find a selected option, then we should just reset to the first option's value
        let element = fieldDetail.elementWeakRef.get();
        this._resetSelectElementValue(element);
        this.handler.changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
      }
      this.filledRecordGUID = null;
    }
  }

  /**
   * Clear preview text and background highlight of all fields.
   */
  clearPreviewedFormFields() {
    lazy.log.debug("clear previewed fields");

    for (const fieldDetail of this.fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      if (!element) {
        lazy.log.warn(fieldDetail.fieldName, "is unreachable");
        continue;
      }

      element.previewValue = "";

      // We keep the state if this field has
      // already been auto-filled.
      if (
        this.handler.getFilledStateByElement(element) ==
        FIELD_STATES.AUTO_FILLED
      ) {
        continue;
      }

      this.handler.changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
    }
  }

  /**
   * Clear value and highlight style of all filled fields.
   */
  clearPopulatedForm() {
    for (let fieldDetail of this.fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      if (!element) {
        lazy.log.warn(fieldDetail.fieldName, "is unreachable");
        continue;
      }

      if (
        this.handler.getFilledStateByElement(element) ==
        FIELD_STATES.AUTO_FILLED
      ) {
        if (HTMLInputElement.isInstance(element)) {
          element.setUserInput("");
        } else if (HTMLSelectElement.isInstance(element)) {
          // If we can't find a selected option, then we should just reset to the first option's value
          this._resetSelectElementValue(element);
        }
      }
    }
  }

  resetFieldStates() {
    for (const fieldDetail of this.fieldDetails) {
      const element = fieldDetail.elementWeakRef.get();
      element.removeEventListener("input", this, { mozSystemGroup: true });
      this.handler.changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
    }
    this.filledRecordGUID = null;
  }

  isFilled() {
    return !!this.filledRecordGUID;
  }

  /**
   *  Condenses multiple credit card number fields into one fieldDetail
   *  in order to submit the credit card record correctly.
   *
   * @param {Array.<object>} condensedDetails
   *  An array of fieldDetails
   * @memberof FormAutofillSection
   */
  _condenseMultipleCCNumberFields(condensedDetails) {
    let countOfCCNumbers = 0;
    // We ignore the cases where there are more than or less than four credit card number
    // fields in a form as this is not a valid case for filling the credit card number.
    for (let i = condensedDetails.length - 1; i >= 0; i--) {
      if (condensedDetails[i].fieldName == "cc-number") {
        countOfCCNumbers++;
        if (countOfCCNumbers == 4) {
          countOfCCNumbers = 0;
          condensedDetails[i].fieldValue =
            condensedDetails[i].elementWeakRef.get()?.value +
            condensedDetails[i + 1].elementWeakRef.get()?.value +
            condensedDetails[i + 2].elementWeakRef.get()?.value +
            condensedDetails[i + 3].elementWeakRef.get()?.value;
          condensedDetails.splice(i + 1, 3);
        }
      } else {
        countOfCCNumbers = 0;
      }
    }
  }
  /**
   * Return the record that is converted from `fieldDetails` and only valid
   * form record is included.
   *
   * @returns {object | null}
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
      section: this,
    };
    if (this.flowId) {
      data.flowId = this.flowId;
    }
    let condensedDetails = this.fieldDetails;

    // TODO: This is credit card specific code...
    this._condenseMultipleCCNumberFields(condensedDetails);

    condensedDetails.forEach(detail => {
      const element = detail.elementWeakRef.get();
      // Remove the unnecessary spaces
      let value = detail.fieldValue ?? (element && element.value.trim());
      value = this.computeFillingValue(value, detail, element);

      if (!value || value.length > FormAutofillUtils.MAX_FIELD_VALUE_LENGTH) {
        // Keep the property and preserve more information for updating
        data.record[detail.fieldName] = "";
        return;
      }

      data.record[detail.fieldName] = value;

      if (
        this.handler.getFilledStateByElement(element) ==
        FIELD_STATES.AUTO_FILLED
      ) {
        data.untouchedFields.push(detail.fieldName);
      }
    });

    this.createNormalizedRecord(data);

    if (!this.isRecordCreatable(data.record)) {
      return null;
    }

    return data;
  }

  /**
   * Resets a <select> element to its selected option or the first option if there is none selected.
   *
   * @param {HTMLElement} element
   * @memberof FormAutofillSection
   */
  _resetSelectElementValue(element) {
    if (!element.options.length) {
      return;
    }
    let selected = [...element.options].find(option =>
      option.hasAttribute("selected")
    );
    element.value = selected ? selected.value : element.options[0].value;
    element.dispatchEvent(
      new element.ownerGlobal.Event("input", { bubbles: true })
    );
    element.dispatchEvent(
      new element.ownerGlobal.Event("change", { bubbles: true })
    );
  }
}

class FormAutofillAddressSection extends FormAutofillSection {
  constructor(fieldDetails, handler) {
    super(fieldDetails, handler);

    this._cacheValue.oneLineStreetAddress = null;

    lazy.AutofillTelemetry.recordDetectedSectionCount(this);
    lazy.AutofillTelemetry.recordFormInteractionEvent("detected", this);
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
      !FormAutofill.isAutofillAddressesAvailableInCountry(record.country)
    ) {
      // We don't want to save data in the wrong fields due to not having proper
      // heuristic regexes in countries we don't yet support.
      lazy.log.warn(
        "isRecordCreatable: Country not supported:",
        record.country
      );
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
        HTMLInputElement.isInstance(streetAddressDetail.elementWeakRef.get())
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
   *
   * @param {object} profile
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
        detail.reason == "autocomplete" &&
        profile.tel.length <= element.maxLength
      ) {
        return;
      }
    }

    if (detail.reason != "autocomplete") {
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
      HTMLSelectElement.isInstance(element)
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

  createNormalizedRecord(address) {
    if (!address) {
      return;
    }

    // Normalize Country
    if (address.record.country) {
      let detail = this.getFieldDetailByName("country");
      // Try identifying country field aggressively if it doesn't come from
      // @autocomplete.
      if (detail.reason != "autocomplete") {
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

export class FormAutofillCreditCardSection extends FormAutofillSection {
  /**
   * Credit Card Section Constructor
   *
   * @param {object} fieldDetails
   *        The fieldDetail objects for the fields in this section
   * @param {object} handler
   *        The FormAutofillHandler responsible for this section
   */
  constructor(fieldDetails, handler) {
    super(fieldDetails, handler);

    if (!this.isValidSection()) {
      return;
    }

    lazy.AutofillTelemetry.recordDetectedSectionCount(this);
    lazy.AutofillTelemetry.recordFormInteractionEvent("detected", this);

    // Check whether the section is in an <iframe>; and, if so,
    // watch for the <iframe> to pagehide.
    if (handler.window.location != handler.window.parent?.location) {
      lazy.log.debug(
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
    lazy.log.debug("Credit card subframe is pagehideing", this.handler.form);
    this.handler.onFormSubmitted();
  }

  /**
   * Determine whether a set of cc fields identified by our heuristics form a
   * valid credit card section.
   * There are 4 different cases when a field is considered a credit card field
   * 1. Identified by autocomplete attribute. ex <input autocomplete="cc-number">
   * 2. Identified by fathom and fathom is pretty confident (when confidence
   *    value is higher than `highConfidenceThreshold`)
   * 3. Identified by fathom. Confidence value is between `fathom.confidenceThreshold`
   *    and `fathom.highConfidenceThreshold`
   * 4. Identified by regex-based heurstic. There is no confidence value in thise case.
   *
   * A form is considered a valid credit card form when one of the following condition
   * is met:
   * A. One of the cc field is identified by autocomplete (case 1)
   * B. One of the cc field is identified by fathom (case 2 or 3), and there is also
   *    another cc field found by any of our heuristic (case 2, 3, or 4)
   * C. Only one cc field is found in the section, but fathom is very confident (Case 2).
   *    Currently we add an extra restriction to this rule to decrease the false-positive
   *    rate. See comments below for details.
   *
   * @returns {boolean} True for a valid section, otherwise false
   */
  isValidSection() {
    let ccNumberDetail = null;
    let ccNameDetail = null;
    let ccExpiryDetail = null;

    for (let detail of this.fieldDetails) {
      switch (detail.fieldName) {
        case "cc-number":
          ccNumberDetail = detail;
          break;
        case "cc-name":
        case "cc-given-name":
        case "cc-additional-name":
        case "cc-family-name":
          ccNameDetail = detail;
          break;
        case "cc-exp":
        case "cc-exp-month":
        case "cc-exp-year":
          ccExpiryDetail = detail;
          break;
      }
    }

    // Condition A. Always trust autocomplete attribute. A section is considered a valid
    // cc section as long as a field has autocomplete=cc-number, cc-name or cc-exp*
    if (
      ccNumberDetail?.reason == "autocomplete" ||
      ccNameDetail?.reason == "autocomplete" ||
      ccExpiryDetail?.reason == "autocomplete"
    ) {
      return true;
    }

    // Condition B. One of the field is identified by fathom, if this section also
    // contains another cc field found by our heuristic (Case 2, 3, or 4), we consider
    // this section a valid credit card seciton
    if (ccNumberDetail?.confidence > 0) {
      if (ccNameDetail || ccExpiryDetail) {
        return true;
      }
    } else if (ccNameDetail?.confidence > 0) {
      if (ccNumberDetail || ccExpiryDetail) {
        return true;
      }
    }

    // Condition C.
    let highConfidenceThreshold =
      FormAutofillUtils.ccFathomHighConfidenceThreshold;
    let highConfidenceField;
    if (ccNumberDetail?.confidence > highConfidenceThreshold) {
      highConfidenceField = ccNumberDetail;
    } else if (ccNameDetail?.confidence > highConfidenceThreshold) {
      highConfidenceField = ccNameDetail;
    }
    if (highConfidenceField) {
      // Temporarily add an addtional "the field is the only visible input" constraint
      // when determining whether a form has only a high-confidence cc-* field a valid
      // credit card section. We can remove this restriction once we are confident
      // about only using fathom.
      const element = highConfidenceField.elementWeakRef.get();
      const root = element.form || element.ownerDocument;
      const inputs = root.querySelectorAll("input:not([type=hidden])");
      if (inputs.length == 1 && inputs[0] == element) {
        return true;
      }
    }

    return false;
  }

  isEnabled() {
    return FormAutofill.isAutofillCreditCardsEnabled;
  }

  isRecordCreatable(record) {
    return (
      record["cc-number"] && FormAutofillUtils.isCCNumber(record["cc-number"])
    );
  }

  /**
   * Handles credit card expiry date transformation when
   * the expiry date exists in a cc-exp field.
   *
   * @param {object} profile
   * @memberof FormAutofillCreditCardSection
   */
  creditCardExpiryDateTransformer(profile) {
    if (!profile["cc-exp"]) {
      return;
    }

    let detail = this.getFieldDetailByName("cc-exp");
    if (!detail) {
      return;
    }

    function monthYearOrderCheck(
      _expiryDateTransformFormat,
      _ccExpMonth,
      _ccExpYear
    ) {
      // Bug 1687681: This is a short term fix to other locales having
      // different characters to represent year.
      // For example, FR locales may use "A" to represent year.
      // For example, DE locales may use "J" to represent year.
      // This approach will not scale well and should be investigated in a follow up bug.
      let monthChars = "m";
      let yearChars = "yaj";
      let result;

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
      result = monthFirstCheck.exec(_expiryDateTransformFormat);
      if (result) {
        return (
          _ccExpMonth.toString().padStart(result[1].length, "0") +
          result[2] +
          _ccExpYear.toString().substr(-1 * result[3].length)
        );
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
      result = yearFirstCheck.exec(_expiryDateTransformFormat);

      if (result) {
        return (
          _ccExpYear.toString().substr(-1 * result[1].length) +
          result[2] +
          _ccExpMonth.toString().padStart(result[3].length, "0")
        );
      }
      return null;
    }

    let element = detail.elementWeakRef.get();
    let result;
    let ccExpMonth = profile["cc-exp-month"];
    let ccExpYear = profile["cc-exp-year"];
    if (element.tagName == "INPUT") {
      // Use the placeholder to determine the expiry string format.
      if (element.placeholder) {
        result = monthYearOrderCheck(
          element.placeholder,
          ccExpMonth,
          ccExpYear
        );
      }
      // If the previous sibling is a label, it is most likely meant to describe the
      // expiry field.
      if (!result && element.previousElementSibling?.tagName == "LABEL") {
        result = monthYearOrderCheck(
          element.previousElementSibling.textContent,
          ccExpMonth,
          ccExpYear
        );
      }
    }

    if (result) {
      profile["cc-exp"] = result;
    } else {
      // Bug 1688576: Change YYYY-MM to MM/YYYY since MM/YYYY is the
      // preferred presentation format for credit card expiry dates.
      profile["cc-exp"] =
        ccExpMonth.toString().padStart(2, "0") + "/" + ccExpYear.toString();
    }
  }

  /**
   * Handles credit card expiry date transformation when the expiry date exists in
   * the separate cc-exp-month and cc-exp-year fields
   *
   * @param {object} profile
   * @memberof FormAutofillCreditCardSection
   */
  creditCardExpMonthAndYearTransformer(profile) {
    const getInputElementByField = (field, self) => {
      if (!field) {
        return null;
      }
      let detail = self.getFieldDetailByName(field);
      if (!detail) {
        return null;
      }
      let element = detail.elementWeakRef.get();
      return element.tagName === "INPUT" ? element : null;
    };
    let month = getInputElementByField("cc-exp-month", this);
    if (month) {
      // Transform the expiry month to MM since this is a common format needed for filling.
      profile["cc-exp-month-formatted"] = profile["cc-exp-month"]
        ?.toString()
        .padStart(2, "0");
    }
    let year = getInputElementByField("cc-exp-year", this);
    // If the expiration year element is an input,
    // then we examine any placeholder to see if we should format the expiration year
    // as a zero padded string in order to autofill correctly.
    if (year) {
      let placeholder = year.placeholder;

      // Checks for 'YY'|'AA'|'JJ' placeholder and converts the year to a two digit string using the last two digits.
      let result = /(?<!.)(yy|aa|jj)(?!.)/i.test(placeholder);
      if (result) {
        profile["cc-exp-year-formatted"] = profile["cc-exp-year"]
          .toString()
          .substring(2);
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
    // The matchSelectOptions transformer must be placed after the expiry transformers.
    // This ensures that the expiry value that is cached in the matchSelectOptions
    // matches the expiry value that is stored in the profile ensuring that autofill works
    // correctly when dealing with option elements.
    this.creditCardExpiryDateTransformer(profile);
    this.creditCardExpMonthAndYearTransformer(profile);
    this.matchSelectOptions(profile);
    this.adaptFieldMaxLength(profile);
  }

  getFilledValueFromProfile(fieldDetail, profile) {
    const value = super.getFilledValueFromProfile(fieldDetail, profile);
    if (fieldDetail.fieldName == "cc-number" && fieldDetail.part != null) {
      const part = fieldDetail.part;
      return value.slice((part - 1) * 4, part * 4);
    }
    return value;
  }

  computeFillingValue(value, fieldDetail, element) {
    if (
      fieldDetail.fieldName != "cc-type" ||
      !HTMLSelectElement.isInstance(element)
    ) {
      return value;
    }

    if (lazy.CreditCard.isValidNetwork(value)) {
      return value;
    }

    // Don't save the record when the option value is empty *OR* there
    // are multiple options being selected. The empty option is usually
    // assumed to be default along with a meaningless text to users.
    if (value && element.selectedOptions.length == 1) {
      let selectedOption = element.selectedOptions[0];
      let networkType =
        lazy.CreditCard.getNetworkFromName(selectedOption.text) ??
        lazy.CreditCard.getNetworkFromName(selectedOption.value);
      if (networkType) {
        return networkType;
      }
    }
    // If we couldn't match the value to any network, we'll
    // strip this field when submitting.
    return value;
  }

  /**
   * Customize for previewing profile
   *
   * @param {object} profile
   *        A profile for pre-processing before previewing values.
   * @override
   */
  preparePreviewProfile(profile) {
    // Always show the decrypted credit card number when Master Password is
    // disabled.
    if (profile["cc-number-decrypted"]) {
      profile["cc-number"] = profile["cc-number-decrypted"];
    } else if (!profile["cc-number"].startsWith("****")) {
      // Show the previewed credit card as "**** 4444" which is
      // needed when a credit card number field has a maxlength of four.
      profile["cc-number"] = "****" + profile["cc-number"];
    }
  }

  /**
   * Customize for filling profile
   *
   * @param {object} profile
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
        lazy.reauthPasswordPromptMessage
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

    return true;
  }

  createNormalizedRecord(creditCard) {
    if (!creditCard?.record["cc-number"]) {
      return;
    }
    // Normalize cc-number
    creditCard.record["cc-number"] = lazy.CreditCard.normalizeCardNumber(
      creditCard.record["cc-number"]
    );

    // Normalize cc-exp-month and cc-exp-year
    let { month, year } = lazy.CreditCard.normalizeExpiration({
      expirationString: creditCard.record["cc-exp"],
      expirationMonth: creditCard.record["cc-exp-month"],
      expirationYear: creditCard.record["cc-exp-year"],
    });
    if (month) {
      creditCard.record["cc-exp-month"] = month;
    }
    if (year) {
      creditCard.record["cc-exp-year"] = year;
    }
  }
}

/**
 * Handles profile autofill for a DOM Form element.
 */
export class FormAutofillHandler {
  // The window to which this form belongs
  window = null;

  // A WindowUtils reference of which Window the form belongs
  winUtils = null;

  // DOM Form element to which this object is attached
  form = null;

  // An array of section that are found in this form
  sections = [];

  // The section contains the focused input
  #focusedSection = null;

  // Caches the element to section mapping
  #cachedSectionByElement = new WeakMap();

  // Keeps track of filled state for all identified elements
  #filledStateByElement = new WeakMap();
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
  fieldDetails = null;

  /**
   * Initialize the form from `FormLike` object to handle the section or form
   * operations.
   *
   * @param {FormLike} form Form that need to be auto filled
   * @param {Function} onFormSubmitted Function that can be invoked
   *                   to simulate form submission. Function is passed
   *                   three arguments: (1) a FormLike for the form being
   *                   submitted, (2) the corresponding Window, and (3) the
   *                   responsible FormAutofillHandler.
   */
  constructor(form, onFormSubmitted = () => {}) {
    this._updateForm(form);

    this.window = this.form.rootElement.ownerGlobal;
    this.winUtils = this.window.windowUtils;

    // Enum for form autofill MANUALLY_MANAGED_STATES values
    this.FIELD_STATE_ENUM = {
      // not themed
      [FIELD_STATES.NORMAL]: null,
      // highlighted
      [FIELD_STATES.AUTO_FILLED]: "autofill",
      // highlighted && grey color text
      [FIELD_STATES.PREVIEW]: "-moz-autofill-preview",
    };

    /**
     * This function is used if the form handler (or one of its sections)
     * determines that it needs to act as if the form had been submitted.
     */
    this.onFormSubmitted = () => {
      onFormSubmitted(this.form, this.window, this);
    };
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
          !HTMLSelectElement.isInstance(target) &&
          isCreditCardField &&
          target.value === ""
        ) {
          formFillController.showPopup();
        }

        if (this.getFilledStateByElement(target) == FIELD_STATES.NORMAL) {
          return;
        }

        this.changeFieldState(targetFieldDetail, FIELD_STATES.NORMAL);
        const section = this.getSectionByElement(
          targetFieldDetail.elementWeakRef.get()
        );
        section?.clearFilled(targetFieldDetail);
      }
    }
  }

  set focusedInput(element) {
    const section = this.getSectionByElement(element);
    if (!section) {
      return;
    }

    this.#focusedSection = section;
    this.#focusedSection.focusedInput = element;
  }

  getSectionByElement(element) {
    const section =
      this.#cachedSectionByElement.get(element) ??
      this.sections.find(s => s.getFieldDetailByElement(element));
    if (!section) {
      return null;
    }

    this.#cachedSectionByElement.set(element, section);
    return section;
  }

  getFieldDetailByElement(element) {
    for (const section of this.sections) {
      const detail = section.getFieldDetailByElement(element);
      if (detail) {
        return detail;
      }
    }
    return null;
  }

  get activeSection() {
    return this.#focusedSection;
  }

  /**
   * Check the form is necessary to be updated. This function should be able to
   * detect any changes including all control elements in the form.
   *
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
    const getFormLike = () => {
      if (!_formLike) {
        _formLike = lazy.FormLikeFactory.createFromField(element);
      }
      return _formLike;
    };

    const currentForm = element.form ?? getFormLike();
    if (currentForm.elements.length != this.form.elements.length) {
      lazy.log.debug("The count of form elements is changed.");
      this._updateForm(getFormLike());
      return true;
    }

    if (!this.form.elements.includes(element)) {
      lazy.log.debug("The element can not be found in the current form.");
      this._updateForm(getFormLike());
      return true;
    }

    return false;
  }

  /**
   * Update the form with a new FormLike, and the related fields should be
   * updated or clear to ensure the data consistency.
   *
   * @param {FormLike} form a new FormLike to replace the original one.
   */
  _updateForm(form) {
    this.form = form;

    this.fieldDetails = null;

    this.sections = [];
    this.#cachedSectionByElement = new WeakMap();
  }

  /**
   * Set fieldDetails from the form about fields that can be autofilled.
   *
   * @returns {Array} The valid address and credit card details.
   */
  collectFormFields() {
    const sections = lazy.FormAutofillHeuristics.getFormInfo(this.form);
    const allValidDetails = [];
    for (const { fieldDetails, type } of sections) {
      let section;
      if (type == FormAutofillUtils.SECTION_TYPES.ADDRESS) {
        section = new FormAutofillAddressSection(fieldDetails, this);
      } else if (type == FormAutofillUtils.SECTION_TYPES.CREDIT_CARD) {
        section = new FormAutofillCreditCardSection(fieldDetails, this);
      } else {
        throw new Error("Unknown field type.");
      }
      this.sections.push(section);
      allValidDetails.push(...section.fieldDetails);
    }

    this.fieldDetails = allValidDetails;
    return allValidDetails;
  }

  #hasFilledSection() {
    return this.sections.some(section => section.isFilled());
  }

  getFilledStateByElement(element) {
    return this.#filledStateByElement.get(element);
  }

  /**
   * Change the state of a field to correspond with different presentations.
   *
   * @param {object} fieldDetail
   *        A fieldDetail of which its element is about to update the state.
   * @param {string} nextState
   *        Used to determine the next state
   */
  changeFieldState(fieldDetail, nextState) {
    const element = fieldDetail.elementWeakRef.get();
    if (!element) {
      lazy.log.warn(
        fieldDetail.fieldName,
        "is unreachable while changing state"
      );
      return;
    }
    if (!(nextState in this.FIELD_STATE_ENUM)) {
      lazy.log.warn(
        fieldDetail.fieldName,
        "is trying to change to an invalid state"
      );
      return;
    }

    if (this.#filledStateByElement.get(element) == nextState) {
      return;
    }

    let nextStateValue = null;
    for (const [state, mmStateValue] of Object.entries(this.FIELD_STATE_ENUM)) {
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

    this.#filledStateByElement.set(element, nextState);
  }

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {object} profile
   *        A profile to be filled in.
   */
  async autofillFormFields(profile) {
    const noFilledSectionsPreviously = !this.#hasFilledSection();
    await this.activeSection.autofillFields(profile);

    const onChangeHandler = e => {
      if (!e.isTrusted) {
        return;
      }
      if (e.type == "reset") {
        this.sections.map(section => section.resetFieldStates());
      }
      // Unregister listeners once no field is in AUTO_FILLED state.
      if (!this.#hasFilledSection()) {
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
      lazy.log.debug("register change handler for filled form:", this.form);
      this.form.rootElement.addEventListener("input", onChangeHandler, {
        mozSystemGroup: true,
      });
      this.form.rootElement.addEventListener("reset", onChangeHandler, {
        mozSystemGroup: true,
      });
    }
  }

  /**
   * Collect the filled sections within submitted form and convert all the valid
   * field data into multiple records.
   *
   * @returns {object} records
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

    return records;
  }
}
