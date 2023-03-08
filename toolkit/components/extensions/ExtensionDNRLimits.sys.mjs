/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TODO(Bug 1803370): consider allowing changing DNR limits through about:config prefs).

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

/**
 * The maximum number of dynamic and session rules an extension can add.
 * NOTE: in the Firefox we are enforcing this limit to the session and dynamic rules count separately,
 * instead of enforcing it to the rules count for both combined as the Chrome implementation does.
 *
 * NOTE: this limit may be increased in the future, see https://github.com/w3c/webextensions/issues/319
 */
const MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES = 5000;

/**
 * The maximum number of regular expression rules that an extension can add.
 * Session, dynamic and static rules have their own quota.
 *
 * TODO bug 1821033: Bump limit after optimizing regexFilter.
 */
const MAX_NUMBER_OF_REGEX_RULES = 1000;

// TODO(Bug 1803370): allow extension to exceed the GUARANTEED_MINIMUM_STATIC_RULES limit.
//
// The maximum number of static rules exceeding the per-extension
// GUARANTEED_MINIMUM_STATIC_RULES across every extensions.
//
// const MAX_GLOBAL_NUMBER_OF_STATIC_RULES = 300000;

export const ExtensionDNRLimits = {
  GUARANTEED_MINIMUM_STATIC_RULES,
  MAX_NUMBER_OF_STATIC_RULESETS,
  MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
  MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES,
  MAX_NUMBER_OF_REGEX_RULES,
};
