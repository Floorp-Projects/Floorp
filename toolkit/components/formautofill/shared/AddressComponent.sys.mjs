/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FormAutofill } from "resource://autofill/FormAutofill.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddressParser: "resource://gre/modules/shared/AddressParser.sys.mjs",
  FormAutofillNameUtils:
    "resource://gre/modules/shared/FormAutofillNameUtils.sys.mjs",
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
  PhoneNumber: "resource://autofill/phonenumberutils/PhoneNumber.sys.mjs",
  PhoneNumberNormalizer:
    "resource://autofill/phonenumberutils/PhoneNumberNormalizer.sys.mjs",
});

/**
 * Class representing a collection of tokens extracted from a string.
 */
class Tokens {
  #tokens = null;

  // By default we split passed string with whitespace.
  constructor(value, sep = /\s+/) {
    this.#tokens = value.split(sep);
  }

  get tokens() {
    return this.#tokens;
  }

  /**
   * Checks if all the tokens in the current object can be found in another
   * token object.
   *
   * @param   {Tokens}   other   The other Tokens instance to compare with.
   * @param   {Function} compare An optional custom comparison function.
   * @returns {boolean}          True if the current Token object is a subset of the
   *                             other Token object, false otherwise.
   */
  isSubset(other, compare = (a, b) => a == b) {
    return this.tokens.every(tokenSelf => {
      for (const tokenOther of other.tokens) {
        if (compare(tokenSelf, tokenOther)) {
          return true;
        }
      }
      return false;
    });
  }

  /**
   * Checks if all the tokens in the current object can be found in another
   * Token object's tokens (in order).
   * For example, ["John", "Doe"] is a subset of ["John", "Michael", "Doe"]
   * in order but not a subset of ["Doe", "Michael", "John"] in order.
   *
   * @param   {Tokens}   other   The other Tokens instance to compare with.
   * @param   {Function} compare An optional custom comparison function.
   * @returns {boolean}          True if the current Token object is a subset of the
   *                             other Token object, false otherwise.
   */
  isSubsetInOrder(other, compare = (a, b) => a == b) {
    if (this.tokens.length > other.tokens.length) {
      return false;
    }

    let idx = 0;
    return this.tokens.every(tokenSelf => {
      for (; idx < other.tokens.length; idx++) {
        if (compare(tokenSelf, other.tokens[idx])) {
          return true;
        }
      }
      return false;
    });
  }
}

/**
 * The AddressField class is a base class representing a single address field.
 */
class AddressField {
  #userValue = null;

  #region = null;

  /**
   * Create a representation of a single address field.
   *
   * @param {string} value
   *        The unnormalized value of an address field.
   *
   * @param {string} region
   *        The region of a single address field. Used to determine what collator should be
   *        for string comparisons of the address's field value.
   */
  constructor(value, region) {
    this.#userValue = value?.trim();
    this.#region = region;
  }

  /**
   * Get the unnormalized value of the address field.
   *
   * @returns {string} The unnormalized field value.
   */
  get userValue() {
    return this.#userValue;
  }

  /**
   * Get the collator used for string comparisons.
   *
   * @returns {Intl.Collator} The collator.
   */
  get collator() {
    return lazy.FormAutofillUtils.getSearchCollators(this.#region, {
      ignorePunctuation: false,
    });
  }

  get region() {
    return this.#region;
  }

  /**
   * Compares two strings using the collator.
   *
   * @param   {string} a The first string to compare.
   * @param   {string} b The second string to compare.
   * @returns {number} A negative, zero, or positive value, depending on the comparison result.
   */
  localeCompare(a, b) {
    return lazy.FormAutofillUtils.strCompare(a, b, this.collator);
  }

  /**
   * Checks if the field value is empty.
   *
   * @returns {boolean} True if the field value is empty, false otherwise.
   */
  isEmpty() {
    return !this.#userValue;
  }

  /**
   * Normalizes the unnormalized field value using the provided options.
   *
   * @param {object} options - Options for normalization.
   * @returns {string} The normalized field value.
   */
  normalizeUserValue(options) {
    return lazy.AddressParser.normalizeString(this.#userValue, options);
  }

  /**
   * Returns a string representation of the address field.
   * Ex. "Country: US", "PostalCode: 55123", etc.
   */
  toString() {
    return `${this.constructor.name}: ${this.#userValue}\n`;
  }

  /**
   * Checks if the field value is valid.
   *
   * @returns {boolean} True if the field value is valid, false otherwise.
   */
  isValid() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  /**
   * Compares the current field value with another field value for equality.
   */
  equals() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  /**
   * Checks if the current field value contains another field value.
   */
  contains() {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }
}

/**
 * A street address.
 * See autocomplete="street-address".
 */
class StreetAddress extends AddressField {
  static ac = "street-address";

  #structuredStreetAddress = null;

  constructor(value, region) {
    super(value, region);

    this.#structuredStreetAddress = lazy.AddressParser.parseStreetAddress(
      lazy.AddressParser.replaceControlCharacters(this.userValue, " ")
    );
  }

  get structuredStreetAddress() {
    return this.#structuredStreetAddress;
  }
  get street_number() {
    return this.#structuredStreetAddress?.street_number;
  }
  get street_name() {
    return this.#structuredStreetAddress?.street_name;
  }
  get floor_number() {
    return this.#structuredStreetAddress?.floor_number;
  }
  get apartment_number() {
    return this.#structuredStreetAddress?.apartment_number;
  }

  isValid() {
    return this.userValue ? !!/[\p{Letter}]/u.exec(this.userValue) : true;
  }

  equals(other) {
    if (this.structuredStreetAddress && other.structuredStreetAddress) {
      return (
        this.street_number?.toLowerCase() ==
          other.street_number?.toLowerCase() &&
        this.street_name?.toLowerCase() == other.street_name?.toLowerCase() &&
        this.apartment_number?.toLowerCase() ==
          other.apartment_number?.toLowerCase() &&
        this.floor_number?.toLowerCase() == other.floor_number?.toLowerCase()
      );
    }

    const options = {
      ignore_case: true,
    };

    return (
      this.normalizeUserValue(options) == other.normalizeUserValue(options)
    );
  }

  contains(other) {
    let selfStreetName = this.userValue;
    let otherStreetName = other.userValue;

    // Compare street number, apartment number and floor number if
    // both addresses are parsed successfully.
    if (this.structuredStreetAddress && other.structuredStreetAddress) {
      if (
        (other.street_number && this.street_number != other.street_number) ||
        (other.apartment_number &&
          this.apartment_number != other.apartment_number) ||
        (other.floor_number && this.floor_number != other.floor_number)
      ) {
        return false;
      }

      // Use parsed street name to compare
      selfStreetName = this.street_name;
      otherStreetName = other.street_name;
    }

    // Check if one street name contains the other
    const options = {
      ignore_case: true,
      replace_punctuation: " ",
    };
    const selfTokens = new Tokens(
      lazy.AddressParser.normalizeString(selfStreetName, options),
      /[\s\n\r]+/
    );
    const otherTokens = new Tokens(
      lazy.AddressParser.normalizeString(otherStreetName, options),
      /[\s\n\r]+/
    );

    return otherTokens.isSubsetInOrder(selfTokens, (a, b) =>
      this.localeCompare(a, b)
    );
  }

  static fromRecord(record, region) {
    return new StreetAddress(record[StreetAddress.ac], region);
  }
}

/**
 * A postal code / zip code
 * See autocomplete="postal-code"
 */
class PostalCode extends AddressField {
  static ac = "postal-code";

  constructor(value, region) {
    super(value, region);
  }

  isValid() {
    const { postalCodePattern } = lazy.FormAutofillUtils.getFormFormat(
      this.region
    );
    const regexp = new RegExp(`^${postalCodePattern}$`);
    return regexp.test(this.userValue);
  }

  equals(other) {
    const options = {
      ignore_case: true,
      remove_whitespace: true,
      remove_punctuation: true,
    };

    return (
      this.normalizeUserValue(options) == other.normalizeUserValue(options)
    );
  }

  contains(other) {
    const options = {
      ignore_case: true,
      remove_whitespace: true,
      remove_punctuation: true,
    };

    const self_normalized_value = this.normalizeUserValue(options);
    const other_normalized_value = other.normalizeUserValue(options);

    return (
      self_normalized_value.endsWith(other_normalized_value) ||
      self_normalized_value.startsWith(other_normalized_value)
    );
  }

  static fromRecord(record, region) {
    return new PostalCode(record[PostalCode.ac], region);
  }
}

/**
 * City name.
 * See autocomplete="address-level2"
 */
class City extends AddressField {
  static ac = "address-level2";

  #city = null;

  constructor(value, region) {
    super(value, region);

    const options = {
      ignore_case: true,
    };
    this.#city = this.normalizeUserValue(options);
  }

  get city() {
    return this.#city;
  }

  isValid() {
    return this.userValue ? !!/[\p{Letter}]/u.exec(this.userValue) : true;
  }

  equals(other) {
    return this.city == other.city;
  }

  contains(other) {
    const options = {
      ignore_case: true,
      replace_punctuation: " ",
      merge_whitespace: true,
    };

    const selfTokens = new Tokens(this.normalizeUserValue(options));
    const otherTokens = new Tokens(other.normalizeUserValue(options));

    return otherTokens.isSubsetInOrder(selfTokens, (a, b) =>
      this.localeCompare(a, b)
    );
  }

  static fromRecord(record, region) {
    return new City(record[City.ac], region);
  }
}

/**
 * State.
 * See autocomplete="address-level1"
 */
class State extends AddressField {
  static ac = "address-level1";

  // The abbreviated region name. For example, California is abbreviated as CA
  #state = null;

  constructor(value, region) {
    super(value, region);

    if (!this.userValue) {
      return;
    }

    const options = {
      merge_whitespace: true,
      remove_punctuation: true,
    };
    this.#state = lazy.FormAutofillUtils.getAbbreviatedSubregionName(
      this.normalizeUserValue(options),
      region
    );
  }

  get state() {
    return this.#state;
  }

  isValid() {
    // If we can't get the abbreviated name, assume this is an invalid state name
    return !!this.#state;
  }

  equals(other) {
    // If we have an abbreviated name, compare with it.
    if (this.state) {
      return this.state == other.state;
    }

    // If we don't have an abbreviated name, just compare the userValue
    return this.userValue == other.userValue;
  }

  contains(other) {
    return this.equals(other);
  }

  static fromRecord(record, region) {
    return new State(record[State.ac], region);
  }
}

/**
 * A country or territory code.
 * See autocomplete="country"
 */
class Country extends AddressField {
  static ac = "country";

  // iso 3166 2-alpha code
  #country_code = null;

  constructor(value, region) {
    super(value, region);

    if (this.isEmpty()) {
      return;
    }

    const options = {
      merge_whitespace: true,
      remove_punctuation: true,
    };

    const country = this.normalizeUserValue(options);
    this.#country_code = lazy.FormAutofillUtils.identifyCountryCode(country);

    // When the country name is not a valid one, we use the current region instead
    if (!this.#country_code) {
      this.#country_code = lazy.FormAutofillUtils.identifyCountryCode(region);
    }
  }

  get country_code() {
    return this.#country_code;
  }

  isValid() {
    return !!this.#country_code;
  }

  equals(other) {
    return this.country_code == other.country_code;
  }

  contains(other) {
    return false;
  }

  static fromRecord(record, region) {
    return new Country(record[Country.ac], region);
  }
}

/**
 * The field expects the value to be a person's full name.
 * See autocomplete="name"
 */
class Name extends AddressField {
  static ac = "name";

  constructor(value, region) {
    super(value, region);
  }

  // Reference:
  // https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/data_model/autofill_profile_comparator.cc;drc=566369da19275cc306eeb51a3d3451885299dabb;bpv=1;bpt=1;l=935
  static createNameVariants(name) {
    let tokens = name.trim().split(" ");

    let variants = [""];
    if (!tokens[0]) {
      return variants;
    }

    for (const token of tokens) {
      let tmp = [];
      for (const variant of variants) {
        tmp.push(variant + " " + token);
        tmp.push(variant + " " + token[0]);
      }
      variants = variants.concat(tmp);
    }

    const options = {
      merge_whitespace: true,
    };
    return variants.map(v => lazy.AddressParser.normalizeString(v, options));
  }

  isValid() {
    return this.userValue ? !!/[\p{Letter}]/u.exec(this.userValue) : true;
  }

  equals(other) {
    const options = {
      ignore_case: true,
    };
    return (
      this.normalizeUserValue(options) == other.normalizeUserValue(options)
    );
  }

  contains(other) {
    // Unify puncutation while comparing so users can choose the right one
    // if the only different part is puncutation
    // Ex. John O'Brian is similar to John O`Brian
    let options = {
      ignore_case: true,
      replace_punctuation: " ",
      merge_whitespace: true,
    };
    let selfName = this.normalizeUserValue(options);
    let otherName = other.normalizeUserValue(options);
    let selfTokens = new Tokens(selfName);
    let otherTokens = new Tokens(otherName);

    if (
      otherTokens.isSubsetInOrder(selfTokens, (a, b) =>
        this.localeCompare(a, b)
      )
    ) {
      return true;
    }

    // Remove puncutation from self and test whether current contains other
    // Ex. John O'Brian is similar to John OBrian
    selfName = this.normalizeUserValue({
      ignore_case: true,
      remove_punctuation: true,
      merge_whitespace: true,
    });
    otherName = other.normalizeUserValue({
      ignore_case: true,
      remove_punctuation: true,
      merge_whitespace: true,
    });

    selfTokens = new Tokens(selfName);
    otherTokens = new Tokens(otherName);
    if (
      otherTokens.isSubsetInOrder(selfTokens, (a, b) =>
        this.localeCompare(a, b)
      )
    ) {
      return true;
    }

    // Create variants of the names by generating initials for given and middle names.

    selfName = lazy.FormAutofillNameUtils.splitName(selfName);
    otherName = lazy.FormAutofillNameUtils.splitName(otherName);
    // In the following we compare cases when people abbreviate first name
    // and middle name with initials. So if family name is different,
    // we can just skip and assume the two names are different
    if (!this.localeCompare(selfName.family, otherName.family)) {
      return false;
    }

    const otherNameWithoutFamily = lazy.FormAutofillNameUtils.joinNameParts({
      given: otherName.given,
      middle: otherName.middle,
    });
    let givenVariants = Name.createNameVariants(selfName.given);
    let middleVariants = Name.createNameVariants(selfName.middle);

    for (const given of givenVariants) {
      for (const middle of middleVariants) {
        const nameVariant = lazy.FormAutofillNameUtils.joinNameParts({
          given,
          middle,
        });

        if (this.localeCompare(nameVariant, otherNameWithoutFamily)) {
          return true;
        }
      }
    }

    // Check cases when given name and middle name are abbreviated with initial
    // and the initials are put together. ex. John Michael Doe to JM. Doe
    if (selfName.given && selfName.middle) {
      const nameVariant = [
        ...selfName.given.split(" "),
        ...selfName.middle.split(" "),
      ].reduce((initials, name) => {
        initials += name[0];
        return initials;
      }, "");

      if (this.localeCompare(nameVariant, otherNameWithoutFamily)) {
        return true;
      }
    }

    return false;
  }

  static fromRecord(record, region) {
    return new Name(record[Name.ac], region);
  }
}

/**
 * A full telephone number, including the country code.
 * See autocomplete="tel"
 */
class Tel extends AddressField {
  static ac = "tel";

  #valid = false;

  // The country code part of a telphone number, such as "1" for the United States
  #country_code = null;

  // The national part of a telphone number. For example, the phone number "+1 520-248-6621"
  // national part is "520-248-6621".
  #national_number = null;

  constructor(value, region) {
    super(value, region);

    if (!this.userValue) {
      return;
    }

    // TODO: Support parse telephone extension
    // We compress all tel-related fields into a single tel field when an an form
    // is submitted, so we need to decompress it here.
    const parsed_tel = lazy.PhoneNumber.Parse(this.userValue, region);
    if (parsed_tel) {
      this.#national_number = parsed_tel?.nationalNumber;
      this.#country_code = parsed_tel?.countryCode;

      this.#valid = true;
    } else {
      this.#national_number = lazy.PhoneNumberNormalizer.Normalize(
        this.userValue
      );

      const md = lazy.PhoneNumber.FindMetaDataForRegion(region);
      this.#country_code = md ? "+" + md.nationalPrefix : null;

      this.#valid = lazy.PhoneNumber.IsValid(this.#national_number, md);
    }
  }

  get country_code() {
    return this.#country_code;
  }

  get national_number() {
    return this.#national_number;
  }

  isValid() {
    return this.#valid;
  }

  equals(other) {
    return (
      this.national_number == other.national_number &&
      this.country_code == other.country_code
    );
  }

  contains(other) {
    if (!this.country_code || this.country_code != other.country_code) {
      return false;
    }

    return this.national_number.endsWith(other.national_number);
  }

  toString() {
    return `${this.constructor.name}: ${this.country_code} ${this.national_number}\n`;
  }

  static fromRecord(record, region) {
    return new Tel(record[Tel.ac], region);
  }
}

/**
 * A company or organization name.
 * See autocomplete="organization".
 */
class Organization extends AddressField {
  static ac = "organization";

  constructor(value, region) {
    super(value, region);
  }

  isValid() {
    return this.userValue
      ? !!/[\p{Letter}\p{Number}]/u.exec(this.userValue)
      : true;
  }

  /**
   * Two company names are considered equal only when everything is the same.
   */
  equals(other) {
    return this.userValue == other.userValue;
  }

  // Mergeable use locale compare
  contains(other) {
    const options = {
      replace_punctuation: " ", // mozilla org vs mozilla-org
      merge_whitespace: true,
      ignore_case: true, // mozilla vs Mozilla
    };

    // If every token in B can be found in A without considering order
    // Example, 'Food & Pharmacy' contains 'Pharmacy & Food'
    const selfTokens = new Tokens(this.normalizeUserValue(options));
    const otherTokens = new Tokens(other.normalizeUserValue(options));

    return otherTokens.isSubset(selfTokens, (a, b) => this.localeCompare(a, b));
  }

  static fromRecord(record, region) {
    return new Organization(record[Organization.ac], region);
  }
}

/**
 * An email address
 * See autocomplete="email".
 */
class Email extends AddressField {
  static ac = "email";

  constructor(value, region) {
    super(value, region);
  }

  // Since we are using the valid check to determine whether we capture the email field when users submitting a forma,
  // use a less restrict email verification method so we capture an email for most of the cases.
  // The current algorithm is based on the regular expression defined in
  // https://html.spec.whatwg.org/multipage/input.html#valid-e-mail-address
  //
  // We might also change this to something similar to the algorithm used in
  // EmailInputType::IsValidEmailAddress if we want a more strict email validation algorithm.
  isValid() {
    const regex =
      /^[a-zA-Z0-9.!#$%&'*+\/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$/;
    const match = this.userValue.match(regex);
    if (!match) {
      return false;
    }

    return true;
  }

  /*
  // JS version of EmailInputType::IsValidEmailAddress
  isValid() {
    const regex = /^([a-zA-Z0-9.!#$%&'*+-/=?^_`{|}~]+)@([a-zA-Z0-9-]+\.[a-zA-Z]{2,})$/;
    const match = this.userValue.match(regex);
    if (!match) {
      return false;
    }
    const local = match[1];
    const domain = match[2];

    // The domain name can't begin with a dot or a dash.
    if (['-', '.'].includes(domain[0])) {
      return false;
    }

    // A dot can't follow a dot or a dash.
    // A dash can't follow a dot.
    const pattern = /(\.\.)|(\.-)|(-\.)/;
    if (pattern.test(domain)) {
      return false;
    }

    return true;
  }
*/

  equals(other) {
    const options = {
      ignore_case: true,
    };

    // email is case-insenstive
    return (
      this.normalizeUserValue(options) == other.normalizeUserValue(options)
    );
  }

  contains(other) {
    return false;
  }

  static fromRecord(record, region) {
    return new Email(record[Email.ac], region);
  }
}

/**
 * The AddressComparison class compares two AddressComponent instances and
 * provides information about the differences or similarities between them.
 *
 * The comparison result is stored and the object and can be retrieved by calling
 * 'result' getter.
 */
export class AddressComparison {
  // Const to define the comparison result for two address fields
  static BOTH_EMPTY = 0;
  static A_IS_EMPTY = 1;
  static B_IS_EMPTY = 2;
  static A_CONTAINS_B = 3;
  static B_CONTAINS_A = 4;
  // When A contains B and B contains A Ex. "Pizza & Food vs Food & Pizza"
  static SIMILAR = 5;
  static SAME = 6;
  static DIFFERENT = 7;

  // The comparion result, keyed by field name.
  #result = {};

  /**
   * Constructs AddressComparison by comparing two AddressComponent objects.
   *
   * @class
   * @param {AddressComponent} addressA - The first address to compare.
   * @param {AddressComponent} addressB - The second address to compare.
   */
  constructor(addressA, addressB) {
    for (const fieldA of addressA.getAllFields()) {
      const fieldName = fieldA.constructor.ac;
      const fieldB = addressB.getField(fieldName);
      if (fieldB) {
        this.#result[fieldName] = AddressComparison.compare(fieldA, fieldB);
      } else {
        this.#result[fieldName] = AddressComparison.B_IS_EMPTY;
      }
    }

    for (const fieldB of addressB.getAllFields()) {
      const fieldName = fieldB.constructor.ac;
      if (!addressB.getField(fieldName)) {
        this.#result[fieldName] = AddressComparison.A_IS_EMPTY;
      }
    }
  }

  /**
   * Retrieves the result object containing the comparison results.
   *
   * @returns {object} The result object with keys corresponding to field names
   *                  and values being comparison constants.
   */
  get result() {
    return this.#result;
  }

  /**
   * Compares two address fields and returns the comparison result.
   *
   * @param  {AddressField} fieldA The first field to compare.
   * @param  {AddressField} fieldB The second field to compare.
   * @returns {number}       A constant representing the comparison result.
   */
  static compare(fieldA, fieldB) {
    if (fieldA.isEmpty()) {
      return fieldB.isEmpty()
        ? AddressComparison.BOTH_EMPTY
        : AddressComparison.A_IS_EMPTY;
    } else if (fieldB.isEmpty()) {
      return AddressComparison.B_IS_EMPTY;
    }

    if (fieldA.equals(fieldB)) {
      return AddressComparison.SAME;
    }

    if (fieldB.contains(fieldA)) {
      if (fieldA.contains(fieldB)) {
        return AddressComparison.SIMILAR;
      }
      return AddressComparison.B_CONTAINS_A;
    } else if (fieldA.contains(fieldB)) {
      return AddressComparison.A_CONTAINS_B;
    }

    return AddressComparison.DIFFERENT;
  }

  /**
   * Converts a comparison result constant to a readable string.
   *
   * @param  {number} result The comparison result constant.
   * @returns {string}        A readable string representing the comparison result.
   */
  static resultToString(result) {
    switch (result) {
      case AddressComparison.BOTH_EMPTY:
        return "both fields are empty";
      case AddressComparison.A_IS_EMPTY:
        return "field A is empty";
      case AddressComparison.B_IS_EMPTY:
        return "field B is empty";
      case AddressComparison.A_CONTAINS_B:
        return "field A contains field B";
      case AddressComparison.B_CONTAINS_B:
        return "field B contains field A";
      case AddressComparison.SIMILAR:
        return "field A and field B are similar";
      case AddressComparison.SAME:
        return "two fields are the same";
      case AddressComparison.DIFFERENT:
        return "two fields are different";
    }
    return "";
  }

  /**
   * Returns a formatted string representing the comparison results for each field.
   *
   * @returns {string} A formatted string with field names and their respective
   *                  comparison results.
   */
  toString() {
    let string = "Comparison Result:\n";
    for (const [name, result] of Object.entries(this.#result)) {
      string += `${name}: ${AddressComparison.resultToString(result)}\n`;
    }
    return string;
  }
}

/**
 * The AddressComponent class represents a structured address that is transformed
 * from address record created in FormAutofillHandler 'createRecord' function.
 *
 * An AddressComponent object consisting of various fields such as state, city,
 * country, postal code, etc. The class provides a compare methods
 * to compare another AddressComponent against the current instance.
 *
 * Note. This class assumes records that pass to it have already been normalized.
 */
export class AddressComponent {
  /**
   * An object that stores individual address field instances
   * (e.g., class State, class City, class Country, etc.), keyed by the
   * field's clas name.
   */
  #fields = {};

  /**
   * Constructs an AddressComponent object by converting passed address record object.
   *
   * @class
   * @param {object}  record         The address record object containing address data.
   * @param {object}  [options = {}] a list of options for this method
   * @param {boolean} [options.ignoreInvalid = true]  Whether to ignore invalid address
   *                                 fields in the AddressComponent object. If set to true,
   *                                 invalid fields will be ignored.
   */
  constructor(record, { ignoreInvalid = true } = {}) {
    this.record = {};

    // Get country code first so we can use it to parse other fields
    const country = new Country(
      record[Country.ac],
      FormAutofill.DEFAULT_REGION
    );
    const region =
      country.country_code ||
      lazy.FormAutofillUtils.identifyCountryCode(FormAutofill.DEFAULT_REGION);

    // Build an mapping that the key is field name and the value is the AddressField object
    [
      country,
      new StreetAddress(record[StreetAddress.ac], region),
      new PostalCode(record[PostalCode.ac], region),
      new State(record[State.ac], region),
      new City(record[City.ac], region),
      new Name(record[Name.ac], region),
      new Tel(record[Tel.ac], region),
      new Organization(record[Organization.ac], region),
      new Email(record[Email.ac], region),
    ].forEach(addressField => {
      if (
        !addressField.isEmpty() &&
        (!ignoreInvalid || addressField.isValid())
      ) {
        const fieldName = addressField.constructor.ac;
        this.#fields[fieldName] = addressField;
        this.record[fieldName] = record[fieldName];
      }
    });
  }

  /**
   * Retrieves all the address fields.
   *
   * @returns {Array} An array of address field objects.
   */
  getAllFields() {
    return Object.values(this.#fields);
  }

  /**
   * Retrieves the field object with the specified name.
   *
   * @param  {string} name The name of the field to retrieve.
   * @returns {object}      The address field object with the specified name,
   *                       or undefined if the field is not found.
   */
  getField(name) {
    return this.#fields[name];
  }

  /**
   * Compares the current AddressComponent with another AddressComponent.
   *
   * @param  {AddressComponent} address The AddressComponent object to compare
   *                                    against the current one.
   * @returns {object} An object containing comparison results. The keys of the object represent
   *                  individual address field, and the values are strings indicating the comparison result:
   *                  - "same" if both components are either empty or the same,
   *                  - "superset" if the current contains the input or the input is empty,
   *                  - "subset" if the input contains the current or the current is empty,
   *                  - "similar" if the two address components are similar,
   *                  - "different" if the two address components are different.
   */
  compare(address) {
    let result = {};

    const comparison = new AddressComparison(this, address);
    for (const [k, v] of Object.entries(comparison.result)) {
      if ([AddressComparison.BOTH_EMPTY, AddressComparison.SAME].includes(v)) {
        result[k] = "same";
      } else if (
        [AddressComparison.B_IS_EMPTY, AddressComparison.A_CONTAINS_B].includes(
          v
        )
      ) {
        result[k] = "superset";
      } else if (
        [AddressComparison.A_IS_EMPTY, AddressComparison.B_CONTAINS_A].includes(
          v
        )
      ) {
        result[k] = "subset";
      } else if ([AddressComparison.SIMILAR].includes(v)) {
        result[k] = "similar";
      } else {
        result[k] = "different";
      }
    }
    return result;
  }

  /**
   * Print all the fields in this AddressComponent object.
   */
  toString() {
    let string = "";
    for (const field of Object.values(this.#fields)) {
      string += field.toString();
    }
    return string;
  }
}
