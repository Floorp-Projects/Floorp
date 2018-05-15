/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CreditCard"];

ChromeUtils.defineModuleGetter(this, "MasterPassword",
                               "resource://formautofill/MasterPassword.jsm");

class CreditCard {
  /**
   * @param {string} name
   * @param {string} number
   * @param {string} expirationString
   * @param {string|number} expirationMonth
   * @param {string|number} expirationYear
   * @param {string|number} ccv
   * @param {string} encryptedNumber
   */
  constructor({
    name,
    number,
    expirationString,
    expirationMonth,
    expirationYear,
    ccv,
    encryptedNumber
  }) {
    this._name = name;
    this._unmodifiedNumber = number;
    this._encryptedNumber = encryptedNumber;
    this._ccv = ccv;
    this.number = number;
    // Only prefer the string version if missing one or both parsed formats.
    if (expirationString && (!expirationMonth || !expirationYear)) {
      this.expirationString = expirationString;
    } else {
      this.expirationMonth = expirationMonth;
      this.expirationYear = expirationYear;
    }
  }

  set name(value) {
    this._name = value;
  }

  set expirationMonth(value) {
    if (typeof value == "undefined") {
      this._expirationMonth = undefined;
      return;
    }
    this._expirationMonth = this._normalizeExpirationMonth(value);
  }

  get expirationMonth() {
    return this._expirationMonth;
  }

  set expirationYear(value) {
    if (typeof value == "undefined") {
      this._expirationYear = undefined;
      return;
    }
    this._expirationYear = this._normalizeExpirationYear(value);
  }

  get expirationYear() {
    return this._expirationYear;
  }

  set expirationString(value) {
    let {month, year} = this._parseExpirationString(value);
    this.expirationMonth = month;
    this.expirationYear = year;
  }

  set ccv(value) {
    this._ccv = value;
  }

  get number() {
    return this._number;
  }

  set number(value) {
    if (value) {
      let normalizedNumber = value.replace(/[-\s]/g, "");
      // Based on the information on wiki[1], the shortest valid length should be
      // 9 digits (Canadian SIN).
      // [1] https://en.wikipedia.org/wiki/Social_Insurance_Number
      normalizedNumber = normalizedNumber.match(/^\d{9,}$/) ?
        normalizedNumber : null;
      this._number = normalizedNumber;
    }
  }

  // Implements the Luhn checksum algorithm as described at
  // http://wikipedia.org/wiki/Luhn_algorithm
  isValidNumber() {
    if (!this._number) {
      return false;
    }

    // Remove dashes and whitespace
    let number = this._number.replace(/[\-\s]/g, "");

    let len = number.length;
    if (len != 9 && len != 15 && len != 16) {
      return false;
    }

    if (!/^\d+$/.test(number)) {
      return false;
    }

    let total = 0;
    for (let i = 0; i < len; i++) {
      let ch = parseInt(number[len - i - 1], 10);
      if (i % 2 == 1) {
        // Double it, add digits together if > 10
        ch *= 2;
        if (ch > 9) {
          ch -= 9;
        }
      }
      total += ch;
    }
    return total % 10 == 0;
  }

  /**
   * Returns true if the card number is valid and the
   * expiration date has not passed. Otherwise false.
   *
   * @returns {boolean}
   */
  isValid() {
    if (!this.isValidNumber()) {
      return false;
    }

    let currentDate = new Date();
    let currentYear = currentDate.getFullYear();
    if (this._expirationYear > currentYear) {
      return true;
    }

    // getMonth is 0-based, so add 1 because credit cards are 1-based
    let currentMonth = currentDate.getMonth() + 1;
    return this._expirationYear == currentYear &&
           this._expirationMonth >= currentMonth;
  }

  get maskedNumber() {
    if (!this.isValidNumber()) {
      throw new Error("Invalid credit card number");
    }
    return "*".repeat(4) + " " + this._number.substr(-4);
  }

  get longMaskedNumber() {
    if (!this.isValidNumber()) {
      throw new Error("Invalid credit card number");
    }
    return "*".repeat(this.number.length - 4) + this.number.substr(-4);
  }

  /**
   * Get credit card display label. It should display masked numbers and the
   * cardholder's name, separated by a comma. If `showNumbers` is set to
   * true, decrypted credit card numbers are shown instead.
   */
  async getLabel({showNumbers} = {}) {
    let parts = [];
    let label;

    if (showNumbers) {
      if (this._encryptedNumber) {
        label = await MasterPassword.decrypt(this._encryptedNumber);
      } else {
        label = this._number;
      }
    }
    if (this._unmodifiedNumber && !label) {
      if (this.isValidNumber()) {
        label = this.maskedNumber;
      } else {
        let maskedNumber = CreditCard.formatMaskedNumber(this._unmodifiedNumber);
        label = `${maskedNumber.affix} ${maskedNumber.label}`;
      }
    }

    if (label) {
      parts.push(label);
    }
    if (this._name) {
      parts.push(this._name);
    }
    return parts.join(", ");
  }

  _normalizeExpirationMonth(month) {
    month = parseInt(month, 10);
    if (isNaN(month) || month < 1 || month > 12) {
      return undefined;
    }
    return month;
  }

  _normalizeExpirationYear(year) {
    year = parseInt(year, 10);
    if (isNaN(year) || year < 0) {
      return undefined;
    }
    if (year < 100) {
      year += 2000;
    }
    return year;
  }

  _parseExpirationString(expirationString) {
    let rules = [
      {
        regex: "(\\d{4})[-/](\\d{1,2})",
        yearIndex: 1,
        monthIndex: 2,
      },
      {
        regex: "(\\d{1,2})[-/](\\d{4})",
        yearIndex: 2,
        monthIndex: 1,
      },
      {
        regex: "(\\d{1,2})[-/](\\d{1,2})",
      },
      {
        regex: "(\\d{2})(\\d{2})",
      },
    ];

    for (let rule of rules) {
      let result = new RegExp(`(?:^|\\D)${rule.regex}(?!\\d)`).exec(expirationString);
      if (!result) {
        continue;
      }

      let year, month;

      if (!rule.yearIndex || !rule.monthIndex) {
        month = parseInt(result[1], 10);
        if (month > 12) {
          year = parseInt(result[1], 10);
          month = parseInt(result[2], 10);
        } else {
          year = parseInt(result[2], 10);
        }
      } else {
        year = parseInt(result[rule.yearIndex], 10);
        month = parseInt(result[rule.monthIndex], 10);
      }

      if ((month < 1 || month > 12) ||
          (year >= 100 && year < 2000)) {
        continue;
      }

      return {month, year};
    }
    return {month: undefined, year: undefined};
  }

  static formatMaskedNumber(maskedNumber) {
    return {
      affix: "****",
      label: maskedNumber.replace(/^\**/, ""),
    };
  }

  static getMaskedNumber(number) {
    let creditCard = new CreditCard({number});
    return creditCard.maskedNumber;
  }

  static getLongMaskedNumber(number) {
    let creditCard = new CreditCard({number});
    return creditCard.longMaskedNumber;
  }

  static isValidNumber(number) {
    let creditCard = new CreditCard({number});
    return creditCard.isValidNumber();
  }
}
