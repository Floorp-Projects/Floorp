/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Ensures that HSTS (HTTP Strict Transport Security) information is cleared
// when using "Forget About This Site".

const { ForgetAboutSite } = ChromeUtils.import(
  "resource://gre/modules/ForgetAboutSite.jsm"
);

do_get_profile(); // must be done before instantiating nsIX509CertDB

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.cert_pinning.enforcement_level");
});

const GOOD_MAX_AGE_SECONDS = 69403;
const GOOD_MAX_AGE = `max-age=${GOOD_MAX_AGE_SECONDS};`;

const sss = Cc["@mozilla.org/ssservice;1"].getService(
  Ci.nsISiteSecurityService
);
const uri = Services.io.newURI("https://a.pinning.example.com");

function add_tests() {
  let secInfo = null;

  add_connection_test(
    "a.pinning.example.com",
    PRErrorCodeSuccess,
    undefined,
    aSecInfo => {
      secInfo = aSecInfo;
    }
  );

  // Test the normal case of processing HSTS headers for a.pinning.example.com,
  // using "Forget About Site" on a.pinning2.example.com, and then checking
  // that the platform doesn't consider a.pinning.example.com to be HSTS any
  // longer.
  add_task(async function() {
    sss.processHeader(uri, GOOD_MAX_AGE, secInfo);

    Assert.ok(sss.isSecureURI(uri), "a.pinning.example.com should be HSTS");

    await ForgetAboutSite.removeDataFromDomain("a.pinning.example.com");

    Assert.ok(
      !sss.isSecureURI(uri),
      "a.pinning.example.com should not be HSTS now"
    );
  });

  // Test the case of processing HSTS headers for a.pinning.example.com, using
  // "Forget About Site" on example.com, and then checking that the platform
  // doesn't consider the subdomain to be HSTS any longer. Also test that
  // unrelated sites don't also get removed.
  add_task(async function() {
    sss.processHeader(uri, GOOD_MAX_AGE, secInfo);

    Assert.ok(
      sss.isSecureURI(uri),
      "a.pinning.example.com should be HSTS (subdomain case)"
    );

    // Add an unrelated site to HSTS.
    let unrelatedURI = Services.io.newURI("https://example.org");
    sss.processHeader(unrelatedURI, GOOD_MAX_AGE, secInfo);
    Assert.ok(sss.isSecureURI(unrelatedURI), "example.org should be HSTS");

    await ForgetAboutSite.removeDataFromDomain("example.com");

    Assert.ok(
      !sss.isSecureURI(uri),
      "a.pinning.example.com should not be HSTS now (subdomain case)"
    );

    Assert.ok(
      sss.isSecureURI(unrelatedURI),
      "example.org should still be HSTS"
    );
  });

  // Test the case of processing HSTS headers for a.pinning.example.com with
  // various originAttributes, using "Forget About Site" on example.com, and
  // then checking that the platform doesn't consider the subdomain to be HSTS
  // for any originAttributes any longer. Also test that unrelated sites don't
  // also get removed.
  add_task(async function() {
    let originAttributesList = [
      {},
      { userContextId: 1 },
      { firstPartyDomain: "foo.com" },
      { userContextId: 1, firstPartyDomain: "foo.com" },
    ];

    let unrelatedURI = Services.io.newURI("https://example.org");

    for (let originAttributes of originAttributesList) {
      sss.processHeader(uri, GOOD_MAX_AGE, secInfo, originAttributes);

      Assert.ok(
        sss.isSecureURI(uri, originAttributes),
        "a.pinning.example.com should be HSTS (originAttributes case)"
      );

      // Add an unrelated site to HSTS.
      sss.processHeader(unrelatedURI, GOOD_MAX_AGE, secInfo, originAttributes);
      Assert.ok(
        sss.isSecureURI(unrelatedURI, originAttributes),
        "example.org should be HSTS (originAttributes case)"
      );
    }

    await ForgetAboutSite.removeDataFromDomain("example.com");

    for (let originAttributes of originAttributesList) {
      Assert.ok(
        !sss.isSecureURI(uri, originAttributes),
        "a.pinning.example.com should not be HSTS now " +
          "(originAttributes case)"
      );

      Assert.ok(
        sss.isSecureURI(unrelatedURI, originAttributes),
        "example.org should still be HSTS (originAttributes case)"
      );
    }
  });
}

function run_test() {
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);
  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");
  add_tests();
  run_next_test();
}
