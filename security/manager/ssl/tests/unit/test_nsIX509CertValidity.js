// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// This file tests the nsIX509CertValidity implementation.

function fuzzyEqual(attributeName, dateString, expectedTime) {
  info(`${attributeName}: ${dateString}`);
  let absTimeDiff = Math.abs(expectedTime - Date.parse(dateString));
  const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;
  lessOrEqual(absTimeDiff, ONE_DAY_IN_MS,
              `Time difference for ${attributeName} should be <= one day`);
}

function run_test() {
  // Date.parse("2013-01-01T00:00:00Z")
  const NOT_BEFORE_IN_MS = 1356998400000;
  // Date.parse("2014-01-01T00:00:00Z")
  const NOT_AFTER_IN_MS = 1388534400000;
  let cert = constructCertFromFile("bad_certs/expired-ee.pem");

  equal(cert.validity.notBefore, NOT_BEFORE_IN_MS * 1000,
        "Actual and expected notBefore should be equal");
  equal(cert.validity.notAfter, NOT_AFTER_IN_MS * 1000,
        "Actual and expected notAfter should be equal");

  // The following tests rely on the implementation of nsIX509CertValidity
  // providing long formatted dates to work. If this is not true, a returned
  // short formatted date such as "12/31/12" may be interpreted as something
  // other than the expected "December 31st, 2012".
  //
  // On Android, the implementation of nsIDateTimeFormat currently does not
  // return a long formatted date even if it is asked to. This, combined with
  // the reason above is why the following tests are disabled on Android.
  if (AppConstants.platform != "android") {
    fuzzyEqual("notBeforeLocalTime", cert.validity.notBeforeLocalTime,
               NOT_BEFORE_IN_MS);
    fuzzyEqual("notBeforeLocalDay", cert.validity.notBeforeLocalDay,
               NOT_BEFORE_IN_MS);
    fuzzyEqual("notBeforeGMT", cert.validity.notBeforeGMT, NOT_BEFORE_IN_MS);

    fuzzyEqual("notAfterLocalTime", cert.validity.notAfterLocalTime,
               NOT_AFTER_IN_MS);
    fuzzyEqual("notAfterLocalDay", cert.validity.notAfterLocalDay,
               NOT_AFTER_IN_MS);
    fuzzyEqual("notAfterGMT", cert.validity.notAfterGMT, NOT_AFTER_IN_MS);
  }
}
