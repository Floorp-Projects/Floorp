/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Security"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.jsm",

  Domain: "chrome://remote/content/cdp/domains/Domain.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  sss: ["@mozilla.org/ssservice;1", "nsISiteSecurityService"],
  certOverrideService: [
    "@mozilla.org/security/certoverride;1",
    "nsICertOverrideService",
  ],
});

const CERT_PINNING_ENFORCEMENT_PREF = "security.cert_pinning.enforcement_level";
const HSTS_PRELOAD_LIST_PREF = "network.stricttransportsecurity.preloadlist";

class Security extends Domain {
  destructor() {
    this.setIgnoreCertificateErrors({ ignore: false });
  }

  /**
   * Enable/disable whether all certificate errors should be ignored
   *
   * @param {Object} options
   * @param {boolean=} options.ignore
   *    if true, all certificate errors will be ignored.
   */
  setIgnoreCertificateErrors(options = {}) {
    const { ignore } = options;

    if (ignore) {
      // make it possible to register certificate overrides for domains
      // that use HSTS or HPKP
      Preferences.set(HSTS_PRELOAD_LIST_PREF, false);
      Preferences.set(CERT_PINNING_ENFORCEMENT_PREF, 0);
    } else {
      Preferences.reset(HSTS_PRELOAD_LIST_PREF);
      Preferences.reset(CERT_PINNING_ENFORCEMENT_PREF);

      // clear collected HSTS and HPKP state
      sss.clearAll();
      sss.clearPreloads();
    }

    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      ignore
    );
  }
}
