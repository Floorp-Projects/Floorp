/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import('resource://gre/modules/identity/LogUtils.jsm');

const idService = Cc["@mozilla.org/identity/crypto-service;1"]
                    .getService(Ci.nsIIdentityCryptoService);

const ALG_DSA = "DS160";
const ALG_RSA = "RS256";

// When the output of an operation is a
function do_check_eq_or_slightly_less(x, y) {
  do_check_true(x >= y - (3 * 8));
}

function test_dsa() {
  idService.generateKeyPair(ALG_DSA, function (rv, keyPair) {
    log("DSA generateKeyPair finished ", rv);
    do_check_true(Components.isSuccessCode(rv));
    do_check_eq(typeof keyPair.sign, "function");
    do_check_eq(keyPair.keyType, ALG_DSA);
    do_check_eq_or_slightly_less(keyPair.hexDSAGenerator.length, 1024 / 8 * 2);
    do_check_eq_or_slightly_less(keyPair.hexDSAPrime.length, 1024 / 8 * 2);
    do_check_eq_or_slightly_less(keyPair.hexDSASubPrime.length, 160 / 8 * 2);
    do_check_eq_or_slightly_less(keyPair.hexDSAPublicValue.length, 1024 / 8 * 2);
    // XXX: test that RSA parameters throw the correct error

    log("about to sign with DSA key");
    keyPair.sign("foo", function (rv, signature) {
      log("DSA sign finished ", rv, signature);
      do_check_true(Components.isSuccessCode(rv));
      do_check_true(signature.length > 1);
      // TODO: verify the signature with the public key
      run_next_test();
    });
  });
}

function test_rsa() {
  idService.generateKeyPair(ALG_RSA, function (rv, keyPair) {
    log("RSA generateKeyPair finished ", rv);
    do_check_true(Components.isSuccessCode(rv));
    do_check_eq(typeof keyPair.sign, "function");
    do_check_eq(keyPair.keyType, ALG_RSA);
    do_check_eq_or_slightly_less(keyPair.hexRSAPublicKeyModulus.length,
                                 2048 / 8);
    do_check_true(keyPair.hexRSAPublicKeyExponent.length > 1);

    log("about to sign with RSA key");
    keyPair.sign("foo", function (rv, signature) {
      log("RSA sign finished ", rv, signature);
      do_check_true(Components.isSuccessCode(rv));
      do_check_true(signature.length > 1);
      run_next_test();
    });
  });
}

add_test(test_dsa);
add_test(test_rsa);

function run_test() {
  run_next_test();
}
