/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Ensures nsISiteSecurityService APIs respects origin attributes.

const GOOD_MAX_AGE_SECONDS = 69403;
const GOOD_MAX_AGE = `max-age=${GOOD_MAX_AGE_SECONDS};`;

do_get_profile(); // must be done before instantiating nsIX509CertDB

let sss = Cc["@mozilla.org/ssservice;1"].getService(Ci.nsISiteSecurityService);
let host = "a.pinning.example.com";
let uri = Services.io.newURI("https://" + host);

// Check if originAttributes1 and originAttributes2 are isolated with respect
// to HSTS storage.
function doTest(originAttributes1, originAttributes2, shouldShare) {
  sss.clearAll();
  let header = GOOD_MAX_AGE;
  // Set HSTS for originAttributes1.
  sss.processHeader(uri, header, originAttributes1);
  ok(
    sss.isSecureURI(uri, originAttributes1),
    "URI should be secure given original origin attributes"
  );
  equal(
    sss.isSecureURI(uri, originAttributes2),
    shouldShare,
    "URI should be secure given different origin attributes if and " +
      "only if shouldShare is true"
  );

  if (!shouldShare) {
    // Remove originAttributes2 from the storage.
    sss.resetState(uri, originAttributes2);
    ok(
      sss.isSecureURI(uri, originAttributes1),
      "URI should still be secure given original origin attributes"
    );
  }

  // Remove originAttributes1 from the storage.
  sss.resetState(uri, originAttributes1);
  ok(
    !sss.isSecureURI(uri, originAttributes1),
    "URI should not be secure after removeState"
  );

  sss.clearAll();
}

function testInvalidOriginAttributes(originAttributes) {
  let header = GOOD_MAX_AGE;

  let callbacks = [
    () => sss.processHeader(uri, header, originAttributes),
    () => sss.isSecureURI(uri, originAttributes),
    () => sss.resetState(uri, originAttributes),
  ];

  for (let callback of callbacks) {
    throws(
      callback,
      /NS_ERROR_ILLEGAL_VALUE/,
      "Should get an error with invalid origin attributes"
    );
  }
}

function run_test() {
  sss.clearAll();

  let originAttributesList = [];
  for (let userContextId of [0, 1, 2]) {
    for (let firstPartyDomain of ["", "foo.com", "bar.com"]) {
      originAttributesList.push({ userContextId, firstPartyDomain });
    }
  }
  for (let attrs1 of originAttributesList) {
    for (let attrs2 of originAttributesList) {
      // SSS storage is not isolated by userContext
      doTest(
        attrs1,
        attrs2,
        attrs1.firstPartyDomain == attrs2.firstPartyDomain
      );
    }
  }

  doTest(
    { partitionKey: "(http,example.com,8443)" },
    { partitionKey: "(https,example.com)" },
    true
  );

  testInvalidOriginAttributes(undefined);
  testInvalidOriginAttributes(null);
  testInvalidOriginAttributes(1);
  testInvalidOriginAttributes("foo");
}
