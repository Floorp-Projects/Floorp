/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  JsonSchema: "resource://gre/modules/JsonSchema.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DEFAULT_EXPIRY_RELATIVE",
  "cookiebanners.cookieInjector.defaultExpiryRelative"
);

const PREF_SKIP_REMOTE_SETTINGS =
  "cookiebanners.listService.testSkipRemoteSettings";
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "TEST_SKIP_REMOTE_SETTINGS",
  PREF_SKIP_REMOTE_SETTINGS
);

const PREF_TEST_RULES = "cookiebanners.listService.testRules";
XPCOMUtils.defineLazyPreferenceGetter(lazy, "testRulesPref", PREF_TEST_RULES);

// Name of the RemoteSettings collection containing the rules.
const COLLECTION_NAME = "cookie-banner-rules-list";

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "CookieBannerListService",
    maxLogLevelPref: "cookiebanners.listService.logLevel",
  });
});

// Lazy getter for the JSON schema of cookie banner rules. It is used for
// validation of rules defined by pref.
ChromeUtils.defineLazyGetter(lazy, "CookieBannerRuleSchema", async () => {
  let response = await fetch(
    "chrome://global/content/cookiebanners/CookieBannerRule.schema.json"
  );
  if (!response.ok) {
    lazy.logConsole.error("Fetch for CookieBannerRuleSchema failed", response);
    throw new Error("Failed to fetch CookieBannerRuleSchema.");
  }
  return response.json();
});

/**
 * See nsICookieBannerListService
 */
export class CookieBannerListService {
  classId = Components.ID("{1d8d9470-97d3-4885-a108-44a5c4fb36e2}");
  QueryInterface = ChromeUtils.generateQI(["nsICookieBannerListService"]);

  // RemoteSettings collection holding the cookie banner rules.
  #rs = null;
  // Stores the this-wrapped on-sync callback so it can be unregistered on
  // shutdown.
  #onSyncCallback = null;

  constructor() {
    this.#rs = lazy.RemoteSettings(COLLECTION_NAME);
  }

  async init() {
    lazy.logConsole.debug("init");

    await this.importAllRules();

    // Register listener to import rules when test pref changes.
    Services.prefs.addObserver(PREF_TEST_RULES, this);
    Services.prefs.addObserver(PREF_SKIP_REMOTE_SETTINGS, this);

    // Register callback for collection changes.
    // Only register if not already registered.
    if (!this.#onSyncCallback) {
      this.#onSyncCallback = this.onSync.bind(this);
      this.#rs.on("sync", this.#onSyncCallback);
    }
  }

  initForTest() {
    return this.init();
  }

  async importAllRules() {
    lazy.logConsole.debug("importAllRules");

    try {
      let rules = await this.#rs.get();

      // While getting rules from RemoteSettings the enabled state of the
      // feature could have changed. Ensure the service is still enabled before
      // attempting to import rules.
      if (!Services.cookieBanners.isEnabled) {
        lazy.logConsole.warn("Skip import nsICookieBannerService is disabled");
        return;
      }
      if (!lazy.TEST_SKIP_REMOTE_SETTINGS) {
        this.#importRules(rules);
      }
    } catch (error) {
      lazy.logConsole.error(
        "Error while importing cookie banner rules from RemoteSettings",
        error
      );
    }

    // We import test rules, even if fetching rules from RemoteSettings failed.
    await this.#importTestRules();
  }

  shutdown() {
    lazy.logConsole.debug("shutdown");

    // Unregister callback for collection changes.
    if (this.#onSyncCallback) {
      this.#rs.off("sync", this.#onSyncCallback);
      this.#onSyncCallback = null;
    }

    Services.prefs.removeObserver(PREF_TEST_RULES, this);
    Services.prefs.removeObserver(PREF_SKIP_REMOTE_SETTINGS, this);
  }

  /**
   * Called for remote settings "sync" events.
   */
  onSync({ data: { created, updated, deleted } }) {
    if (lazy.TEST_SKIP_REMOTE_SETTINGS) {
      return;
    }
    lazy.logConsole.debug("onSync", { created, updated, deleted });

    // Remove deleted rules.
    this.#removeRules(deleted);

    // Import new rules and override updated rules.
    this.#importRules(created.concat(updated.map(u => u.new)));

    // Re-import test rules in case they need to overwrite existing rules or a test rule was deleted above.
    this.#importTestRules();
  }

  observe(subject, topic, prefName) {
    if (prefName != PREF_TEST_RULES && prefName != PREF_SKIP_REMOTE_SETTINGS) {
      return;
    }

    // When the test rules update we need to clear all rules and import them
    // again. This is required because we don't have a mechanism for deleting
    // specific test rules.
    // Also reimport when rule import from RemoteSettings is enabled / disabled.
    // Passing `doImport = false` since we trigger the import ourselves.
    Services.cookieBanners.resetRules(false);
    this.importAllRules();

    // Reset executed records (private and normal browsing) for easier testing
    // of rules.
    Services.cookieBanners.removeAllExecutedRecords(false);
    Services.cookieBanners.removeAllExecutedRecords(true);
  }

  #removeRules(rules = []) {
    lazy.logConsole.debug("removeRules", rules);

    // For each js rule, construct a temporary nsICookieBannerRule to pass into
    // Services.cookieBanners.removeRule. For removal only domain and id are
    // relevant.
    rules
      .map(({ id, domain, domains }) => {
        // Provide backwards-compatibility with single-domain rules.
        if (domain) {
          domains = [domain];
        }

        let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
          Ci.nsICookieBannerRule
        );
        rule.id = id;
        rule.domains = domains;
        return rule;
      })
      .forEach(r => {
        Services.cookieBanners.removeRule(r);

        // Clear the fact if we have reported telemetry for the domain. So, we
        // can collect again with the updated rules.
        Services.cookieBanners.resetDomainTelemetryRecord(r.domain);
      });
  }

  #importRules(rules) {
    lazy.logConsole.debug("importRules", rules);

    rules.forEach(({ id, domain, domains, cookies, click }) => {
      // Provide backwards-compatibility with single-domain rules.
      if (domain) {
        domains = [domain];
      }

      let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
        Ci.nsICookieBannerRule
      );
      rule.id = id;
      rule.domains = domains;

      // Import the cookie rule.
      this.#importCookieRule(rule, cookies);

      // Import the click rule.
      this.#importClickRule(rule, click);

      Services.cookieBanners.insertRule(rule);

      // Clear the fact if we have reported telemetry for the domain. Note that
      // this function could handle rule update and the initial rule import. In
      // both cases, we should clear to make sure we will collect with the
      // latest rules.
      Services.cookieBanners.resetDomainTelemetryRecord(domain);
    });
  }

  async #importTestRules() {
    lazy.logConsole.debug("importTestRules");

    if (!Services.prefs.prefHasUserValue(PREF_TEST_RULES)) {
      lazy.logConsole.debug(
        "Skip importing test rules: Pref has default value."
      );
      return;
    }

    // Parse array of rules from pref value string as JSON.
    let testRules;
    try {
      testRules = JSON.parse(lazy.testRulesPref);
    } catch (error) {
      lazy.logConsole.error(
        `Failed to parse test rules JSON string. Make sure ${PREF_TEST_RULES} contains valid JSON. ${error?.name}`,
        error
      );
      return;
    }

    // Ensure we have an array we can iterate over and not an object.
    if (!Array.isArray(testRules)) {
      lazy.logConsole.error(
        "Failed to parse test rules JSON String: Not an array."
      );
      return;
    }

    // Validate individual array elements (rules) via the schema defined in
    // CookieBannerRule.schema.json.
    let schema = await lazy.CookieBannerRuleSchema;
    let validator = new lazy.JsonSchema.Validator(schema);
    let validatedTestRules = [];

    let i = 0;
    for (let rule of testRules) {
      let { valid, errors } = validator.validate(rule);

      if (!valid) {
        lazy.logConsole.error(
          `Skipping invalid test rule at index ${i}. Errors: ${JSON.stringify(
            errors,
            null,
            2
          )}`
        );
        lazy.logConsole.debug("Test rule validation error", rule, errors);

        i += 1;
        continue;
      }

      // Only import rules if they are valid.
      validatedTestRules.push(rule);
      i += 1;
    }

    this.#importRules(validatedTestRules);
  }

  #importCookieRule(rule, cookies) {
    // Skip rules that don't have cookies.
    if (!cookies) {
      return;
    }

    // Import opt-in and opt-out cookies if defined.
    for (let category of ["optOut", "optIn"]) {
      if (!cookies[category]) {
        continue;
      }

      let isOptOut = category == "optOut";

      for (let c of cookies[category]) {
        let { expiryRelative } = c;
        if (expiryRelative == null || expiryRelative <= 0) {
          expiryRelative = lazy.DEFAULT_EXPIRY_RELATIVE;
        }

        rule.addCookie(
          isOptOut,
          c.name,
          c.value,
          // The following fields are optional and may not be defined by the
          // rule.
          // If unset, host falls back to ".<domain>" internally.
          c.host,
          c.path || "/",
          expiryRelative,
          c.unsetValue,
          c.isSecure,
          c.isHTTPOnly,
          // Default injected cookies to session expiry.
          c.isSession ?? true,
          c.sameSite,
          c.schemeMap
        );
      }
    }
  }

  /**
   * Converts runContext string field to nsIClickRule::RunContext
   * @param {('top'|'child'|'all')} runContextStr - Run context as string.
   * @returns nsIClickRule::RunContext representation.
   */
  #runContextStrToNative(runContextStr) {
    let strToNative = {
      top: Ci.nsIClickRule.RUN_TOP,
      child: Ci.nsIClickRule.RUN_CHILD,
      all: Ci.nsIClickRule.RUN_ALL,
    };

    // Default to RUN_TOP;
    return strToNative[runContextStr] ?? Ci.nsIClickRule.RUN_TOP;
  }

  #importClickRule(rule, click) {
    // Skip importing the rule if there is no click object or the click rule is
    // empty - it doesn't have the mandatory presence attribute.
    if (!click || !click.presence) {
      return;
    }

    rule.addClickRule(
      click.presence,
      click.skipPresenceVisibilityCheck,
      this.#runContextStrToNative(click.runContext),
      click.hide,
      click.optOut,
      click.optIn
    );
  }
}
