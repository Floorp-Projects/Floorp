/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

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
export const allowAllCerts = {};

/**
 * Disable all security check and allow all certs.
 */
allowAllCerts.enable = function () {
  // make it possible to register certificate overrides for domains
  // that use HSTS or HPKP
  Services.prefs.setBoolPref(HSTS_PRELOAD_LIST_PREF, false);
  Services.prefs.setIntPref(CERT_PINNING_ENFORCEMENT_PREF, 0);

  lazy.certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );
};

/**
 * Enable all security check.
 */
allowAllCerts.disable = function () {
  lazy.certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );

  Services.prefs.clearUserPref(HSTS_PRELOAD_LIST_PREF);
  Services.prefs.clearUserPref(CERT_PINNING_ENFORCEMENT_PREF);

  // clear collected HSTS and HPKP state
  // through the site security service
  lazy.sss.clearAll();
};
