/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  PasswordGenerator: "resource://gre/modules/PasswordGenerator.jsm",
  PasswordRulesParser: "resource://gre/modules/PasswordRulesParser.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
});

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  let logger = lazy.LoginHelper.createLogger("PasswordRulesManager");
  return logger.log.bind(logger);
});

const IMPROVED_PASSWORD_GENERATION_HISTOGRAM =
  "PWMGR_NUM_IMPROVED_GENERATED_PASSWORDS";

const EXPORTED_SYMBOLS = ["PasswordRulesManagerParent"];

/**
 * Handles interactions between PasswordRulesParser and the "password-rules" Remote Settings collection
 *
 * @class PasswordRulesManagerParent
 * @extends {JSWindowActorParent}
 */
class PasswordRulesManagerParent extends JSWindowActorParent {
  /**
   * @type RemoteSettingsClient
   *
   * @memberof PasswordRulesManagerParent
   */
  _passwordRulesClient = null;

  async initPasswordRulesCollection() {
    if (!this._passwordRulesClient) {
      this._passwordRulesClient = lazy.RemoteSettings(
        lazy.LoginHelper.improvedPasswordRulesCollection
      );
    }
  }
  /**
   * Transforms the parsed rules returned from PasswordRulesParser into a Map for easier access.
   * The returned Map could have the following keys: "allowed", "required", "maxlength", "minlength", and "max-consecutive"
   * @example
   * // Returns a Map with a key-value pair of "allowed": "ascii-printable"
   * _transformRulesToMap([{ _name: "allowed", value: [{ _name: "ascii-printable" }] }])
   * @param {Object[]} rules rules from PasswordRulesParser.parsePasswordRules
   * @return {Map} mapped rules
   * @memberof PasswordRulesManagerParent
   */
  _transformRulesToMap(rules) {
    let map = new Map();
    for (let rule of rules) {
      let { _name, value } = rule;
      if (
        _name === "minlength" ||
        _name === "maxlength" ||
        _name === "max-consecutive"
      ) {
        map.set(_name, value);
      } else {
        let _value = [];
        if (map.get(_name)) {
          _value = map.get(_name);
        }
        for (let _class of value) {
          let { _name: _className } = _class;
          if (_className) {
            _value.push(_className);
          } else {
            let { _characters } = _class;
            _value.push(_characters);
          }
        }
        map.set(_name, _value);
      }
    }
    return map;
  }

  /**
   * Generates a password based on rules from the origin parameters.
   * @param {nsIURI} uri
   * @return {string} password
   * @memberof PasswordRulesManagerParent
   */
  async generatePassword(uri) {
    await this.initPasswordRulesCollection();
    let originDisplayHost = uri.displayHost;
    let records = await this._passwordRulesClient.get();
    let currentRecord;
    for (let record of records) {
      if (Services.eTLD.hasRootDomain(originDisplayHost, record.Domain)) {
        currentRecord = record;
        break;
      }
    }
    let isCustomRule = false;
    // If we found a matching result, use that to generate a stronger password.
    // Otherwise, generate a password using the default rules set.
    if (currentRecord?.Domain) {
      isCustomRule = true;
      lazy.log(
        `Password rules for ${currentRecord.Domain}:  ${currentRecord["password-rules"]}.`
      );
      let currentRules = lazy.PasswordRulesParser.parsePasswordRules(
        currentRecord["password-rules"]
      );
      let mapOfRules = this._transformRulesToMap(currentRules);
      Services.telemetry
        .getHistogramById(IMPROVED_PASSWORD_GENERATION_HISTOGRAM)
        .add(isCustomRule);
      return lazy.PasswordGenerator.generatePassword({ rules: mapOfRules });
    }
    lazy.log(
      `No password rules for specified origin, generating standard password.`
    );
    Services.telemetry
      .getHistogramById(IMPROVED_PASSWORD_GENERATION_HISTOGRAM)
      .add(isCustomRule);
    return lazy.PasswordGenerator.generatePassword({});
  }
}
