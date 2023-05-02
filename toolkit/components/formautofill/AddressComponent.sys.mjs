/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FormAutofill } from "resource://autofill/FormAutofill.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
  PhoneNumber: "resource://autofill/phonenumberutils/PhoneNumber.sys.mjs",
});

class AddressField {
  constructor(collator) {
    this.collator = collator;
  }

  isEmpty() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  isSame() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  include() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  toString() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }
}

/**
 * TODO: This is only a prototype now
 * Bug 1820526 - Implement Address field Deduplication algorithm
 */
class StreetAddress extends AddressField {
  street_address = null;

  constructor(record, collator) {
    super(collator);

    // TODO: Convert street-address to "hause number", "street name", "floor" and "apartment number"
    this.street_address = record["street-address"];
  }

  isEmpty() {
    return !this.street_address;
  }

  isSame(field) {
    return lazy.FormAutofillUtils.compareStreetAddress(
      this.street_address,
      field.street_address,
      this.collator
    );
  }

  include(field) {
    return this.isSame(field);
  }

  toString() {
    return `Street Address: ${this.street_address}\n`;
  }
}

/**
 * TODO: This class is only a prototype now
 * Bug 1820526 - Implement Address field Deduplication algorithm
 */
class PostalCode extends AddressField {
  postal_code = null;

  constructor(record, collator) {
    super(collator);

    this.postal_code = record["postal-code"];
  }

  isEmpty() {
    return !this.postal_code;
  }

  isSame(field) {
    return this.postal_code == field.postal_code;
  }

  include(field) {
    return this.isSame(field);
  }

  toString() {
    return `Postal Code: ${this.postal_code}\n`;
  }
}

/**
 * TODO: This is only a prototype now
 * Bug 1820526 - Implement Address field Deduplication algorithm
 */
class Country extends AddressField {
  country = null;

  constructor(record, collator) {
    super(collator);

    this.country = record.country;
  }

  // TODO: Support isEmpty, isSame and includes
  isEmpty() {
    return !this.country;
  }

  isSame(field) {
    return this.country?.toLowerCase() == field.country?.toLowerCase();
  }

  include(field) {
    return this.isSame(field);
  }

  toString() {
    return `Country: ${this.country}\n`;
  }
}

/**
 * TODO: This is only a prototype now
 * Bug 1820525 - Implement Name field Deduplication algorithm
 */
class Name extends AddressField {
  given = null;
  additional = null;
  family = null;

  constructor(record, collator) {
    super(collator);

    this.given = record["given-name"] ?? "";
    this.additional = record["additional-name"] ?? "";
    this.family = record["family-name"] ?? "";
  }

  // TODO: Support isEmpty, isSame and includes
  isEmpty() {
    return !this.given && !this.additional && !this.family;
  }

  // TODO: Consider Name Variant
  isSame(field) {
    return (
      lazy.FormAutofillUtils.strCompare(
        this.given,
        field.given,
        this.collator
      ) &&
      lazy.FormAutofillUtils.strCompare(
        this.additional,
        field.additional,
        this.collator
      ) &&
      lazy.FormAutofillUtils.strCompare(
        this.family,
        field.family,
        this.collator
      )
    );
  }

  include(field) {
    return (
      (!field.given ||
        lazy.FormAutofillUtils.strInclude(
          this.given,
          field.given,
          this.collator
        )) &&
      (!field.additional ||
        lazy.FormAutofillUtils.strInclude(
          this.additional,
          field.additional,
          this.collator
        )) &&
      (!field.family ||
        lazy.FormAutofillUtils.strInclude(
          this.family,
          field.family,
          this.collator
        ))
    );
  }

  toString() {
    return (
      `Given Name: ${this.given}\n` +
      `Middle Name: ${this.additional}\n` +
      `Family Name: ${this.family}\n`
    );
  }
}

/**
 * TODO: This is only a prototype now
 * Bug 1820524 - Implement Telephone field Deduplication algorithm
 */
class Tel extends AddressField {
  country = null;
  national = null;

  constructor(record, collator) {
    super(collator);

    // We compress all tel-related fields into a single tel field when an an form
    // is submitted, so we need to decompress it here.
    let parsedTel = lazy.PhoneNumber.Parse(record.tel);
    this.country = parsedTel?.countryCode;
    this.national = parsedTel?.nationalNumber;
  }

  isEmpty() {
    return !this.country && !this.national;
  }

  isSame(field) {
    return this.country == field.country && this.national == field.national;
  }

  include(field) {
    if (this.country != field.country && field.country) {
      return false;
    }

    return this.national == field.national;
  }

  toString() {
    return `Telephone Number: ${this.country} ${this.national}\n`;
  }
}

/**
 * TODO: This is only a prototype now
 * Bug 1820523 - Implement Organiztion field Deduplication algorithm
 */
class Organization extends AddressField {
  constructor(record, collator) {
    super(collator);
    this.organization = record.organization ?? "";
  }

  // TODO: Support isEmpty, isSame and includes
  isEmpty() {
    return !this.organization;
  }

  isSame(field) {
    let x = lazy.FormAutofillUtils.strCompare(
      this.organization,
      field.organization,
      this.collator
    );
    return x;
  }

  include(field) {
    // TODO: Split organization into components
    return lazy.FormAutofillUtils.strInclude(
      this.organization,
      field.organization,
      this.collator
    );
  }

  toString() {
    return `Company Name: ${this.organization}\n`;
  }
}

/**
 * TODO: This is only a prototype now
 * Bug 1820522 - Implement Email field Deduplication algorithm
 */
class Email extends AddressField {
  constructor(record, collator) {
    super(collator);
    this.email = record.email ?? "";
  }

  isEmpty() {
    return !this.email;
  }

  isSame(field) {
    return this.email?.toLowerCase() == field?.email.toLowerCase();
  }

  // TODO: include should be rename to isMergeable
  include(field) {
    // TODO: Do we want to popup update doorhanger when test@gmail.com -> test2@gmail.com
    return this.isSame(field);
  }

  toString() {
    return `Email: ${this.email}\n`;
  }
}

/**
 * Class to compare two address components and store their comparison result.
 */
export class AddressComparison {
  // Store the comparion result in an object, keyed by field name.
  #result = {};

  constructor(addressA, addressB) {
    for (const fieldA of addressA.getAllFields()) {
      const fieldName = fieldA.constructor.name;
      let fieldB = addressB.getField(fieldName);
      this.#result[fieldName] = AddressComparison.compareFields(fieldA, fieldB);
    }
  }

  // Const to define the comparison result for two given fields
  static BOTH_EMPTY = 0;
  static A_IS_EMPTY = 1;
  static B_IS_EMPTY = 2;
  static A_IS_SUBSET_OF_B = 3;
  static A_IS_SUPERSET_OF_B = 4;
  static SAME = 5;
  static DIFFERENT = 6;

  static isFieldDifferent(result) {
    return [AddressComparison.DIFFERENT].includes(result);
  }

  static isFieldMergeable(result) {
    return [
      AddressComparison.A_IS_SUPERSET_OF_B,
      AddressComparison.B_IS_EMPTY,
    ].includes(result);
  }
  /**
   * Compare two address fields and return the comparison result
   */
  static compareFields(fieldA, fieldB) {
    if (fieldA.isEmpty()) {
      return fieldB.isEmpty()
        ? AddressComparison.BOTH_EMPTY
        : AddressComparison.A_IS_EMPTY;
    } else if (fieldB.isEmpty()) {
      return AddressComparison.B_IS_EMPTY;
    }

    if (fieldA.isSame(fieldB)) {
      return AddressComparison.SAME;
    }

    if (fieldB.include(fieldA)) {
      return AddressComparison.A_IS_SUBSET_OF_B;
    }

    if (fieldA.include(fieldB)) {
      return AddressComparison.A_IS_SUPERSET_OF_B;
    }

    return AddressComparison.DIFFERENT;
  }

  /**
   * Determine whether addressA is a duplicate of addressB
   *
   * An address is considered as duplicated of another address when every field
   * in the address are either the same or is subset of another address.
   *
   * @returns {boolean} True if address1 is a duplicate of address2
   */
  isDuplicate() {
    return Object.values(this.#result).every(v =>
      [
        AddressComparison.BOTH_EMPTY,
        AddressComparison.SAME,
        AddressComparison.A_IS_EMPTY,
        AddressComparison.A_IS_SUBSET_OF_B,
      ].includes(v)
    );
  }

  /**
   * Determine whether addressA may be merged into addressB
   *
   * An address can be merged into another address when none of any
   * fields are different and the address has at least one mergeable
   * field.
   *
   * @returns {boolean} True if address1 can be merged into address2
   */
  isMergeable() {
    let hasMergeableField = false;
    for (const result of Object.values(this.#result)) {
      // As long as any of the field is different, these two addresses are not mergeable
      if (AddressComparison.isFieldDifferent(result)) {
        return false;
      } else if (AddressComparison.isFieldMergeable(result)) {
        hasMergeableField = true;
      }
    }
    return hasMergeableField;
  }

  /**
   * Return fields that are mergeable. note that this function doesn't consider
   * whether two address component may be merged. If you want to get mergeable
   * fields when two address are mergeable, call isMergeable first.
   *
   * @returns {Array} fields of address1 that can be merged into address2
   */
  getMergeableFields() {
    return Object.entries(this.#result)
      .filter(e => AddressComparison.isFieldMergeable(e[1]))
      .map(e => e[0]);
  }

  /**
   * For debugging
   */
  toString() {
    function resultToString(result) {
      switch (result) {
        case AddressComparison.BOTH_EMPTY:
          return "both fields are empty";
        case AddressComparison.A_IS_EMPTY:
          return "field A is empty";
        case AddressComparison.B_IS_EMPTY:
          return "field B is empty";
        case AddressComparison.A_IS_SUBSET_OF_B:
          return "field A is a subset of field B";
        case AddressComparison.A_IS_SUPERSET_OF_B:
          return "field A is a superset of field B";
        case AddressComparison.SAME:
          return "two fields are the same";
        case AddressComparison.DIFFERENT:
          return "two fields are different";
      }
      return "";
    }
    let ret = "Comparison Result:\n";
    for (const [name, result] of Object.entries(this.#result)) {
      ret += `${name}: ${resultToString(result)}\n`;
    }
    return ret;
  }
}

/**
 * Class that transforms record (created in FormAutofillHandler createRecord)
 * into an address component object to more easily compare two address records.
 *
 * Note. This class assumes records that pass to it have already been normalized.
 */
export class AddressComponent {
  #fields = [];

  constructor(record) {
    const region = record.country ?? FormAutofill.DEFAULT_REGION;
    const collator = lazy.FormAutofillUtils.getSearchCollators(region);

    this.#fields.push(new StreetAddress(record, collator));
    this.#fields.push(new Country(record, collator));
    this.#fields.push(new PostalCode(record, collator));
    this.#fields.push(new Name(record, collator));
    this.#fields.push(new Tel(record, collator));
    this.#fields.push(new Organization(record, collator));
    this.#fields.push(new Email(record, collator));
  }

  getField(name) {
    return this.#fields.find(field => field.constructor.name == name);
  }

  getAllFields() {
    return this.#fields;
  }

  /**
   * For debugging
   */
  toString() {
    let ret = "";
    for (const field of this.#fields) {
      ret += field.toString();
    }
    return ret;
  }
}
