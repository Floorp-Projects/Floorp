/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import { Domain } from "chrome://remote/content/cdp/domains/Domain.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  sss: ["@mozilla.org/ssservice;1", "nsISiteSecurityService"],
  certOverrideService: [
    "@mozilla.org/security/certoverride;1",
    "nsICertOverrideService",
  ],
});

const CERT_PINNING_ENFORCEMENT_PREF = "security.cert_pinning.enforcement_level";
const HSTS_PRELOAD_LIST_PREF = "network.stricttransportsecurity.preloadlist";

export class Security extends Domain {
  destructor() {
    this.setIgnoreCertificateErrors({ ignore: false });
  }

  /**
   * Enable/disable whether all certificate errors should be ignored
   *
   * @param {object} options
   * @param {boolean=} options.ignore
   *    if true, all certificate errors will be ignored.
   */
  setIgnoreCertificateErrors(options = {}) {
    const { ignore } = options;

    if (ignore) {
      // make it possible to register certificate overrides for domains
      // that use HSTS or HPKP
      lazy.Preferences.set(HSTS_PRELOAD_LIST_PREF, false);
      lazy.Preferences.set(CERT_PINNING_ENFORCEMENT_PREF, 0);
    } else {
      lazy.Preferences.reset(HSTS_PRELOAD_LIST_PREF);
      lazy.Preferences.reset(CERT_PINNING_ENFORCEMENT_PREF);

      // clear collected HSTS and HPKP state
      lazy.sss.clearAll();
    }

    lazy.certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      ignore
    );
  }
}
