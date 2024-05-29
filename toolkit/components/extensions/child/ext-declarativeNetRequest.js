/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionDNRLimits: "resource://gre/modules/ExtensionDNRLimits.sys.mjs",
});

this.declarativeNetRequest = class extends ExtensionAPI {
  getAPI() {
    return {
      declarativeNetRequest: {
        get GUARANTEED_MINIMUM_STATIC_RULES() {
          return ExtensionDNRLimits.GUARANTEED_MINIMUM_STATIC_RULES;
        },
        get MAX_NUMBER_OF_STATIC_RULESETS() {
          return ExtensionDNRLimits.MAX_NUMBER_OF_STATIC_RULESETS;
        },
        get MAX_NUMBER_OF_ENABLED_STATIC_RULESETS() {
          return ExtensionDNRLimits.MAX_NUMBER_OF_ENABLED_STATIC_RULESETS;
        },
        get MAX_NUMBER_OF_DISABLED_STATIC_RULES() {
          return ExtensionDNRLimits.MAX_NUMBER_OF_DISABLED_STATIC_RULES;
        },
        get MAX_NUMBER_OF_DYNAMIC_RULES() {
          return ExtensionDNRLimits.MAX_NUMBER_OF_DYNAMIC_RULES;
        },
        get MAX_NUMBER_OF_SESSION_RULES() {
          return ExtensionDNRLimits.MAX_NUMBER_OF_SESSION_RULES;
        },
        get MAX_NUMBER_OF_REGEX_RULES() {
          return ExtensionDNRLimits.MAX_NUMBER_OF_REGEX_RULES;
        },
        // NOTE:this property is deprecated and it has been replaced by
        // MAX_NUMBER_OF_DYNAMIC_RULES/MAX_NUMBER_OF_SESSION_RULES, but kept
        // in the short term for backward compatibility.
        //
        // This getter returns the minimum value set on both the other two
        // new properties, in case MAX_NUMBER_OF_DYNAMIC_RULES or
        // MAX_NUMBER_OF_SESSION_RULES have been customized through prefs.
        get MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES() {
          return Math.min(
            ExtensionDNRLimits.MAX_NUMBER_OF_DYNAMIC_RULES,
            ExtensionDNRLimits.MAX_NUMBER_OF_SESSION_RULES
          );
        },
      },
    };
  }
};
