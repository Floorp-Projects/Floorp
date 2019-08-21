/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Ensures that HSTS (HTTP Strict Transport Security) and HPKP (HTTP Public key
// pinning) are cleared when using "Forget About This Site".

const { ForgetAboutSite } = ChromeUtils.import(
  "resource://gre/modules/ForgetAboutSite.jsm"
);

do_get_profile(); // must be done before instantiating nsIX509CertDB

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.cert_pinning.enforcement_level");
  Services.prefs.clearUserPref(
    "security.cert_pinning.process_headers_from_non_builtin_roots"
  );
});

const GOOD_MAX_AGE_SECONDS = 69403;
const NON_ISSUED_KEY_HASH = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
const PINNING_ROOT_KEY_HASH = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";
const VALID_PIN = `pin-sha256="${PINNING_ROOT_KEY_HASH}";`;
const BACKUP_PIN = `pin-sha256="${NON_ISSUED_KEY_HASH}";`;
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

  // Test the normal case of processing HSTS and HPKP headers for
  // a.pinning.example.com, using "Forget About Site" on a.pinning2.example.com,
  // and then checking that the platform doesn't consider a.pinning.example.com
  // to be HSTS or HPKP any longer.
  add_task(async function() {
    sss.processHeader(
      Ci.nsISiteSecurityService.HEADER_HSTS,
      uri,
      GOOD_MAX_AGE,
      secInfo,
      0,
      Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
    );
    sss.processHeader(
      Ci.nsISiteSecurityService.HEADER_HPKP,
      uri,
      GOOD_MAX_AGE + VALID_PIN + BACKUP_PIN,
      secInfo,
      0,
      Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
    );

    Assert.ok(
      sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0),
      "a.pinning.example.com should be HSTS"
    );
    Assert.ok(
      sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HPKP, uri, 0),
      "a.pinning.example.com should be HPKP"
    );

    await ForgetAboutSite.removeDataFromDomain("a.pinning.example.com");

    Assert.ok(
      !sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0),
      "a.pinning.example.com should not be HSTS now"
    );
    Assert.ok(
      !sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HPKP, uri, 0),
      "a.pinning.example.com should not be HPKP now"
    );
  });

  // Test the case of processing HSTS and HPKP headers for a.pinning.example.com,
  // using "Forget About Site" on example.com, and then checking that the platform
  // doesn't consider the subdomain to be HSTS or HPKP any longer. Also test that
  // unrelated sites don't also get removed.
  add_task(async function() {
    sss.processHeader(
      Ci.nsISiteSecurityService.HEADER_HSTS,
      uri,
      GOOD_MAX_AGE,
      secInfo,
      0,
      Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
    );
    sss.processHeader(
      Ci.nsISiteSecurityService.HEADER_HPKP,
      uri,
      GOOD_MAX_AGE + VALID_PIN + BACKUP_PIN,
      secInfo,
      0,
      Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
    );

    Assert.ok(
      sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0),
      "a.pinning.example.com should be HSTS (subdomain case)"
    );
    Assert.ok(
      sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HPKP, uri, 0),
      "a.pinning.example.com should be HPKP (subdomain case)"
    );

    // Add an unrelated site to HSTS.  Not HPKP because we have no valid keys for
    // example.org.
    let unrelatedURI = Services.io.newURI("https://example.org");
    sss.processHeader(
      Ci.nsISiteSecurityService.HEADER_HSTS,
      unrelatedURI,
      GOOD_MAX_AGE,
      secInfo,
      0,
      Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
    );
    Assert.ok(
      sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, unrelatedURI, 0),
      "example.org should be HSTS"
    );

    await ForgetAboutSite.removeDataFromDomain("example.com");

    Assert.ok(
      !sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0),
      "a.pinning.example.com should not be HSTS now (subdomain case)"
    );
    Assert.ok(
      !sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HPKP, uri, 0),
      "a.pinning.example.com should not be HPKP now (subdomain case)"
    );

    Assert.ok(
      sss.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, unrelatedURI, 0),
      "example.org should still be HSTS"
    );
  });

  // Test the case of processing HSTS and HPKP headers for a.pinning.example.com
  // with various originAttributes, using "Forget About Site" on example.com, and
  // then checking that the platform doesn't consider the subdomain to be HSTS or
  // HPKP for any originAttributes any longer. Also test that unrelated sites
  // don't also get removed.
  add_task(async function() {
    let originAttributesList = [
      {},
      { userContextId: 1 },
      { firstPartyDomain: "foo.com" },
      { userContextId: 1, firstPartyDomain: "foo.com" },
    ];

    let unrelatedURI = Services.io.newURI("https://example.org");

    for (let originAttributes of originAttributesList) {
      sss.processHeader(
        Ci.nsISiteSecurityService.HEADER_HSTS,
        uri,
        GOOD_MAX_AGE,
        secInfo,
        0,
        Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST,
        originAttributes
      );
      sss.processHeader(
        Ci.nsISiteSecurityService.HEADER_HPKP,
        uri,
        GOOD_MAX_AGE + VALID_PIN + BACKUP_PIN,
        secInfo,
        0,
        Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST,
        originAttributes
      );

      Assert.ok(
        sss.isSecureURI(
          Ci.nsISiteSecurityService.HEADER_HSTS,
          uri,
          0,
          originAttributes
        ),
        "a.pinning.example.com should be HSTS (originAttributes case)"
      );
      Assert.ok(
        sss.isSecureURI(
          Ci.nsISiteSecurityService.HEADER_HPKP,
          uri,
          0,
          originAttributes
        ),
        "a.pinning.example.com should be HPKP (originAttributes case)"
      );

      // Add an unrelated site to HSTS.  Not HPKP because we have no valid keys.
      sss.processHeader(
        Ci.nsISiteSecurityService.HEADER_HSTS,
        unrelatedURI,
        GOOD_MAX_AGE,
        secInfo,
        0,
        Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST,
        originAttributes
      );
      Assert.ok(
        sss.isSecureURI(
          Ci.nsISiteSecurityService.HEADER_HSTS,
          unrelatedURI,
          0,
          originAttributes
        ),
        "example.org should be HSTS (originAttributes case)"
      );
    }

    await ForgetAboutSite.removeDataFromDomain("example.com");

    for (let originAttributes of originAttributesList) {
      Assert.ok(
        !sss.isSecureURI(
          Ci.nsISiteSecurityService.HEADER_HSTS,
          uri,
          0,
          originAttributes
        ),
        "a.pinning.example.com should not be HSTS now " +
          "(originAttributes case)"
      );
      Assert.ok(
        !sss.isSecureURI(
          Ci.nsISiteSecurityService.HEADER_HPKP,
          uri,
          0,
          originAttributes
        ),
        "a.pinning.example.com should not be HPKP now " +
          "(originAttributes case)"
      );

      Assert.ok(
        sss.isSecureURI(
          Ci.nsISiteSecurityService.HEADER_HSTS,
          unrelatedURI,
          0,
          originAttributes
        ),
        "example.org should still be HSTS (originAttributes case)"
      );
    }
  });
}

function run_test() {
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);
  Services.prefs.setBoolPref(
    "security.cert_pinning.process_headers_from_non_builtin_roots",
    true
  );

  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");

  add_tests();

  run_next_test();
}
