/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

this.EXPORTED_SYMBOLS = ["allowAllCerts"];

const sss = Cc["@mozilla.org/ssservice;1"].getService(
  Ci.nsISiteSecurityService
);

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

const CERT_PINNING_ENFORCEMENT_PREF = "security.cert_pinning.enforcement_level";
const HSTS_PRELOAD_LIST_PREF = "network.stricttransportsecurity.preloadlist";

/** @namespace */
this.allowAllCerts = {};

/**
 * Disable all security check and allow all certs.
 */
allowAllCerts.enable = function() {
  // make it possible to register certificate overrides for domains
  // that use HSTS or HPKP
  Preferences.set(HSTS_PRELOAD_LIST_PREF, false);
  Preferences.set(CERT_PINNING_ENFORCEMENT_PREF, 0);

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );
};

/**
 * Enable all security check.
 */
allowAllCerts.disable = function() {
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );

  Preferences.reset(HSTS_PRELOAD_LIST_PREF);
  Preferences.reset(CERT_PINNING_ENFORCEMENT_PREF);

  // clear collected HSTS and HPKP state
  // through the site security service
  sss.clearAll();
  sss.clearPreloads();
};
