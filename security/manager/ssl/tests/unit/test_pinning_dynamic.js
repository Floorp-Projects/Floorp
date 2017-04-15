/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create a site security service state file
// and see that the site security service reads it properly.

function writeLine(aLine, aOutputStream) {
  aOutputStream.write(aLine, aLine.length);
}

var gSSService = null;
var gSSSStateSeen = false;
var gPreloadStateSeen = false;

var profileDir = do_get_profile();
var certdb;

function certFromFile(cert_name) {
  return constructCertFromFile("test_pinning_dynamic/" + cert_name + ".pem");
}

function loadCert(cert_name, trust_string) {
  let cert_filename = "test_pinning_dynamic/" + cert_name + ".pem";
  addCertFromFile(certdb, cert_filename, trust_string);
  return constructCertFromFile(cert_filename);
}

function checkOK(cert, hostname) {
  return checkCertErrorGeneric(certdb, cert, PRErrorCodeSuccess,
                               certificateUsageSSLServer, {}, hostname);
}

function checkFail(cert, hostname) {
  return checkCertErrorGeneric(certdb, cert, MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE,
                               certificateUsageSSLServer, {}, hostname);
}

const NON_ISSUED_KEY_HASH = "KHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAN=";
const PINNING_ROOT_KEY_HASH = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";

function run_test() {
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);

  let stateFile = profileDir.clone();
  stateFile.append(SSS_STATE_FILE_NAME);
  // Assuming we're working with a clean slate, the SSS_STATE file shouldn't
  // exist until we create it.
  ok(!stateFile.exists(),
     "State file should not exist when working with a clean slate");
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let now = (new Date()).getTime();
  writeLine(`a.pinning2.example.com:HPKP\t0\t0\t${now + 100000},1,0,${PINNING_ROOT_KEY_HASH}\n`, outputStream);
  writeLine(`b.pinning2.example.com:HPKP\t0\t0\t${now + 100000},1,1,${PINNING_ROOT_KEY_HASH}\n`, outputStream);

  outputStream.close();

  let preloadFile = profileDir.clone();
  preloadFile.append(PRELOAD_STATE_FILE_NAME);
  ok(!preloadFile.exists(),
     "Preload file should not exist when working with a clean slate");

  outputStream = FileUtils.openFileOutputStream(preloadFile);
  writeLine(`a.preload.example.com:HPKP\t0\t0\t${now + 100000},1,1,${PINNING_ROOT_KEY_HASH}\n`, outputStream);
  outputStream.close();

  Services.obs.addObserver(checkStateRead, "data-storage-ready");
  do_test_pending();
  gSSService = Cc["@mozilla.org/ssservice;1"]
                 .getService(Ci.nsISiteSecurityService);
  notEqual(gSSService, null,
           "SiteSecurityService should have initialized successfully using" +
           " the generated state file");
}

function checkDefaultSiteHPKPStatus() {
  ok(gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HPKP,
       Services.io.newURI("https://a.pinning2.example.com"), 0),
     "a.pinning2.example.com should have HPKP status");
  ok(!gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HPKP,
       Services.io.newURI("https://x.a.pinning2.example.com"), 0),
     "x.a.pinning2.example.com should not have HPKP status");
  ok(gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HPKP,
       Services.io.newURI("https://b.pinning2.example.com"), 0),
     "b.pinning2.example.com should have HPKP status");
  ok(gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HPKP,
       Services.io.newURI("https://x.b.pinning2.example.com"), 0),
     "x.b.pinning2.example.com should have HPKP status");
}

function checkStateRead(aSubject, aTopic, aData) {
  if (aData == SSS_STATE_FILE_NAME) {
    gSSSStateSeen = true;
  } else if (aData == PRELOAD_STATE_FILE_NAME) {
    gPreloadStateSeen = true;
  } else {
    throw new Error("Observed data should either be the Site Security " +
                    "Service state file name or the preload file name");
  }

  if (!gSSSStateSeen || !gPreloadStateSeen) {
    return;
  }

  notEqual(gSSService, null, "SiteSecurityService should be initialized");

  // Initializing the certificate DB will cause NSS-initialization, which in
  // turn initializes the site security service. Since we're in part testing
  // that the site security service correctly reads its state file, we have to
  // make sure it doesn't start up before we've populated the file
  certdb = Cc["@mozilla.org/security/x509certdb;1"]
             .getService(Ci.nsIX509CertDB);

  loadCert("pinningroot", "CTu,CTu,CTu");
  loadCert("badca", "CTu,CTu,CTu");

  // the written entry is for a.pinning2.example.com without subdomains
  // and b.pinning2.example.com with subdomains
  checkFail(certFromFile("a.pinning2.example.com-badca"),
            "a.pinning2.example.com");
  checkOK(certFromFile("a.pinning2.example.com-pinningroot"),
          "a.pinning2.example.com");
  checkOK(certFromFile("x.a.pinning2.example.com-badca"),
          "x.a.pinning2.example.com");
  checkOK(certFromFile("x.a.pinning2.example.com-pinningroot"),
          "x.a.pinning2.example.com");

  checkFail(certFromFile("b.pinning2.example.com-badca"),
            "b.pinning2.example.com");
  checkOK(certFromFile("b.pinning2.example.com-pinningroot"),
          "b.pinning2.example.com");
  checkFail(certFromFile("x.b.pinning2.example.com-badca"),
            "x.b.pinning2.example.com");
  checkOK(certFromFile("x.b.pinning2.example.com-pinningroot"),
          "x.b.pinning2.example.com");

  checkDefaultSiteHPKPStatus();


  // add includeSubdomains to a.pinning2.example.com
  gSSService.setKeyPins("a.pinning2.example.com", true,
                        new Date().getTime() + 1000000, 2,
                        [NON_ISSUED_KEY_HASH, PINNING_ROOT_KEY_HASH]);
  checkFail(certFromFile("a.pinning2.example.com-badca"),
            "a.pinning2.example.com");
  checkOK(certFromFile("a.pinning2.example.com-pinningroot"),
          "a.pinning2.example.com");
  checkFail(certFromFile("x.a.pinning2.example.com-badca"),
            "x.a.pinning2.example.com");
  checkOK(certFromFile("x.a.pinning2.example.com-pinningroot"),
          "x.a.pinning2.example.com");
  checkFail(certFromFile("b.pinning2.example.com-badca"),
            "b.pinning2.example.com");
  checkOK(certFromFile("b.pinning2.example.com-pinningroot"),
          "b.pinning2.example.com");
  checkFail(certFromFile("x.b.pinning2.example.com-badca"),
            "x.b.pinning2.example.com");
  checkOK(certFromFile("x.b.pinning2.example.com-pinningroot"),
          "x.b.pinning2.example.com");

  ok(gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HPKP,
       Services.io.newURI("https://a.pinning2.example.com"), 0),
     "a.pinning2.example.com should still have HPKP status after adding" +
     " includeSubdomains to a.pinning2.example.com");
  ok(gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HPKP,
       Services.io.newURI("https://x.a.pinning2.example.com"), 0),
     "x.a.pinning2.example.com should now have HPKP status after adding" +
     " includeSubdomains to a.pinning2.example.com");

  // Now setpins without subdomains
  gSSService.setKeyPins("a.pinning2.example.com", false,
                        new Date().getTime() + 1000000, 2,
                        [NON_ISSUED_KEY_HASH, PINNING_ROOT_KEY_HASH]);
  checkFail(certFromFile("a.pinning2.example.com-badca"),
            "a.pinning2.example.com");
  checkOK(certFromFile("a.pinning2.example.com-pinningroot"),
          "a.pinning2.example.com");
  checkOK(certFromFile("x.a.pinning2.example.com-badca"),
          "x.a.pinning2.example.com");
  checkOK(certFromFile("x.a.pinning2.example.com-pinningroot"),
          "x.a.pinning2.example.com");

  checkFail(certFromFile("b.pinning2.example.com-badca"),
            "b.pinning2.example.com");
  checkOK(certFromFile("b.pinning2.example.com-pinningroot"),
          "b.pinning2.example.com");
  checkFail(certFromFile("x.b.pinning2.example.com-badca"),
            "x.b.pinning2.example.com");
  checkOK(certFromFile("x.b.pinning2.example.com-pinningroot"),
          "x.b.pinning2.example.com");

  checkDefaultSiteHPKPStatus();

  // failure to insert new pin entry leaves previous pin behavior
  throws(() => {
    gSSService.setKeyPins("a.pinning2.example.com", true,
                          new Date().getTime() + 1000000, 1, ["not a hash"]);
  }, /NS_ERROR_ILLEGAL_VALUE/, "Attempting to set an invalid pin should fail");
  checkFail(certFromFile("a.pinning2.example.com-badca"),
            "a.pinning2.example.com");
  checkOK(certFromFile("a.pinning2.example.com-pinningroot"),
          "a.pinning2.example.com");
  checkOK(certFromFile("x.a.pinning2.example.com-badca"),
          "x.a.pinning2.example.com");
  checkOK(certFromFile("x.a.pinning2.example.com-pinningroot"),
          "x.a.pinning2.example.com");

  checkFail(certFromFile("b.pinning2.example.com-badca"),
            "b.pinning2.example.com");
  checkOK(certFromFile("b.pinning2.example.com-pinningroot"),
          "b.pinning2.example.com");
  checkFail(certFromFile("x.b.pinning2.example.com-badca"),
            "x.b.pinning2.example.com");
  checkOK(certFromFile("x.b.pinning2.example.com-pinningroot"),
          "x.b.pinning2.example.com");

  checkDefaultSiteHPKPStatus();

  // Incorrect size results in failure
  throws(() => {
    gSSService.setKeyPins("a.pinning2.example.com", true,
                          new Date().getTime() + 1000000, 2, ["not a hash"]);
  }, /NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY/,
     "Attempting to set a pin with an incorrect size should fail");

  // Ensure built-in pins work as expected
  ok(!gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HPKP,
       Services.io.newURI("https://nonexistent.example.com"), 0),
     "Not built-in nonexistent.example.com should not have HPKP status");
  ok(gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HPKP,
       Services.io.newURI("https://include-subdomains.pinning.example.com"), 0),
     "Built-in include-subdomains.pinning.example.com should have HPKP status");

  gSSService.setKeyPins("a.pinning2.example.com", false, new Date().getTime(),
                        1, [NON_ISSUED_KEY_HASH]);

  // Check that a preload pin loaded from file works as expected
  checkFail(certFromFile("a.preload.example.com-badca"), "a.preload.example.com");
  checkOK(certFromFile("a.preload.example.com-pinningroot"), "a.preload.example.com");

  // Check a dynamic addition works as expected
  // first, it should succeed with the badCA - because there's no pin
  checkOK(certFromFile("b.preload.example.com-badca"), "b.preload.example.com");
  // then we add a pin, and we should get a failure (ensuring the expiry is
  // after the test timeout)
  gSSService.setKeyPins("b.preload.example.com", false,
                        new Date().getTime() + 1000000, 2,
                        [NON_ISSUED_KEY_HASH, PINNING_ROOT_KEY_HASH], true);
  checkFail(certFromFile("b.preload.example.com-badca"), "b.preload.example.com");

  do_timeout(1250, checkExpiredState);
}

function checkExpiredState() {
  checkOK(certFromFile("a.pinning2.example.com-badca"),
          "a.pinning2.example.com");
  checkOK(certFromFile("a.pinning2.example.com-pinningroot"),
          "a.pinning2.example.com");
  checkOK(certFromFile("x.a.pinning2.example.com-badca"),
          "x.a.pinning2.example.com");
  checkOK(certFromFile("x.a.pinning2.example.com-pinningroot"),
          "x.a.pinning2.example.com");

  checkFail(certFromFile("b.pinning2.example.com-badca"),
          "b.pinning2.example.com");
  checkOK(certFromFile("b.pinning2.example.com-pinningroot"),
          "b.pinning2.example.com");
  checkFail(certFromFile("x.b.pinning2.example.com-badca"),
          "x.b.pinning2.example.com");
  checkOK(certFromFile("x.b.pinning2.example.com-pinningroot"),
          "x.b.pinning2.example.com");
  checkPreloadClear();
}

function checkPreloadClear() {
  // Check that the preloaded pins still work after private data is cleared
  gSSService.clearAll();
  checkFail(certFromFile("b.preload.example.com-badca"), "b.preload.example.com");

  // Check that the preloaded pins are cleared when we clear preloads
  gSSService.clearPreloads();
  checkOK(certFromFile("b.preload.example.com-badca"), "b.preload.example.com");

  do_test_finished();
}
