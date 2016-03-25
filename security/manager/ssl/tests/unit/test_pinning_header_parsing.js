/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to check that parsing of HPKP headers
// is correct.

var profileDir = do_get_profile();
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
var gSSService = Cc["@mozilla.org/ssservice;1"]
                   .getService(Ci.nsISiteSecurityService);

function certFromFile(cert_name) {
  return constructCertFromFile("test_pinning_dynamic/" + cert_name + ".pem");
}

function loadCert(cert_name, trust_string) {
  let cert_filename = "test_pinning_dynamic/" + cert_name + ".pem";
  addCertFromFile(certdb, cert_filename, trust_string);
  return constructCertFromFile(cert_filename);
}

function checkFailParseInvalidPin(pinValue) {
  let sslStatus = new FakeSSLStatus(
                        certFromFile('a.pinning2.example.com-pinningroot'));
  let uri = Services.io.newURI("https://a.pinning2.example.com", null, null);
  throws(() => {
    gSSService.processHeader(Ci.nsISiteSecurityService.HEADER_HPKP, uri,
                             pinValue, sslStatus, 0);
  }, /NS_ERROR_FAILURE/, `Invalid pin "${pinValue}" should be rejected`);
}

function checkPassValidPin(pinValue, settingPin) {
  let sslStatus = new FakeSSLStatus(
                        certFromFile('a.pinning2.example.com-pinningroot'));
  let uri = Services.io.newURI("https://a.pinning2.example.com", null, null);

  // setup preconditions for the test, if setting ensure there is no previous
  // state, if removing ensure there is a valid pin in place.
  if (settingPin) {
    gSSService.removeState(Ci.nsISiteSecurityService.HEADER_HPKP, uri, 0);
  } else {
    // add a known valid pin!
    let validPinValue = "max-age=5000;" + VALID_PIN1 + BACKUP_PIN1;
    gSSService.processHeader(Ci.nsISiteSecurityService.HEADER_HPKP, uri,
                             validPinValue, sslStatus, 0);
  }
  try {
    gSSService.processHeader(Ci.nsISiteSecurityService.HEADER_HPKP, uri,
                             pinValue, sslStatus, 0);
    ok(true, "Valid pin should be accepted");
  } catch (e) {
    ok(false, "Valid pin should have been accepted");
  }
  // after processing ensure that the postconditions are true, if setting
  // the host must be pinned, if removing the host must not be pinned
  let hostIsPinned = gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                             "a.pinning2.example.com", 0);
  if (settingPin) {
    ok(hostIsPinned, "Host should be considered pinned");
  } else {
    ok(!hostIsPinned, "Host should not be considered pinned");
  }
}

function checkPassSettingPin(pinValue) {
  return checkPassValidPin(pinValue, true);
}

function checkPassRemovingPin(pinValue) {
  return checkPassValidPin(pinValue, false);
}

const NON_ISSUED_KEY_HASH1 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
const NON_ISSUED_KEY_HASH2 = "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ=";
const PINNING_ROOT_KEY_HASH = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";
const MAX_AGE_ZERO = "max-age=0;";
const VALID_PIN1 = `pin-sha256="${PINNING_ROOT_KEY_HASH}";`;
const BACKUP_PIN1 = `pin-sha256="${NON_ISSUED_KEY_HASH1}";`;
const BACKUP_PIN2 = `pin-sha256="${NON_ISSUED_KEY_HASH2}";`;
const BROKEN_PIN1 = "pin-sha256=\"jdjsjsjs\";";
const GOOD_MAX_AGE = "max-age=69403;";
const INCLUDE_SUBDOMAINS = "includeSubdomains;";
const REPORT_URI = "report-uri=\"https://www.example.com/report/\";";
const UNRECOGNIZED_DIRECTIVE = "unreconized-dir=12343;";

function run_test() {
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);
  Services.prefs.setBoolPref("security.cert_pinning.process_headers_from_non_builtin_roots", true);

  loadCert("pinningroot", "CTu,CTu,CTu");
  loadCert("badca", "CTu,CTu,CTu");

  checkFailParseInvalidPin("max-age=INVALID");
  // check that incomplete headers are failure
  checkFailParseInvalidPin(GOOD_MAX_AGE);
  checkFailParseInvalidPin(VALID_PIN1);
  checkFailParseInvalidPin(REPORT_URI);
  checkFailParseInvalidPin(UNRECOGNIZED_DIRECTIVE);
  checkFailParseInvalidPin(VALID_PIN1 + BACKUP_PIN1);
  checkFailParseInvalidPin(GOOD_MAX_AGE + VALID_PIN1);
  checkFailParseInvalidPin(GOOD_MAX_AGE + VALID_PIN1 + BROKEN_PIN1);
  // next ensure a backup pin is present
  checkFailParseInvalidPin(GOOD_MAX_AGE + VALID_PIN1 + VALID_PIN1);
  // next section ensure duplicate directives result in failure
  checkFailParseInvalidPin(GOOD_MAX_AGE + GOOD_MAX_AGE + VALID_PIN1 + BACKUP_PIN1);
  checkFailParseInvalidPin(GOOD_MAX_AGE + VALID_PIN1 + BACKUP_PIN1 + INCLUDE_SUBDOMAINS + INCLUDE_SUBDOMAINS);
  checkFailParseInvalidPin(GOOD_MAX_AGE + VALID_PIN1 + BACKUP_PIN1 + REPORT_URI + REPORT_URI);
  checkFailParseInvalidPin("thisisinvalidtest");
  checkFailParseInvalidPin("invalid" + GOOD_MAX_AGE + VALID_PIN1 + BACKUP_PIN1);

  checkPassRemovingPin("max-age=0"); //test removal without terminating ';'
  checkPassRemovingPin(MAX_AGE_ZERO);
  checkPassRemovingPin(MAX_AGE_ZERO + VALID_PIN1);

  checkPassRemovingPin(VALID_PIN1 + MAX_AGE_ZERO + VALID_PIN1);
  checkPassSettingPin(GOOD_MAX_AGE + VALID_PIN1 + BACKUP_PIN1);
  checkPassSettingPin(GOOD_MAX_AGE + VALID_PIN1 + BACKUP_PIN2);
  checkPassSettingPin(GOOD_MAX_AGE + VALID_PIN1 + BACKUP_PIN2 + INCLUDE_SUBDOMAINS);
  checkPassSettingPin(VALID_PIN1 + GOOD_MAX_AGE + BACKUP_PIN2 + INCLUDE_SUBDOMAINS);
  checkPassSettingPin(VALID_PIN1 + GOOD_MAX_AGE + BACKUP_PIN2 + REPORT_URI + INCLUDE_SUBDOMAINS);
  checkPassSettingPin(INCLUDE_SUBDOMAINS + VALID_PIN1 + GOOD_MAX_AGE + BACKUP_PIN2);
  checkPassSettingPin(GOOD_MAX_AGE + VALID_PIN1 + BACKUP_PIN1 + UNRECOGNIZED_DIRECTIVE);
}
