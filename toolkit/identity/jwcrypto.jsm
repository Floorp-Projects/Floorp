/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";


const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/identity/LogUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "IdentityCryptoService",
                                   "@mozilla.org/identity/crypto-service;1",
                                   "nsIIdentityCryptoService");

this.EXPORTED_SYMBOLS = ["jwcrypto"];

const ALGORITHMS = { RS256: "RS256", DS160: "DS160" };
const DURATION_MS = 1000 * 60 * 2; // 2 minutes default assertion lifetime

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["jwcrypto"].concat(aMessageArgs));
}

function generateKeyPair(aAlgorithmName, aCallback) {
  log("Generate key pair; alg =", aAlgorithmName);

  IdentityCryptoService.generateKeyPair(aAlgorithmName, function(rv, aKeyPair) {
    if (!Components.isSuccessCode(rv)) {
      return aCallback("key generation failed");
    }

    var publicKey;

    switch (aKeyPair.keyType) {
     case ALGORITHMS.RS256:
      publicKey = {
        algorithm: "RS",
        exponent:  aKeyPair.hexRSAPublicKeyExponent,
        modulus:   aKeyPair.hexRSAPublicKeyModulus
      };
      break;

     case ALGORITHMS.DS160:
      publicKey = {
        algorithm: "DS",
        y: aKeyPair.hexDSAPublicValue,
        p: aKeyPair.hexDSAPrime,
        q: aKeyPair.hexDSASubPrime,
        g: aKeyPair.hexDSAGenerator
      };
      break;

    default:
      return aCallback("unknown key type");
    }

    let keyWrapper = {
      serializedPublicKey: JSON.stringify(publicKey),
      _kp: aKeyPair
    };

    return aCallback(null, keyWrapper);
  });
}

function sign(aPayload, aKeypair, aCallback) {
  aKeypair._kp.sign(aPayload, function(rv, signature) {
    if (!Components.isSuccessCode(rv)) {
      log("ERROR: signer.sign failed");
      return aCallback("Sign failed");
    }
    log("signer.sign: success");
    return aCallback(null, signature);
  });
}

function jwcryptoClass()
{
}

jwcryptoClass.prototype = {
  /*
   * Determine the expiration of the assertion.  Returns expiry date
   * in milliseconds as integer.
   *
   * @param localtimeOffsetMsec (optional)
   *        The number of milliseconds that must be added to the local clock
   *        for it to agree with the server.  For example, if the local clock
   *        if two minutes fast, localtimeOffsetMsec would be -120000
   *
   * @param now (options)
   *        Current date in milliseconds.  Useful for mocking clock
   *        skew in testing.
   */
  getExpiration: function(duration=DURATION_MS, localtimeOffsetMsec=0, now=Date.now()) {
    return now + localtimeOffsetMsec + duration;
  },

  isCertValid: function(aCert, aCallback) {
    // XXX check expiration, bug 769850
    aCallback(true);
  },

  generateKeyPair: function(aAlgorithmName, aCallback) {
    log("generating");
    generateKeyPair(aAlgorithmName, aCallback);
  },

  /*
   * Generate an assertion and return it through the provided callback.
   *
   * @param aCert
   *        Identity certificate
   *
   * @param aKeyPair
   *        KeyPair object
   *
   * @param aAudience
   *        Audience of the assertion
   *
   * @param aOptions (optional)
   *        Can include:
   *        {
   *          localtimeOffsetMsec: <clock offset in milliseconds>,
   *          now: <current date in milliseconds>
   *          duration: <validity duration for this assertion in milliseconds>
   *        }
   *
   *        localtimeOffsetMsec is the number of milliseconds that need to be
   *        added to the local clock time to make it concur with the server.
   *        For example, if the local clock is two minutes fast, the offset in
   *        milliseconds would be -120000.
   *
   * @param aCallback
   *        Function to invoke with resulting assertion.  Assertion
   *        will be string or null on failure.
   */
  generateAssertion: function(aCert, aKeyPair, aAudience, aOptions, aCallback) {
    if (typeof aOptions == "function") {
      aCallback = aOptions;
      aOptions = { };
    }

    // for now, we hack the algorithm name
    // XXX bug 769851
    var header = {"alg": "DS128"};
    var headerBytes = IdentityCryptoService.base64UrlEncode(
                          JSON.stringify(header));

    var payload = {
      exp: this.getExpiration(
               aOptions.duration, aOptions.localtimeOffsetMsec, aOptions.now),
      aud: aAudience
    };
    var payloadBytes = IdentityCryptoService.base64UrlEncode(
                          JSON.stringify(payload));

    log("payload bytes", payload, payloadBytes);
    sign(headerBytes + "." + payloadBytes, aKeyPair, function(err, signature) {
      if (err)
        return aCallback(err);

      var signedAssertion = headerBytes + "." + payloadBytes + "." + signature;
      return aCallback(null, aCert + "~" + signedAssertion);
    });
  }

};

this.jwcrypto = new jwcryptoClass();
this.jwcrypto.ALGORITHMS = ALGORITHMS;
