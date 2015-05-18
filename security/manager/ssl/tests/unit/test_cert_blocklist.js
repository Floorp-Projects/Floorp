/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks a number of things:
// * it ensures that data loaded from revocations.txt on startup is present
// * it ensures that certItems in blocklist.xml are persisted correctly
// * it ensures that items in the CertBlocklist are seen as revoked by the
//   cert verifier
// * it does a sanity check to ensure other cert verifier behavior is
//   unmodified

let { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

// First, we need to setup appInfo for the blocklist service to work
let id = "xpcshell@tests.mozilla.org";
let appName = "XPCShell";
let version = "1";
let platformVersion = "1.9.2";
let appInfo = {
  // nsIXULAppInfo
  vendor: "Mozilla",
  name: appName,
  ID: id,
  version: version,
  appBuildID: "2007010101",
  platformVersion: platformVersion ? platformVersion : "1.0",
  platformBuildID: "2007010101",

  // nsIXULRuntime
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",
  invalidateCachesOnRestart: function invalidateCachesOnRestart() {
    // Do nothing
  },

  // nsICrashReporter
  annotations: {},

  annotateCrashReport: function(key, data) {
    this.annotations[key] = data;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo,
                                         Ci.nsIXULRuntime,
                                         Ci.nsICrashReporter,
                                         Ci.nsISupports])
};

let XULAppInfoFactory = {
  createInstance: function (outer, iid) {
    appInfo.QueryInterface(iid);
    if (outer != null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return appInfo.QueryInterface(iid);
  }
};

let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");
registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                          XULAPPINFO_CONTRACTID, XULAppInfoFactory);

// we need to ensure we setup revocation data before certDB, or we'll start with
// no revocation.txt in the profile
let profile = do_get_profile();
let revocations = profile.clone();
revocations.append("revocations.txt");
if (!revocations.exists()) {
  let existing = do_get_file("test_onecrl/sample_revocations.txt", false);
  existing.copyTo(profile,"revocations.txt");
}

let certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

// set up a test server to serve the blocklist.xml
let testserver = new HttpServer();

let blocklist_contents =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" +
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
    // to test-int.der in tlsserver/
    "<certItem issuerName='MBIxEDAOBgNVBAMTB1Rlc3QgQ0E='>" +
    "<serialNumber>oops! more nonsense.</serialNumber>" +
    "<serialNumber>X1o=</serialNumber></certItem>" +
    // ... and some good
    // In this case, the issuer name and the valid serialNumber correspond
    // to other-test-ca.der in tlsserver/ (for testing root revocation)
    "<certItem issuerName='MBgxFjAUBgNVBAMTDU90aGVyIHRlc3QgQ0E='>" +
    "<serialNumber>AKEIivg=</serialNumber></certItem>" +
    // This item corresponds to an entry in sample_revocations.txt where:
    // isser name is "another imaginary issuer" base-64 encoded, and
    // serialNumbers are:
    // "serial2." base-64 encoded, and
    // "another serial." base-64 encoded
    // We need this to ensure that existing items are retained if they're
    // also in the blocklist
    "<certItem issuerName='YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy'>" +
    "<serialNumber>c2VyaWFsMi4=</serialNumber>" +
    "<serialNumber>YW5vdGhlciBzZXJpYWwu</serialNumber>" +
    "</certItem><certItem subject='MCIxIDAeBgNVBAMTF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5'"+
    " pubKeyHash='2ETEb0QP574JkM+35JVwS899PLUmt1rrJyWOV6GRfAE='>" +
    "</certItem></certItems></blocklist>";
testserver.registerPathHandler("/push_blocked_cert/",
  function serveResponse(request, response) {
    response.write(blocklist_contents);
  });

// start the test server
testserver.start(-1);
let port = testserver.identity.primaryPort;

// Setup the addonManager
let addonManager = Cc["@mozilla.org/addons/integration;1"]
                     .getService(Ci.nsIObserver)
                     .QueryInterface(Ci.nsITimerCallback);
addonManager.observe(null, "addons-startup", null);

let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                  .createInstance(Ci.nsIScriptableUnicodeConverter);
converter.charset = "UTF-8";

function verify_cert(file, expectedError) {
  let cert_der = readFile(do_get_file(file));
  let ee = certDB.constructX509(cert_der, cert_der.length);
  checkCertErrorGeneric(certDB, ee, expectedError, certificateUsageSSLServer);
}

function load_cert(cert, trust) {
  let file = "tlsserver/" + cert + ".der";
  addCertFromFile(certDB, file, trust);
}

function test_is_revoked(certList, issuerString, serialString, subjectString,
                         pubKeyString) {
  let issuer = converter.convertToByteArray(issuerString ? issuerString : '',
                                            {});

  let serial = converter.convertToByteArray(serialString ? serialString : '',
                                            {});

  let subject = converter.convertToByteArray(subjectString ? subjectString : '',
                                             {});

  let pubKey = converter.convertToByteArray(pubKeyString ? pubKeyString : '',
                                            {});

  return certList.isCertRevoked(issuer,
                                issuerString ? issuerString.length : 0,
                                serial,
                                serialString ? serialString.length : 0,
                                subject,
                                subjectString ? subjectString.length : 0,
                                pubKey,
                                pubKeyString ? pubKeyString.length : 0);
}

function run_test() {
  // import the certificates we need
  load_cert("test-ca", "CTu,CTu,CTu");
  load_cert("test-int", ",,");
  load_cert("other-test-ca", "CTu,CTu,CTu");

  let certList = Cc["@mozilla.org/security/certblocklist;1"]
                   .getService(Ci.nsICertBlocklist);

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

  // Soon we'll load a blocklist which revokes test-int.der, which issued
  // test-int-ee.der.
  // Check the cert validates before we load the blocklist
  let file = "tlsserver/test-int-ee.der";
  verify_cert(file, PRErrorCodeSuccess);

  // The blocklist also revokes other-test-ca.der, which issued other-ca-ee.der.
  // Check the cert validates before we load the blocklist
  file = "tlsserver/other-issuer-ee.der";
  verify_cert(file, PRErrorCodeSuccess);

  // The blocklist will revoke same-issuer-ee.der via subject / pubKeyHash.
  // Check the cert validates before we load the blocklist
  file = "tlsserver/same-issuer-ee.der";
  verify_cert(file, PRErrorCodeSuccess);

  // blocklist load is async so we must use add_test from here
  add_test(function() {
    let certblockObserver = {
      observe: function(aSubject, aTopic, aData) {
        Services.obs.removeObserver(this, "blocklist-updated");
        run_next_test();
      }
    }

    Services.obs.addObserver(certblockObserver, "blocklist-updated", false);
    Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:" +
                               port + "/push_blocked_cert/");
    let blocklist = Cc["@mozilla.org/extensions/blocklist;1"]
                      .getService(Ci.nsITimerCallback);
    blocklist.notify(null);
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
    let profile = do_get_profile();
    let revocations = profile.clone();
    revocations.append("revocations.txt");
    ok(revocations.exists(), "the revocations file should exist");
    let inputStream = Cc["@mozilla.org/network/file-input-stream;1"]
                        .createInstance(Ci.nsIFileInputStream);
    inputStream.init(revocations,-1, -1, 0);
    inputStream.QueryInterface(Ci.nsILineInputStream);
    let contents = "";
    let hasmore = false;
    do {
      var line = {};
      hasmore = inputStream.readLine(line);
      contents = contents + (contents.length == 0 ? "" : "\n") + line.value;
    } while (hasmore);
    let expected = "# Auto generated contents. Do not edit.\n" +
                  "MCIxIDAeBgNVBAMTF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5\n"+
                  "\t2ETEb0QP574JkM+35JVwS899PLUmt1rrJyWOV6GRfAE=\n"+
                  "MBgxFjAUBgNVBAMTDU90aGVyIHRlc3QgQ0E=\n" +
                  " AKEIivg=\n" +
                  "MBIxEDAOBgNVBAMTB1Rlc3QgQ0E=\n" +
                  " X1o=\n" +
                  "YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy\n" +
                  " YW5vdGhlciBzZXJpYWwu\n" +
                  " c2VyaWFsMi4=";
    equal(contents, expected, "revocations.txt should be as expected");

    // Check the blocklisted intermediate now causes a failure
    let file = "tlsserver/test-int-ee.der";
    verify_cert(file, SEC_ERROR_REVOKED_CERTIFICATE);

    // Check the ee with the blocklisted root also causes a failure
    file = "tlsserver/other-issuer-ee.der";
    verify_cert(file, SEC_ERROR_REVOKED_CERTIFICATE);

    // Check the ee blocked by subject / pubKey causes a failure
    file = "tlsserver/same-issuer-ee.der";
    verify_cert(file, SEC_ERROR_REVOKED_CERTIFICATE);

    // Check a non-blocklisted chain still validates OK
    file = "tlsserver/default-ee.der";
    verify_cert(file, PRErrorCodeSuccess);

    // Check a bad cert is still bad (unknown issuer)
    file = "tlsserver/unknown-issuer.der";
    verify_cert(file, SEC_ERROR_UNKNOWN_ISSUER);

    // check that save with no further update is a no-op
    let lastModified = revocations.lastModifiedTime;
    // add an already existing entry
    certList.revokeCertByIssuerAndSerial("YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy",
                                         "c2VyaWFsMi4=");
    certList.saveEntries();
    let newModified = revocations.lastModifiedTime;
    equal(lastModified, newModified,
          "saveEntries with no modifications should not update the backing file");

    run_next_test();
  });

  // we need to start the async portions of the test
  run_next_test();
}
