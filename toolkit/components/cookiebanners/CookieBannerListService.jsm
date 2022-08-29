/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "RemoteSettings",
  "resource://services-settings/remote-settings.js"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DEFAULT_EXPIRY_RELATIVE",
  "cookiebanners.cookieInjector.defaultExpiryRelative"
);

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

  init() {
    log("init");

    // Register callback for collection changes.
    // Only register if not already registered.
    if (!this.#onSyncCallback) {
      this.#onSyncCallback = this.onSync.bind(this);
      this.#rs.on("sync", this.#onSyncCallback);
    }

    return this.importAllRules();
  }

  async importAllRules() {
    log("importAllRules");

    let rules = await this.#rs.get();
    this.#importRules(rules);
  }

  shutdown() {
    log("shutdown");

    // Unregister callback for collection changes.
    if (this.#onSyncCallback) {
      this.#rs.off("sync", this.#onSyncCallback);
      this.#onSyncCallback = null;
    }
  }

  /**
   * Called for remote settings "sync" events.
   */
  onSync({ data: { created, updated, deleted } }) {
    log("onSync", { created, updated, deleted });

    // Remove deleted rules.
    this.removeRules(deleted);

    // Import new rules and override updated rules.
    this.#importRules(created.concat(updated.map(u => u.new)));
  }

  removeRules(rules = []) {
    log("removeRules", rules);

    rules
      .map(rule => rule.domain)
      .forEach(Services.cookieBanners.removeRuleForDomain);
  }

  #importRules(rules) {
    log("importRules", rules);

    rules.forEach(({ domain, cookies, click }) => {
      let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
        Ci.nsICookieBannerRule
      );
      rule.domain = domain;

      // Import the cookie rule.
      this.#importCookieRule(rule, cookies);

      // Import the click rule.
      this.#importClickRule(rule, click);

      Services.cookieBanners.insertRule(rule);
    });
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
