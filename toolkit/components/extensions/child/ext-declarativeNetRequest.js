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
  getAPI(context) {
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
        get MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES() {
          return ExtensionDNRLimits.MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES;
        },
        get MAX_NUMBER_OF_REGEX_RULES() {
          return ExtensionDNRLimits.MAX_NUMBER_OF_REGEX_RULES;
        },
      },
    };
  }
};
