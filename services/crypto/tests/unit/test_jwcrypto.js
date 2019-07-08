/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "jwcrypto",
  "resource://services-crypto/jwcrypto.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "CryptoService",
  "@mozilla.org/identity/crypto-service;1",
  "nsIIdentityCryptoService"
);

Cu.importGlobalProperties(["crypto"]);

const RP_ORIGIN = "http://123done.org";
const INTERNAL_ORIGIN = "browserid://";

const SECOND_MS = 1000;
const MINUTE_MS = SECOND_MS * 60;
const HOUR_MS = MINUTE_MS * 60;

// Enable logging from jwcrypto.jsm.
Services.prefs.setCharPref("services.crypto.jwcrypto.log.level", "Debug");

function promisify(fn) {
  return (...args) => {
    return new Promise((res, rej) => {
      fn(...args, (err, result) => {
        err ? rej(err) : res(result);
      });
    });
  };
}
const generateKeyPair = promisify(jwcrypto.generateKeyPair);
const generateAssertion = promisify(jwcrypto.generateAssertion);

add_task(async function test_jwe_roundtrip_ecdh_es_encryption() {
  const data = crypto.getRandomValues(new Uint8Array(123));
  const localEpk = await crypto.subtle.generateKey(
    {
      name: "ECDH",
      namedCurve: "P-256",
    },
    true,
    ["deriveKey"]
  );
  const remoteEpk = await crypto.subtle.generateKey(
    {
      name: "ECDH",
      namedCurve: "P-256",
    },
    true,
    ["deriveKey"]
  );
  const jwe = await jwcrypto._generateJWE(localEpk, remoteEpk.publicKey, data);
  const decryptedJWE = await jwcrypto.decryptJWE(jwe, remoteEpk.privateKey);
  Assert.deepEqual(data, decryptedJWE);
});

add_task(async function test_sanity() {
  // Shouldn't reject.
  await generateKeyPair("DS160");
});

add_task(async function test_generate() {
  let kp = await generateKeyPair("DS160");
  Assert.notEqual(kp, null);
});

add_task(async function test_get_assertion() {
  let kp = await generateKeyPair("DS160");
  let backedAssertion = await generateAssertion("fake-cert", kp, RP_ORIGIN);
  Assert.equal(backedAssertion.split("~").length, 2);
  Assert.equal(backedAssertion.split(".").length, 3);
});

add_task(async function test_rsa() {
  let kpo = await generateKeyPair("RS256");
  Assert.notEqual(kpo, undefined);
  info(kpo.serializedPublicKey);
  let pk = JSON.parse(kpo.serializedPublicKey);
  Assert.equal(pk.algorithm, "RS");
  /* TODO
  do_check_neq(kpo.sign, null);
  do_check_eq(typeof kpo.sign, "function");
  do_check_neq(kpo.userID, null);
  do_check_neq(kpo.url, null);
  do_check_eq(kpo.url, INTERNAL_ORIGIN);
  do_check_neq(kpo.exponent, null);
  do_check_neq(kpo.modulus, null);

  // TODO: should sign be async?
  let sig = kpo.sign("This is a message to sign");

  do_check_neq(sig, null);
  do_check_eq(typeof sig, "string");
  do_check_true(sig.length > 1);
  */
});

add_task(async function test_dsa() {
  let kpo = await generateKeyPair("DS160");
  info(kpo.serializedPublicKey);
  let pk = JSON.parse(kpo.serializedPublicKey);
  Assert.equal(pk.algorithm, "DS");
  /* TODO
  do_check_neq(kpo.sign, null);
  do_check_eq(typeof kpo.sign, "function");
  do_check_neq(kpo.userID, null);
  do_check_neq(kpo.url, null);
  do_check_eq(kpo.url, INTERNAL_ORIGIN);
  do_check_neq(kpo.generator, null);
  do_check_neq(kpo.prime, null);
  do_check_neq(kpo.subPrime, null);
  do_check_neq(kpo.publicValue, null);

  let sig = kpo.sign("This is a message to sign");

  do_check_neq(sig, null);
  do_check_eq(typeof sig, "string");
  do_check_true(sig.length > 1);
  */
});

add_task(async function test_get_assertion_with_offset() {
  // Use an arbitrary date in the past to ensure we don't accidentally pass
  // this test with current dates, missing offsets, etc.
  let serverMsec = Date.parse("Tue Oct 31 2000 00:00:00 GMT-0800");

  // local clock skew
  // clock is 12 hours fast; -12 hours offset must be applied
  let localtimeOffsetMsec = -1 * 12 * HOUR_MS;
  let localMsec = serverMsec - localtimeOffsetMsec;

  let kp = await generateKeyPair("DS160");
  let backedAssertion = await generateAssertion("fake-cert", kp, RP_ORIGIN, {
    duration: MINUTE_MS,
    localtimeOffsetMsec,
    now: localMsec,
  });
  // properly formed
  let cert;
  let assertion;
  [cert, assertion] = backedAssertion.split("~");

  Assert.equal(cert, "fake-cert");
  Assert.equal(assertion.split(".").length, 3);

  let components = extractComponents(assertion);

  // Expiry is within two minutes, corrected for skew
  let exp = parseInt(components.payload.exp, 10);
  Assert.ok(exp - serverMsec === MINUTE_MS);
});

add_task(async function test_assertion_lifetime() {
  let kp = await generateKeyPair("DS160");
  let backedAssertion = await generateAssertion("fake-cert", kp, RP_ORIGIN, {
    duration: MINUTE_MS,
  });
  // properly formed
  let cert;
  let assertion;
  [cert, assertion] = backedAssertion.split("~");

  Assert.equal(cert, "fake-cert");
  Assert.equal(assertion.split(".").length, 3);

  let components = extractComponents(assertion);

  // Expiry is within one minute, as we specified above
  let exp = parseInt(components.payload.exp, 10);
  Assert.ok(Math.abs(Date.now() - exp) > 50 * SECOND_MS);
  Assert.ok(Math.abs(Date.now() - exp) <= MINUTE_MS);
});

add_task(async function test_audience_encoding_bug972582() {
  let audience = "i-like-pie.com";
  let kp = await generateKeyPair("DS160");
  let backedAssertion = await generateAssertion("fake-cert", kp, audience);
  let [, /* cert */ assertion] = backedAssertion.split("~");
  let components = extractComponents(assertion);
  Assert.equal(components.payload.aud, audience);
});

function extractComponents(signedObject) {
  if (typeof signedObject != "string") {
    throw new Error("malformed signature " + typeof signedObject);
  }

  let parts = signedObject.split(".");
  if (parts.length != 3) {
    throw new Error(
      "signed object must have three parts, this one has " + parts.length
    );
  }

  let headerSegment = parts[0];
  let payloadSegment = parts[1];
  let cryptoSegment = parts[2];

  let header = JSON.parse(base64UrlDecode(headerSegment));
  let payload = JSON.parse(base64UrlDecode(payloadSegment));

  // Ensure well-formed header
  Assert.equal(Object.keys(header).length, 1);
  Assert.ok(!!header.alg);

  // Ensure well-formed payload
  for (let field of ["exp", "aud"]) {
    Assert.ok(!!payload[field]);
  }

  return { header, payload, headerSegment, payloadSegment, cryptoSegment };
}
