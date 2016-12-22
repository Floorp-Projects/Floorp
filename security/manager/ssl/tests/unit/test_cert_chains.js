// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

function build_cert_chain(certNames) {
  let certList = Cc["@mozilla.org/security/x509certlist;1"]
                   .createInstance(Ci.nsIX509CertList);
  certNames.forEach(function(certName) {
    let cert = constructCertFromFile("bad_certs/" + certName + ".pem");
    certList.addCert(cert);
  });
  return certList;
}

function test_cert_equals() {
  let certA = constructCertFromFile("bad_certs/default-ee.pem");
  let certB = constructCertFromFile("bad_certs/default-ee.pem");
  let certC = constructCertFromFile("bad_certs/expired-ee.pem");

  ok(certA != certB,
     "Cert objects constructed from the same file should not be equal" +
     " according to the equality operators");
  ok(certA.equals(certB),
     "equals() on cert objects constructed from the same cert file should" +
     " return true");
  ok(!certA.equals(certC),
     "equals() on cert objects constructed from files for different certs" +
     " should return false");
}

function test_cert_list_serialization() {
  let certList = build_cert_chain(["default-ee", "expired-ee"]);

  // Serialize the cert list to a string
  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"]
                    .getService(Ci.nsISerializationHelper);
  certList.QueryInterface(Ci.nsISerializable);
  let serialized = serHelper.serializeToString(certList);

  // Deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsIX509CertList);
  ok(certList.equals(deserialized),
     "Deserialized cert list should equal the original");
}

function test_security_info_serialization(securityInfo, expectedErrorCode) {
  // Serialize the securityInfo to a string
  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"]
                    .getService(Ci.nsISerializationHelper);
  let serialized = serHelper.serializeToString(securityInfo);

  // Deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsITransportSecurityInfo);
  equal(securityInfo.securityState, deserialized.securityState,
        "Original and deserialized security state should match");
  equal(securityInfo.errorMessage, deserialized.errorMessage,
        "Original and deserialized error message should match");
  equal(securityInfo.errorCode, expectedErrorCode,
        "Original and expected error code should match");
  equal(deserialized.errorCode, expectedErrorCode,
        "Deserialized and expected error code should match");
}

function run_test() {
  do_get_profile();
  add_tls_server_setup("BadCertServer", "bad_certs");

  // Test nsIX509Cert.equals
  add_test(function() {
    test_cert_equals();
    run_next_test();
  });

  // Test serialization of nsIX509CertList
  add_test(function() {
    test_cert_list_serialization();
    run_next_test();
  });

  // Test successful connection (failedCertChain should be null)
  add_connection_test(
    // re-use pinning certs (keeler)
    "good.include-subdomains.pinning.example.com", PRErrorCodeSuccess, null,
    function withSecurityInfo(aTransportSecurityInfo) {
      aTransportSecurityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(aTransportSecurityInfo, 0);
      equal(aTransportSecurityInfo.failedCertChain, null,
            "failedCertChain for a successful connection should be null");
    }
  );

  // Test overrideable connection failure (failedCertChain should be non-null)
  add_connection_test(
    "expired.example.com",
    SEC_ERROR_EXPIRED_CERTIFICATE,
    null,
    function withSecurityInfo(securityInfo) {
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(securityInfo, SEC_ERROR_EXPIRED_CERTIFICATE);
      notEqual(securityInfo.failedCertChain, null,
               "failedCertChain should not be null for an overrideable" +
               " connection failure");
      let originalCertChain = build_cert_chain(["expired-ee", "test-ca"]);
      ok(originalCertChain.equals(securityInfo.failedCertChain),
         "failedCertChain should equal the original cert chain for an" +
         " overrideable connection failure");
    }
  );

  // Test non-overrideable error (failedCertChain should be non-null)
  add_connection_test(
    "inadequatekeyusage.example.com",
    SEC_ERROR_INADEQUATE_KEY_USAGE,
    null,
    function withSecurityInfo(securityInfo) {
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(securityInfo, SEC_ERROR_INADEQUATE_KEY_USAGE);
      notEqual(securityInfo.failedCertChain, null,
               "failedCertChain should not be null for a non-overrideable" +
               " connection failure");
      let originalCertChain = build_cert_chain(["inadequatekeyusage-ee", "test-ca"]);
      ok(originalCertChain.equals(securityInfo.failedCertChain),
         "failedCertChain should equal the original cert chain for a" +
         " non-overrideable connection failure");
    }
  );

  run_next_test();
}
