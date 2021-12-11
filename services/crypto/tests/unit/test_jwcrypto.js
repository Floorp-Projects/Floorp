/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "jwcrypto",
  "resource://services-crypto/jwcrypto.jsm"
);

Cu.importGlobalProperties(["crypto"]);

const SECOND_MS = 1000;
const MINUTE_MS = SECOND_MS * 60;
const HOUR_MS = MINUTE_MS * 60;

// Enable logging from jwcrypto.jsm.
Services.prefs.setCharPref("services.crypto.jwcrypto.log.level", "Debug");

add_task(async function test_jwe_roundtrip_ecdh_es_encryption() {
  const plaintext = crypto.getRandomValues(new Uint8Array(123));
  const remoteKey = await crypto.subtle.generateKey(
    {
      name: "ECDH",
      namedCurve: "P-256",
    },
    true,
    ["deriveKey"]
  );
  const remoteJWK = await crypto.subtle.exportKey("jwk", remoteKey.publicKey);
  delete remoteJWK.key_ops;
  const jwe = await jwcrypto.generateJWE(remoteJWK, plaintext);
  const decrypted = await jwcrypto.decryptJWE(jwe, remoteKey.privateKey);
  Assert.deepEqual(plaintext, decrypted);
});

add_task(async function test_jwe_header_includes_key_id() {
  const plaintext = crypto.getRandomValues(new Uint8Array(123));
  const remoteKey = await crypto.subtle.generateKey(
    {
      name: "ECDH",
      namedCurve: "P-256",
    },
    true,
    ["deriveKey"]
  );
  const remoteJWK = await crypto.subtle.exportKey("jwk", remoteKey.publicKey);
  delete remoteJWK.key_ops;
  remoteJWK.kid = "key identifier";
  const jwe = await jwcrypto.generateJWE(remoteJWK, plaintext);
  let [header /* other items deliberately ignored */] = jwe.split(".");
  header = JSON.parse(
    new TextDecoder().decode(
      ChromeUtils.base64URLDecode(header, { padding: "reject" })
    )
  );
  Assert.equal(header.kid, "key identifier");
});
