/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill field heuristics.
 */

export let FormAutofillHeuristics;
import { FormAutofill } from "resource://autofill/FormAutofill.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CreditCard: "resource://gre/modules/CreditCard.sys.mjs",
  FormAutofillUtils: "resource://autofill/FormAutofillUtils.sys.mjs",
  LabelUtils: "resource://autofill/FormAutofillUtils.sys.mjs",
  creditCardRulesets: "resource://autofill/CreditCardRuleset.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "log", () =>
  FormAutofill.defineLogGetter(lazy, "FormAutofillHeuristics")
);

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
 * Represents the detailed information about a form field, including
 * the inferred field name, the approach used for inferring, and additional metadata.
 */
export class FieldDetail {
  // Reference to the elemenet
  elementWeakRef = null;

  // The inferred field name for this element
  fieldName = null;

  // The approach we use to infer the information for this element
  // The possible values are "autocomplete", "fathom", and "regex-heuristic"
  reason = null;

  /*
   * The "section", "addressType", and "contactType" values are
   * used to identify the exact field when the serializable data is received
   * from the backend.  There cannot be multiple fields which have
   * the same exact combination of these values.
   */
  section = "";
  addressType = "";
  contactType = "";

  // When a field is split into N fields, we use part to record which field it is
  // For example, a credit card number field is split into 4 fields, the value of
  // "part" for the first cc-number field is 1, for the last one is 4.
  // If the field is not split, the value is null
  part = null;

  // Confidence value when the field name is inferred by "fathom"
  confidence = null;

  constructor(
    element,
    fieldName,
    { autocompleteInfo = {}, confidence = null }
  ) {
    this.elementWeakRef = Cu.getWeakReference(element);
    this.fieldName = fieldName;

    if (autocompleteInfo) {
      this.reason = "autocomplete";
      this.section = autocompleteInfo.section;
      this.addressType = autocompleteInfo.addressType;
      this.contactType = autocompleteInfo.contactType;
    } else if (confidence) {
      this.reason = "fathom";
      this.confidence = confidence;
    } else {
      this.reason = "regex-heuristic";
    }
  }

  isSame(other) {
    return (
      this.fieldName == other.fieldName &&
      this.section == other.section &&
      this.addressType == other.addressType &&
      !this.part &&
      !other.part
    );
  }
}

/**
 * A scanner for traversing all elements in a form and retrieving the field
 * detail with FormAutofillHeuristics.getInferredInfo function. It also provides a
 * cursor (parsingIndex) to indicate which element is waiting for parsing.
 */
export class FieldScanner {
  #elementsWeakRef = null;

  #parsingIndex = 0;

  #sections = [];

  fieldDetails = [];

  /**
   * Create a FieldScanner based on form elements with the existing
   * fieldDetails.
   *
   * @param {Array.DOMElement} elements
   *        The elements from a form for each parser.
   */
  constructor(elements) {
    this.#elementsWeakRef = Cu.getWeakReference(elements);
  }

  get #elements() {
    return this.#elementsWeakRef.get();
  }

  /**
   * This cursor means the index of the element which is waiting for parsing.
   *
   * @returns {number}
   *          The index of the element which is waiting for parsing.
   */
  get parsingIndex() {
    return this.#parsingIndex;
  }

  /**
   * Move the parsingIndex to the next elements. Any elements behind this index
   * means the parsing tasks are finished.
   *
   * @param {number} index
   *        The latest index of elements waiting for parsing.
   */
  set parsingIndex(index) {
    if (index > this.#elements.length) {
      throw new Error("The parsing index is out of range.");
    }
    this.#parsingIndex = index;
  }

  /**
   * Retrieve the field detail by the index. If the field detail is not ready,
   * the elements will be traversed until matching the index.
   *
   * @param {number} index
   *        The index of the element that you want to retrieve.
   * @returns {object}
   *          The field detail at the specific index.
   */
  getFieldDetailByIndex(index) {
    if (index >= this.#elements.length) {
      throw new Error(
        `The index ${index} is out of range.(${this.#elements.length})`
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
    return this.parsingIndex >= this.#elements.length;
  }

  #pushToSection(name, fieldDetail) {
    const section = this.#sections.find(s => s.name == name);
    if (section) {
      section.fieldDetails.push(fieldDetail);
      return;
    }
    this.#sections.push({ name, fieldDetails: [fieldDetail] });
  }
  /**
   * Merges the next N fields if the currentType is in the list of MULTI_N_FIELD_NAMES
   *
   * @param {number} mergeNextNFields How many of the next N fields to merge into the current section
   * @param {string} currentType Type of the current field detail
   * @param {Array<object>} fieldDetails List of current field details
   * @param {number} i Index to keep track of the fieldDetails list
   * @param {boolean} createNewSection Determines if a new section should be created
   * @returns {Array<(number|boolean)>} mergeNextNFields and creatNewSection for use in #classifySections
   * @memberof FieldScanner
   */
  #mergeNextNFields(
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
      const nextN = MULTI_N_FIELD_NAMES[currentType] - 2;
      const array = fieldDetails.slice(i + 1, i + 1 + nextN);
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

  #classifySections() {
    const fieldDetails = this.#sections[0].fieldDetails;
    this.#sections = [];
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
          ({ mergeNextNFields, createNewSection } = this.#mergeNextNFields(
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
      this.#pushToSection(
        DEFAULT_SECTION_NAME + "-" + sectionCount,
        fieldDetails[i]
      );
    }
  }

  /**
   * The result is an array contains the sections with its belonging field
   * details. If `this.#sections` contains one section only with the default
   * section name (DEFAULT_SECTION_NAME), `this.#classifySections` should be
   * able to identify all sections in the heuristic way.
   *
   * @returns {Array<object>}
   *          The array with the sections, and the belonging fieldDetails are in
   *          each section. For example, it may return something like this:
   *          [{
   *             type: FormAutofillUtils.SECTION_TYPES.ADDRESS,  // section type
   *             fieldDetails: [{  // a record for each field
   *                 fieldName: "email",
   *                 section: "",
   *                 addressType: "",
   *                 contactType: "",
   *                 elementWeakRef: the element
   *               }, ...]
   *           },
   *           {
   *             type: FormAutofillUtils.SECTION_TYPES.CREDIT_CARD,
   *             fieldDetails: [{
   *                fieldName: "cc-exp-month",
   *                section: "",
   *                addressType: "",
   *                contactType: "",
   *                 elementWeakRef: the element
   *               }, ...]
   *           }]
   */
  getSectionFieldDetails() {
    if (!this.#sections.length) {
      return [];
    }
    if (
      this.#sections.length == 1 &&
      this.#sections[0].name == DEFAULT_SECTION_NAME
    ) {
      this.#classifySections();
    }

    return this.#sections.reduce((sections, current) => {
      sections.push(...this.#getFinalDetails(current.fieldDetails));
      return sections;
    }, []);
  }

  /**
   * This function will prepare an autocomplete info object with getInferredInfo
   * function and push the detail to fieldDetails property.
   * Any field will be pushed into `this.#sections` based on the section name
   * in `autocomplete` attribute.
   *
   * Any element without the related detail will be used for adding the detail
   * to the end of field details.
   */
  pushDetail() {
    const elementIndex = this.fieldDetails.length;
    if (elementIndex >= this.#elements.length) {
      throw new Error("Try to push the non-existing element info.");
    }
    const element = this.#elements[elementIndex];
    const [
      fieldName,
      autocompleteInfo,
      confidence,
    ] = FormAutofillHeuristics.getInferredInfo(element, this);
    const fieldDetail = new FieldDetail(element, fieldName, {
      autocompleteInfo,
      confidence,
    });

    this.fieldDetails.push(fieldDetail);
    this.#pushToSection(this.#getSectionName(fieldDetail), fieldDetail);
  }

  #getSectionName(info) {
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

  /**
   * When a site has four credit card number fields and
   * these fields have a max length of four
   * then we transform the credit card number into
   * four subsections in order to fill correctly.
   *
   * @param {Array<object>} creditCardFieldDetails
   *        The credit card field details to be transformed for multiple cc-number fields filling
   * @memberof FieldScanner
   */
  #transformCCNumberForMultipleFields(creditCardFieldDetails) {
    const details = creditCardFieldDetails.filter(
      field =>
        field.fieldName == "cc-number" &&
        field.elementWeakRef.get().maxLength == 4
    );
    if (details.length != 4) {
      return;
    }

    details.map((detail, idx) => {
      detail.part = idx + 1;
    });
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
   * @param   {Array<object>} fieldDetails
   *          The field details for trimming.
   * @returns {Array<object>}
   *          The array with the field details without invalid field name and
   *          duplicated fields.
   */
  #getFinalDetails(fieldDetails) {
    let addressFieldDetails = [];
    let creditCardFieldDetails = [];
    for (const fieldDetail of fieldDetails) {
      const fieldName = fieldDetail.fieldName;
      if (lazy.FormAutofillUtils.isAddressField(fieldName)) {
        addressFieldDetails.push(fieldDetail);
      } else if (lazy.FormAutofillUtils.isCreditCardField(fieldName)) {
        creditCardFieldDetails.push(fieldDetail);
      } else {
        lazy.log.debug(
          "Not collecting a field with a unknown fieldName",
          fieldDetail
        );
      }
    }
    dump(`[Dimi]getFinalDetails: ${JSON.stringify(addressFieldDetails)}\n`);
    this.#transformCCNumberForMultipleFields(creditCardFieldDetails);
    return [
      {
        type: lazy.FormAutofillUtils.SECTION_TYPES.ADDRESS,
        fieldDetails: addressFieldDetails,
      },
      {
        type: lazy.FormAutofillUtils.SECTION_TYPES.CREDIT_CARD,
        fieldDetails: creditCardFieldDetails,
      },
    ]
      .map(section => {
        // Deduplicate each set of fieldDetails
        const details = section.fieldDetails;
        section.fieldDetails = details.filter((detail, index) => {
          const previousFields = details.slice(0, index);
          return !previousFields.find(f => f.isSame(detail));
        });
        dump(`[Dimi]section:${JSON.stringify(section.fieldDetails)}\n`);
        return section;
      })
      .filter(section => !!section.fieldDetails.length);
  }

  elementExisting(index) {
    return index < this.#elements.length;
  }

  /**
   * Using Fathom, say what kind of CC field an element is most likely to be.
   * This function deoesn't only run fathom on the passed elements. It also
   * runs fathom for all elements in the FieldScanner for optimization purpose.
   *
   * @param {HTMLElement} element
   * @param {Array} fields
   * @returns {Array} A tuple of [field name, probability] describing the
   *   highest-confidence classification
   */
  getFathomField(element, fields) {
    if (!fields.length) {
      return [null, null];
    }

    if (!this._fathomConfidences?.get(element)) {
      this._fathomConfidences = new Map();

      let elements = [];
      if (this.#elements?.includes(element)) {
        elements = this.#elements;
      } else {
        elements = [element];
      }

      // This should not throw unless we run into an OOM situation, at which
      // point we have worse problems and this failing is not a big deal.
      const confidences = FieldScanner.getFormAutofillConfidences(elements);
      for (let i = 0; i < elements.length; i++) {
        this._fathomConfidences.set(elements[i], confidences[i]);
      }
    }

    const elementConfidences = this._fathomConfidences.get(element);
    if (!elementConfidences) {
      return [null, null];
    }

    let highestField = null;
    let highestConfidence = lazy.FormAutofillUtils.ccFathomConfidenceThreshold; // Start with a threshold of 0.5
    for (let [key, value] of Object.entries(elementConfidences)) {
      if (!fields.includes(key)) {
        // ignore field that we don't care
        continue;
      }

      if (value > highestConfidence) {
        highestConfidence = value;
        highestField = key;
      }
    }

    if (!highestField) {
      return [null, null];
    }

    // Used by test ONLY! This ensure testcases always get the same confidence
    if (lazy.FormAutofillUtils.ccFathomTestConfidence > 0) {
      highestConfidence = lazy.FormAutofillUtils.ccFathomTestConfidence;
    }

    return [highestField, highestConfidence];
  }

  /**
   * @param {Array} elements Array of elements that we want to get result from fathom cc rules
   * @returns {object} Fathom confidence keyed by field-type.
   */
  static getFormAutofillConfidences(elements) {
    if (
      lazy.FormAutofillUtils.ccHeuristicsMode ==
      lazy.FormAutofillUtils.CC_FATHOM_NATIVE
    ) {
      const confidences = ChromeUtils.getFormAutofillConfidences(elements);
      return confidences.map(c => {
        let result = {};
        for (let [fieldName, confidence] of Object.entries(c)) {
          let type = lazy.FormAutofillUtils.formAutofillConfidencesKeyToCCFieldType(
            fieldName
          );
          result[type] = confidence;
        }
        return result;
      });
    }

    return elements.map(element => {
      /**
       * Return how confident our ML model is that `element` is a field of the
       * given type.
       *
       * @param {string} fieldName The Fathom type to check against. This is
       *   conveniently the same as the autocomplete attribute value that means
       *   the same thing.
       * @returns {number} Confidence in range [0, 1]
       */
      function confidence(fieldName) {
        const ruleset = lazy.creditCardRulesets[fieldName];
        const fnodes = ruleset.against(element).get(fieldName);

        // fnodes is either 0 or 1 item long, since we ran the ruleset
        // against a single element:
        return fnodes.length ? fnodes[0].scoreFor(fieldName) : 0;
      }

      // Bang the element against the ruleset for every type of field:
      const confidences = {};
      lazy.creditCardRulesets.types.map(fieldName => {
        confidences[fieldName] = confidence(fieldName);
      });

      return confidences;
    });
  }
}

/**
 * Returns the autocomplete information of fields according to heuristics.
 */
FormAutofillHeuristics = {
  RULES: null,

  CREDIT_CARD_FIELDNAMES: [],
  ADDRESS_FIELDNAMES: [],
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
    if (!HTMLSelectElement.isInstance(element)) {
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
    if (!HTMLSelectElement.isInstance(element)) {
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
          detail?.reason == "autocomplete"
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
      nextField.reason != "autocomplete" &&
      fieldScanner.parsingIndex > 0
    ) {
      const regExpTelExtension = new RegExp(
        "\\bext|ext\\b|extension|ramal", // pt-BR, pt-PT
        "iu"
      );
      const previousField = fieldScanner.getFieldDetailByIndex(
        fieldScanner.parsingIndex - 1
      );
      const previousFieldType = lazy.FormAutofillUtils.getCategoryFromFieldName(
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
        detail.reason == "autocomplete"
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

  // The old heuristics can be removed when we fully adopt fathom, so disable the
  // esline complexity check for now
  /* eslint-disable complexity */
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
    if (!detail || detail?.reason == "autocomplete") {
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

    // The heuristic below should be covered by fathom rules, so we can skip doing
    // it.
    if (
      lazy.FormAutofillUtils.isFathomCreditCardsEnabled() &&
      lazy.creditCardRulesets.types.includes(detail.fieldName)
    ) {
      fieldScanner.parsingIndex++;
      return true;
    }

    const element = detail.elementWeakRef.get();

    // If we didn't auto-discover type field, check every select for options that
    // match credit card network names in value or label.
    if (HTMLSelectElement.isInstance(element)) {
      for (let option of element.querySelectorAll("option")) {
        if (
          lazy.CreditCard.getNetworkFromName(option.value) ||
          lazy.CreditCard.getNetworkFromName(option.text)
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
   * @param {HTMLFormElement} form
   *        the elements in this form to be predicted the field info.
   * @returns {Array<Array<object>>}
   *        all sections within its field details in the form.
   */
  getFormInfo(form) {
    const eligibleFields = Array.from(form.elements).filter(elem =>
      lazy.FormAutofillUtils.isCreditCardOrAddressFieldType(elem)
    );

    if (eligibleFields.length <= 0) {
      return [];
    }

    let fieldScanner = new FieldScanner(eligibleFields);
    while (!fieldScanner.parsingFinished) {
      let parsedPhoneFields = this._parsePhoneFields(fieldScanner);
      let parsedAddressFields = this._parseAddressFields(fieldScanner);
      let parsedExpirationDateFields = this._parseCreditCardFields(
        fieldScanner
      );

      // If there is no field parsed, the parsing cursor can be moved
      // forward to the next one.
      if (
        !parsedPhoneFields &&
        !parsedAddressFields &&
        !parsedExpirationDateFields
      ) {
        fieldScanner.parsingIndex++;
      }
    }

    lazy.LabelUtils.clearLabelMap();

    return fieldScanner.getSectionFieldDetails();
  },

  _getPossibleFieldNames(element) {
    let fieldNames = [];
    const isAutoCompleteOff =
      element.autocomplete == "off" || element.form?.autocomplete == "off";
    if (
      FormAutofill.isAutofillCreditCardsAvailable &&
      (!isAutoCompleteOff || FormAutofill.creditCardsAutocompleteOff)
    ) {
      fieldNames.push(...this.CREDIT_CARD_FIELDNAMES);
    }
    if (
      FormAutofill.isAutofillAddressesAvailable &&
      (!isAutoCompleteOff || FormAutofill.addressesAutocompleteOff)
    ) {
      fieldNames.push(...this.ADDRESS_FIELDNAMES);
    }

    if (HTMLSelectElement.isInstance(element)) {
      const FIELDNAMES_FOR_SELECT_ELEMENT = [
        "address-level1",
        "address-level2",
        "country",
        "cc-exp-month",
        "cc-exp-year",
        "cc-exp",
        "cc-type",
      ];
      fieldNames = fieldNames.filter(name =>
        FIELDNAMES_FOR_SELECT_ELEMENT.includes(name)
      );
    }

    return fieldNames;
  },

  /**
   * Get inferred information about an input element using autocomplete info, fathom and regex-based heuristics.
   *
   * @param {HTMLElement} element - The input element to infer information about.
   * @param {object} scanner - Scanner object used to analyze elements with fathom.
   * @returns {Array} - An array containing:
   *                    [0]the inferred field name
   *                    [1]autocomplete information if the element has autocompelte attribute, null otherwise.
   *                    [2]fathom confidence if fathom considers it a cc field, null otherwise.
   */
  getInferredInfo(element, scanner) {
    const autocompleteInfo = element.getAutocompleteInfo();

    // An input[autocomplete="on"] will not be early return here since it stll
    // needs to find the field name.
    if (
      autocompleteInfo?.fieldName &&
      !["on", "off"].includes(autocompleteInfo.fieldName)
    ) {
      return [autocompleteInfo.fieldName, autocompleteInfo, null];
    }

    const fields = this._getPossibleFieldNames(element);

    // "email" type of input is accurate for heuristics to determine its Email
    // field or not. However, "tel" type is used for ZIP code for some web site
    // (e.g. HomeDepot, BestBuy), so "tel" type should be not used for "tel"
    // prediction.
    if (element.type == "email" && fields.includes("email")) {
      return ["email", null, null];
    }

    if (lazy.FormAutofillUtils.isFathomCreditCardsEnabled()) {
      // We don't care fields that are not supported by fathom
      const fathomFields = fields.filter(r =>
        lazy.creditCardRulesets.types.includes(r)
      );
      const [matchedFieldName, confidence] = scanner.getFathomField(
        element,
        fathomFields
      );
      // At this point, use fathom's recommendation if it has one
      if (matchedFieldName) {
        return [matchedFieldName, null, confidence];
      }

      // Continue to run regex-based heuristics even when fathom doesn't recognize
      // the field. Since the regex-based heuristic has good search coverage but
      // has a worse precision. We use it in conjunction with fathom to maximize
      // our search coverage. For example, when a <input> is not considered cc-name
      // by fathom but is considered cc-name by regex-based heuristic, if the form
      // also contains a cc-number identified by fathom, we will treat the form as a
      // valid cc form; hence both cc-number & cc-name are identified.
    }

    if (fields.length) {
      // Find a matched field name using regex-based heuristics
      const matchedFieldName = this._findMatchedFieldName(element, fields);
      if (matchedFieldName) {
        return [matchedFieldName, null, null];
      }
    }

    return [null, null, null];
  },

  /**
   * @typedef ElementStrings
   * @type {object}
   * @yields {string} id - element id.
   * @yields {string} name - element name.
   * @yields {Array<string>} labels - extracted labels.
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
        yield element.placeholder?.trim();

        const labels = lazy.LabelUtils.findLabelElements(element);
        for (let label of labels) {
          yield* lazy.LabelUtils.extractLabelStrings(label);
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
        if (this.RULES[regexp].test(string?.toLowerCase())) {
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

XPCOMUtils.defineLazyGetter(
  FormAutofillHeuristics,
  "CREDIT_CARD_FIELDNAMES",
  () =>
    Object.keys(FormAutofillHeuristics.RULES).filter(name =>
      lazy.FormAutofillUtils.isCreditCardField(name)
    )
);

XPCOMUtils.defineLazyGetter(FormAutofillHeuristics, "ADDRESS_FIELDNAMES", () =>
  Object.keys(FormAutofillHeuristics.RULES).filter(name =>
    lazy.FormAutofillUtils.isAddressField(name)
  )
);
