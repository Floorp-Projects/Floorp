/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file is a port of a subset of Chromium's implementation from
 * https://cs.chromium.org/chromium/src/components/password_manager/core/browser/generation/password_generator.cc?l=93&rcl=a896a3ac4ea731b5ab3d2ab5bd76a139885d5c4f
 * which is Copyright 2018 The Chromium Authors. All rights reserved.
 */

const EXPORTED_SYMBOLS = ["PasswordGenerator"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["crypto"]);

const DEFAULT_PASSWORD_LENGTH = 15;
const MAX_UINT8 = Math.pow(2, 8) - 1;
const MAX_UINT32 = Math.pow(2, 32) - 1;

// Some characters are removed due to visual similarity:
const LOWER_CASE_ALPHA = "abcdefghijkmnpqrstuvwxyz"; // no 'l' or 'o'
const UPPER_CASE_ALPHA = "ABCDEFGHJKLMNPQRSTUVWXYZ"; // no 'I' or 'O'
const DIGITS = "23456789"; // no '1' or '0'
const ALL_CHARACTERS = LOWER_CASE_ALPHA + UPPER_CASE_ALPHA + DIGITS;

const REQUIRED_CHARACTER_CLASSES = [LOWER_CASE_ALPHA, UPPER_CASE_ALPHA, DIGITS];

this.PasswordGenerator = {
  /**
   * @param {Number} length of the password to generate
   * @returns {string} password that was generated
   * @throws Error if `length` is invalid
   * @copyright 2018 The Chromium Authors. All rights reserved.
   * @see https://cs.chromium.org/chromium/src/components/password_manager/core/browser/generation/password_generator.cc?l=93&rcl=a896a3ac4ea731b5ab3d2ab5bd76a139885d5c4f
   */
  generatePassword(length = DEFAULT_PASSWORD_LENGTH) {
    if (length < REQUIRED_CHARACTER_CLASSES.length) {
      throw new Error("requested password length is too short");
    }

    if (length > MAX_UINT8) {
      throw new Error("requested password length is too long");
    }
    let password = "";

    // Generate one of each required class
    for (const charClassString of REQUIRED_CHARACTER_CLASSES) {
      password +=
        charClassString[this._randomUInt8Index(charClassString.length)];
    }

    // Now fill the rest of the password with random characters.
    while (password.length < length) {
      password += ALL_CHARACTERS[this._randomUInt8Index(ALL_CHARACTERS.length)];
    }

    // So far the password contains the minimally required characters at the
    // the beginning. Therefore, we create a random permutation.
    password = this._shuffleString(password);

    return password;
  },

  /**
   * @param range to generate the number in
   * @returns a random number in range [0, range).
   * @copyright 2018 The Chromium Authors. All rights reserved.
   * @see https://cs.chromium.org/chromium/src/base/rand_util.cc?l=58&rcl=648a59893e4ed5303b5c381b03ce0c75e4165617
   */
  _randomUInt8Index(range) {
    if (range > MAX_UINT8) {
      throw new Error("`range` cannot fit into uint8");
    }
    // We must discard random results above this number, as they would
    // make the random generator non-uniform (consider e.g. if
    // MAX_UINT64 was 7 and |range| was 5, then a result of 1 would be twice
    // as likely as a result of 3 or 4).
    // See https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#Modulo_bias
    const MAX_ACCEPTABLE_VALUE = Math.floor(MAX_UINT8 / range) * range - 1;

    const randomValueArr = new Uint8Array(1);
    do {
      crypto.getRandomValues(randomValueArr);
    } while (randomValueArr[0] > MAX_ACCEPTABLE_VALUE);
    return randomValueArr[0] % range;
  },

  /**
   * Shuffle the order of characters in a string.
   * @param {string} str to shuffle
   * @returns {string} shuffled string
   */
  _shuffleString(str) {
    let arr = Array.from(str);
    // Generate all the random numbers that will be needed.
    const randomValues = new Uint32Array(arr.length - 1);
    crypto.getRandomValues(randomValues);

    // Fisher-Yates Shuffle
    // https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
    for (let i = arr.length - 1; i > 0; i--) {
      const j = Math.floor((randomValues[i - 1] / MAX_UINT32) * (i + 1));
      [arr[i], arr[j]] = [arr[j], arr[i]];
    }
    return arr.join("");
  },
};
