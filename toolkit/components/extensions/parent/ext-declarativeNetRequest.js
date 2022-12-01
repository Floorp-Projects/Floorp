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
  onManifestEntry(entryName) {
    if (entryName === "declarative_net_request") {
      ExtensionDNR.validateManifestEntry(this.extension);
    }
  }

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

        async getEnabledRulesets() {
          await ExtensionDNR.ensureInitialized(extension);
          const ruleManager = ExtensionDNR.getRuleManager(extension);
          return ruleManager.enabledStaticRulesetIds;
        },

        async getAvailableStaticRuleCount() {
          await ExtensionDNR.ensureInitialized(extension);
          const ruleManager = ExtensionDNR.getRuleManager(extension);
          return ruleManager.availableStaticRuleCount;
        },

        updateEnabledRulesets({ disableRulesetIds, enableRulesetIds }) {
          return ExtensionDNR.updateEnabledStaticRulesets(extension, {
            disableRulesetIds,
            enableRulesetIds,
          });
        },

        getSessionRules() {
          // ruleManager.getSessionRules() returns an array of Rule instances.
          // When these are structurally cloned (to send them to the child),
          // the enumerable public fields of the class instances are copied to
          // plain objects, as desired.
          return ExtensionDNR.getRuleManager(extension).getSessionRules();
        },

        async testMatchOutcome(request, options) {
          let { url, initiator, ...req } = request;
          req.requestURI = Services.io.newURI(url);
          if (initiator) {
            req.initiatorURI = Services.io.newURI(initiator);
          }
          const matchedRules = ExtensionDNR.getMatchedRulesForRequest(
            req,
            options?.includeOtherExtensions ? null : extension
          ).map(matchedRule => {
            // Converts an internal MatchedRule instance to an object described
            // by the "MatchedRule" type in declarative_net_request.json.
            const result = {
              ruleId: matchedRule.rule.id,
              rulesetId: matchedRule.ruleset.id,
            };
            if (matchedRule.ruleManager.extension !== extension) {
              result.extensionId = matchedRule.ruleManager.extension.id;
            }
            return result;
          });
          return { matchedRules };
        },
      },
    };
  }
};
