/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Each extension that uses DNR has one RuleManager. All registered RuleManagers
// are checked whenever a network request occurs. Individual extensions may
// occasionally modify their rules (e.g. via the updateSessionRules API).
const gRuleManagers = [];

// The RuleCondition class represents a rule's "condition" type as described in
// schemas/declarative_net_request.json. This class exists to allow the JS
// engine to use one Shape for all Rule instances.
class RuleCondition {
  constructor(cond) {
    this.urlFilter = cond.urlFilter;
    this.regexFilter = cond.regexFilter;
    this.isUrlFilterCaseSensitive = cond.isUrlFilterCaseSensitive;
    this.initiatorDomains = cond.initiatorDomains;
    this.excludedInitiatorDomains = cond.excludedInitiatorDomains;
    this.requestDomains = cond.requestDomains;
    this.excludedRequestDomains = cond.excludedRequestDomains;
    this.resourceTypes = cond.resourceTypes;
    this.excludedResourceTypes = cond.excludedResourceTypes;
    this.requestMethods = cond.requestMethods;
    this.excludedRequestMethods = cond.excludedRequestMethods;
    this.domainType = cond.domainType;
    this.tabIds = cond.tabIds;
    this.excludedTabIds = cond.excludedTabIds;
  }
}

class Rule {
  constructor(rule) {
    this.id = rule.id;
    this.priority = rule.priority;
    this.condition = new RuleCondition(rule.condition);
    this.action = rule.action;
  }
}

class RuleValidator {
  constructor(alreadyValidatedRules) {
    this.rulesMap = new Map(alreadyValidatedRules.map(r => [r.id, r]));
    this.failures = [];
  }

  removeRuleIds(ruleIds) {
    for (const ruleId of ruleIds) {
      this.rulesMap.delete(ruleId);
    }
  }

  addRules(rules) {
    for (const rule of rules) {
      if (this.rulesMap.has(rule.id)) {
        this.#collectInvalidRule(rule, `Duplicate rule ID: ${rule.id}`);
        continue;
      }
      const newRule = new Rule(rule);

      // TODO bug 1745758: Need more validation, before rules are evaluated.

      this.rulesMap.set(rule.id, newRule);
    }
  }

  #collectInvalidRule(rule, message) {
    this.failures.push({ rule, message });
  }

  getValidatedRules() {
    return Array.from(this.rulesMap.values());
  }

  getFailures() {
    return this.failures;
  }
}

class RuleManager {
  constructor(extension) {
    this.extension = extension;
    this.sessionRules = [];
  }

  setSessionRules(validatedSessionRules) {
    this.sessionRules = validatedSessionRules;
  }

  getSessionRules() {
    return this.sessionRules;
  }
}

function getRuleManager(extension, createIfMissing = true) {
  let ruleManager = gRuleManagers.find(rm => rm.extension === extension);
  if (!ruleManager && createIfMissing) {
    ruleManager = new RuleManager(extension);
    // TODO bug 1786059: order extensions by "installation time".
    gRuleManagers.push(ruleManager);
  }
  return ruleManager;
}

function clearRuleManager(extension) {
  let i = gRuleManagers.findIndex(rm => rm.extension === extension);
  if (i !== -1) {
    gRuleManagers.splice(i, 1);
  }
}

export const ExtensionDNR = {
  RuleValidator,
  getRuleManager,
  clearRuleManager,
};
