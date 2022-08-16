/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Some test rules for cookie injection.
const RULES_TESTING = [
  // {
  //   domain: "example.com",
  //   cookies: {
  //     optOut: [
  //       {
  //         name: "consentCookieTest",
  //         value: "rejectAll",
  //         unsetValue: "UNSET",
  //       },
  //       {
  //         name: "consentCookieTestSecondary",
  //         value: "true",
  //       },
  //     ],
  //     optIn: [
  //       {
  //         name: "consentCookieTest",
  //         value: "acceptAll",
  //         expiryRelative: 3600,
  //         unsetValue: "UNSET",
  //       },
  //     ],
  //   },
  // },
  // {
  //   domain: "example.org",
  //   cookies: {
  //     optIn: [
  //       {
  //         host: "example.org",
  //         isSecure: false,
  //         name: "consentCookieTest",
  //         path: "/foo",
  //         value: "acceptAll",
  //         expiryRelative: 3600,
  //         unsetValue: "UNSET",
  //       },
  //     ],
  //   },
  // },
];

/**
 * See nsICookieBannerListService
 */
class CookieBannerListService {
  classId = Components.ID("{1d8d9470-97d3-4885-a108-44a5c4fb36e2}");
  QueryInterface = ChromeUtils.generateQI(["nsICookieBannerListService"]);

  /**
   * Iterate over RULES_TESTING and insert rules via nsICookieBannerService.
   */
  importRules() {
    RULES_TESTING.forEach(({ domain, cookies }) => {
      let rule = Services.cookieBanners.lookupOrInsertRuleForDomain(domain);

      // Clear any previously stored cookie rules.
      rule.clearCookies();

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
          rule.addCookie(
            isOptOut,
            c.host,
            c.name,
            c.value,
            // The following fields are optional and may not be defined by the
            // rule. They will fall back to defaults.
            c.path,
            c.expiryRelative,
            c.unsetValue,
            c.isSecure,
            c.isHTTPOnly,
            c.isSession,
            c.sameSite,
            c.schemeMap
          );
        }
      }
    });
  }
}

var EXPORTED_SYMBOLS = ["CookieBannerListService"];
