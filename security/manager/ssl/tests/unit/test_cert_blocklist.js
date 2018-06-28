/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test checks a number of things:
// * it ensures that data loaded from revocations.txt on startup is present
// * it ensures that data served from OneCRL are persisted correctly
// * it ensures that items in the CertBlocklist are seen as revoked by the
//   cert verifier
// * it does a sanity check to ensure other cert verifier behavior is
//   unmodified

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm", {});

// First, we need to setup appInfo for the blocklist service to work
var id = "xpcshell@tests.mozilla.org";
var appName = "XPCShell";
var version = "1";
var platformVersion = "1.9.2";
ChromeUtils.import("resource://testing-common/AppInfo.jsm", this);
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

var gRevocations = gProfile.clone();
gRevocations.append("revocations.txt");
if (!gRevocations.exists()) {
  let existing = do_get_file("test_onecrl/sample_revocations.txt", false);
  existing.copyTo(gProfile, "revocations.txt");
}

var certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

// set up a test server to serve the kinto views.
var testserver = new HttpServer();


const kintoHelloViewJSON = `{"settings":{"batch_max_requests":25}}`;
const kintoChangesJSON = `{
  "data": [
    {
      "host": "firefox.settings.services.mozilla.com",
      "id": "3ace9d8e-00b5-a353-7fd5-1f081ff482ba",
      "last_modified": 100000000000000000001,
      "bucket": "blocklists",
      "collection": "certificates"
    }
  ]
}`;
const certMetadataJSON = `{"data": {}}`;
const certBlocklistJSON = `{
  "data": [` +
  // test with some bad data ...
  ` {
      "id": "1",
      "last_modified": 100000000000000000001,
      "issuerName": "Some nonsense in issuer",
      "serialNumber": "AkHVNA=="
    },
    {
      "id": "2",
      "last_modified": 100000000000000000002,
      "issuerName": "MA0xCzAJBgNVBAMMAmNh",
      "serialNumber": "some nonsense in serial"
    },
    {
      "id": "3",
      "last_modified": 100000000000000000003,
      "issuerName": "and serial",
      "serialNumber": "some nonsense in both issuer"
    },` +
  // some mixed
  // In these case, the issuer name and the valid serialNumber correspond
  // to test-int.pem in bad_certs/
  ` {
      "id": "4",
      "last_modified": 100000000000000000004,
      "issuerName": "MBIxEDAOBgNVBAMMB1Rlc3QgQ0E=",
      "serialNumber": "oops! more nonsense."
    },` +
  ` {
      "id": "5",
      "last_modified": 100000000000000000004,
      "issuerName": "MBIxEDAOBgNVBAMMB1Rlc3QgQ0E=",
      "serialNumber": "a0X7/7DlTaedpgrIJg25iBPOkIM="
    },` +
  // ... and some good
  // In this case, the issuer name and the valid serialNumber correspond
  // to other-test-ca.pem in bad_certs/ (for testing root revocation)
  ` {
      "id": "6",
      "last_modified": 100000000000000000005,
      "issuerName": "MBgxFjAUBgNVBAMMDU90aGVyIHRlc3QgQ0E=",
      "serialNumber": "Rym6o+VN9xgZXT/QLrvN/nv1ZN4="
    },` +
  // These items correspond to an entry in sample_revocations.txt where:
  // isser name is "another imaginary issuer" base-64 encoded, and
  // serialNumbers are:
  // "serial2." base-64 encoded, and
  // "another serial." base-64 encoded
  // We need this to ensure that existing items are retained if they're
  // also in the blocklist
  ` {
      "id": "7",
      "last_modified": 100000000000000000006,
      "issuerName": "YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy",
      "serialNumber": "c2VyaWFsMi4="
    },` +
  ` {
      "id": "8",
      "last_modified": 100000000000000000006,
      "issuerName": "YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy",
      "serialNumber": "YW5vdGhlciBzZXJpYWwu"
    },` +
  // This item revokes same-issuer-ee.pem by subject and pubKeyHash.
  ` {
      "id": "9",
      "last_modified": 100000000000000000007,
      "subject": "MCIxIDAeBgNVBAMMF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5",
      "pubKeyHash": "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8="
    }
  ]
}`;

function serveResponse(body) {
  return (req, response) => {
    response.setStatusLine(null, 200, "OK");
    response.write(body);
  };
}

testserver.registerPathHandler("/v1/",
                               serveResponse(kintoHelloViewJSON));
testserver.registerPathHandler("/v1/buckets/monitor/collections/changes/records",
                               serveResponse(kintoChangesJSON));
testserver.registerPathHandler("/v1/buckets/blocklists/collections/certificates",
                               serveResponse(certMetadataJSON));
testserver.registerPathHandler("/v1/buckets/blocklists/collections/certificates/records",
                               serveResponse(certBlocklistJSON));

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
  return checkCertErrorGeneric(certDB, ee, expectedError,
                               certificateUsageSSLServer);
}

// The certificate blocklist currently only applies to TLS server certificates.
async function verify_non_tls_usage_succeeds(file) {
  let ee = constructCertFromFile(file);
  await checkCertErrorGeneric(certDB, ee, PRErrorCodeSuccess,
                              certificateUsageSSLClient);
  await checkCertErrorGeneric(certDB, ee, PRErrorCodeSuccess,
                              certificateUsageEmailSigner);
  await checkCertErrorGeneric(certDB, ee, PRErrorCodeSuccess,
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

function fetch_blocklist() {
  Services.prefs.setBoolPref("services.settings.load_dump", false);
  Services.prefs.setBoolPref("services.settings.verify_signature", false);
  Services.prefs.setCharPref("services.settings.server",
                             `http://localhost:${port}/v1`);
  Services.prefs.setCharPref("extensions.blocklist.url",
                              `http://localhost:${port}/blocklist.xml`);
  let blocklist = Cc["@mozilla.org/extensions/blocklist;1"]
                    .getService(Ci.nsITimerCallback);

  return new Promise((resolve) => {
    const e = "remote-settings-changes-polled";
    const changesPolledObserver = {
      observe(aSubject, aTopic, aData) {
        Services.obs.removeObserver(this, e);
        resolve();
      }
    };
    Services.obs.addObserver(changesPolledObserver, e);
    blocklist.notify(null);
  });
}

function* generate_revocations_txt_lines() {
  let profile = do_get_profile();
  let revocations = profile.clone();
  revocations.append("revocations.txt");
  ok(revocations.exists(), "the revocations file should exist");
  let inputStream = Cc["@mozilla.org/network/file-input-stream;1"]
                      .createInstance(Ci.nsIFileInputStream);
  inputStream.init(revocations, -1, -1, 0);
  inputStream.QueryInterface(Ci.nsILineInputStream);
  let hasmore = false;
  do {
    let line = {};
    hasmore = inputStream.readLine(line);
    yield line.value;
  } while (hasmore);
}

// Check that revocations.txt contains, in any order, the lines
// ("top-level lines") that are the keys in |expected|, each followed
// immediately by the lines ("sublines") in expected[topLevelLine]
// (again, in any order).
function check_revocations_txt_contents(expected) {
  let lineGenerator = generate_revocations_txt_lines();
  let firstLine = lineGenerator.next();
  equal(firstLine.done, false,
        "first line of revocations.txt should be present");
  equal(firstLine.value, "# Auto generated contents. Do not edit.",
        "first line of revocations.txt");
  let line = lineGenerator.next();
  let topLevelFound = {};
  while (true) {
    if (line.done) {
      break;
    }

    ok(line.value in expected,
       `${line.value} should be an expected top-level line in revocations.txt`);
    ok(!(line.value in topLevelFound),
       `should not have seen ${line.value} before in revocations.txt`);
    topLevelFound[line.value] = true;
    let topLevelLine = line.value;

    let sublines = expected[line.value];
    let subFound = {};
    while (true) {
      line = lineGenerator.next();
      if (line.done || !(line.value in sublines)) {
        break;
      }
      ok(!(line.value in subFound),
         `should not have seen ${line.value} before in revocations.txt`);
      subFound[line.value] = true;
    }
    for (let subline in sublines) {
      ok(subFound[subline],
         `should have found ${subline} below ${topLevelLine} in revocations.txt`);
    }
  }
  for (let topLevelLine in expected) {
    ok(topLevelFound[topLevelLine],
       `should have found ${topLevelLine} in revocations.txt`);
  }
}

function run_test() {
  // import the certificates we need
  load_cert("test-ca", "CTu,CTu,CTu");
  load_cert("test-int", ",,");
  load_cert("other-test-ca", "CTu,CTu,CTu");

  let certList = Cc["@mozilla.org/security/certblocklist;1"]
                  .getService(Ci.nsICertBlocklist);

  let expected = { "MCIxIDAeBgNVBAMMF0Fub3RoZXIgVGVzdCBFbmQtZW50aXR5":
                     { "\tVCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=": true },
                   "MBgxFjAUBgNVBAMMDU90aGVyIHRlc3QgQ0E=":
                     { " Rym6o+VN9xgZXT/QLrvN/nv1ZN4=": true},
                   "MBIxEDAOBgNVBAMMB1Rlc3QgQ0E=":
                     { " a0X7/7DlTaedpgrIJg25iBPOkIM=": true},
                   "YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy":
                     { " YW5vdGhlciBzZXJpYWwu": true,
                       " c2VyaWFsMi4=": true }
                 };

  add_task(async function() {
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
    await verify_cert(file, PRErrorCodeSuccess);

    // The blocklist also revokes other-test-ca.pem, which issued
    // other-ca-ee.pem. Check the cert validates before we load the blocklist
    file = "bad_certs/other-issuer-ee.pem";
    await verify_cert(file, PRErrorCodeSuccess);

    // The blocklist will revoke same-issuer-ee.pem via subject / pubKeyHash.
    // Check the cert validates before we load the blocklist
    file = "test_onecrl/same-issuer-ee.pem";
    await verify_cert(file, PRErrorCodeSuccess);
  });

  // blocklist load is async so we must use add_test from here
  add_task(fetch_blocklist);

  add_task(async function() {
    // The blocklist will be loaded now. Let's check the data is sane.
    // In particular, we should still have the revoked issuer / serial pair
    // that was in both revocations.txt and the blocklist.
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
    await verify_cert(file, SEC_ERROR_REVOKED_CERTIFICATE);
    await verify_non_tls_usage_succeeds(file);

    // Check the ee with the blocklisted root also causes a failure
    file = "bad_certs/other-issuer-ee.pem";
    await verify_cert(file, SEC_ERROR_REVOKED_CERTIFICATE);
    await verify_non_tls_usage_succeeds(file);

    // Check the ee blocked by subject / pubKey causes a failure
    file = "test_onecrl/same-issuer-ee.pem";
    await verify_cert(file, SEC_ERROR_REVOKED_CERTIFICATE);
    await verify_non_tls_usage_succeeds(file);

    // Check a non-blocklisted chain still validates OK
    file = "bad_certs/default-ee.pem";
    await verify_cert(file, PRErrorCodeSuccess);

    // Check a bad cert is still bad (unknown issuer)
    file = "bad_certs/unknownissuer.pem";
    await verify_cert(file, SEC_ERROR_UNKNOWN_ISSUER);

    // check that save with no further update is a no-op
    let lastModified = gRevocations.lastModifiedTime;
    // add an already existing entry
    certList.revokeCertByIssuerAndSerial("YW5vdGhlciBpbWFnaW5hcnkgaXNzdWVy",
                                         "c2VyaWFsMi4=");
    certList.saveEntries();
    let newModified = gRevocations.lastModifiedTime;
    equal(lastModified, newModified,
          "saveEntries with no modifications should not update the backing file");
  });

  add_test(function() {
    // Check the blocklist entry has not changed
    check_revocations_txt_contents(expected);
    run_next_test();
  });

  run_next_test();
}
