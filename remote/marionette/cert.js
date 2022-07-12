/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["allowAllCerts"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Preferences: "resource://gre/modules/Preferences.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "sss",
  "@mozilla.org/ssservice;1",
  "nsISiteSecurityService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "certOverrideService",
  "@mozilla.org/security/certoverride;1",
  "nsICertOverrideService"
);

const CERT_PINNING_ENFORCEMENT_PREF = "security.cert_pinning.enforcement_level";
const HSTS_PRELOAD_LIST_PREF = "network.stricttransportsecurity.preloadlist";

/** @namespace */
const allowAllCerts = {};

/**
 * Disable all security check and allow all certs.
 */
allowAllCerts.enable = function() {
  // make it possible to register certificate overrides for domains
  // that use HSTS or HPKP
  lazy.Preferences.set(HSTS_PRELOAD_LIST_PREF, false);
  lazy.Preferences.set(CERT_PINNING_ENFORCEMENT_PREF, 0);

  lazy.certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );
};

/**
 * Enable all security check.
 */
allowAllCerts.disable = function() {
  lazy.certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );

  lazy.Preferences.reset(HSTS_PRELOAD_LIST_PREF);
  lazy.Preferences.reset(CERT_PINNING_ENFORCEMENT_PREF);

  // clear collected HSTS and HPKP state
  // through the site security service
  lazy.sss.clearAll();
};
