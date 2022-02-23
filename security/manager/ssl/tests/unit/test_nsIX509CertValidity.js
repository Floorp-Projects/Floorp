// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// This file tests the nsIX509CertValidity implementation.

function run_test() {
  // Date.parse("2013-01-01T00:00:00Z")
  const NOT_BEFORE_IN_MS = 1356998400000;
  // Date.parse("2014-01-01T00:00:00Z")
  const NOT_AFTER_IN_MS = 1388534400000;
  let cert = constructCertFromFile("bad_certs/expired-ee.pem");

  equal(
    cert.validity.notBefore,
    NOT_BEFORE_IN_MS * 1000,
    "Actual and expected notBefore should be equal"
  );
  equal(
    cert.validity.notAfter,
    NOT_AFTER_IN_MS * 1000,
    "Actual and expected notAfter should be equal"
  );
}
