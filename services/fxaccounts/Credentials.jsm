/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module implements client-side key stretching for use in Firefox
 * Accounts account creation and login.
 *
 * See https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol
 */

"use strict";

var EXPORTED_SYMBOLS = ["Credentials"];

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { CryptoUtils } = ChromeUtils.import(
  "resource://services-crypto/utils.js"
);
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);

const PROTOCOL_VERSION = "identity.mozilla.com/picl/v1/";
const PBKDF2_ROUNDS = 1000;
const STRETCHED_PW_LENGTH_BYTES = 32;
const HKDF_SALT = CommonUtils.hexToBytes("00");
const HKDF_LENGTH = 32;

// loglevel preference should be one of: "FATAL", "ERROR", "WARN", "INFO",
// "CONFIG", "DEBUG", "TRACE" or "ALL". We will be logging error messages by
// default.
const PREF_LOG_LEVEL = "identity.fxaccounts.loglevel";
let LOG_LEVEL = Log.Level.Error;
try {
  LOG_LEVEL =
    Services.prefs.getPrefType(PREF_LOG_LEVEL) ==
      Ci.nsIPrefBranch.PREF_STRING &&
    Services.prefs.getCharPref(PREF_LOG_LEVEL);
} catch (e) {}

var log = Log.repository.getLogger("Identity.FxAccounts");
log.level = LOG_LEVEL;
log.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));

var Credentials = Object.freeze({
  /**
   * Make constants accessible to tests
   */
  constants: {
    PROTOCOL_VERSION,
    PBKDF2_ROUNDS,
    STRETCHED_PW_LENGTH_BYTES,
    HKDF_SALT,
    HKDF_LENGTH,
  },

  /**
   * KW function from https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol
   *
   * keyWord derivation for use as a salt.
   *
   *
   *   @param {String} context  String for use in generating salt
   *
   *   @return {bitArray} the salt
   *
   * Note that PROTOCOL_VERSION does not refer in any way to the version of the
   * Firefox Accounts API.
   */
  keyWord(context) {
    return CommonUtils.stringToBytes(PROTOCOL_VERSION + context);
  },

  /**
   * KWE function from https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol
   *
   * keyWord extended with a name and an email.
   *
   *   @param {String} name The name of the salt
   *   @param {String} email The email of the user.
   *
   *   @return {bitArray} the salt combination with the namespace
   *
   * Note that PROTOCOL_VERSION does not refer in any way to the version of the
   * Firefox Accounts API.
   */
  keyWordExtended(name, email) {
    return CommonUtils.stringToBytes(PROTOCOL_VERSION + name + ":" + email);
  },

  setup(emailInput, passwordInput, options = {}) {
    return new Promise(resolve => {
      log.debug("setup credentials for " + emailInput);

      let hkdfSalt = options.hkdfSalt || HKDF_SALT;
      let hkdfLength = options.hkdfLength || HKDF_LENGTH;
      let stretchedPWLength =
        options.stretchedPassLength || STRETCHED_PW_LENGTH_BYTES;
      let pbkdf2Rounds = options.pbkdf2Rounds || PBKDF2_ROUNDS;

      let result = {};

      let password = CommonUtils.encodeUTF8(passwordInput);
      let salt = this.keyWordExtended("quickStretch", emailInput);

      let runnable = async () => {
        let start = Date.now();
        let quickStretchedPW = await CryptoUtils.pbkdf2Generate(
          password,
          salt,
          pbkdf2Rounds,
          stretchedPWLength
        );

        result.quickStretchedPW = quickStretchedPW;

        result.authPW = await CryptoUtils.hkdfLegacy(
          quickStretchedPW,
          hkdfSalt,
          this.keyWord("authPW"),
          hkdfLength
        );

        result.unwrapBKey = await CryptoUtils.hkdfLegacy(
          quickStretchedPW,
          hkdfSalt,
          this.keyWord("unwrapBkey"),
          hkdfLength
        );

        log.debug("Credentials set up after " + (Date.now() - start) + " ms");
        resolve(result);
      };

      Services.tm.dispatchToMainThread(runnable);
      log.debug("Dispatched thread for credentials setup crypto work");
    });
  },
});
