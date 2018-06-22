/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// STS parser tests

let sss = Cc["@mozilla.org/ssservice;1"].getService(Ci.nsISiteSecurityService);
let sslStatus = new FakeSSLStatus();

function testSuccess(header, expectedMaxAge, expectedIncludeSubdomains) {
  let dummyUri = Services.io.newURI("https://foo.com/bar.html");
  let maxAge = {};
  let includeSubdomains = {};

  sss.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS, dummyUri, header,
                    sslStatus, 0, sss.SOURCE_ORGANIC_REQUEST, {}, maxAge,
                    includeSubdomains);

  equal(maxAge.value, expectedMaxAge, "Did not correctly parse maxAge");
  equal(includeSubdomains.value, expectedIncludeSubdomains,
        "Did not correctly parse presence/absence of includeSubdomains");
}

function testFailure(header) {
  let dummyUri = Services.io.newURI("https://foo.com/bar.html");
  let maxAge = {};
  let includeSubdomains = {};

  throws(() => {
    sss.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS, dummyUri, header,
                      sslStatus, 0, sss.SOURCE_ORGANIC_REQUEST, {}, maxAge,
                      includeSubdomains);
  }, /NS_ERROR_FAILURE/, "Parsed invalid header: " + header);
}

function run_test() {
    // SHOULD SUCCEED:
    testSuccess("max-age=100", 100, false);
    testSuccess("max-age  =100", 100, false);
    testSuccess(" max-age=100", 100, false);
    testSuccess("max-age = 100 ", 100, false);
    testSuccess('max-age = "100" ', 100, false);
    testSuccess('max-age="100"', 100, false);
    testSuccess(' max-age ="100" ', 100, false);
    testSuccess("\tmax-age\t=\t\"100\"\t", 100, false);
    testSuccess("max-age  =       100             ", 100, false);

    testSuccess("maX-aGe=100", 100, false);
    testSuccess("MAX-age  =100", 100, false);
    testSuccess("max-AGE=100", 100, false);
    testSuccess("Max-Age = 100 ", 100, false);
    testSuccess("MAX-AGE = 100 ", 100, false);

    testSuccess("max-age=100;includeSubdomains", 100, true);
    testSuccess("max-age=100\t; includeSubdomains", 100, true);
    testSuccess(" max-age=100; includeSubdomains", 100, true);
    testSuccess("max-age = 100 ; includeSubdomains", 100, true);
    testSuccess("max-age  =       100             ; includeSubdomains", 100,
                true);

    testSuccess("maX-aGe=100; includeSUBDOMAINS", 100, true);
    testSuccess("MAX-age  =100; includeSubDomains", 100, true);
    testSuccess("max-AGE=100; iNcLuDeSuBdoMaInS", 100, true);
    testSuccess("Max-Age = 100; includesubdomains ", 100, true);
    testSuccess("INCLUDESUBDOMAINS;MaX-AgE = 100 ", 100, true);
    // Turns out, the actual directive is entirely optional (hence the
    // trailing semicolon)
    testSuccess("max-age=100;includeSubdomains;", 100, true);

    // these are weird tests, but are testing that some extended syntax is
    // still allowed (but it is ignored)
    testSuccess("max-age=100 ; includesubdomainsSomeStuff", 100, false);
    testSuccess("\r\n\t\t    \tcompletelyUnrelated = foobar; max-age= 34520103"
                + "\t \t; alsoUnrelated;asIsThis;\tincludeSubdomains\t\t \t",
                34520103, true);
    testSuccess('max-age=100; unrelated="quoted \\"thingy\\""', 100, false);

    // SHOULD FAIL:
    // invalid max-ages
    testFailure("max-age");
    testFailure("max-age ");
    testFailure("max-age=p");
    testFailure("max-age=*1p2");
    testFailure("max-age=.20032");
    testFailure("max-age=!20032");
    testFailure("max-age==20032");

    // invalid headers
    testFailure("foobar");
    testFailure("maxage=100");
    testFailure("maxa-ge=100");
    testFailure("max-ag=100");
    testFailure("includesubdomains");
    testFailure(";");
    testFailure('max-age="100');
    // The max-age directive here doesn't conform to the spec, so it MUST
    // be ignored. Consequently, the REQUIRED max-age directive is not
    // present in this header, and so it is invalid.
    testFailure("max-age=100, max-age=200; includeSubdomains");
    testFailure("max-age=100 includesubdomains");
    testFailure("max-age=100 bar foo");
    testFailure("max-age=100randomstuffhere");
    // All directives MUST appear only once in an STS header field.
    testFailure("max-age=100; max-age=200");
    testFailure("includeSubdomains; max-age=200; includeSubdomains");
    testFailure("max-age=200; includeSubdomains; includeSubdomains");
    // The includeSubdomains directive is valueless.
    testFailure("max-age=100; includeSubdomains=unexpected");
    // LWS must have at least one space or horizontal tab
    testFailure("\r\nmax-age=200");
}
