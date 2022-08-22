/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionDNR: "resource://gre/modules/ExtensionDNR.sys.mjs",
});

var { ExtensionError } = ExtensionUtils;

this.declarativeNetRequest = class extends ExtensionAPI {
  onShutdown() {
    ExtensionDNR.clearRuleManager(this.extension);
  }
  getAPI(context) {
    const { extension } = this;

    return {
      declarativeNetRequest: {
        updateSessionRules({ removeRuleIds, addRules }) {
          const ruleManager = ExtensionDNR.getRuleManager(extension);
          let ruleValidator = new ExtensionDNR.RuleValidator(
            ruleManager.getSessionRules()
          );
          if (removeRuleIds) {
            ruleValidator.removeRuleIds(removeRuleIds);
          }
          if (addRules) {
            ruleValidator.addRules(addRules);
          }
          let failures = ruleValidator.getFailures();
          if (failures.length) {
            throw new ExtensionError(failures[0].message);
          }
          ruleManager.setSessionRules(ruleValidator.getValidatedRules());
        },

        getSessionRules() {
          // ruleManager.getSessionRules() returns an array of Rule instances.
          // When these are structurally cloned (to send them to the child),
          // the enumerable public fields of the class instances are copied to
          // plain objects, as desired.
          return ExtensionDNR.getRuleManager(extension).getSessionRules();
        },

        async testMatchOutcome(request) {
          // TODO bug 1745758: Implement rule evaluation engine.
          // Since rule registration has not been implemented yet, the result
          // is always an empty list.
          return [];
        },
      },
    };
  }
};
