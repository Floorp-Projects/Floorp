/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test checks a number of things:
// * it ensures that data loaded from revocations.txt on startup is present
// * it ensures that certItems in blocklist.xml are persisted correctly
// * it ensures that items in the CertBlocklist are seen as revoked by the
//   cert verifier
// * it does a sanity check to ensure other cert verifier behavior is
//   unmodified

// First, we need to setup appInfo for the blocklist service to work
var id = "xpcshell@tests.mozilla.org";
var appName = "XPCShell";
var version = "1";
var platformVersion = "1.9.2";
Cu.import("resource://testing-common/AppInfo.jsm", this);
/* global updateAppInfo:false */ // Imported via AppInfo.jsm.
updateAppInfo({
  name: appName,
  ID: id,
  version,
  platformVersion: platformVersion ? platformVersion : "1.0",
  crashReporter: true,
});

// we need to ensure we setup revocation data before certDB, or we'll start with
// no revocation.txt in the profile
var gProfile = do_get_profile();

// Write out an empty blocklist.xml file to the profile to ensure nothing
// is blocklisted by default
var blockFile = gProfile.clone();
blockFile.append("blocklist.xml");
var stream = Cc["@mozilla.org/network/file-output-stream;1"]
               .createInstance(Ci.nsIFileOutputStream);
stream.init(blockFile,
  FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_TRUNCATE,
  FileUtils.PERMS_FILE, 0);

var data = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
           "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">\n" +
           "</blocklist>\n";
stream.write(data, data.length);
stream.close();

const PREF_BLOCKLIST_UPDATE_ENABLED = "services.blocklist.update_enabled";
const PREF_ONECRL_VIA_AMO = "security.onecrl.via.amo";

var gRevocations = gProfile.clone();
gRevocations.append("revocations.txt");
if (!gRevocations.exists()) {
  let existing = do_get_file("test_onecrl/sample_revocations.txt", false);
  existing.copyTo(gProfile, "revocations.txt");
}

var certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

// set up a test server to serve the blocklist.xml
var testserver = new HttpServer();

var initialBlocklist = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" +
    "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">" +
    // test with some bad data ...
    "<certItems><certItem issuerName='Some nonsense in issuer'>" +
    "<serialNumber>AkHVNA==</serialNumber>" +
    "</certItem><certItem issuerName='MA0xCzAJBgNVBAMMAmNh'>" +
    "<serialNumber>some nonsense in serial</serialNumber>" +
    "</certItem><certItem issuerName='some nonsense in both issuer'>" +
    "<serialNumber>and serial</serialNumber></certItem>" +
    // some mixed
    // In this case, the issuer name and the valid serialNumber correspond
    // to test-int.pem in bad_certs/
    "<certItem issuerName='MBIxEDAOBgNVBAMMB1Rlc3QgQ0E='>" +
    "<serialNumber>oops! more nonsense.</serialNumber>" +
    "<serialNumber>BVio/iQ21GCi2iUven8oJ/gae74=</serialNumber></certItem>" +
    // ... and some good
    // In this case, the issuer name and the valid serialNumber correspond
    // to other-test-ca.pem in bad_certs/ (for testing root revocation)
    "<certItem issuerName='MBgxFjAUBgNVBAMMDU90aGVyIHRlc3QgQ0E='>" +
    "<serialNumber>exJUIJpq50jgqOwQluhVrAzTF74=</serialNumber></certItem>" +
    // This item corresponds to an entry in sample_revocations.txt where:
    // isser name is "another imaginary issuer" base-64 encoded, and
    // serialNumbers are:
    // "serial2." base-64 encoded, and
    // "another serial." base-64 encoded
    // We need this to ensure that existing items are retained if they're
    // also in the blocklist
    "<certItem issuerName='YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy'>" +
    "<serialNumber>c2VyaWFsMi4=</serialNumber>" +
    "<serialNumber>YW5vdGhlciBzZXJpYWwu</serialNumber></certItem>" +
    // This item revokes same-issuer-ee.pem by subject and pubKeyHash.
    "<certItem subject='MCIxIDAeBgNVBAMMF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5'" +
    " pubKeyHash='VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8='>" +
    "</certItem></certItems></blocklist>";

var updatedBlocklist = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" +
    "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">" +
    "<certItems>" +
    "<certItem issuerName='something new in both the issuer'>" +
    "<serialNumber>and the serial number</serialNumber></certItem>" +
    "</certItems></blocklist>";


var blocklists = {
  "/initialBlocklist/": initialBlocklist,
  "/updatedBlocklist/": updatedBlocklist
};

function serveResponse(request, response) {
  do_print("Serving for path " + request.path + "\n");
  response.write(blocklists[request.path]);
}

for (var path in blocklists) {
  testserver.registerPathHandler(path, serveResponse);
}

// start the test server
testserver.start(-1);
var port = testserver.identity.primaryPort;

// Setup the addonManager
var addonManager = Cc["@mozilla.org/addons/integration;1"]
                     .getService(Ci.nsIObserver)
                     .QueryInterface(Ci.nsITimerCallback);
addonManager.observe(null, "addons-startup", null);

var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                  .createInstance(Ci.nsIScriptableUnicodeConverter);
converter.charset = "UTF-8";

function verify_cert(file, expectedError) {
  let ee = constructCertFromFile(file);
  checkCertErrorGeneric(certDB, ee, expectedError, certificateUsageSSLServer);
}

// The certificate blocklist currently only applies to TLS server certificates.
function verify_non_tls_usage_succeeds(file) {
  let ee = constructCertFromFile(file);
  checkCertErrorGeneric(certDB, ee, PRErrorCodeSuccess,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certDB, ee, PRErrorCodeSuccess,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certDB, ee, PRErrorCodeSuccess,
                        certificateUsageEmailRecipient);
}

function load_cert(cert, trust) {
  let file = "bad_certs/" + cert + ".pem";
  addCertFromFile(certDB, file, trust);
}

function test_is_revoked(certList, issuerString, serialString, subjectString,
                         pubKeyString) {
  let issuer = converter.convertToByteArray(issuerString || "", {});
  let serial = converter.convertToByteArray(serialString || "", {});
  let subject = converter.convertToByteArray(subjectString || "", {});
  let pubKey = converter.convertToByteArray(pubKeyString || "", {});

  return certList.isCertRevoked(issuer,
                                issuerString ? issuerString.length : 0,
                                serial,
                                serialString ? serialString.length : 0,
                                subject,
                                subjectString ? subjectString.length : 0,
                                pubKey,
                                pubKeyString ? pubKeyString.length : 0);
}

function fetch_blocklist(blocklistPath) {
  do_print("path is " + blocklistPath + "\n");
  let certblockObserver = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "blocklist-updated");
      run_next_test();
    }
  };

  Services.obs.addObserver(certblockObserver, "blocklist-updated", false);
  Services.prefs.setCharPref("extensions.blocklist.url",
                              `http://localhost:${port}/${blocklistPath}`);
  let blocklist = Cc["@mozilla.org/extensions/blocklist;1"]
                    .getService(Ci.nsITimerCallback);
  blocklist.notify(null);
}

function check_revocations_txt_contents(expected) {
  let profile = do_get_profile();
  let revocations = profile.clone();
  revocations.append("revocations.txt");
  ok(revocations.exists(), "the revocations file should exist");
  let inputStream = Cc["@mozilla.org/network/file-input-stream;1"]
                      .createInstance(Ci.nsIFileInputStream);
  inputStream.init(revocations, -1, -1, 0);
  inputStream.QueryInterface(Ci.nsILineInputStream);
  let contents = "";
  let hasmore = false;
  do {
    let line = {};
    hasmore = inputStream.readLine(line);
    contents += (contents.length == 0 ? "" : "\n") + line.value;
  } while (hasmore);
  equal(contents, expected, "revocations.txt should be as expected");
}

function run_test() {
  // import the certificates we need
  load_cert("test-ca", "CTu,CTu,CTu");
  load_cert("test-int", ",,");
  load_cert("other-test-ca", "CTu,CTu,CTu");

  let certList = Cc["@mozilla.org/security/certblocklist;1"]
                  .getService(Ci.nsICertBlocklist);

  let expected = "# Auto generated contents. Do not edit.\n" +
                 "MCIxIDAeBgNVBAMMF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5\n" +
                 "\tVCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=\n" +
                 "MBIxEDAOBgNVBAMMB1Rlc3QgQ0E=\n" +
                 " BVio/iQ21GCi2iUven8oJ/gae74=\n" +
                 "MBgxFjAUBgNVBAMMDU90aGVyIHRlc3QgQ0E=\n" +
                 " exJUIJpq50jgqOwQluhVrAzTF74=\n" +
                 "YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy\n" +
                 " YW5vdGhlciBzZXJpYWwu\n" +
                 " c2VyaWFsMi4=";

  // This test assumes OneCRL updates via AMO
  Services.prefs.setBoolPref(PREF_BLOCKLIST_UPDATE_ENABLED, false);
  Services.prefs.setBoolPref(PREF_ONECRL_VIA_AMO, true);

  add_test(function () {
    // check some existing items in revocations.txt are blocked. Since the
    // CertBlocklistItems don't know about the data they contain, we can use
    // arbitrary data (not necessarily DER) to test if items are revoked or not.
    // This test corresponds to:
    // issuer: c29tZSBpbWFnaW5hcnkgaXNzdWVy
    // serial: c2VyaWFsLg==
    ok(test_is_revoked(certList, "some imaginary issuer", "serial."),
      "issuer / serial pair should be blocked");

    // This test corresponds to:
    // issuer: YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy
    // serial: c2VyaWFsLg==
    ok(test_is_revoked(certList, "another imaginary issuer", "serial."),
      "issuer / serial pair should be blocked");

    // And this test corresponds to:
    // issuer: YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy
    // serial: c2VyaWFsMi4=
    // (we test this issuer twice to ensure we can read multiple serials)
    ok(test_is_revoked(certList, "another imaginary issuer", "serial2."),
      "issuer / serial pair should be blocked");

    // Soon we'll load a blocklist which revokes test-int.pem, which issued
    // test-int-ee.pem.
    // Check the cert validates before we load the blocklist
    let file = "test_onecrl/test-int-ee.pem";
    verify_cert(file, PRErrorCodeSuccess);

    // The blocklist also revokes other-test-ca.pem, which issued
    // other-ca-ee.pem. Check the cert validates before we load the blocklist
    file = "bad_certs/other-issuer-ee.pem";
    verify_cert(file, PRErrorCodeSuccess);

    // The blocklist will revoke same-issuer-ee.pem via subject / pubKeyHash.
    // Check the cert validates before we load the blocklist
    file = "test_onecrl/same-issuer-ee.pem";
    verify_cert(file, PRErrorCodeSuccess);

    run_next_test();
  });

  // blocklist load is async so we must use add_test from here
  add_test(function() {
    fetch_blocklist("initialBlocklist/");
  });

  add_test(function() {
    // The blocklist will be loaded now. Let's check the data is sane.
    // In particular, we should still have the revoked issuer / serial pair
    // that was in both revocations.txt and the blocklist.xml
    ok(test_is_revoked(certList, "another imaginary issuer", "serial2."),
      "issuer / serial pair should be blocked");

    // Check that both serials in the certItem with multiple serials were read
    // properly
    ok(test_is_revoked(certList, "another imaginary issuer", "serial2."),
       "issuer / serial pair should be blocked");
    ok(test_is_revoked(certList, "another imaginary issuer", "another serial."),
       "issuer / serial pair should be blocked");

    // test a subject / pubKey revocation
    ok(test_is_revoked(certList, "nonsense", "more nonsense",
                       "some imaginary subject", "some imaginary pubkey"),
       "issuer / serial pair should be blocked");

    // Check the blocklist entry has been persisted properly to the backing
    // file
    check_revocations_txt_contents(expected);

    // Check the blocklisted intermediate now causes a failure
    let file = "test_onecrl/test-int-ee.pem";
    verify_cert(file, SEC_ERROR_REVOKED_CERTIFICATE);
    verify_non_tls_usage_succeeds(file);

    // Check the ee with the blocklisted root also causes a failure
    file = "bad_certs/other-issuer-ee.pem";
    verify_cert(file, SEC_ERROR_REVOKED_CERTIFICATE);
    verify_non_tls_usage_succeeds(file);

    // Check the ee blocked by subject / pubKey causes a failure
    file = "test_onecrl/same-issuer-ee.pem";
    verify_cert(file, SEC_ERROR_REVOKED_CERTIFICATE);
    verify_non_tls_usage_succeeds(file);

    // Check a non-blocklisted chain still validates OK
    file = "bad_certs/default-ee.pem";
    verify_cert(file, PRErrorCodeSuccess);

    // Check a bad cert is still bad (unknown issuer)
    file = "bad_certs/unknownissuer.pem";
    verify_cert(file, SEC_ERROR_UNKNOWN_ISSUER);

    // check that save with no further update is a no-op
    let lastModified = gRevocations.lastModifiedTime;
    // add an already existing entry
    certList.revokeCertByIssuerAndSerial("YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy",
                                         "c2VyaWFsMi4=");
    certList.saveEntries();
    let newModified = gRevocations.lastModifiedTime;
    equal(lastModified, newModified,
          "saveEntries with no modifications should not update the backing file");

    run_next_test();
  });

  // disable AMO cert blocklist - and check blocklist.xml changes do not
  // affect the data stored.
  add_test(function() {
    Services.prefs.setBoolPref("security.onecrl.via.amo", false);
    fetch_blocklist("updatedBlocklist/");
  });

  add_test(function() {
    // Check the blocklist entry has not changed
    check_revocations_txt_contents(expected);
    run_next_test();
  });

  run_next_test();
}
