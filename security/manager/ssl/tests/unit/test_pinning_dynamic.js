/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The purpose of this test is to create a site security service state file
// and see that the site security service reads it properly.

function writeLine(aLine, aOutputStream) {
  aOutputStream.write(aLine, aLine.length);
}

let gSSService = null;

let profileDir = do_get_profile();
let certdb;

function certFromFile(filename) {
  let der = readFile(do_get_file("test_pinning_dynamic/" + filename, false));
  return certdb.constructX509(der, der.length);
}

function loadCert(cert_name, trust_string) {
  let cert_filename = cert_name + ".der";
  addCertFromFile(certdb, "test_pinning_dynamic/" + cert_filename, trust_string);
  return certFromFile(cert_filename);
}

function checkOK(cert, hostname) {
  return checkCertErrorGeneric(certdb, cert, PRErrorCodeSuccess,
                               certificateUsageSSLServer, hostname);
}

function checkFail(cert, hostname) {
  return checkCertErrorGeneric(certdb, cert, MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE,
                               certificateUsageSSLServer, hostname);
}

const NON_ISSUED_KEY_HASH = "KHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAN=";
const PINNING_ROOT_KEY_HASH ="kXoHD1ZGyMuowchJwy+xgHlzh0kJFoI9KX0o0IrzTps=";

function run_test() {
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);

  let stateFile = profileDir.clone();
  stateFile.append(SSS_STATE_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  do_check_false(stateFile.exists());
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let now = (new Date()).getTime();
  writeLine("a.pinning2.example.com:HPKP\t0\t0\t" + (now + 100000) + ",1,0,kXoHD1ZGyMuowchJwy+xgHlzh0kJFoI9KX0o0IrzTps=\n", outputStream);
  writeLine("b.pinning2.example.com:HPKP\t0\t0\t" + (now + 100000) + ",1,1,kXoHD1ZGyMuowchJwy+xgHlzh0kJFoI9KX0o0IrzTps=\n", outputStream);

  outputStream.close();
  Services.obs.addObserver(checkStateRead, "data-storage-ready", false);
  do_test_pending();
  gSSService = Cc["@mozilla.org/ssservice;1"]
                 .getService(Ci.nsISiteSecurityService);
  do_check_true(gSSService != null);
}

function checkStateRead(aSubject, aTopic, aData) {
  do_check_eq(aData, SSS_STATE_FILE_NAME);
  do_check_neq(gSSService, null);

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
  checkFail(certFromFile('cn-a.pinning2.example.com-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-a.pinning2.example.com-pinningroot.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-x.a.pinning2.example.com-badca.der'), "x.a.pinning2.example.com");
  checkOK(certFromFile('cn-x.a.pinning2.example.com-pinningroot.der'), "x.a.pinning2.example.com");
  checkFail(certFromFile('cn-www.example.com-alt-a.pinning2.example-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-www.example.com-alt-a.pinning2.example-pinningroot.der'), "a.pinning2.example.com");

  checkFail(certFromFile('cn-b.pinning2.example.com-badca.der'), "b.pinning2.example.com");
  checkOK(certFromFile('cn-b.pinning2.example.com-pinningroot.der'), "b.pinning2.example.com");
  checkFail(certFromFile('cn-x.b.pinning2.example.com-badca.der'), "x.b.pinning2.example.com");
  checkOK(certFromFile('cn-x.b.pinning2.example.com-pinningroot.der'), "x.b.pinning2.example.com");

  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "a.pinning2.example.com", 0));
  do_check_false(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                         "x.a.pinning2.example.com", 0));
  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "b.pinning2.example.com", 0));
  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "x.b.pinning2.example.com", 0));


  // add withSubdomains to a.pinning2.example.com
  gSSService.setKeyPins("a.pinning2.example.com", true, 1000, 2,
                        [NON_ISSUED_KEY_HASH, PINNING_ROOT_KEY_HASH]);
  checkFail(certFromFile('cn-a.pinning2.example.com-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-a.pinning2.example.com-pinningroot.der'), "a.pinning2.example.com");
  checkFail(certFromFile('cn-x.a.pinning2.example.com-badca.der'), "x.a.pinning2.example.com");
  checkOK(certFromFile('cn-x.a.pinning2.example.com-pinningroot.der'), "x.a.pinning2.example.com");
  checkFail(certFromFile('cn-www.example.com-alt-a.pinning2.example-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-www.example.com-alt-a.pinning2.example-pinningroot.der'), "a.pinning2.example.com");
  checkFail(certFromFile('cn-b.pinning2.example.com-badca.der'), "b.pinning2.example.com");
  checkOK(certFromFile('cn-b.pinning2.example.com-pinningroot.der'), "b.pinning2.example.com");
  checkFail(certFromFile('cn-x.b.pinning2.example.com-badca.der'), "x.b.pinning2.example.com");
  checkOK(certFromFile('cn-x.b.pinning2.example.com-pinningroot.der'), "x.b.pinning2.example.com");

  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "a.pinning2.example.com", 0));
  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "x.a.pinning2.example.com", 0));

  // Now setpins without subdomains
  gSSService.setKeyPins("a.pinning2.example.com", false, 1000, 2,
                        [NON_ISSUED_KEY_HASH, PINNING_ROOT_KEY_HASH]);
  checkFail(certFromFile('cn-a.pinning2.example.com-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-a.pinning2.example.com-pinningroot.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-x.a.pinning2.example.com-badca.der'), "x.a.pinning2.example.com");
  checkOK(certFromFile('cn-x.a.pinning2.example.com-pinningroot.der'), "x.a.pinning2.example.com");
  checkFail(certFromFile('cn-www.example.com-alt-a.pinning2.example-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-www.example.com-alt-a.pinning2.example-pinningroot.der'), "a.pinning2.example.com");

  checkFail(certFromFile('cn-b.pinning2.example.com-badca.der'), "b.pinning2.example.com");
  checkOK(certFromFile('cn-b.pinning2.example.com-pinningroot.der'), "b.pinning2.example.com");
  checkFail(certFromFile('cn-x.b.pinning2.example.com-badca.der'), "x.b.pinning2.example.com");
  checkOK(certFromFile('cn-x.b.pinning2.example.com-pinningroot.der'), "x.b.pinning2.example.com");

  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "a.pinning2.example.com", 0));
  do_check_false(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                         "x.a.pinning2.example.com", 0));
  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "b.pinning2.example.com", 0));
  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "x.b.pinning2.example.com", 0));

  // failure to insert new pin entry leaves previous pin behavior
  try {
    gSSService.setKeyPins("a.pinning2.example.com", true, 1000, 1,
                          ["not a hash"]);
    do_check_true(false); // this shouldn't run
  } catch(e) {
  }
  checkFail(certFromFile('cn-a.pinning2.example.com-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-a.pinning2.example.com-pinningroot.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-x.a.pinning2.example.com-badca.der'), "x.a.pinning2.example.com");
  checkOK(certFromFile('cn-x.a.pinning2.example.com-pinningroot.der'), "x.a.pinning2.example.com");
  checkFail(certFromFile('cn-www.example.com-alt-a.pinning2.example-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-www.example.com-alt-a.pinning2.example-pinningroot.der'), "a.pinning2.example.com");

  checkFail(certFromFile('cn-b.pinning2.example.com-badca.der'), "b.pinning2.example.com");
  checkOK(certFromFile('cn-b.pinning2.example.com-pinningroot.der'), "b.pinning2.example.com");
  checkFail(certFromFile('cn-x.b.pinning2.example.com-badca.der'), "x.b.pinning2.example.com");
  checkOK(certFromFile('cn-x.b.pinning2.example.com-pinningroot.der'), "x.b.pinning2.example.com");

  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "a.pinning2.example.com", 0));
  do_check_false(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                         "x.a.pinning2.example.com", 0));
  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "b.pinning2.example.com", 0));
  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "x.b.pinning2.example.com", 0));

  // Incorrect size results in failure
  try {
    gSSService.setKeyPins("a.pinning2.example.com", true, 1000, 2,
                          ["not a hash"]);
    do_check_true(false); // this shouldn't run
  } catch(e) {
  }

  // Ensure built-in pins work as expected
  do_check_false(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                         "nonexistent.example.com", 0));
  do_check_true(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                                        "include-subdomains.pinning.example.com", 0));

  gSSService.setKeyPins("a.pinning2.example.com", false, 0, 1,
                        [NON_ISSUED_KEY_HASH]);

  do_timeout(1250, checkExpiredState);
}

function checkExpiredState() {
  checkOK(certFromFile('cn-a.pinning2.example.com-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-a.pinning2.example.com-pinningroot.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-x.a.pinning2.example.com-badca.der'), "x.a.pinning2.example.com");
  checkOK(certFromFile('cn-x.a.pinning2.example.com-pinningroot.der'), "x.a.pinning2.example.com");
  checkOK(certFromFile('cn-www.example.com-alt-a.pinning2.example-badca.der'), "a.pinning2.example.com");
  checkOK(certFromFile('cn-www.example.com-alt-a.pinning2.example-pinningroot.der'), "a.pinning2.example.com");

  checkFail(certFromFile('cn-b.pinning2.example.com-badca.der'), "b.pinning2.example.com");
  checkOK(certFromFile('cn-b.pinning2.example.com-pinningroot.der'), "b.pinning2.example.com");
  checkFail(certFromFile('cn-x.b.pinning2.example.com-badca.der'), "x.b.pinning2.example.com");
  checkOK(certFromFile('cn-x.b.pinning2.example.com-pinningroot.der'), "x.b.pinning2.example.com");

  do_test_finished();
}
