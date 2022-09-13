/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

XPCOMUtils.defineLazyModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.js",
  JsonSchema: "resource://gre/modules/JsonSchema.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DEFAULT_EXPIRY_RELATIVE",
  "cookiebanners.cookieInjector.defaultExpiryRelative"
);

const PREF_TEST_RULES = "cookiebanners.listService.testRules";
XPCOMUtils.defineLazyPreferenceGetter(lazy, "testRulesPref", PREF_TEST_RULES);
// Name of the RemoteSettings collection containing the rules.
const COLLECTION_NAME = "cookie-banner-rules-list";

let logConsole;
function log(...args) {
  if (!logConsole) {
    logConsole = console.createInstance({
      prefix: "** CookieBannerListService.jsm",
      maxLogLevelPref: "cookiebanners.listService.logLevel",
    });
  }

  logConsole.log(...args);
}

// Lazy getter for the JSON schema of cookie banner rules. It is used for
// validation of rules defined by pref.
XPCOMUtils.defineLazyGetter(lazy, "CookieBannerRuleSchema", async () => {
  let response = await fetch(
    "chrome://global/content/cookiebanners/CookieBannerRule.schema.json"
  );
  if (!response.ok) {
    log("Fetch for CookieBannerRuleSchema failed", response);
    throw new Error("Failed to fetch CookieBannerRuleSchema.");
  }
  return response.json();
});

/**
 * See nsICookieBannerListService
 */
class CookieBannerListService {
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
    log("init");

    await this.importAllRules();

    // Register listener to import rules when test pref changes.
    Services.prefs.addObserver(PREF_TEST_RULES, this);

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
    log("importAllRules");

    let rules = await this.#rs.get();
    this.#importRules(rules);
    await this.#importTestRules();
  }

  shutdown() {
    log("shutdown");

    // Unregister callback for collection changes.
    if (this.#onSyncCallback) {
      this.#rs.off("sync", this.#onSyncCallback);
      this.#onSyncCallback = null;
    }

    Services.prefs.removeObserver(PREF_TEST_RULES, this);
  }

  /**
   * Called for remote settings "sync" events.
   */
  onSync({ data: { created, updated, deleted } }) {
    log("onSync", { created, updated, deleted });

    // Remove deleted rules.
    this.#removeRules(deleted);

    // Import new rules and override updated rules.
    this.#importRules(created.concat(updated.map(u => u.new)));

    // Re-import test rules in case they need to overwrite existing rules or a test rule was deleted above.
    this.#importTestRules();
  }

  observe(subject, topic, prefName) {
    if (prefName != PREF_TEST_RULES) {
      return;
    }

    // When the test rules update we need to clear all rules and import them
    // again. This is required because we don't have a mechanism for deleting specific
    // test rules.
    // Passing `doImport = false` since we trigger the import ourselves.
    Services.cookieBanners.resetRules(false);
    this.importAllRules();
  }

  #removeRules(rules = []) {
    log("removeRules", rules);

    // For each js rule, construct a temporary nsICookieBannerRule to pass into
    // Services.cookieBanners.removeRule. For removal only domain and id are
    // relevant.
    rules
      .map(({ id, domain }) => {
        let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
          Ci.nsICookieBannerRule
        );
        rule.id = id;
        rule.domain = domain;
        return rule;
      })
      .forEach(r => Services.cookieBanners.removeRule(r));
  }

  #importRules(rules) {
    log("importRules", rules);

    rules.forEach(({ id, domain, cookies, click }) => {
      let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
        Ci.nsICookieBannerRule
      );
      rule.id = id;
      rule.domain = domain;

      // Import the cookie rule.
      this.#importCookieRule(rule, cookies);

      // Import the click rule.
      this.#importClickRule(rule, click);

      Services.cookieBanners.insertRule(rule);
    });
  }

  async #importTestRules() {
    log("importTestRules");

    if (!Services.prefs.prefHasUserValue(PREF_TEST_RULES)) {
      log("Skip importing test rules: Pref has default value.");
      return;
    }

    // Parse array of rules from pref value string as JSON.
    let testRules;
    try {
      testRules = JSON.parse(lazy.testRulesPref);
    } catch (error) {
      log("Failed to parse test rules JSON string.", error);
      Cu.reportError(
        `Failed to parse test rules JSON string. Make sure ${PREF_TEST_RULES} contains valid JSON. ${error?.name}`
      );
      return;
    }

    // Ensure we have an array we can iterate over and not an object.
    if (!Array.isArray(testRules)) {
      Cu.reportError("Failed to parse test rules JSON String: Not an array.");
      return;
    }

    // Validate individual array elements (rules) via the schema defined in
    // CookieBannerRule.schema.json.
    // Allow extra properties. They will be discarded.
    let schema = await lazy.CookieBannerRuleSchema;
    let validator = new lazy.JsonSchema.Validator(schema);
    let validatedTestRules = [];

    let i = 0;
    for (let rule of testRules) {
      let { valid, errors } = validator.validate(rule);

      if (!valid) {
        Cu.reportError(
          `Skipping invalid test rule at index ${i}. Errors: ${JSON.stringify(
            errors,
            null,
            2
          )}`
        );
        log("Test rule validation error", rule, errors);

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
          c.host || `.${rule.domain}`,
          c.path || "/",
          expiryRelative,
          c.unsetValue,
          c.isSecure,
          c.isHTTPOnly,
          c.isSession ?? true,
          c.sameSite,
          c.schemeMap
        );
      }
    }
  }

  #importClickRule(rule, click) {
    if (!click) {
      return;
    }

    rule.addClickRule(click.presence, click.hide, click.optOut, click.optIn);
  }
}

var EXPORTED_SYMBOLS = ["CookieBannerListService"];
