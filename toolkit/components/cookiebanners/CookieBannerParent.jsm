/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CookieBannerParent"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serviceMode",
  "cookiebanners.service.mode",
  Ci.nsICookieBannerService.MODE_DISABLED
);

class CookieBannerParent extends JSWindowActorParent {
  async receiveMessage(message) {
    if (message.name == "CookieBanner::Test-FinishClicking") {
      Services.obs.notifyObservers(
        null,
        "cookie-banner-test-clicking-finish",
        this.manager.documentPrincipal?.baseDomain
      );
      return undefined;
    }

    if (message.name != "CookieBanner::GetClickRules") {
      return undefined;
    }

    if (lazy.serviceMode == Ci.nsICookieBannerService.MODE_DISABLED) {
      return [];
    }
    let domain = this.manager.documentPrincipal?.baseDomain;

    if (!domain) {
      return [];
    }

    let rules = Services.cookieBanners.getClickRulesForDomain(domain);

    if (!rules.length) {
      return [];
    }

    return rules.map(rule => {
      let target = rule.optOut;

      if (
        lazy.serviceMode == Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT &&
        !target
      ) {
        target = rule.optIn;
      }
      return {
        hide: rule.hide ?? rule.presence,
        presence: rule.presence,
        target,
      };
    });
  }
}
