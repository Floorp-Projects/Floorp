/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill field heuristics.
 */

"use strict";

var EXPORTED_SYMBOLS = ["FormAutofillHeuristics", "LabelUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
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

XPCOMUtils.defineLazyModuleGetters(this, {
  CreditCard: "resource://gre/modules/CreditCard.jsm",
});

this.log = null;
FormAutofill.defineLazyLogGetter(this, EXPORTED_SYMBOLS[0]);

const PREF_HEURISTICS_ENABLED = "extensions.formautofill.heuristics.enabled";
const PREF_SECTION_ENABLED = "extensions.formautofill.section.enabled";
const DEFAULT_SECTION_NAME = "-moz-section-default";

/**
 * To help us classify sections, we want to know what fields can appear
 * multiple times in a row.
 * Such fields, like `address-line{X}`, should not break sections.
 */
const MULTI_FIELD_NAMES = [
  "address-level3",
  "address-level2",
  "address-level1",
  "tel",
  "postal-code",
  "email",
  "street-address",
];

/**
 * To help us classify sections that can appear only N times in a row.
 * For example, the only time multiple cc-number fields are valid is when
 * there are four of these fields in a row.
 * Otherwise, multiple cc-number fields should be in separate sections.
 */
const MULTI_N_FIELD_NAMES = {
  "cc-number": 4,
};

/**
 * A scanner for traversing all elements in a form and retrieving the field
 * detail with FormAutofillHeuristics.getInfo function. It also provides a
 * cursor (parsingIndex) to indicate which element is waiting for parsing.
 */
class FieldScanner {
  /**
   * Create a FieldScanner based on form elements with the existing
   * fieldDetails.
   *
   * @param {Array.DOMElement} elements
   *        The elements from a form for each parser.
   */
  constructor(elements, { allowDuplicates = false, sectionEnabled = true }) {
    this._elementsWeakRef = Cu.getWeakReference(elements);
    this.fieldDetails = [];
    this._parsingIndex = 0;
    this._sections = [];
    this._allowDuplicates = allowDuplicates;
    this._sectionEnabled = sectionEnabled;
  }

  get _elements() {
    return this._elementsWeakRef.get();
  }

  /**
   * This cursor means the index of the element which is waiting for parsing.
   *
   * @returns {number}
   *          The index of the element which is waiting for parsing.
   */
  get parsingIndex() {
    return this._parsingIndex;
  }

  /**
   * Move the parsingIndex to the next elements. Any elements behind this index
   * means the parsing tasks are finished.
   *
   * @param {number} index
   *        The latest index of elements waiting for parsing.
   */
  set parsingIndex(index) {
    if (index > this._elements.length) {
      throw new Error("The parsing index is out of range.");
    }
    this._parsingIndex = index;
  }

  /**
   * Retrieve the field detail by the index. If the field detail is not ready,
   * the elements will be traversed until matching the index.
   *
   * @param {number} index
   *        The index of the element that you want to retrieve.
   * @returns {Object}
   *          The field detail at the specific index.
   */
  getFieldDetailByIndex(index) {
    if (index >= this._elements.length) {
      throw new Error(
        `The index ${index} is out of range.(${this._elements.length})`
      );
    }

    if (index < this.fieldDetails.length) {
      return this.fieldDetails[index];
    }

    for (let i = this.fieldDetails.length; i < index + 1; i++) {
      this.pushDetail();
    }

    return this.fieldDetails[index];
  }

  get parsingFinished() {
    return this.parsingIndex >= this._elements.length;
  }

  _pushToSection(name, fieldDetail) {
    for (let section of this._sections) {
      if (section.name == name) {
        section.fieldDetails.push(fieldDetail);
        return;
      }
    }
    this._sections.push({
      name,
      fieldDetails: [fieldDetail],
    });
  }
  /**
   * Merges the next N fields if the currentType is in the list of MULTI_N_FIELD_NAMES
   *
   * @param {number} mergeNextNFields How many of the next N fields to merge into the current section
   * @param {string} currentType Type of the current field detail
   * @param {Array<Object>} fieldDetails List of current field details
   * @param {number} i Index to keep track of the fieldDetails list
   * @param {boolean} createNewSection Determines if a new section should be created
   * @returns {[number, boolean]} mergeNextNFields and creatNewSection for use in _classifySections
   * @memberof FieldScanner
   */
  _mergeNextNFields(
    mergeNextNFields,
    currentType,
    fieldDetails,
    i,
    createNewSection
  ) {
    if (mergeNextNFields) {
      mergeNextNFields--;
    } else {
      // We use -2 here because we have already seen two consecutive fields,
      // the previous one and the current one.
      // This ensures we don't accidentally add a field we've already seen.
      let nextN = MULTI_N_FIELD_NAMES[currentType] - 2;
      let array = fieldDetails.slice(i + 1, i + 1 + nextN);
      if (
        array.length == nextN &&
        array.every(detail => detail.fieldName == currentType)
      ) {
        mergeNextNFields = nextN;
      } else {
        createNewSection = true;
      }
    }
    return { mergeNextNFields, createNewSection };
  }
  _classifySections() {
    let fieldDetails = this._sections[0].fieldDetails;
    this._sections = [];
    let seenTypes = new Set();
    let previousType;
    let sectionCount = 0;
    let mergeNextNFields = 0;

    for (let i = 0; i < fieldDetails.length; i++) {
      let currentType = fieldDetails[i].fieldName;
      if (!currentType) {
        continue;
      }

      let createNewSection = false;
      if (seenTypes.has(currentType)) {
        if (previousType != currentType) {
          // If we have seen this field before and it is different from
          // the previous one, always create a new section.
          createNewSection = true;
        } else if (MULTI_FIELD_NAMES.includes(currentType)) {
          // For fields that can appear multiple times in a row
          // within one section, don't create a new section
        } else if (currentType in MULTI_N_FIELD_NAMES) {
          // This is the heuristic to handle special cases where we can have multiple
          // fields in one section, but only if the field has appeared N times in a row.
          // For example, websites can use 4 consecutive 4-digit `cc-number` fields
          // instead of one 16-digit `cc-number` field.
          ({ mergeNextNFields, createNewSection } = this._mergeNextNFields(
            mergeNextNFields,
            currentType,
            fieldDetails,
            i,
            createNewSection
          ));
        } else {
          // Fields that should not appear multiple times in one section.
          createNewSection = true;
        }
      }

      if (createNewSection) {
        mergeNextNFields = 0;
        seenTypes.clear();
        sectionCount++;
      }

      previousType = currentType;
      seenTypes.add(currentType);
      this._pushToSection(
        DEFAULT_SECTION_NAME + "-" + sectionCount,
        fieldDetails[i]
      );
    }
  }

  /**
   * The result is an array contains the sections with its belonging field
   * details. If `this._sections` contains one section only with the default
   * section name (DEFAULT_SECTION_NAME), `this._classifySections` should be
   * able to identify all sections in the heuristic way.
   *
   * @returns {Array<Object>}
   *          The array with the sections, and the belonging fieldDetails are in
   *          each section.
   */
  getSectionFieldDetails() {
    // When the section feature is disabled, `getSectionFieldDetails` should
    // provide a single address and credit card section result.
    if (!this._sectionEnabled) {
      return this._getFinalDetails(this.fieldDetails);
    }
    if (!this._sections.length) {
      return [];
    }
    if (
      this._sections.length == 1 &&
      this._sections[0].name == DEFAULT_SECTION_NAME
    ) {
      this._classifySections();
    }

    return this._sections.reduce((sections, current) => {
      sections.push(...this._getFinalDetails(current.fieldDetails));
      return sections;
    }, []);
  }

  /**
   * This function will prepare an autocomplete info object with getInfo
   * function and push the detail to fieldDetails property.
   * Any field will be pushed into `this._sections` based on the section name
   * in `autocomplete` attribute.
   *
   * Any element without the related detail will be used for adding the detail
   * to the end of field details.
   */
  pushDetail() {
    let elementIndex = this.fieldDetails.length;
    if (elementIndex >= this._elements.length) {
      throw new Error("Try to push the non-existing element info.");
    }
    let element = this._elements[elementIndex];
    let info = FormAutofillHeuristics.getInfo(element);
    let fieldInfo = {
      section: info ? info.section : "",
      addressType: info ? info.addressType : "",
      contactType: info ? info.contactType : "",
      fieldName: info ? info.fieldName : "",
      elementWeakRef: Cu.getWeakReference(element),
    };

    if (info && info._reason) {
      fieldInfo._reason = info._reason;
    }

    this.fieldDetails.push(fieldInfo);
    this._pushToSection(this._getSectionName(fieldInfo), fieldInfo);
  }

  _getSectionName(info) {
    let names = [];
    if (info.section) {
      names.push(info.section);
    }
    if (info.addressType) {
      names.push(info.addressType);
    }
    return names.length ? names.join(" ") : DEFAULT_SECTION_NAME;
  }

  /**
   * When a field detail should be changed its fieldName after parsing, use
   * this function to update the fieldName which is at a specific index.
   *
   * @param {number} index
   *        The index indicates a field detail to be updated.
   * @param {string} fieldName
   *        The new fieldName
   */
  updateFieldName(index, fieldName) {
    if (index >= this.fieldDetails.length) {
      throw new Error("Try to update the non-existing field detail.");
    }
    this.fieldDetails[index].fieldName = fieldName;
  }

  _isSameField(field1, field2) {
    return (
      field1.section == field2.section &&
      field1.addressType == field2.addressType &&
      field1.fieldName == field2.fieldName &&
      !field1.transform &&
      !field2.transform
    );
  }

  /**
   * When a site has four credit card number fields and
   * these fields have a max length of four
   * then we transform the credit card number into
   * four subsections in order to fill correctly.
   *
   * @param {Array<Object>} creditCardFieldDetails
   *        The credit card field details to be transformed for multiple cc-number fields filling
   * @memberof FieldScanner
   */
  _transformCCNumberForMultipleFields(creditCardFieldDetails) {
    let ccNumberFields = creditCardFieldDetails.filter(
      field =>
        field.fieldName == "cc-number" &&
        field.elementWeakRef.get().maxLength == 4
    );
    if (ccNumberFields.length == 4) {
      ccNumberFields[0].transform = fullCCNumber => fullCCNumber.slice(0, 4);
      ccNumberFields[1].transform = fullCCNumber => fullCCNumber.slice(4, 8);
      ccNumberFields[2].transform = fullCCNumber => fullCCNumber.slice(8, 12);
      ccNumberFields[3].transform = fullCCNumber => fullCCNumber.slice(12, 16);
    }
  }

  /**
   * Provide the final field details without invalid field name, and the
   * duplicated fields will be removed as well. For the debugging purpose,
   * the final `fieldDetails` will include the duplicated fields if
   * `_allowDuplicates` is true.
   *
   * Each item should contain one type of fields only, and the two valid types
   * are Address and CreditCard.
   *
   * @param   {Array<Object>} fieldDetails
   *          The field details for trimming.
   * @returns {Array<Object>}
   *          The array with the field details without invalid field name and
   *          duplicated fields.
   */
  _getFinalDetails(fieldDetails) {
    let addressFieldDetails = [];
    let creditCardFieldDetails = [];
    for (let fieldDetail of fieldDetails) {
      let fieldName = fieldDetail.fieldName;
      if (FormAutofillUtils.isAddressField(fieldName)) {
        addressFieldDetails.push(fieldDetail);
      } else if (FormAutofillUtils.isCreditCardField(fieldName)) {
        creditCardFieldDetails.push(fieldDetail);
      } else {
        log.debug(
          "Not collecting a field with a unknown fieldName",
          fieldDetail
        );
      }
    }
    this._transformCCNumberForMultipleFields(creditCardFieldDetails);
    return [
      {
        type: FormAutofillUtils.SECTION_TYPES.ADDRESS,
        fieldDetails: addressFieldDetails,
      },
      {
        type: FormAutofillUtils.SECTION_TYPES.CREDIT_CARD,
        fieldDetails: creditCardFieldDetails,
      },
    ]
      .map(section => {
        if (this._allowDuplicates) {
          return section;
        }
        // Deduplicate each set of fieldDetails
        let details = section.fieldDetails;
        section.fieldDetails = details.filter((detail, index) => {
          let previousFields = details.slice(0, index);
          return !previousFields.find(f => this._isSameField(detail, f));
        });
        return section;
      })
      .filter(section => !!section.fieldDetails.length);
  }

  elementExisting(index) {
    return index < this._elements.length;
  }
}

var LabelUtils = {
  // The tag name list is from Chromium except for "STYLE":
  // eslint-disable-next-line max-len
  // https://cs.chromium.org/chromium/src/components/autofill/content/renderer/form_autofill_util.cc?l=216&rcl=d33a171b7c308a64dc3372fac3da2179c63b419e
  EXCLUDED_TAGS: ["SCRIPT", "NOSCRIPT", "OPTION", "STYLE"],

  // A map object, whose keys are the id's of form fields and each value is an
  // array consisting of label elements correponding to the id.
  // @type {Map<string, array>}
  _mappedLabels: null,

  // An array consisting of label elements whose correponding form field doesn't
  // have an id attribute.
  // @type {Array<HTMLLabelElement>}
  _unmappedLabels: null,

  // A weak map consisting of label element and extracted strings pairs.
  // @type {WeakMap<HTMLLabelElement, array>}
  _labelStrings: null,

  /**
   * Extract all strings of an element's children to an array.
   * "element.textContent" is a string which is merged of all children nodes,
   * and this function provides an array of the strings contains in an element.
   *
   * @param  {Object} element
   *         A DOM element to be extracted.
   * @returns {Array}
   *          All strings in an element.
   */
  extractLabelStrings(element) {
    if (this._labelStrings.has(element)) {
      return this._labelStrings.get(element);
    }
    let strings = [];
    let _extractLabelStrings = el => {
      if (this.EXCLUDED_TAGS.includes(el.tagName)) {
        return;
      }

      if (el.nodeType == el.TEXT_NODE || !el.childNodes.length) {
        let trimmedText = el.textContent.trim();
        if (trimmedText) {
          strings.push(trimmedText);
        }
        return;
      }

      for (let node of el.childNodes) {
        let nodeType = node.nodeType;
        if (nodeType != node.ELEMENT_NODE && nodeType != node.TEXT_NODE) {
          continue;
        }
        _extractLabelStrings(node);
      }
    };
    _extractLabelStrings(element);
    this._labelStrings.set(element, strings);
    return strings;
  },

  generateLabelMap(doc) {
    let mappedLabels = new Map();
    let unmappedLabels = [];

    for (let label of doc.querySelectorAll("label")) {
      let id = label.htmlFor;
      if (!id) {
        let control = label.control;
        if (!control) {
          continue;
        }
        id = control.id;
      }
      if (id) {
        let labels = mappedLabels.get(id);
        if (labels) {
          labels.push(label);
        } else {
          mappedLabels.set(id, [label]);
        }
      } else {
        unmappedLabels.push(label);
      }
    }

    this._mappedLabels = mappedLabels;
    this._unmappedLabels = unmappedLabels;
    this._labelStrings = new WeakMap();
  },

  clearLabelMap() {
    this._mappedLabels = null;
    this._unmappedLabels = null;
    this._labelStrings = null;
  },

  findLabelElements(element) {
    if (!this._mappedLabels) {
      this.generateLabelMap(element.ownerDocument);
    }

    let id = element.id;
    if (!id) {
      return this._unmappedLabels.filter(label => label.control == element);
    }
    return this._mappedLabels.get(id) || [];
  },
};

/**
 * Returns the autocomplete information of fields according to heuristics.
 */
this.FormAutofillHeuristics = {
  RULES: null,

  /**
   * Try to find a contiguous sub-array within an array.
   *
   * @param {Array} array
   * @param {Array} subArray
   *
   * @returns {boolean}
   *          Return whether subArray was found within the array or not.
   */
  _matchContiguousSubArray(array, subArray) {
    return array.some((elm, i) =>
      subArray.every((sElem, j) => sElem == array[i + j])
    );
  },

  /**
   * Try to find the field that is look like a month select.
   *
   * @param {DOMElement} element
   * @returns {boolean}
   *          Return true if we observe the trait of month select in
   *          the current element.
   */
  _isExpirationMonthLikely(element) {
    if (ChromeUtils.getClassName(element) !== "HTMLSelectElement") {
      return false;
    }

    const options = [...element.options];
    const desiredValues = Array(12)
      .fill(1)
      .map((v, i) => v + i);

    // The number of month options shouldn't be less than 12 or larger than 13
    // including the default option.
    if (options.length < 12 || options.length > 13) {
      return false;
    }

    return (
      this._matchContiguousSubArray(
        options.map(e => +e.value),
        desiredValues
      ) ||
      this._matchContiguousSubArray(
        options.map(e => +e.label),
        desiredValues
      )
    );
  },

  /**
   * Try to find the field that is look like a year select.
   *
   * @param {DOMElement} element
   * @returns {boolean}
   *          Return true if we observe the trait of year select in
   *          the current element.
   */
  _isExpirationYearLikely(element) {
    if (ChromeUtils.getClassName(element) !== "HTMLSelectElement") {
      return false;
    }

    const options = [...element.options];
    // A normal expiration year select should contain at least the last three years
    // in the list.
    const curYear = new Date().getFullYear();
    const desiredValues = Array(3)
      .fill(0)
      .map((v, i) => v + curYear + i);

    return (
      this._matchContiguousSubArray(
        options.map(e => +e.value),
        desiredValues
      ) ||
      this._matchContiguousSubArray(
        options.map(e => +e.label),
        desiredValues
      )
    );
  },

  /**
   * Try to match the telephone related fields to the grammar
   * list to see if there is any valid telephone set and correct their
   * field names.
   *
   * @param {FieldScanner} fieldScanner
   *        The current parsing status for all elements
   * @returns {boolean}
   *          Return true if there is any field can be recognized in the parser,
   *          otherwise false.
   */
  _parsePhoneFields(fieldScanner) {
    let matchingResult;

    const GRAMMARS = this.PHONE_FIELD_GRAMMARS;
    for (let i = 0; i < GRAMMARS.length; i++) {
      let detailStart = fieldScanner.parsingIndex;
      let ruleStart = i;
      for (
        ;
        i < GRAMMARS.length &&
        GRAMMARS[i][0] &&
        fieldScanner.elementExisting(detailStart);
        i++, detailStart++
      ) {
        let detail = fieldScanner.getFieldDetailByIndex(detailStart);
        if (
          !detail ||
          GRAMMARS[i][0] != detail.fieldName ||
          (detail._reason && detail._reason == "autocomplete")
        ) {
          break;
        }
        let element = detail.elementWeakRef.get();
        if (!element) {
          break;
        }
        if (
          GRAMMARS[i][2] &&
          (!element.maxLength || GRAMMARS[i][2] < element.maxLength)
        ) {
          break;
        }
      }
      if (i >= GRAMMARS.length) {
        break;
      }

      if (!GRAMMARS[i][0]) {
        matchingResult = {
          ruleFrom: ruleStart,
          ruleTo: i,
        };
        break;
      }

      // Fast rewinding to the next rule.
      for (; i < GRAMMARS.length; i++) {
        if (!GRAMMARS[i][0]) {
          break;
        }
      }
    }

    let parsedField = false;
    if (matchingResult) {
      let { ruleFrom, ruleTo } = matchingResult;
      let detailStart = fieldScanner.parsingIndex;
      for (let i = ruleFrom; i < ruleTo; i++) {
        fieldScanner.updateFieldName(detailStart, GRAMMARS[i][1]);
        fieldScanner.parsingIndex++;
        detailStart++;
        parsedField = true;
      }
    }

    if (fieldScanner.parsingFinished) {
      return parsedField;
    }

    let nextField = fieldScanner.getFieldDetailByIndex(
      fieldScanner.parsingIndex
    );
    if (
      nextField &&
      nextField._reason != "autocomplete" &&
      fieldScanner.parsingIndex > 0
    ) {
      const regExpTelExtension = new RegExp(
        "\\bext|ext\\b|extension|ramal", // pt-BR, pt-PT
        "iu"
      );
      const previousField = fieldScanner.getFieldDetailByIndex(
        fieldScanner.parsingIndex - 1
      );
      const previousFieldType = FormAutofillUtils.getCategoryFromFieldName(
        previousField.fieldName
      );
      if (
        previousField &&
        previousFieldType == "tel" &&
        this._matchRegexp(nextField.elementWeakRef.get(), regExpTelExtension)
      ) {
        fieldScanner.updateFieldName(
          fieldScanner.parsingIndex,
          "tel-extension"
        );
        fieldScanner.parsingIndex++;
        parsedField = true;
      }
    }

    return parsedField;
  },

  /**
   * Try to find the correct address-line[1-3] sequence and correct their field
   * names.
   *
   * @param {FieldScanner} fieldScanner
   *        The current parsing status for all elements
   * @returns {boolean}
   *          Return true if there is any field can be recognized in the parser,
   *          otherwise false.
   */
  _parseAddressFields(fieldScanner) {
    let parsedFields = false;
    const addressLines = ["address-line1", "address-line2", "address-line3"];

    // TODO: These address-line* regexps are for the lines with numbers, and
    // they are the subset of the regexps in `heuristicsRegexp.js`. We have to
    // find a better way to make them consistent.
    const addressLineRegexps = {
      "address-line1": new RegExp(
        "address[_-]?line(1|one)|address1|addr1" +
        "|addrline1|address_1" + // Extra rules by Firefox
        "|indirizzo1" + // it-IT
        "|住所1" + // ja-JP
        "|地址1" + // zh-CN
          "|주소.?1", // ko-KR
        "iu"
      ),
      "address-line2": new RegExp(
        "address[_-]?line(2|two)|address2|addr2" +
        "|addrline2|address_2" + // Extra rules by Firefox
        "|indirizzo2" + // it-IT
        "|住所2" + // ja-JP
        "|地址2" + // zh-CN
          "|주소.?2", // ko-KR
        "iu"
      ),
      "address-line3": new RegExp(
        "address[_-]?line(3|three)|address3|addr3" +
        "|addrline3|address_3" + // Extra rules by Firefox
        "|indirizzo3" + // it-IT
        "|住所3" + // ja-JP
        "|地址3" + // zh-CN
          "|주소.?3", // ko-KR
        "iu"
      ),
    };
    while (!fieldScanner.parsingFinished) {
      let detail = fieldScanner.getFieldDetailByIndex(
        fieldScanner.parsingIndex
      );
      if (
        !detail ||
        !addressLines.includes(detail.fieldName) ||
        detail._reason == "autocomplete"
      ) {
        // When the field is not related to any address-line[1-3] fields or
        // determined by autocomplete attr, it means the parsing process can be
        // terminated.
        break;
      }
      const elem = detail.elementWeakRef.get();
      for (let regexp of Object.keys(addressLineRegexps)) {
        if (this._matchRegexp(elem, addressLineRegexps[regexp])) {
          fieldScanner.updateFieldName(fieldScanner.parsingIndex, regexp);
          parsedFields = true;
        }
      }
      fieldScanner.parsingIndex++;
    }

    return parsedFields;
  },

  /**
   * Try to look for expiration date fields and revise the field names if needed.
   *
   * @param {FieldScanner} fieldScanner
   *        The current parsing status for all elements
   * @returns {boolean}
   *          Return true if there is any field can be recognized in the parser,
   *          otherwise false.
   */
  _parseCreditCardFields(fieldScanner) {
    if (fieldScanner.parsingFinished) {
      return false;
    }

    const savedIndex = fieldScanner.parsingIndex;
    const detail = fieldScanner.getFieldDetailByIndex(
      fieldScanner.parsingIndex
    );

    // Respect to autocomplete attr
    if (!detail || (detail._reason && detail._reason == "autocomplete")) {
      return false;
    }

    const monthAndYearFieldNames = ["cc-exp-month", "cc-exp-year"];
    // Skip the uninteresting fields
    if (
      !["cc-exp", "cc-type", ...monthAndYearFieldNames].includes(
        detail.fieldName
      )
    ) {
      return false;
    }

    const element = detail.elementWeakRef.get();

    // If we didn't auto-discover type field, check every select for options that
    // match credit card network names in value or label.
    if (ChromeUtils.getClassName(element) == "HTMLSelectElement") {
      for (let option of element.querySelectorAll("option")) {
        if (
          CreditCard.getNetworkFromName(option.value) ||
          CreditCard.getNetworkFromName(option.text)
        ) {
          fieldScanner.updateFieldName(fieldScanner.parsingIndex, "cc-type");
          fieldScanner.parsingIndex++;
          return true;
        }
      }
    }

    // If the input type is a month picker, then assume it's cc-exp.
    if (element.type == "month") {
      fieldScanner.updateFieldName(fieldScanner.parsingIndex, "cc-exp");
      fieldScanner.parsingIndex++;

      return true;
    }

    // Don't process the fields if expiration month and expiration year are already
    // matched by regex in correct order.
    if (
      fieldScanner.getFieldDetailByIndex(fieldScanner.parsingIndex++)
        .fieldName == "cc-exp-month" &&
      !fieldScanner.parsingFinished &&
      fieldScanner.getFieldDetailByIndex(fieldScanner.parsingIndex++)
        .fieldName == "cc-exp-year"
    ) {
      return true;
    }
    fieldScanner.parsingIndex = savedIndex;

    // Determine the field name by checking if the fields are month select and year select
    // likely.
    if (this._isExpirationMonthLikely(element)) {
      fieldScanner.updateFieldName(fieldScanner.parsingIndex, "cc-exp-month");
      fieldScanner.parsingIndex++;
      if (!fieldScanner.parsingFinished) {
        const nextDetail = fieldScanner.getFieldDetailByIndex(
          fieldScanner.parsingIndex
        );
        const nextElement = nextDetail.elementWeakRef.get();
        if (this._isExpirationYearLikely(nextElement)) {
          fieldScanner.updateFieldName(
            fieldScanner.parsingIndex,
            "cc-exp-year"
          );
          fieldScanner.parsingIndex++;
          return true;
        }
      }
    }
    fieldScanner.parsingIndex = savedIndex;

    // Verify that the following consecutive two fields can match cc-exp-month and cc-exp-year
    // respectively.
    if (this._findMatchedFieldName(element, ["cc-exp-month"])) {
      fieldScanner.updateFieldName(fieldScanner.parsingIndex, "cc-exp-month");
      fieldScanner.parsingIndex++;
      if (!fieldScanner.parsingFinished) {
        const nextDetail = fieldScanner.getFieldDetailByIndex(
          fieldScanner.parsingIndex
        );
        const nextElement = nextDetail.elementWeakRef.get();
        if (this._findMatchedFieldName(nextElement, ["cc-exp-year"])) {
          fieldScanner.updateFieldName(
            fieldScanner.parsingIndex,
            "cc-exp-year"
          );
          fieldScanner.parsingIndex++;
          return true;
        }
      }
    }
    fieldScanner.parsingIndex = savedIndex;

    // Look for MM and/or YY(YY).
    if (this._matchRegexp(element, /^mm$/gi)) {
      fieldScanner.updateFieldName(fieldScanner.parsingIndex, "cc-exp-month");
      fieldScanner.parsingIndex++;
      if (!fieldScanner.parsingFinished) {
        const nextDetail = fieldScanner.getFieldDetailByIndex(
          fieldScanner.parsingIndex
        );
        const nextElement = nextDetail.elementWeakRef.get();
        if (this._matchRegexp(nextElement, /^(yy|yyyy)$/)) {
          fieldScanner.updateFieldName(
            fieldScanner.parsingIndex,
            "cc-exp-year"
          );
          fieldScanner.parsingIndex++;

          return true;
        }
      }
    }
    fieldScanner.parsingIndex = savedIndex;

    // Look for a cc-exp with 2-digit or 4-digit year.
    if (
      this._matchRegexp(
        element,
        /(?:exp.*date[^y\\n\\r]*|mm\\s*[-/]?\\s*)yy(?:[^y]|$)/gi
      ) ||
      this._matchRegexp(
        element,
        /(?:exp.*date[^y\\n\\r]*|mm\\s*[-/]?\\s*)yyyy(?:[^y]|$)/gi
      )
    ) {
      fieldScanner.updateFieldName(fieldScanner.parsingIndex, "cc-exp");
      fieldScanner.parsingIndex++;
      return true;
    }
    fieldScanner.parsingIndex = savedIndex;

    // Match general cc-exp regexp at last.
    if (this._findMatchedFieldName(element, ["cc-exp"])) {
      fieldScanner.updateFieldName(fieldScanner.parsingIndex, "cc-exp");
      fieldScanner.parsingIndex++;
      return true;
    }
    fieldScanner.parsingIndex = savedIndex;

    // Set current field name to null as it failed to match any patterns.
    fieldScanner.updateFieldName(fieldScanner.parsingIndex, null);
    fieldScanner.parsingIndex++;
    return true;
  },

  /**
   * This function should provide all field details of a form which are placed
   * in the belonging section. The details contain the autocomplete info
   * (e.g. fieldName, section, etc).
   *
   * `allowDuplicates` is used for the xpcshell-test purpose currently because
   * the heuristics should be verified that some duplicated elements still can
   * be predicted correctly.
   *
   * @param {HTMLFormElement} form
   *        the elements in this form to be predicted the field info.
   * @param {boolean} allowDuplicates
   *        true to remain any duplicated field details otherwise to remove the
   *        duplicated ones.
   * @returns {Array<Array<Object>>}
   *        all sections within its field details in the form.
   */
  getFormInfo(form, allowDuplicates = false) {
    const eligibleFields = Array.from(form.elements).filter(elem =>
      FormAutofillUtils.isFieldEligibleForAutofill(elem)
    );

    if (eligibleFields.length <= 0) {
      return [];
    }

    let fieldScanner = new FieldScanner(eligibleFields, {
      allowDuplicates,
      sectionEnabled: this._sectionEnabled,
    });
    while (!fieldScanner.parsingFinished) {
      let parsedPhoneFields = this._parsePhoneFields(fieldScanner);
      let parsedAddressFields = this._parseAddressFields(fieldScanner);
      let parsedExpirationDateFields = this._parseCreditCardFields(
        fieldScanner
      );

      // If there is no any field parsed, the parsing cursor can be moved
      // forward to the next one.
      if (
        !parsedPhoneFields &&
        !parsedAddressFields &&
        !parsedExpirationDateFields
      ) {
        fieldScanner.parsingIndex++;
      }
    }

    LabelUtils.clearLabelMap();

    return fieldScanner.getSectionFieldDetails();
  },

  _regExpTableHashValue(...signBits) {
    return signBits.reduce((p, c, i) => p | (!!c << i), 0);
  },

  _setRegExpListCache(regexps, b0, b1, b2) {
    if (!this._regexpList) {
      this._regexpList = [];
    }
    this._regexpList[this._regExpTableHashValue(b0, b1, b2)] = regexps;
  },

  _getRegExpListCache(b0, b1, b2) {
    if (!this._regexpList) {
      return null;
    }
    return this._regexpList[this._regExpTableHashValue(b0, b1, b2)] || null;
  },

  _getRegExpList(isAutoCompleteOff, elementTagName) {
    let isSelectElem = elementTagName == "SELECT";
    let regExpListCache = this._getRegExpListCache(
      isAutoCompleteOff,
      FormAutofill.isAutofillCreditCardsAvailable,
      isSelectElem
    );
    if (regExpListCache) {
      return regExpListCache;
    }
    const FIELDNAMES_IGNORING_AUTOCOMPLETE_OFF = [
      "cc-name",
      "cc-number",
      "cc-exp-month",
      "cc-exp-year",
      "cc-exp",
      "cc-type",
    ];
    let regexps = isAutoCompleteOff
      ? FIELDNAMES_IGNORING_AUTOCOMPLETE_OFF
      : Object.keys(this.RULES);

    if (!FormAutofill.isAutofillCreditCardsAvailable) {
      regexps = regexps.filter(
        name => !FormAutofillUtils.isCreditCardField(name)
      );
    }

    if (isSelectElem) {
      const FIELDNAMES_FOR_SELECT_ELEMENT = [
        "address-level1",
        "address-level2",
        "country",
        "cc-exp-month",
        "cc-exp-year",
        "cc-exp",
        "cc-type",
      ];
      regexps = regexps.filter(name =>
        FIELDNAMES_FOR_SELECT_ELEMENT.includes(name)
      );
    }

    this._setRegExpListCache(
      regexps,
      isAutoCompleteOff,
      FormAutofill.isAutofillCreditCardsAvailable,
      isSelectElem
    );

    return regexps;
  },

  getInfo(element) {
    let info = element.getAutocompleteInfo();
    // An input[autocomplete="on"] will not be early return here since it stll
    // needs to find the field name.
    if (
      info &&
      info.fieldName &&
      info.fieldName != "on" &&
      info.fieldName != "off"
    ) {
      info._reason = "autocomplete";
      return info;
    }

    if (!this._prefEnabled) {
      return null;
    }

    let isAutoCompleteOff =
      element.autocomplete == "off" ||
      (element.form && element.form.autocomplete == "off");

    // "email" type of input is accurate for heuristics to determine its Email
    // field or not. However, "tel" type is used for ZIP code for some web site
    // (e.g. HomeDepot, BestBuy), so "tel" type should be not used for "tel"
    // prediction.
    if (element.type == "email" && !isAutoCompleteOff) {
      return {
        fieldName: "email",
        section: "",
        addressType: "",
        contactType: "",
      };
    }

    let regexps = this._getRegExpList(isAutoCompleteOff, element.tagName);
    if (!regexps.length) {
      return null;
    }

    let matchedFieldName = this._findMatchedFieldName(element, regexps);
    if (matchedFieldName) {
      return {
        fieldName: matchedFieldName,
        section: "",
        addressType: "",
        contactType: "",
      };
    }

    return null;
  },

  /**
   * @typedef ElementStrings
   * @type {object}
   * @yield {string} id - element id.
   * @yield {string} name - element name.
   * @yield {Array<string>} labels - extracted labels.
   */

  /**
   * Extract all the signature strings of an element.
   *
   * @param {HTMLElement} element
   * @returns {ElementStrings}
   */
  _getElementStrings(element) {
    return {
      *[Symbol.iterator]() {
        yield element.id;
        yield element.name;

        const labels = LabelUtils.findLabelElements(element);
        for (let label of labels) {
          yield* LabelUtils.extractLabelStrings(label);
        }
      },
    };
  },

  /**
   * Find the first matched field name of the element wih given regex list.
   *
   * @param {HTMLElement} element
   * @param {Array<string>} regexps
   *        The regex key names that correspond to pattern in the rule list. It will
   *        be matched against the element string converted to lower case.
   * @returns {?string} The first matched field name
   */
  _findMatchedFieldName(element, regexps) {
    const getElementStrings = this._getElementStrings(element);
    for (let regexp of regexps) {
      for (let string of getElementStrings) {
        if (this.RULES[regexp].test(string.toLowerCase())) {
          return regexp;
        }
      }
    }

    return null;
  },

  /**
   * Determine whether the regexp can match any of element strings.
   *
   * @param {HTMLElement} element
   * @param {RegExp} regexp
   *
   * @returns {boolean}
   */
  _matchRegexp(element, regexp) {
    const elemStrings = this._getElementStrings(element);
    for (const str of elemStrings) {
      if (regexp.test(str)) {
        return true;
      }
    }
    return false;
  },

  /**
   * Phone field grammars - first matched grammar will be parsed. Grammars are
   * separated by { REGEX_SEPARATOR, FIELD_NONE, 0 }. Suffix and extension are
   * parsed separately unless they are necessary parts of the match.
   * The following notation is used to describe the patterns:
   * <cc> - country code field.
   * <ac> - area code field.
   * <phone> - phone or prefix.
   * <suffix> - suffix.
   * <ext> - extension.
   * :N means field is limited to N characters, otherwise it is unlimited.
   * (pattern <field>)? means pattern is optional and matched separately.
   *
   * This grammar list from Chromium will be enabled partially once we need to
   * support more cases of Telephone fields.
   */
  PHONE_FIELD_GRAMMARS: [
    // Country code: <cc> Area Code: <ac> Phone: <phone> (- <suffix>

    // (Ext: <ext>)?)?
    // {REGEX_COUNTRY, FIELD_COUNTRY_CODE, 0},
    // {REGEX_AREA, FIELD_AREA_CODE, 0},
    // {REGEX_PHONE, FIELD_PHONE, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // \( <ac> \) <phone>:3 <suffix>:4 (Ext: <ext>)?
    // {REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 3},
    // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
    // {REGEX_PHONE, FIELD_SUFFIX, 4},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc> <ac>:3 - <phone>:3 - <suffix>:4 (Ext: <ext>)?
    // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    // {REGEX_PHONE, FIELD_AREA_CODE, 3},
    // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
    // {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 4},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc>:3 <ac>:3 <phone>:3 <suffix>:4 (Ext: <ext>)?
    ["tel", "tel-country-code", 3],
    ["tel", "tel-area-code", 3],
    ["tel", "tel-local-prefix", 3],
    ["tel", "tel-local-suffix", 4],
    [null, null, 0],

    // Area Code: <ac> Phone: <phone> (- <suffix> (Ext: <ext>)?)?
    // {REGEX_AREA, FIELD_AREA_CODE, 0},
    // {REGEX_PHONE, FIELD_PHONE, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <ac> <phone>:3 <suffix>:4 (Ext: <ext>)?
    // {REGEX_PHONE, FIELD_AREA_CODE, 0},
    // {REGEX_PHONE, FIELD_PHONE, 3},
    // {REGEX_PHONE, FIELD_SUFFIX, 4},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc> \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
    // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    // {REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 0},
    // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
    // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    // {REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 0},
    // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc> - <ac> - <phone> - <suffix> (Ext: <ext>)?
    // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    // {REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE, 0},
    // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0},
    // {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Area code: <ac>:3 Prefix: <prefix>:3 Suffix: <suffix>:4 (Ext: <ext>)?
    // {REGEX_AREA, FIELD_AREA_CODE, 3},
    // {REGEX_PREFIX, FIELD_PHONE, 3},
    // {REGEX_SUFFIX, FIELD_SUFFIX, 4},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <ac> Prefix: <phone> Suffix: <suffix> (Ext: <ext>)?
    // {REGEX_PHONE, FIELD_AREA_CODE, 0},
    // {REGEX_PREFIX, FIELD_PHONE, 0},
    // {REGEX_SUFFIX, FIELD_SUFFIX, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <ac> - <phone>:3 - <suffix>:4 (Ext: <ext>)?
    ["tel", "tel-area-code", 0],
    ["tel", "tel-local-prefix", 3],
    ["tel", "tel-local-suffix", 4],
    [null, null, 0],

    // Phone: <cc> - <ac> - <phone> (Ext: <ext>)?
    // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
    // {REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE, 0},
    // {REGEX_SUFFIX_SEPARATOR, FIELD_PHONE, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <ac> - <phone> (Ext: <ext>)?
    // {REGEX_AREA, FIELD_AREA_CODE, 0},
    // {REGEX_PHONE, FIELD_PHONE, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc>:3 - <phone>:10 (Ext: <ext>)?
    // {REGEX_PHONE, FIELD_COUNTRY_CODE, 3},
    // {REGEX_PHONE, FIELD_PHONE, 10},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Ext: <ext>
    // {REGEX_EXTENSION, FIELD_EXTENSION, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <phone> (Ext: <ext>)?
    // {REGEX_PHONE, FIELD_PHONE, 0},
    // {REGEX_SEPARATOR, FIELD_NONE, 0},
  ],
};

XPCOMUtils.defineLazyGetter(FormAutofillHeuristics, "RULES", () => {
  let sandbox = {};
  const HEURISTICS_REGEXP = "resource://autofill/content/heuristicsRegexp.js";
  Services.scriptloader.loadSubScript(HEURISTICS_REGEXP, sandbox);
  return sandbox.HeuristicsRegExp.RULES;
});

XPCOMUtils.defineLazyGetter(FormAutofillHeuristics, "_prefEnabled", () => {
  return Services.prefs.getBoolPref(PREF_HEURISTICS_ENABLED);
});

Services.prefs.addObserver(PREF_HEURISTICS_ENABLED, () => {
  FormAutofillHeuristics._prefEnabled = Services.prefs.getBoolPref(
    PREF_HEURISTICS_ENABLED
  );
});

XPCOMUtils.defineLazyGetter(FormAutofillHeuristics, "_sectionEnabled", () => {
  return Services.prefs.getBoolPref(PREF_SECTION_ENABLED);
});

Services.prefs.addObserver(PREF_SECTION_ENABLED, () => {
  FormAutofillHeuristics._sectionEnabled = Services.prefs.getBoolPref(
    PREF_SECTION_ENABLED
  );
});
