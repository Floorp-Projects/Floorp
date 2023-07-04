/* eslint-disable no-useless-concat */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NamedCaptureGroup class represents a named capturing group in a regular expression
class NamedCaptureGroup {
  // The named of this capturing group
  #name = null;

  // The capturing group
  #capture = null;

  // The matched result
  #match = null;

  constructor(name, capture) {
    this.#name = name;
    this.#capture = capture;
  }

  get name() {
    return this.#name;
  }

  get capture() {
    return this.#capture;
  }

  get match() {
    return this.#match;
  }

  // Setter for the matched result based on the match groups
  setMatch(matchGroups) {
    this.#match = matchGroups[this.#name];
  }
}

// Base class for different part of a street address regular expression.
// The regular expression is constructed with prefix, pattern, suffix
// and separator to extract "value" part.
// For examplem, when we write "apt 4." to for floor number, its prefix is `apt`,
// suffix is `.` and value to represent apartment number is `4`.
class StreetAddressPartRegExp extends NamedCaptureGroup {
  constructor(name, prefix, pattern, suffix, sep, optional = false) {
    prefix = prefix ?? "";
    suffix = suffix ?? "";
    super(
      name,
      `((?:${prefix})(?<${name}>${pattern})(?:${suffix})(?:${sep})+)${
        optional ? "?" : ""
      }`
    );
  }
}

// A regular expression to match the street number portion of a street address,
class StreetNumberRegExp extends StreetAddressPartRegExp {
  static PREFIX = "((no|°|º|number)(\\.|-|\\s)*)?"; // From chromium source

  static PATTERN = "\\d+\\w?";

  // TODO: possible suffix : (th\\.|\\.)?
  static SUFFIX = null;

  constructor(sep, optional) {
    super(
      StreetNumberRegExp.name,
      StreetNumberRegExp.PREFIX,
      StreetNumberRegExp.PATTERN,
      StreetNumberRegExp.SUFFIX,
      sep,
      optional
    );
  }
}

// A regular expression to match the street name portion of a street address,
class StreetNameRegExp extends StreetAddressPartRegExp {
  static PREFIX = null;

  static PATTERN = "(?:[^\\s,]+(?:[^\\S\\r\\n]+[^\\s,]+)*?)"; // From chromium source

  // TODO: Should we consider suffix like (ave|st)?
  static SUFFIX = null;

  constructor(sep, optional) {
    super(
      StreetNameRegExp.name,
      StreetNameRegExp.PREFIX,
      StreetNameRegExp.PATTERN,
      StreetNameRegExp.SUFFIX,
      sep,
      optional
    );
  }
}

// A regular expression to match the apartment number portion of a street address,
class ApartmentNumberRegExp extends StreetAddressPartRegExp {
  static keyword = "apt|apartment|wohnung|apto|-" + "|unit|suite|ste|#|room"; // From chromium source // Firefox specific
  static PREFIX = `(${ApartmentNumberRegExp.keyword})(\\.|\\s|-)*`;

  static PATTERN = "\\w*([-|\\/]\\w*)?";

  static SUFFIX = "(\\.|\\s|-)*(ª)?"; // From chromium source

  constructor(sep, optional) {
    super(
      ApartmentNumberRegExp.name,
      ApartmentNumberRegExp.PREFIX,
      ApartmentNumberRegExp.PATTERN,
      ApartmentNumberRegExp.SUFFIX,
      sep,
      optional
    );
  }
}

// A regular expression to match the floor number portion of a street address,
class FloorNumberRegExp extends StreetAddressPartRegExp {
  static keyword =
    "floor|flur|fl|og|obergeschoss|ug|untergeschoss|geschoss|andar|piso|º" + // From chromium source
    "|level|lvl"; // Firefox specific
  static PREFIX = `(${FloorNumberRegExp.keyword})?(\\.|\\s|-)*`; // TODO
  static PATTERN = "\\d{1,3}\\w?";
  static SUFFIX = `(st|nd|rd|th)?(\\.|\\s|-)*(${FloorNumberRegExp.keyword})?`; // TODO

  constructor(sep, optional) {
    super(
      FloorNumberRegExp.name,
      FloorNumberRegExp.PREFIX,
      FloorNumberRegExp.PATTERN,
      FloorNumberRegExp.SUFFIX,
      sep,
      optional
    );
  }
}

/**
 * Class represents a street address with the following fields:
 * - street number
 * - street name
 * - apartment number
 * - floor number
 */
export class StructuredStreetAddress {
  #street_number = null;
  #street_name = null;
  #apartment_number = null;
  #floor_number = null;

  constructor(street_number, street_name, apartment_number, floor_number) {
    this.#street_number = street_number?.toString();
    this.#street_name = street_name?.toString();
    this.#apartment_number = apartment_number?.toString();
    this.#floor_number = floor_number?.toString();
  }

  get street_number() {
    return this.#street_number;
  }

  get street_name() {
    return this.#street_name;
  }

  get apartment_number() {
    return this.#apartment_number;
  }

  get floor_number() {
    return this.#floor_number;
  }

  toString() {
    return `
      street number: ${this.#street_number}\n
      street name: ${this.#street_name}\n
      apartment number: ${this.#apartment_number}\n
      floor number: ${this.#floor_number}\n
    `;
  }
}

export class AddressParser {
  /**
   * Parse street address with the following pattern.
   * street number, street name, apartment number(optional), floor number(optional)
   * For example, 2 Harrison St #175 floor 2
   *
   * @param {string} address The street address to be parsed.
   * @returns {StructuredStreetAddress}
   */
  static parseStreetAddress(address) {
    if (!address) {
      return null;
    }

    const separator = "(\\s|,|$)";

    const regexpes = [
      new StreetNumberRegExp(separator),
      new StreetNameRegExp(separator),
      new ApartmentNumberRegExp(separator, true),
      new FloorNumberRegExp(separator, true),
    ];

    return AddressParser.parse(address, regexpes)
      ? new StructuredStreetAddress(...regexpes.map(regexp => regexp.match))
      : null;
  }

  static parse(address, regexpes) {
    const options = {
      trim: true,
      merge_whitespace: true,
      ignore_case: true,
    };
    address = AddressParser.normalizeString(address, options);

    const match = address.match(
      new RegExp(`^(${regexpes.map(regexp => regexp.capture).join("")})$`)
    );
    if (!match) {
      return null;
    }

    regexpes.forEach(regexp => regexp.setMatch(match.groups));
    return regexpes.reduce((acc, current) => {
      return { ...acc, [current.name]: current.match };
    }, {});
  }

  static normalizeString(s, options) {
    if (typeof s != "string") {
      return s;
    }

    if (options.ignore_case) {
      s = s.toLowerCase();
    }

    // process punctuation before whitespace because if a punctuation
    // is replaced with whitespace, we might want to merge it later
    if (options.remove_punctuation) {
      s = AddressParser.replacePunctuation(s, "");
    } else if ("replace_punctuation" in options) {
      const replace = options.replace_punctuation;
      s = AddressParser.replacePunctuation(s, replace);
    }

    // process whitespace
    if (options.merge_whitespace) {
      s = AddressParser.mergeWhitespace(s);
    } else if (options.remove_whitespace) {
      s = AddressParser.removeWhitespace(s);
    }

    return s.trim();
  }

  static replacePunctuation(s, replace) {
    const regex = /\p{Punctuation}/gu;
    return s?.replace(regex, replace);
  }

  static removePunctuation(s) {
    return s?.replace(/[.,\/#!$%\^&\*;:{}=\-_~()]/g, "");
  }

  static replaceControlCharacters(s, replace) {
    return s?.replace(/[\t\n\r]/g, " ");
  }

  static removeWhitespace(s) {
    return s?.replace(/[\s]/g, "");
  }

  static mergeWhitespace(s) {
    return s?.replace(/\s{2,}/g, " ");
  }
}
