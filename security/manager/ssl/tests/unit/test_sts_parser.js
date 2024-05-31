/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// STS parser tests

let sss = Cc["@mozilla.org/ssservice;1"].getService(Ci.nsISiteSecurityService);

function test_valid_header(header, expectedMaxAge, expectedIncludeSubdomains) {
  let dummyUri = Services.io.newURI("https://foo.com/bar.html");
  let maxAge = {};
  let includeSubdomains = {};

  sss.processHeader(dummyUri, header, {}, maxAge, includeSubdomains);

  equal(maxAge.value, expectedMaxAge, "Correctly parsed maxAge");
  equal(
    includeSubdomains.value,
    expectedIncludeSubdomains,
    "Correctly parsed presence/absence of includeSubdomains"
  );
}

function test_invalid_header(header) {
  let dummyUri = Services.io.newURI("https://foo.com/bar.html");
  let maxAge = {};
  let includeSubdomains = {};

  throws(
    () => {
      sss.processHeader(dummyUri, header, {}, maxAge, includeSubdomains);
    },
    /NS_ERROR_FAILURE/,
    "Correctly rejected invalid header: " + header
  );
}

function run_test() {
  // SHOULD SUCCEED:
  test_valid_header("max-age=100", 100, false);
  test_valid_header("max-age  =100", 100, false);
  test_valid_header(" max-age=100", 100, false);
  test_valid_header("max-age = 100 ", 100, false);
  test_valid_header('max-age = "100" ', 100, false);
  test_valid_header('max-age="100"', 100, false);
  test_valid_header(' max-age ="100" ', 100, false);
  test_valid_header('\tmax-age\t=\t"100"\t', 100, false);
  test_valid_header("max-age  =       100             ", 100, false);

  test_valid_header("maX-aGe=100", 100, false);
  test_valid_header("MAX-age  =100", 100, false);
  test_valid_header("max-AGE=100", 100, false);
  test_valid_header("Max-Age = 100 ", 100, false);
  test_valid_header("MAX-AGE = 100 ", 100, false);

  test_valid_header("max-age=100;includeSubdomains", 100, true);
  test_valid_header("max-age=100\t; includeSubdomains", 100, true);
  test_valid_header(" max-age=100; includeSubdomains", 100, true);
  test_valid_header("max-age = 100 ; includeSubdomains", 100, true);
  test_valid_header(
    "max-age  =       100             ; includeSubdomains",
    100,
    true
  );

  test_valid_header("maX-aGe=100; includeSUBDOMAINS", 100, true);
  test_valid_header("MAX-age  =100; includeSubDomains", 100, true);
  test_valid_header("max-AGE=100; iNcLuDeSuBdoMaInS", 100, true);
  test_valid_header("Max-Age = 100; includesubdomains ", 100, true);
  test_valid_header("INCLUDESUBDOMAINS;MaX-AgE = 100 ", 100, true);
  // Turns out, the actual directive is entirely optional (hence the
  // trailing semicolon)
  test_valid_header("max-age=100;includeSubdomains;", 100, true);

  // these are weird tests, but are testing that some extended syntax is
  // still allowed (but it is ignored)
  test_valid_header("max-age=100 ; includesubdomainsSomeStuff", 100, false);
  test_valid_header(
    "\r\n\t\t    \tcompletelyUnrelated = foobar; max-age= 34520103" +
      "\t \t; alsoUnrelated;asIsThis;\tincludeSubdomains\t\t \t",
    34520103,
    true
  );
  test_valid_header('max-age=100; unrelated="quoted \\"thingy\\""', 100, false);

  // Test a max-age greater than 100 years. It will be capped at 100 years.
  test_valid_header("max-age=4294967296", 60 * 60 * 24 * 365 * 100, false);

  // SHOULD FAIL:
  // invalid max-ages
  test_invalid_header("max-age");
  test_invalid_header("max-age ");
  test_invalid_header("max-age=");
  test_invalid_header("max-age=p");
  test_invalid_header("max-age=*1p2");
  test_invalid_header("max-age=.20032");
  test_invalid_header("max-age=!20032");
  test_invalid_header("max-age==20032");

  // invalid headers
  test_invalid_header("foobar");
  test_invalid_header("maxage=100");
  test_invalid_header("maxa-ge=100");
  test_invalid_header("max-ag=100");
  test_invalid_header("includesubdomains");
  test_invalid_header("includesubdomains=");
  test_invalid_header("max-age=100; includesubdomains=");
  test_invalid_header(";");
  test_invalid_header('max-age="100');
  // The max-age directive here doesn't conform to the spec, so it MUST
  // be ignored. Consequently, the REQUIRED max-age directive is not
  // present in this header, and so it is invalid.
  test_invalid_header("max-age=100, max-age=200; includeSubdomains");
  test_invalid_header("max-age=100 includesubdomains");
  test_invalid_header("max-age=100 bar foo");
  test_invalid_header("max-age=100randomstuffhere");
  // All directives MUST appear only once in an STS header field.
  test_invalid_header("max-age=100; max-age=200");
  test_invalid_header("includeSubdomains; max-age=200; includeSubdomains");
  test_invalid_header("max-age=200; includeSubdomains; includeSubdomains");
  // The includeSubdomains directive is valueless.
  test_invalid_header("max-age=100; includeSubdomains=unexpected");
  // LWS must have at least one space or horizontal tab
  test_invalid_header("\r\nmax-age=200");
}
