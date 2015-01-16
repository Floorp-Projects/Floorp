// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

function build_cert_chain(certNames) {
  let certList = Cc["@mozilla.org/security/x509certlist;1"]
                   .createInstance(Ci.nsIX509CertList);
  certNames.forEach(function(certName) {
    let cert = constructCertFromFile("tlsserver/" + certName + ".der");
    certList.addCert(cert);
  });
  return certList;
}

function test_cert_equals() {
  let certA = constructCertFromFile("tlsserver/default-ee.der");
  let certB = constructCertFromFile("tlsserver/default-ee.der");
  let certC = constructCertFromFile("tlsserver/expired-ee.der");

  do_check_false(certA == certB);
  do_check_true(certA.equals(certB));
  do_check_false(certA.equals(certC));
}

function test_cert_list_serialization() {
  let certList = build_cert_chain(['default-ee', 'expired-ee']);

  // Serialize the cert list to a string
  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"]
                    .getService(Ci.nsISerializationHelper);
  certList.QueryInterface(Ci.nsISerializable);
  let serialized = serHelper.serializeToString(certList);

  // Deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsIX509CertList);
  do_check_true(certList.equals(deserialized));
}

function test_security_info_serialization(securityInfo, expectedErrorCode) {
  // Serialize the securityInfo to a string
  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"]
                    .getService(Ci.nsISerializationHelper);
  let serialized = serHelper.serializeToString(securityInfo);

  // Deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsITransportSecurityInfo);
  do_check_eq(securityInfo.securityState, deserialized.securityState);
  do_check_eq(securityInfo.errorMessage, deserialized.errorMessage);
  do_check_eq(securityInfo.errorCode, expectedErrorCode);
  do_check_eq(deserialized.errorCode, expectedErrorCode);
}

function run_test() {
  do_get_profile();
  add_tls_server_setup("BadCertServer");

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
    "good.include-subdomains.pinning.example.com", Cr.NS_OK, null,
    function withSecurityInfo(aTransportSecurityInfo) {
      aTransportSecurityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(aTransportSecurityInfo, 0);
      do_check_eq(aTransportSecurityInfo.failedCertChain, null);
    }
  );

  // Test overrideable connection failure (failedCertChain should be non-null)
  add_connection_test(
    "expired.example.com",
    getXPCOMStatusFromNSS(SEC_ERROR_EXPIRED_CERTIFICATE),
    null,
    function withSecurityInfo(securityInfo) {
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(securityInfo, SEC_ERROR_EXPIRED_CERTIFICATE);
      do_check_neq(securityInfo.failedCertChain, null);
      let originalCertChain = build_cert_chain(["expired-ee", "test-ca"]);
      do_check_true(originalCertChain.equals(securityInfo.failedCertChain));
    }
  );

  // Test non-overrideable error (failedCertChain should be non-null)
  add_connection_test(
    "inadequatekeyusage.example.com",
    getXPCOMStatusFromNSS(SEC_ERROR_INADEQUATE_KEY_USAGE),
    null,
    function withSecurityInfo(securityInfo) {
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(securityInfo, SEC_ERROR_INADEQUATE_KEY_USAGE);
      do_check_neq(securityInfo.failedCertChain, null);
      let originalCertChain = build_cert_chain(["inadequatekeyusage-ee", "test-ca"]);
      do_check_true(originalCertChain.equals(securityInfo.failedCertChain));
    }
  );

  run_next_test();
}
