/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// These tests ensure content signatures are working correctly.

// First, we need to set up some data
const PREF_SIGNATURE_ROOT = "security.content.signature.root_hash";

const TEST_DATA_DIR = "test_content_signing/";

const ONECRL_NAME = "oneCRL-signer.mozilla.org";
const ABOUT_NEWTAB_NAME = "remotenewtab.content-signature.mozilla.org";
var VERIFICATION_HISTOGRAM = Services.telemetry
                                     .getHistogramById("CONTENT_SIGNATURE_VERIFICATION_STATUS");
var ERROR_HISTOGRAM = Services.telemetry
                              .getKeyedHistogramById("CONTENT_SIGNATURE_VERIFICATION_ERRORS");

function getSignatureVerifier() {
  return Cc["@mozilla.org/security/contentsignatureverifier;1"]
           .getService(Ci.nsIContentSignatureVerifier);
}

function setRoot(filename) {
  let cert = constructCertFromFile(filename);
  Services.prefs.setCharPref(PREF_SIGNATURE_ROOT, cert.sha256Fingerprint);
}

function getCertHash(name) {
  let cert = constructCertFromFile(`test_content_signing/${name}.pem`);
  return cert.sha256Fingerprint.replace(/:/g, "");
}

function loadChain(prefix, names) {
  let chain = [];
  for (let name of names) {
    let filename = `${prefix}_${name}.pem`;
    chain.push(readFile(do_get_file(filename)));
  }
  return chain;
}

function check_telemetry(expected_index, expected, expectedId) {
  for (let i = 0; i < 10; i++) {
    let expected_value = 0;
    if (i == expected_index) {
      expected_value = expected;
    }
    let errorSnapshot = ERROR_HISTOGRAM.snapshot();
    for (let k in errorSnapshot) {
      // We clear the histogram every time so there should be only this one
      // category.
      equal(k, expectedId);
      equal(errorSnapshot[k].values[i] || 0, expected_value);
    }
    equal(VERIFICATION_HISTOGRAM.snapshot().values[i] || 0, expected_value,
      "count " + i + ": " + VERIFICATION_HISTOGRAM.snapshot().values[i] +
      " expected " + expected_value);
  }
  VERIFICATION_HISTOGRAM.clear();
  ERROR_HISTOGRAM.clear();
}

add_task(async function run_test() {
  // set up some data
  const DATA = readFile(do_get_file(TEST_DATA_DIR + "test.txt"));
  const GOOD_SIGNATURE = "p384ecdsa=" +
      readFile(do_get_file(TEST_DATA_DIR + "test.txt.signature"))
      .trim();

  const BAD_SIGNATURE = "p384ecdsa=WqRXFQ7tnlVufpg7A-ZavXvWd2Zln0o4woHBy26C2r" +
                        "UWM4GJke4pE8ecHiXoi-7KnZXty6Pe3s4o3yAIyKDP9jUC52Ek1G" +
                        "q25j_X703nP5rk5gM1qz5Fe-qCWakPPl6L";

  let remoteNewTabChain = loadChain(TEST_DATA_DIR + "content_signing",
                                    ["remote_newtab_ee", "int", "root"]);

  let oneCRLChain = loadChain(TEST_DATA_DIR + "content_signing",
                              ["onecrl_ee", "int", "root"]);

  let oneCRLBadKeyChain = loadChain(TEST_DATA_DIR + "content_signing",
                                    ["onecrl_wrong_key_ee", "int", "root"]);

  let noSANChain = loadChain(TEST_DATA_DIR + "content_signing",
                             ["onecrl_no_SAN_ee", "int", "root"]);

  let expiredOneCRLChain = loadChain(TEST_DATA_DIR + "content_signing",
                                     ["onecrl_ee_expired", "int", "root"]);

  let notValidYetOneCRLChain = loadChain(TEST_DATA_DIR + "content_signing",
                                         ["onecrl_ee_not_valid_yet", "int",
                                          "root"]);

  // Check signature verification works without error before the root is set
  VERIFICATION_HISTOGRAM.clear();
  let chain1 = oneCRLChain.join("\n");
  let verifier = getSignatureVerifier();
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chain1, ONECRL_NAME),
     "Before the root is set, signatures should fail to verify but not throw.");
  // Check for generic chain building error.
  check_telemetry(6, 1, getCertHash("content_signing_onecrl_ee"));

  setRoot(TEST_DATA_DIR + "content_signing_root.pem");

  // Check good signatures from good certificates with the correct SAN
  ok(await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chain1, ONECRL_NAME),
     "A OneCRL signature should verify with the OneCRL chain");
  let chain2 = remoteNewTabChain.join("\n");
  ok(await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chain2,
                                                ABOUT_NEWTAB_NAME),
     "A newtab signature should verify with the newtab chain");
  // Check for valid signature
  check_telemetry(0, 2, getCertHash("content_signing_remote_newtab_ee"));

  // Check a bad signature when a good chain is provided
  chain1 = oneCRLChain.join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, BAD_SIGNATURE, chain1, ONECRL_NAME),
     "A bad signature should not verify");
  // Check for invalid signature
  check_telemetry(1, 1, getCertHash("content_signing_onecrl_ee"));

  // Check a good signature from cert with good SAN but a different key than the
  // one used to create the signature
  let badKeyChain = oneCRLBadKeyChain.join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, badKeyChain,
                                                 ONECRL_NAME),
     "A signature should not verify if the signing key is wrong");
  // Check for wrong key in cert.
  check_telemetry(9, 1, getCertHash("content_signing_onecrl_wrong_key_ee"));

  // Check a good signature from cert with good SAN but a different key than the
  // one used to create the signature (this time, an RSA key)
  let rsaKeyChain = oneCRLBadKeyChain.join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, rsaKeyChain,
                                                 ONECRL_NAME),
     "A signature should not verify if the signing key is wrong (RSA)");
  // Check for wrong key in cert.
  check_telemetry(9, 1, getCertHash("content_signing_onecrl_wrong_key_ee"));

  // Check a good signature from cert with good SAN but with chain missing root
  let missingRoot = [oneCRLChain[0], oneCRLChain[1]].join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, missingRoot,
                                                 ONECRL_NAME),
     "A signature should not verify if the chain is incomplete (missing root)");
  // Check for generic chain building error.
  check_telemetry(6, 1, getCertHash("content_signing_onecrl_ee"));

  // Check a good signature from cert with good SAN but with no path to root
  let missingInt = [oneCRLChain[0], oneCRLChain[2]].join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, missingInt,
                                                 ONECRL_NAME),
     "A signature should not verify if the chain is incomplete (missing int)");
  // Check for generic chain building error.
  check_telemetry(6, 1, getCertHash("content_signing_onecrl_ee"));

  // Check good signatures from good certificates with the wrong SANs
  chain1 = oneCRLChain.join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chain1,
                                                 ABOUT_NEWTAB_NAME),
     "A OneCRL signature should not verify if we require the newtab SAN");
  // Check for invalid EE cert.
  check_telemetry(7, 1, getCertHash("content_signing_onecrl_ee"));

  chain2 = remoteNewTabChain.join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chain2,
                                                 ONECRL_NAME),
     "A newtab signature should not verify if we require the OneCRL SAN");
  // Check for invalid EE cert.
  check_telemetry(7, 1, getCertHash("content_signing_remote_newtab_ee"));

  // Check good signatures with good chains with some other invalid names
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chain1, ""),
     "A signature should not verify if the SANs do not match an empty name");
  // Check for invalid EE cert.
  check_telemetry(7, 1, getCertHash("content_signing_onecrl_ee"));

  // Test expired certificate.
  let chainExpired = expiredOneCRLChain.join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chainExpired, ""),
     "A signature should not verify if the signing certificate is expired");
  // Check for expired cert.
  check_telemetry(4, 1, getCertHash("content_signing_onecrl_ee_expired"));

  // Test not valid yet certificate.
  let chainNotValidYet = notValidYetOneCRLChain.join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chainNotValidYet, ""),
     "A signature should not verify if the signing certificate is not valid yet");
  // Check for not yet valid cert.
  check_telemetry(5, 1, getCertHash("content_signing_onecrl_ee_not_valid_yet"));

  let relatedName = "subdomain." + ONECRL_NAME;
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chain1,
                                                 relatedName),
     "A signature should not verify if the SANs do not match a related name");

  let randomName = "\xb1\x9bU\x1c\xae\xaa3\x19H\xdb\xed\xa1\xa1\xe0\x81\xfb" +
                   "\xb2\x8f\x1cP\xe5\x8b\x9c\xc2s\xd3\x1f\x8e\xbbN";
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chain1, randomName),
     "A signature should not verify if the SANs do not match a random name");

  // check good signatures with chains that have strange or missing SANs
  chain1 = noSANChain.join("\n");
  ok(!await verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, chain1,
                                                 ONECRL_NAME),
     "A signature should not verify if the SANs do not match a supplied name");

  // Check malformed signature data
  chain1 = oneCRLChain.join("\n");
  let bad_signatures = [
    // wrong length
    "p384ecdsa=WqRXFQ7tnlVufpg7A-ZavXvWd2Zln0o4woHBy26C2rUWM4GJke4pE8ecHiXoi-" +
    "7KnZXty6Pe3s4o3yAIyKDP9jUC52Ek1Gq25j_X703nP5rk5gM1qz5Fe-qCWakPPl6L==",
    // incorrectly encoded
    "p384ecdsa='WqRXFQ7tnlVufpg7A-ZavXvWd2Zln0o4woHBy26C2rUWM4GJke4pE8ecHiXoi" +
    "-7KnZXty6Pe3s4o3yAIyKDP9jUC52Ek1Gq25j_X703nP5rk5gM1qz5Fe-qCWakPPl6L=",
    // missing directive
    "other_directive=WqRXFQ7tnlVufpg7A-ZavXvWd2Zln0o4woHBy26C2rUWM4GJke4pE8ec" +
    "HiXoi-7KnZXty6Pe3s4o3yAIyKDP9jUC52Ek1Gq25j_X703nP5rk5gM1qz5Fe-qCWakPPl6L",
    // actually sha256 with RSA
    "p384ecdsa=XS_jiQsS5qlzQyUKaA1nAnQn_OvxhvDfKybflB8Xe5gNH1wNmPGK1qN-jpeTfK" +
    "6ob3l3gCTXrsMnOXMeht0kPP3wLfVgXbuuO135pQnsv0c-ltRMWLe56Cm4S4Z6E7WWKLPWaj" +
    "jhAcG5dZxjffP9g7tuPP4lTUJztyc4d1z_zQZakEG7R0vN7P5_CaX9MiMzP4R7nC3H4Ba6yi" +
    "yjlGvsZwJ_C5zDQzWWs95czUbMzbDScEZ_7AWnidw91jZn-fUK3xLb6m-Zb_b4GAqZ-vnXIf" +
    "LpLB1Nzal42BQZn7i4rhAldYdcVvy7rOMlsTUb5Zz6vpVW9LCT9lMJ7Sq1xbU-0g==",
    ];
  for (let badSig of bad_signatures) {
    await Assert.rejects(verifier.asyncVerifyContentSignature(DATA, badSig, chain1, ONECRL_NAME),
                         /NS_ERROR/, `Bad or malformed signature "${badSig}" should be rejected`);
  }

  // Check malformed and missing certificate chain data
  let chainSuffix = [oneCRLChain[1], oneCRLChain[2]].join("\n");
  let badChains = [
    // no data
    "",
    // completely wrong data
    "blah blah \n blah",
    ];

  let badSections = [
    // data that looks like PEM but isn't
    "-----BEGIN CERTIFICATE-----\nBSsPRlYp5+gaFMRIczwUzaioRfteCjr94xyz0g==\n",
    "-----BEGIN CERTIFICATE-----\nBSsPRlYp5+gaFMRIczwUzaioRfteCjr94xyz0g==\n-----END CERTIFICATE-----",
    // data that will start to parse but won't base64decode
    "-----BEGIN CERTIFICATE-----\nnon-base64-stuff\n-----END CERTIFICATE-----",
    // data with garbage outside of PEM sections
    "this data is garbage\n-----BEGIN CERTIFICATE-----\nnon-base64-stuff\n" +
    "-----END CERTIFICATE-----",
    ];

  for (let badSection of badSections) {
    // ensure we test each bad section on its own...
    badChains.push(badSection);
    // ... and as part of a chain with good certificates
    badChains.push(badSection + "\n" + chainSuffix);
  }

  for (let badChain of badChains) {
    await Assert.rejects(verifier.asyncVerifyContentSignature(DATA, GOOD_SIGNATURE, badChain,
                                                              ONECRL_NAME),
                         /NS_ERROR/,
                         `Bad chain data starting "${badChain.substring(0, 80)}" ` +
                         "should be rejected");
  }

  ok(!await verifier.asyncVerifyContentSignature(DATA + "appended data", GOOD_SIGNATURE, chain1,
                                                 ONECRL_NAME),
     "A good signature should not verify if the data is tampered with (append)");
  ok(!await verifier.asyncVerifyContentSignature("prefixed data" + DATA, GOOD_SIGNATURE, chain1,
                                                 ONECRL_NAME),
     "A good signature should not verify if the data is tampered with (prefix)");
  ok(!await verifier.asyncVerifyContentSignature(DATA.replace(/e/g, "i"), GOOD_SIGNATURE, chain1,
                                                 ONECRL_NAME),
     "A good signature should not verify if the data is tampered with (modify)");
});
