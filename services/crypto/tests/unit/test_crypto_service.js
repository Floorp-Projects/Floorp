/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Log.jsm");

const idService = Cc["@mozilla.org/identity/crypto-service;1"]
                    .getService(Ci.nsIIdentityCryptoService);

const ALG_DSA = "DS160";
const ALG_RSA = "RS256";

const BASE64_URL_ENCODINGS = [
  // The vectors from RFC 4648 are very silly, but we may as well include them.
  ["", ""],
  ["f", "Zg=="],
  ["fo", "Zm8="],
  ["foo", "Zm9v"],
  ["foob", "Zm9vYg=="],
  ["fooba", "Zm9vYmE="],
  ["foobar", "Zm9vYmFy"],

  // It's quite likely you could get a string like this in an assertion audience
  ["i-like-pie.com", "aS1saWtlLXBpZS5jb20="],

  // A few extra to be really sure
  ["andré@example.com", "YW5kcsOpQGV4YW1wbGUuY29t"],
  ["πόλλ' οἶδ' ἀλώπηξ, ἀλλ' ἐχῖνος ἓν μέγα",
   "z4DPjM67zrsnIM6_4by2zrQnIOG8gM67z47PgM63zr4sIOG8gM67zrsnIOG8kM-H4b-Wzr3Ov8-CIOG8k869IM68zq3Os86x"],
];

let log = Log.repository.getLogger("crypto.service.test");
(function() {
  let appender = new Log.DumpAppender();
  log.level = Log.Level.Debug;
  log.addAppender(appender);
  }
)();

// When the output of an operation is a
function do_check_eq_or_slightly_less(x, y) {
  Assert.ok(x >= y - (3 * 8));
}

function test_base64_roundtrip() {
  let message = "Attack at dawn!";
  let encoded = idService.base64UrlEncode(message);
  let decoded = base64UrlDecode(encoded);
  Assert.notEqual(message, encoded);
  Assert.equal(decoded, message);
  run_next_test();
}

function test_dsa() {
  idService.generateKeyPair(ALG_DSA, function(rv, keyPair) {
    log.debug("DSA generateKeyPair finished " + rv);
    Assert.ok(Components.isSuccessCode(rv));
    Assert.equal(typeof keyPair.sign, "function");
    Assert.equal(keyPair.keyType, ALG_DSA);
    do_check_eq_or_slightly_less(keyPair.hexDSAGenerator.length, 1024 / 8 * 2);
    do_check_eq_or_slightly_less(keyPair.hexDSAPrime.length, 1024 / 8 * 2);
    do_check_eq_or_slightly_less(keyPair.hexDSASubPrime.length, 160 / 8 * 2);
    do_check_eq_or_slightly_less(keyPair.hexDSAPublicValue.length, 1024 / 8 * 2);
    // XXX: test that RSA parameters throw the correct error

    log.debug("about to sign with DSA key");
    keyPair.sign("foo", function(rv2, signature) {
      log.debug("DSA sign finished " + rv2 + " " + signature);
      Assert.ok(Components.isSuccessCode(rv2));
      Assert.ok(signature.length > 1);
      // TODO: verify the signature with the public key
      run_next_test();
    });
  });
}

function test_rsa() {
  idService.generateKeyPair(ALG_RSA, function(rv, keyPair) {
    log.debug("RSA generateKeyPair finished " + rv);
    Assert.ok(Components.isSuccessCode(rv));
    Assert.equal(typeof keyPair.sign, "function");
    Assert.equal(keyPair.keyType, ALG_RSA);
    do_check_eq_or_slightly_less(keyPair.hexRSAPublicKeyModulus.length,
                                 2048 / 8);
    Assert.ok(keyPair.hexRSAPublicKeyExponent.length > 1);

    log.debug("about to sign with RSA key");
    keyPair.sign("foo", function(rv2, signature) {
      log.debug("RSA sign finished " + rv2 + " " + signature);
      Assert.ok(Components.isSuccessCode(rv2));
      Assert.ok(signature.length > 1);
      run_next_test();
    });
  });
}

function test_base64UrlEncode() {
  for (let [source, target] of BASE64_URL_ENCODINGS) {
    Assert.equal(target, idService.base64UrlEncode(source));
  }
  run_next_test();
}

function test_base64UrlDecode() {
  let utf8Converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                        .createInstance(Ci.nsIScriptableUnicodeConverter);
  utf8Converter.charset = "UTF-8";

  // We know the encoding of our inputs - on conversion back out again, make
  // sure they're the same.
  for (let [source, target] of BASE64_URL_ENCODINGS) {
    let result = utf8Converter.ConvertToUnicode(base64UrlDecode(target));
    result += utf8Converter.Finish();
    Assert.equal(source, result);
  }
  run_next_test();
}

add_test(test_base64_roundtrip);
add_test(test_dsa);
add_test(test_rsa);
add_test(test_base64UrlEncode);
add_test(test_base64UrlDecode);
