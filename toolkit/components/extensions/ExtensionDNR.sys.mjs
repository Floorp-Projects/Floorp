/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Each extension that uses DNR has one RuleManager. All registered RuleManagers
// are checked whenever a network request occurs. Individual extensions may
// occasionally modify their rules (e.g. via the updateSessionRules API).
const gRuleManagers = [];

/**
 * Whenever a request occurs, the rules of each RuleManager are matched against
 * the request to determine the final action to take. The RequestEvaluator class
 * is responsible for evaluating rules, and its behavior is described below.
 *
 * Short version:
 * Find the highest-priority rule that matches the given request. If the
 * request is not canceled, all matching allowAllRequests and modifyHeaders
 * actions are returned.
 *
 * Longer version:
 * Unless stated otherwise, the explanation below describes the behavior within
 * an extension.
 * An extension can specify rules, optionally in multiple rulesets. The ability
 * to have multiple ruleset exists to support bulk updates of rules. Rulesets
 * are NOT independent - rules from different rulesets can affect each other.
 *
 * When multiple rules match, the order between rules are defined as follows:
 * - Ruleset precedence: session > dynamic > static (order from manifest.json).
 * - Rules in ruleset precedence: ordered by rule.id, lowest (numeric) ID first.
 * - Across all rules+rulesets: highest rule.priority (default 1) first,
 *                              action precedence if rule priority are the same.
 *
 * The primary documented way for extensions to describe precedence is by
 * specifying rule.priority. Between same-priority rules, their precedence is
 * dependent on the rule action. The ruleset/rule ID precedence is only used to
 * have a defined ordering if multiple rules have the same priority+action.
 *
 * Rule actions have the following order of precedence and meaning:
 * - "allow" can be used to ignore other same-or-lower-priority rules.
 * - "allowAllRequests" (for main_frame / sub_frame resourceTypes only) has the
 *      same effect as allow, but also applies to (future) subresource loads in
 *      the document (including descendant frames) generated from the request.
 * - "block" cancels the matched request.
 * - "upgradeScheme" upgrades the scheme of the request.
 * - "redirect" redirects the request.
 * - "modifyHeaders" rewrites request/response headers.
 *
 * The matched rules are evaluated in two passes:
 * 1. findMatchingRules():
 *    Find the highest-priority rule(s), and choose the action with the highest
 *    precedence (across all rulesets, any action except modifyHeaders).
 *    This also accounts for any allowAllRequests from an ancestor frame.
 *
 * 2. getMatchingModifyHeadersRules():
 *    Find matching rules with the "modifyHeaders" action, minus ignored rules.
 *    Reaching this step implies that the request was not canceled, so either
 *    the first step did not yield a rule, or the rule action is "allow" or
 *    "allowAllRequests" (i.e. ignore same-or-lower-priority rules).
 *
 * If an extension does not have sufficient permissions for the action, the
 * resulting action is ignored.
 *
 * The above describes the evaluation within one extension. When a sequence of
 * (multiple) extensions is given, they may return conflicting actions in the
 * first pass. This is resolved by choosing the action with the following order
 * of precedence, in RequestEvaluator.evaluateRequest():
 *  - block
 *  - redirect / upgradeScheme
 *  - allow / allowAllRequests
 */

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "WebRequest",
  "resource://gre/modules/WebRequest.jsm"
);

ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionDNRStore: "resource://gre/modules/ExtensionDNRStore.sys.mjs",
});

/**
 * The minimum number of static rules guaranteed to an extension across its
 * enabled static rulesets. Any rules above this limit will count towards the
 * global static rule limit.
 */
const GUARANTEED_MINIMUM_STATIC_RULES = 30000;

/**
 * The maximum number of static Rulesets an extension can specify as part of
 * the "rule_resources" manifest key.
 *
 * NOTE: this limit may be increased in the future, see https://github.com/w3c/webextensions/issues/318
 */
const MAX_NUMBER_OF_STATIC_RULESETS = 50;

/**
 * The maximum number of static Rulesets an extension can enable at any one time.
 *
 * NOTE: this limit may be increased in the future, see https://github.com/w3c/webextensions/issues/318
 */
const MAX_NUMBER_OF_ENABLED_STATIC_RULESETS = 10;

// TODO(Bug 1803370): allow extension to exceed the GUARANTEED_MINIMUM_STATIC_RULES limit.
//
// The maximum number of static rules exceeding the per-extension
// GUARANTEED_MINIMUM_STATIC_RULES across every extensions.
//
// const MAX_GLOBAL_NUMBER_OF_STATIC_RULES = 300000;

// As documented above:
// Ruleset precedence: session > dynamic > static (order from manifest.json).
const PRECEDENCE_SESSION_RULESET = 1;
const PRECEDENCE_DYNAMIC_RULESET = 2;
const PRECEDENCE_STATIC_RULESETS_BASE = 3;

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

  // The precedence of rules within an extension. This method is frequently
  // used during the first pass of the RequestEvaluator.
  actionPrecedence() {
    switch (this.action.type) {
      case "allow":
        return 1; // Highest precedence.
      case "allowAllRequests":
        return 2;
      case "block":
        return 3;
      case "upgradeScheme":
        return 4;
      case "redirect":
        return 5;
      case "modifyHeaders":
        return 6;
      default:
        throw new Error(`Unexpected action type: ${this.action.type}`);
    }
  }

  isAllowOrAllowAllRequestsAction() {
    const type = this.action.type;
    return type === "allow" || type === "allowAllRequests";
  }
}

class Ruleset {
  /**
   * @param {string} rulesetId - extension-defined ruleset ID.
   * @param {integer} rulesetPrecedence
   * @param {Rule[]} rules - extension-defined rules
   * @param {RuleManager} ruleManager - owner of this ruleset.
   */
  constructor(rulesetId, rulesetPrecedence, rules, ruleManager) {
    this.id = rulesetId;
    this.rulesetPrecedence = rulesetPrecedence;
    this.rules = rules;
    // For use by MatchedRule.
    this.ruleManager = ruleManager;
  }
}

/**
 * @param {string} uriQuery - The query of a nsIURI to transform.
 * @param {object} queryTransform - The value of the
 *   Rule.action.redirect.transform.queryTransform property as defined in
 *   declarative_net_request.json.
 * @returns {string} The uriQuery with the queryTransform applied to it.
 */
function applyQueryTransform(uriQuery, queryTransform) {
  // URLSearchParams cannot be applied to the full query string, because that
  // API formats the full query string using form-urlencoding. But the input
  // may be in a different format. So we try to only modify matched params.

  function urlencode(s) {
    // Encode in application/x-www-form-urlencoded format.
    // The only JS API to do that is URLSearchParams. encodeURIComponent is not
    // the same, it differs in how it handles " " ("%20") and "!'()~" (raw).
    // But urlencoded space should be "+" and the latter be "%21%27%28%29%7E".
    return new URLSearchParams({ s }).toString().slice(2);
  }
  if (!uriQuery.length && !queryTransform.addOrReplaceParams) {
    // Nothing to do.
    return "";
  }
  const removeParamsSet = new Set(queryTransform.removeParams?.map(urlencode));
  const addParams = (queryTransform.addOrReplaceParams || []).map(orig => ({
    normalizedKey: urlencode(orig.key),
    orig,
  }));
  const finalParams = [];
  if (uriQuery.length) {
    for (let part of uriQuery.split("&")) {
      let key = part.split("=", 1)[0];
      if (removeParamsSet.has(key)) {
        continue;
      }
      let i = addParams.findIndex(p => p.normalizedKey === key);
      if (i !== -1) {
        // Replace found param with the key-value from addOrReplaceParams.
        finalParams.push(`${key}=${urlencode(addParams[i].orig.value)}`);
        // Omit param so that a future search for the same key can find the next
        // specified key-value pair, if any. And to prevent the already-used
        // key-value pairs from being appended after the loop.
        addParams.splice(i, 1);
      } else {
        finalParams.push(part);
      }
    }
  }
  // Append remaining, unused key-value pairs.
  for (let { normalizedKey, orig } of addParams) {
    if (!orig.replaceOnly) {
      finalParams.push(`${normalizedKey}=${urlencode(orig.value)}`);
    }
  }
  return finalParams.length ? `?${finalParams.join("&")}` : "";
}

/**
 * @param {nsIURI} uri - Usually a http(s) URL.
 * @param {object} transform - The value of the Rule.action.redirect.transform
 *   property as defined in declarative_net_request.json.
 * @returns {nsIURI} uri - The new URL.
 * @throws if the transformation is invalid.
 */
function applyURLTransform(uri, transform) {
  let mut = uri.mutate();
  if (transform.scheme) {
    // Note: declarative_net_request.json only allows http(s)/moz-extension:.
    mut.setScheme(transform.scheme);
    if (uri.port !== -1 || transform.port) {
      // If the URI contains a port or transform.port was specified, the default
      // port is significant. So we must set it in that case.
      if (transform.scheme === "https") {
        mut.QueryInterface(Ci.nsIStandardURLMutator).setDefaultPort(443);
      } else if (transform.scheme === "http") {
        mut.QueryInterface(Ci.nsIStandardURLMutator).setDefaultPort(80);
      }
    }
  }
  if (transform.username != null) {
    mut.setUsername(transform.username);
  }
  if (transform.password != null) {
    mut.setPassword(transform.password);
  }
  if (transform.host != null) {
    mut.setHost(transform.host);
  }
  if (transform.port != null) {
    // The caller ensures that transform.port is a string consisting of digits
    // only. When it is an empty string, it should be cleared (-1).
    mut.setPort(transform.port || -1);
  }
  if (transform.path != null) {
    mut.setFilePath(transform.path);
  }
  if (transform.query != null) {
    mut.setQuery(transform.query);
  } else if (transform.queryTransform) {
    mut.setQuery(applyQueryTransform(uri.query, transform.queryTransform));
  }
  if (transform.fragment != null) {
    mut.setRef(transform.fragment);
  }
  return mut.finalize();
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

  /**
   * @param {object[]} rules - A list of objects that adhere to the Rule type
   *    from declarative_net_request.json.
   */
  addRules(rules) {
    for (const rule of rules) {
      if (this.rulesMap.has(rule.id)) {
        this.#collectInvalidRule(rule, `Duplicate rule ID: ${rule.id}`);
        continue;
      }
      // declarative_net_request.json defines basic types, such as the expected
      // object properties and (primitive) type. Trivial constraints such as
      // minimum array lengths are also expressed in the schema.
      // Anything more complex is validated here. In particular, constraints
      // involving multiple properties (e.g. mutual exclusiveness).
      //
      // The following conditions have already been validated by the schema:
      // - isUrlFilterCaseSensitive (boolean)
      // - domainType (enum string)
      // - initiatorDomains & excludedInitiatorDomains & requestDomains &
      //   excludedRequestDomains (array of string in canonicalDomain format)
      // TODO bug 1745759: urlFilter validation
      // TODO bug 1745760: regexFilter validation
      if (
        !this.#checkCondResourceTypes(rule) ||
        !this.#checkCondRequestMethods(rule) ||
        !this.#checkCondTabIds(rule) ||
        !this.#checkAction(rule)
      ) {
        continue;
      }

      const newRule = new Rule(rule);

      this.rulesMap.set(rule.id, newRule);
    }
  }

  // Checks: resourceTypes & excludedResourceTypes
  #checkCondResourceTypes(rule) {
    const { resourceTypes, excludedResourceTypes } = rule.condition;
    if (this.#hasOverlap(resourceTypes, excludedResourceTypes)) {
      this.#collectInvalidRule(
        rule,
        "resourceTypes and excludedResourceTypes should not overlap"
      );
      return false;
    }
    if (rule.action.type === "allowAllRequests") {
      if (!resourceTypes) {
        this.#collectInvalidRule(
          rule,
          "An allowAllRequests rule must have a non-empty resourceTypes array"
        );
        return false;
      }
      if (resourceTypes.some(r => r !== "main_frame" && r !== "sub_frame")) {
        this.#collectInvalidRule(
          rule,
          "An allowAllRequests rule may only include main_frame/sub_frame in resourceTypes"
        );
        return false;
      }
    }
    return true;
  }

  // Checks: requestMethods & excludedRequestMethods
  #checkCondRequestMethods(rule) {
    const { requestMethods, excludedRequestMethods } = rule.condition;
    if (this.#hasOverlap(requestMethods, excludedRequestMethods)) {
      this.#collectInvalidRule(
        rule,
        "requestMethods and excludedRequestMethods should not overlap"
      );
      return false;
    }
    const isInvalidRequestMethod = method => method.toLowerCase() !== method;
    if (
      requestMethods?.some(isInvalidRequestMethod) ||
      excludedRequestMethods?.some(isInvalidRequestMethod)
    ) {
      this.#collectInvalidRule(rule, "request methods must be in lower case");
      return false;
    }
    return true;
  }

  // Checks: tabIds & excludedTabIds
  #checkCondTabIds(rule) {
    const { tabIds, excludedTabIds } = rule.condition;
    if (this.#hasOverlap(tabIds, excludedTabIds)) {
      this.#collectInvalidRule(
        rule,
        "tabIds and excludedTabIds should not overlap"
      );
      return false;
    }
    // TODO bug 1745764 / bug 1745763: after adding support for dynamic/static
    // rules, validate that we only have a session ruleset here.
    return true;
  }

  #checkAction(rule) {
    switch (rule.action.type) {
      case "allow":
      case "allowAllRequests":
      case "block":
      case "upgradeScheme":
        // These actions have no extra properties.
        break;
      case "redirect":
        return this.#checkActionRedirect(rule);
      case "modifyHeaders":
        return this.#checkActionModifyHeaders(rule);
      default:
        // Other values are not possible because declarative_net_request.json
        // only accepts the above action types.
        throw new Error(`Unexpected action type: ${rule.action.type}`);
    }
    return true;
  }

  #checkActionRedirect(rule) {
    const { extensionPath, url, transform } = rule.action.redirect ?? {};
    if (!url && extensionPath == null && !transform) {
      this.#collectInvalidRule(
        rule,
        "A redirect rule must have a non-empty action.redirect object"
      );
      return false;
    }
    if (url && extensionPath != null) {
      this.#collectInvalidRule(
        rule,
        "redirect.extensionPath and redirect.url are mutually exclusive"
      );
      return false;
    }
    if (extensionPath != null && !extensionPath.startsWith("/")) {
      this.#collectInvalidRule(
        rule,
        "redirect.extensionPath should start with a '/'"
      );
      return false;
    }
    // If specified, the "url" property is described as "format": "url" in the
    // JSON schema, which ensures that the URL is a canonical form, and that
    // the extension is allowed to trigger a navigation to the URL.
    // E.g. javascript: and privileged about:-URLs cannot be navigated to, but
    // http(s) URLs can (regardless of extension permissions).
    // data:-URLs are currently blocked due to bug 1622986.

    if (transform) {
      if (transform.query != null && transform.queryTransform) {
        this.#collectInvalidRule(
          rule,
          "redirect.transform.query and redirect.transform.queryTransform are mutually exclusive"
        );
        return false;
      }
      // Most of the validation is done by nsIURIMutator via applyURLTransform.
      // nsIURIMutator is not very strict, so we perform some extra checks here
      // to reject values that are not technically valid URLs.

      if (transform.port && /\D/.test(transform.port)) {
        // nsIURIMutator's setPort takes an int, so any string will implicitly
        // be converted to a number. This part verifies that the input only
        // consists of digits. setPort will ensure that it is at most 65535.
        this.#collectInvalidRule(
          rule,
          "redirect.transform.port should be empty or an integer"
        );
        return false;
      }

      // Note: we don't verify whether transform.query starts with '/', because
      // Chrome does not require it, and nsIURIMutator prepends it if missing.

      if (transform.query && !transform.query.startsWith("?")) {
        this.#collectInvalidRule(
          rule,
          "redirect.transform.query should be empty or start with a '?'"
        );
        return false;
      }
      if (transform.fragment && !transform.fragment.startsWith("#")) {
        this.#collectInvalidRule(
          rule,
          "redirect.transform.fragment should be empty or start with a '#'"
        );
        return false;
      }
      try {
        const dummyURI = Services.io.newURI("http://dummy");
        // applyURLTransform uses nsIURIMutator to transform a URI, and throws
        // if |transform| is invalid, e.g. invalid host, port, etc.
        applyURLTransform(dummyURI, transform);
      } catch (e) {
        this.#collectInvalidRule(
          rule,
          "redirect.transform does not describe a valid URL transformation"
        );
        return false;
      }
    }

    // TODO bug 1745760: With regexFilter support, implement regexSubstitution.
    return true;
  }

  #checkActionModifyHeaders(rule) {
    const { requestHeaders, responseHeaders } = rule.action;
    if (!requestHeaders && !responseHeaders) {
      this.#collectInvalidRule(
        rule,
        "A modifyHeaders rule must have a non-empty requestHeaders or modifyHeaders list"
      );
      return false;
    }

    const isValidModifyHeadersOp = ({ header, operation, value }) => {
      if (!header) {
        this.#collectInvalidRule(rule, "header must be non-empty");
        return false;
      }
      if (!value && (operation === "append" || operation === "set")) {
        this.#collectInvalidRule(
          rule,
          "value is required for operations append/set"
        );
        return false;
      }
      if (value && operation === "remove") {
        this.#collectInvalidRule(
          rule,
          "value must not be provided for operation remove"
        );
        return false;
      }
      return true;
    };
    if (
      (requestHeaders && !requestHeaders.every(isValidModifyHeadersOp)) ||
      (responseHeaders && !responseHeaders.every(isValidModifyHeadersOp))
    ) {
      return false;
    }
    return true;
  }

  // Conditions with a filter and an exclude-filter should reject overlapping
  // lists, because they can never simultaneously be true.
  #hasOverlap(arrayA, arrayB) {
    return arrayA && arrayB && arrayA.some(v => arrayB.includes(v));
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

/**
 * Compares two rules to determine the relative order of precedence.
 * Rules are only comparable if they are from the same extension!
 *
 * @param {Rule} ruleA
 * @param {Rule} ruleB
 * @param {Ruleset} rulesetA - the ruleset ruleA is part of.
 * @param {Ruleset} rulesetB - the ruleset ruleB is part of.
 * @returns {integer}
 *   0 if equal.
 *   <0 if ruleA comes before ruleB.
 *   >0 if ruleA comes after ruleB.
 */
function compareRule(ruleA, ruleB, rulesetA, rulesetB) {
  // Comparators: 0 if equal, >0 if a after b, <0 if a before b.
  function cmpHighestNumber(a, b) {
    return a === b ? 0 : b - a;
  }
  function cmpLowestNumber(a, b) {
    return a === b ? 0 : a - b;
  }
  return (
    // All compared operands are non-negative integers.
    cmpHighestNumber(ruleA.priority, ruleB.priority) ||
    cmpLowestNumber(ruleA.actionPrecedence(), ruleB.actionPrecedence()) ||
    // As noted in the big comment at the top of the file, the following two
    // comparisons only exist in order to have a stable ordering of rules. The
    // specific comparison is somewhat arbitrary and matches Chrome's behavior.
    // For context, see https://github.com/w3c/webextensions/issues/280
    cmpLowestNumber(rulesetA.rulesetPrecedence, rulesetB.rulesetPrecedence) ||
    cmpLowestNumber(ruleA.id, ruleB.id)
  );
}

class MatchedRule {
  constructor(rule, ruleset) {
    this.rule = rule;
    this.ruleset = ruleset;
  }

  // The RuleManager that generated this MatchedRule.
  get ruleManager() {
    return this.ruleset.ruleManager;
  }
}

// tabId computation is currently not free, and depends on the initialization of
// ExtensionParent.apiManager.global (see WebRequest.getTabIdForChannelWrapper).
// Fortunately, DNR only supports tabIds in session rules, so by keeping track
// of session rules with tabIds/excludedTabIds conditions, we can find tabId
// exactly and only when necessary.
let gHasAnyTabIdConditions = false;

class RequestDetails {
  /**
   * @param {object} options
   * @param {nsIURI} options.requestURI - URL of the requested resource.
   * @param {nsIURI} [options.initiatorURI] - URL of triggering principal (non-null).
   * @param {string} options.type - ResourceType (MozContentPolicyType).
   * @param {string} [options.method] - HTTP method
   * @param {integer} [options.tabId]
   */
  constructor({ requestURI, initiatorURI, type, method, tabId }) {
    this.requestURI = requestURI;
    this.initiatorURI = initiatorURI;
    this.type = type;
    this.method = method;
    this.tabId = tabId;

    this.requestDomain = this.#domainFromURI(requestURI);
    this.initiatorDomain = initiatorURI
      ? this.#domainFromURI(initiatorURI)
      : null;
  }

  static fromChannelWrapper(channel) {
    let tabId = -1;
    if (gHasAnyTabIdConditions) {
      tabId = lazy.WebRequest.getTabIdForChannelWrapper(channel);
    }
    return new RequestDetails({
      requestURI: channel.finalURI,
      // Note: originURI may be null, if missing or null principal, as desired.
      initiatorURI: channel.originURI,
      type: channel.type,
      method: channel.method.toLowerCase(),
      tabId,
    });
  }

  canExtensionModify(extension) {
    const policy = extension.policy;
    return (
      (!this.initiatorURI || policy.canAccessURI(this.initiatorURI)) &&
      policy.canAccessURI(this.requestURI)
    );
  }

  #domainFromURI(uri) {
    let hostname = uri.host;
    // nsIURI omits brackets from IPv6 addresses. But the canonical form of an
    // IPv6 address is with brackets, so add them.
    return hostname.includes(":") ? `[${hostname}]` : hostname;
  }
}

/**
 * This RequestEvaluator class's logic is documented at the top of this file.
 */
class RequestEvaluator {
  // private constructor, only used by RequestEvaluator.evaluateRequest.
  constructor(request, ruleManager) {
    this.req = request;
    this.ruleManager = ruleManager;
    this.canModify = request.canExtensionModify(ruleManager.extension);

    // These values are initialized by findMatchingRules():
    this.matchedRule = null;
    this.matchedModifyHeadersRules = [];
    this.findMatchingRules();
  }

  /**
   * Finds the matched rules for the given request and extensions,
   * according to the logic documented at the top of this file.
   *
   * @param {RequestDetails} request
   * @param {RuleManager[]} ruleManagers
   *    The list of RuleManagers, ordered by importance of its extension.
   * @returns {MatchedRule[]}
   */
  static evaluateRequest(request, ruleManagers) {
    // Helper to determine precedence of rules from different extensions.
    function precedence(matchedRule) {
      switch (matchedRule.rule.action.type) {
        case "block":
          return 1;
        case "redirect":
        case "upgradeScheme":
          return 2;
        case "allow":
        case "allowAllRequests":
          return 3;
        // case "modifyHeaders": not comparable after the first pass.
        default:
          throw new Error(`Unexpected action: ${matchedRule.rule.action.type}`);
      }
    }

    let requestEvaluators = [];
    let finalMatch;
    let finalAllowAllRequestsMatches = [];
    for (let ruleManager of ruleManagers) {
      // Evaluate request with findMatchingRules():
      const requestEvaluator = new RequestEvaluator(request, ruleManager);
      // RequestEvaluator may be used after the loop when the request is
      // accepted, to collect modifyHeaders/allow/allowAllRequests actions.
      requestEvaluators.push(requestEvaluator);
      let matchedRule = requestEvaluator.matchedRule;
      if (matchedRule) {
        if (matchedRule.rule.action.type === "allowAllRequests") {
          // Even if a different extension wins the final match, an extension
          // may want to record the "allowAllRequests" action for the future.
          finalAllowAllRequestsMatches.push(matchedRule);
        }
        if (!finalMatch || precedence(matchedRule) < precedence(finalMatch)) {
          finalMatch = matchedRule;
          if (finalMatch.rule.action.type === "block") {
            break;
          }
        }
      }
    }
    if (finalMatch && !finalMatch.rule.isAllowOrAllowAllRequestsAction()) {
      // Found block/redirect/upgradeScheme, request will be replaced.
      return [finalMatch];
    }
    // Request not canceled, collect all modifyHeaders actions:
    let matchedRules = requestEvaluators
      .map(re => re.getMatchingModifyHeadersRules())
      .flat(1);

    // ... and collect the allowAllRequests actions:
    if (finalAllowAllRequestsMatches.length) {
      matchedRules = finalAllowAllRequestsMatches.concat(matchedRules);
    }

    // ... and collect the "allow" action. At this point, finalMatch could also
    // be a modifyHeaders or allowAllRequests action, but these would already
    // have been added to the matchedRules result before.
    if (finalMatch && finalMatch.rule.action.type === "allow") {
      matchedRules.unshift(finalMatch);
    }
    return matchedRules;
  }

  /**
   * Finds the matching rules, as documented in the comment before the class.
   */
  findMatchingRules() {
    if (!this.canModify && !this.ruleManager.hasBlockPermission) {
      // If the extension cannot apply any action, don't bother.
      return;
    }

    this.#collectMatchInRuleset(this.ruleManager.sessionRules);
    this.#collectMatchInRuleset(this.ruleManager.dynamicRules);
    for (let ruleset of this.ruleManager.enabledStaticRules) {
      this.#collectMatchInRuleset(ruleset);
    }

    if (this.matchedRule && !this.#isRuleActionAllowed(this.matchedRule.rule)) {
      this.matchedRule = null;
      // Note: this.matchedModifyHeadersRules is [] because canModify access is
      // checked before populating the list.
    }
  }

  /**
   * Retrieves the list of matched modifyHeaders rules that should apply.
   *
   * @returns {MatchedRule[]}
   */
  getMatchingModifyHeadersRules() {
    // The minimum priority is 1. Defaulting to 0 = include all.
    let priorityThreshold = 0;
    if (this.matchedRule?.rule.isAllowOrAllowAllRequestsAction()) {
      priorityThreshold = this.matchedRule.rule.priority;
    }
    // Note: the result cannot be non-empty if this.matchedRule is a non-allow
    // action, because if that were to be the case, then the request would have
    // been canceled, and therefore there would not be any header to modify.
    // Even if another extension were to override the action, it could only be
    // any other non-allow action, which would still cancel the request.
    let matchedRules = this.matchedModifyHeadersRules.filter(matchedRule => {
      return matchedRule.rule.priority > priorityThreshold;
    });
    // Sort output for a deterministic order.
    // NOTE: Sorting rules at registration (in RuleManagers) would avoid the
    // need to sort here. Since the number of matched modifyHeaders rules are
    // expected to be small, we don't bother optimizing.
    matchedRules.sort((a, b) => {
      return compareRule(a.rule, b.rule, a.ruleset, b.ruleset);
    });
    return matchedRules;
  }

  #collectMatchInRuleset(ruleset) {
    for (let rule of ruleset.rules) {
      if (!this.#matchesRuleCondition(rule.condition)) {
        continue;
      }
      if (rule.action.type === "modifyHeaders") {
        if (this.canModify) {
          this.matchedModifyHeadersRules.push(new MatchedRule(rule, ruleset));
        }
        continue;
      }
      if (
        this.matchedRule &&
        compareRule(
          this.matchedRule.rule,
          rule,
          this.matchedRule.ruleset,
          ruleset
        ) <= 0
      ) {
        continue;
      }
      this.matchedRule = new MatchedRule(rule, ruleset);
    }
  }

  /**
   * @param {RuleCondition} cond
   * @returns {boolean} Whether the condition matched.
   */
  #matchesRuleCondition(cond) {
    if (cond.resourceTypes) {
      if (!cond.resourceTypes.includes(this.req.type)) {
        return false;
      }
    } else if (cond.excludedResourceTypes) {
      if (cond.excludedResourceTypes.includes(this.req.type)) {
        return false;
      }
    } else if (this.req.type === "main_frame") {
      // When resourceTypes/excludedResourceTypes are not specified, the
      // documented behavior is to ignore main_frame requests.
      return false;
    }

    // Check this.req.requestURI:
    if (cond.urlFilter) {
      if (
        !this.#matchesUrlFilter(
          this.req.requestURI,
          cond.urlFilter,
          cond.isUrlFilterCaseSensitive
        )
      ) {
        return false;
      }
    } else if (cond.regexFilter) {
      // TODO bug 1745760: check cond.regexFilter + isUrlFilterCaseSensitive
    }
    if (
      cond.excludedRequestDomains &&
      this.#matchesDomains(cond.excludedRequestDomains, this.req.requestDomain)
    ) {
      return false;
    }
    if (
      cond.requestDomains &&
      !this.#matchesDomains(cond.requestDomains, this.req.requestDomain)
    ) {
      return false;
    }
    if (
      cond.excludedInitiatorDomains &&
      // Note: unable to only match null principals (bug 1798225).
      this.req.initiatorDomain &&
      this.#matchesDomains(
        cond.excludedInitiatorDomains,
        this.req.initiatorDomain
      )
    ) {
      return false;
    }
    if (
      cond.initiatorDomains &&
      // Note: unable to only match null principals (bug 1798225).
      (!this.req.initiatorDomain ||
        !this.#matchesDomains(cond.initiatorDomains, this.req.initiatorDomain))
    ) {
      return false;
    }

    // TODO bug 1797408: domainType

    if (cond.requestMethods) {
      if (!cond.requestMethods.includes(this.req.method)) {
        return false;
      }
    } else if (cond.excludedRequestMethods?.includes(this.req.method)) {
      return false;
    }

    if (cond.tabIds) {
      if (!cond.tabIds.includes(this.req.tabId)) {
        return false;
      }
    } else if (cond.excludedTabIds?.includes(this.req.tabId)) {
      return false;
    }

    return true;
  }

  /**
   * @param {nsIURI} uri - The request URI.
   * @param {string} urlFilter
   * @param {boolean} [isUrlFilterCaseSensitive]
   * @returns {boolean} Whether urlFilter matches the given uri.
   */
  #matchesUrlFilter(uri, urlFilter, isUrlFilterCaseSensitive) {
    // TODO bug 1745759: Check cond.urlFilter + isUrlFilterCaseSensitive
    // Placeholder for unit test until we have a complete implementation.
    if (urlFilter === "|https:*") {
      return uri.schemeIs("https");
    }
    throw new Error(`urlFilter not implemented yet: ${urlFilter}`);
    // return true; after all other checks passed.
  }

  /**
   * @param {string[]} domains - A list of canonicalized domain patterns.
   *   Canonical means punycode, no ports, and IPv6 without brackets, and not
   *   starting with a dot. May end with a dot if it is a FQDN.
   * @param {string} host - The canonical representation of the host of a URL.
   * @returns {boolean} Whether the given host is a (sub)domain of any of the
   *   given domains.
   */
  #matchesDomains(domains, host) {
    return domains.some(domain => {
      return (
        host.endsWith(domain) &&
        // either host === domain
        (host.length === domain.length ||
          // or host = "something." + domain (WITH a domain separator).
          host.charAt(host.length - domain.length - 1) === ".")
      );
    });
  }

  /**
   * @param {Rule} rule - The final rule from the first pass.
   * @returns {boolean} Whether the extension is allowed to execute the rule.
   */
  #isRuleActionAllowed(rule) {
    if (this.canModify) {
      return true;
    }
    switch (rule.action.type) {
      case "allow":
      case "allowAllRequests":
      case "block":
      case "upgradeScheme":
        return this.ruleManager.hasBlockPermission;
      case "redirect":
        return false;
      // case "modifyHeaders" is never an action for this.matchedRule.
      default:
        throw new Error(`Unexpected action type: ${rule.action.type}`);
    }
  }
}

const NetworkIntegration = {
  register() {
    // We register via WebRequest.jsm to ensure predictable ordering of DNR and
    // WebRequest behavior.
    lazy.WebRequest.setDNRHandlingEnabled(true);
  },
  unregister() {
    lazy.WebRequest.setDNRHandlingEnabled(false);
  },
  maybeUpdateTabIdChecker() {
    gHasAnyTabIdConditions = gRuleManagers.some(rm => rm.hasRulesWithTabIds);
  },

  startDNREvaluation(channel) {
    let ruleManagers = gRuleManagers;
    if (!channel.canModify) {
      ruleManagers = [];
    }
    if (channel.loadInfo.originAttributes.privateBrowsingId > 0) {
      ruleManagers = ruleManagers.filter(
        rm => rm.extension.privateBrowsingAllowed
      );
    }
    let matchedRules;
    if (ruleManagers.length) {
      const request = RequestDetails.fromChannelWrapper(channel);
      matchedRules = RequestEvaluator.evaluateRequest(request, ruleManagers);
    }
    // Cache for later. In case of redirects, _dnrMatchedRules may exist for
    // the pre-redirect HTTP channel, and is overwritten here again.
    channel._dnrMatchedRules = matchedRules;
  },

  /**
   * Applies the actions of the DNR rules.
   *
   * @param {ChannelWrapper} channel
   * @returns {boolean} Whether to ignore any responses from the webRequest API.
   */
  onBeforeRequest(channel) {
    let matchedRules = channel._dnrMatchedRules;
    if (!matchedRules?.length) {
      return false;
    }
    // If a matched rule closes the channel, it is the sole match.
    const finalMatch = matchedRules[0];
    switch (finalMatch.rule.action.type) {
      case "block":
        this.applyBlock(channel, finalMatch);
        return true;
      case "redirect":
        this.applyRedirect(channel, finalMatch);
        return true;
      case "upgradeScheme":
        this.applyUpgradeScheme(channel, finalMatch);
        return true;
    }
    // If there are multiple rules, then it may be a combination of allow,
    // allowAllRequests and/or modifyHeaders.

    // TODO bug 1797403: Apply allowAllRequests actions.

    return false;
  },

  onBeforeSendHeaders(channel) {
    // TODO bug 1797404: apply modifyHeaders actions (requestHeaders).
  },

  onHeadersReceived(channel) {
    // TODO bug 1797404: apply modifyHeaders actions (responseHeaders).
  },

  applyBlock(channel, matchedRule) {
    // TODO bug 1802259: Consider a DNR-specific reason.
    channel.cancel(
      Cr.NS_ERROR_ABORT,
      Ci.nsILoadInfo.BLOCKING_REASON_EXTENSION_WEBREQUEST
    );
    const addonId = matchedRule.ruleManager.extension.id;
    let properties = channel.channel.QueryInterface(Ci.nsIWritablePropertyBag);
    properties.setProperty("cancelledByExtension", addonId);
  },

  applyUpgradeScheme(channel, matchedRule) {
    // Request upgrade. No-op if already secure (i.e. https).
    channel.upgradeToSecure();
  },

  applyRedirect(channel, matchedRule) {
    // Ambiguity resolution order of redirect dict keys, consistent with Chrome:
    // - url > extensionPath > transform > regexSubstitution
    const redirect = matchedRule.rule.action.redirect;
    const extension = matchedRule.ruleManager.extension;
    let redirectUri;
    if (redirect.url) {
      // redirect.url already validated by checkActionRedirect.
      redirectUri = Services.io.newURI(redirect.url);
    } else if (redirect.extensionPath) {
      redirectUri = extension.baseURI
        .mutate()
        .setPathQueryRef(redirect.extensionPath)
        .finalize();
    } else if (redirect.transform) {
      redirectUri = applyURLTransform(channel.finalURI, redirect.transform);
    } else if (redirect.regexSubstitution) {
      // TODO bug 1745760: Implement along with regexFilter support.
      throw new Error("regexSubstitution not implemented");
    } else {
      // #checkActionRedirect ensures that the redirect action is non-empty.
    }

    channel.redirectTo(redirectUri);

    let properties = channel.channel.QueryInterface(Ci.nsIWritablePropertyBag);
    properties.setProperty("redirectedByExtension", extension.id);

    let origin = channel.getRequestHeader("Origin");
    if (origin) {
      channel.setResponseHeader("Access-Control-Allow-Origin", origin);
      channel.setResponseHeader("Access-Control-Allow-Credentials", "true");
      channel.setResponseHeader("Access-Control-Max-Age", "0");
    }
  },
};

class RuleManager {
  constructor(extension) {
    this.extension = extension;
    this.sessionRules = this.makeRuleset(
      "_session",
      PRECEDENCE_SESSION_RULESET
    );
    // TODO bug 1745764: support registration of (persistent) dynamic rules.
    this.dynamicRules = this.makeRuleset(
      "_dynamic",
      PRECEDENCE_DYNAMIC_RULESET
    );
    this.enabledStaticRules = [];

    this.hasBlockPermission = extension.hasPermission("declarativeNetRequest");
    this.hasRulesWithTabIds = false;
  }

  get enabledStaticRulesetIds() {
    return this.enabledStaticRules.map(ruleset => ruleset.id);
  }

  makeRuleset(rulesetId, rulesetPrecedence, rules = []) {
    return new Ruleset(rulesetId, rulesetPrecedence, rules, this);
  }

  setSessionRules(validatedSessionRules) {
    this.sessionRules.rules = validatedSessionRules;
    this.hasRulesWithTabIds = !!this.sessionRules.rules.find(rule => {
      return rule.condition.tabIds || rule.condition.excludedTabIds;
    });
    NetworkIntegration.maybeUpdateTabIdChecker();
  }

  /**
   * Set the enabled static rulesets.
   *
   * @param {Array<{ id, rules }>} enabledStaticRulesets
   *        Array of objects including the ruleset id and rules.
   *        The order of the rulesets in the Array is expected to
   *        match the order of the rulesets in the extension manifest.
   */
  setEnabledStaticRulesets(enabledStaticRulesets) {
    const rulesets = [];
    for (const [idx, { id, rules }] of enabledStaticRulesets.entries()) {
      rulesets.push(
        this.makeRuleset(id, idx + PRECEDENCE_STATIC_RULESETS_BASE, rules)
      );
    }
    this.enabledStaticRules = rulesets;
  }

  getSessionRules() {
    return this.sessionRules.rules;
  }
}

function getRuleManager(extension, createIfMissing = true) {
  let ruleManager = gRuleManagers.find(rm => rm.extension === extension);
  if (!ruleManager && createIfMissing) {
    if (extension.hasShutdown) {
      throw new Error(
        `Error on creating new DNR RuleManager after extension shutdown: ${extension.id}`
      );
    }
    ruleManager = new RuleManager(extension);
    // The most recently installed extension gets priority, i.e. appears at the
    // start of the gRuleManagers list. It is not yet possible to determine the
    // installation time of a given Extension, so currently the last to
    // instantiate a RuleManager claims the highest priority.
    // TODO bug 1786059: order extensions by "installation time".
    gRuleManagers.unshift(ruleManager);
    if (gRuleManagers.length === 1) {
      // The first DNR registration.
      NetworkIntegration.register();
    }
  }
  return ruleManager;
}

function clearRuleManager(extension) {
  let i = gRuleManagers.findIndex(rm => rm.extension === extension);
  if (i !== -1) {
    gRuleManagers.splice(i, 1);
    NetworkIntegration.maybeUpdateTabIdChecker();
    if (gRuleManagers.length === 0) {
      // The last DNR registration.
      NetworkIntegration.unregister();
    }
  }
}

/**
 * Finds all matching rules for a request, optionally restricted to one
 * extension.
 *
 * @param {object|RequestDetails} request
 * @param {Extension} [extension]
 * @returns {MatchedRule[]}
 */
function getMatchedRulesForRequest(request, extension) {
  let requestDetails = new RequestDetails(request);
  let ruleManagers = gRuleManagers;
  if (extension) {
    ruleManagers = ruleManagers.filter(rm => rm.extension === extension);
  }
  return RequestEvaluator.evaluateRequest(requestDetails, ruleManagers);
}

/**
 * Runs before any webRequest event is notified. Headers may be modified, but
 * the request should not be canceled (see handleRequest instead).
 *
 * @param {ChannelWrapper} channel
 * @param {string} kind - The name of the webRequest event.
 */
function beforeWebRequestEvent(channel, kind) {
  try {
    switch (kind) {
      case "onBeforeRequest":
        NetworkIntegration.startDNREvaluation(channel);
        break;
      case "onBeforeSendHeaders":
        NetworkIntegration.onBeforeSendHeaders(channel);
        break;
      case "onHeadersReceived":
        NetworkIntegration.onHeadersReceived(channel);
        break;
    }
  } catch (e) {
    Cu.reportError(e);
  }
}

/**
 * Applies matching DNR rules, some of which may potentially cancel the request.
 *
 * @param {ChannelWrapper} channel
 * @param {string} kind - The name of the webRequest event.
 * @returns {boolean} Whether to ignore any responses from the webRequest API.
 */
function handleRequest(channel, kind) {
  try {
    if (kind === "onBeforeRequest") {
      return NetworkIntegration.onBeforeRequest(channel);
    }
  } catch (e) {
    Cu.reportError(e);
  }
  return false;
}

async function initExtension(extension) {
  // These permissions are NOT an OptionalPermission, so their status can be
  // assumed to be constant for the lifetime of the extension.
  if (
    extension.hasPermission("declarativeNetRequest") ||
    extension.hasPermission("declarativeNetRequestWithHostAccess")
  ) {
    if (extension.hasShutdown) {
      throw new Error(
        `Aborted ExtensionDNR.initExtension call, extension "${extension.id}" is not active anymore`
      );
    }
    await lazy.ExtensionDNRStore.initExtension(extension);
  }
}

function ensureInitialized(extension) {
  return (extension._dnrReady ??= initExtension(extension));
}

function validateManifestEntry(extension) {
  const ruleResourcesArray =
    extension.manifest.declarative_net_request.rule_resources;

  const getWarningMessage = msg =>
    `Warning processing declarative_net_request: ${msg}`;

  if (ruleResourcesArray.length > MAX_NUMBER_OF_STATIC_RULESETS) {
    extension.manifestWarning(
      getWarningMessage(
        `Static rulesets are exceeding the MAX_NUMBER_OF_STATIC_RULESETS limit (${MAX_NUMBER_OF_STATIC_RULESETS}).`
      )
    );
  }

  const seenRulesetIds = new Set();
  const seenRulesetPaths = new Set();
  const duplicatedRulesetIds = [];
  const duplicatedRulesetPaths = [];
  for (const [idx, { id, path }] of ruleResourcesArray.entries()) {
    if (seenRulesetIds.has(id)) {
      duplicatedRulesetIds.push({ idx, id });
    }
    if (seenRulesetPaths.has(path)) {
      duplicatedRulesetPaths.push({ idx, path });
    }
    seenRulesetIds.add(id);
    seenRulesetPaths.add(path);
  }

  if (duplicatedRulesetIds.length) {
    const errorDetails = duplicatedRulesetIds
      .map(({ idx, id }) => `"${id}" at index ${idx}`)
      .join(", ");
    extension.manifestWarning(
      getWarningMessage(
        `Static ruleset ids should be unique, duplicated ruleset ids: ${errorDetails}.`
      )
    );
  }

  if (duplicatedRulesetPaths.length) {
    // NOTE: technically Chrome allows duplicated paths without any manifest
    // validation warnings or errors, but if this happens it not unlikely to be
    // actually a mistake in the manifest that may have been missed.
    //
    // In Firefox we decided to allow the same behavior to avoid introducing a chrome
    // incompatibility, but we still warn about it to avoid extension developers
    // to investigate more easily issue that may be due to duplicated rulesets
    // paths.
    const errorDetails = duplicatedRulesetPaths
      .map(({ idx, path }) => `"${path}" at index ${idx}`)
      .join(", ");
    extension.manifestWarning(
      getWarningMessage(
        `Static rulesets paths are not unique, duplicated ruleset paths: ${errorDetails}.`
      )
    );
  }

  const enabledRulesets = ruleResourcesArray.filter(rs => rs.enabled);
  if (enabledRulesets.length > MAX_NUMBER_OF_ENABLED_STATIC_RULESETS) {
    const exceedingRulesetIds = enabledRulesets
      .slice(MAX_NUMBER_OF_ENABLED_STATIC_RULESETS)
      .map(ruleset => `"${ruleset.id}"`)
      .join(", ");
    extension.manifestWarning(
      getWarningMessage(
        `Enabled static rulesets are exceeding the MAX_NUMBER_OF_ENABLED_STATIC_RULESETS limit (${MAX_NUMBER_OF_ENABLED_STATIC_RULESETS}): ${exceedingRulesetIds}.`
      )
    );
  }
}

// exports used by the DNR API implementation.
export const ExtensionDNR = {
  RuleValidator,
  clearRuleManager,
  ensureInitialized,
  getMatchedRulesForRequest,
  getRuleManager,
  validateManifestEntry,
  // TODO(Bug 1803370): consider allowing changing DNR limits through about:config prefs).
  limits: {
    GUARANTEED_MINIMUM_STATIC_RULES,
    MAX_NUMBER_OF_STATIC_RULESETS,
    MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
  },

  beforeWebRequestEvent,
  handleRequest,
};
