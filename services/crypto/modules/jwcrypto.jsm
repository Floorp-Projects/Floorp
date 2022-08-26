/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["jwcrypto"];

const ECDH_PARAMS = {
  name: "ECDH",
  namedCurve: "P-256",
};
const AES_PARAMS = {
  name: "AES-GCM",
  length: 256,
};
const AES_TAG_LEN = 128;
const AES_GCM_IV_SIZE = 12;
const UTF8_ENCODER = new TextEncoder();
const UTF8_DECODER = new TextDecoder();

class JWCrypto {
  /**
   * Encrypts the given data into a JWE using AES-256-GCM content encryption.
   *
   * This function implements a very small subset of the JWE encryption standard
   * from https://tools.ietf.org/html/rfc7516. The only supported content encryption
   * algorithm is enc="A256GCM" [1] and the only supported key encryption algorithm
   * is alg="ECDH-ES" [2].
   *
   * @param {Object} key Peer Public JWK.
   * @param {ArrayBuffer} data
   *
   * [1] https://tools.ietf.org/html/rfc7518#section-5.3
   * [2] https://tools.ietf.org/html/rfc7518#section-4.6
   *
   * @returns {Promise<String>}
   */
  async generateJWE(key, data) {
    // Generate an ephemeral key to use just for this encryption.
    // The public component gets embedded in the JWE header.
    const epk = await crypto.subtle.generateKey(ECDH_PARAMS, true, [
      "deriveKey",
    ]);
    const ownPublicJWK = await crypto.subtle.exportKey("jwk", epk.publicKey);
    // Remove properties added by our WebCrypto implementation but that aren't typically
    // used with JWE in the wild. This saves space in the resulting JWE, and makes it easier
    // to re-import the resulting JWK.
    delete ownPublicJWK.key_ops;
    delete ownPublicJWK.ext;
    let header = { alg: "ECDH-ES", enc: "A256GCM", epk: ownPublicJWK };
    // Import the peer's public key.
    const peerPublicKey = await crypto.subtle.importKey(
      "jwk",
      key,
      ECDH_PARAMS,
      false,
      ["deriveKey"]
    );
    if (key.hasOwnProperty("kid")) {
      header.kid = key.kid;
    }
    // Do ECDH agreement to get the content encryption key.
    const contentKey = await deriveECDHSharedAESKey(
      epk.privateKey,
      peerPublicKey,
      ["encrypt"]
    );
    // Encrypt with AES-GCM using the generated key.
    // Note that the IV is generated randomly, which *in general* is not safe to do with AES-GCM because
    // it's too short to guarantee uniqueness. But we know that the AES-GCM key itself is unique and will
    // only be used for this single encryption, making a random IV safe to use for this particular use-case.
    let iv = crypto.getRandomValues(new Uint8Array(AES_GCM_IV_SIZE));
    // Yes, additionalData is the byte representation of the base64 representation of the stringified header.
    const additionalData = UTF8_ENCODER.encode(
      ChromeUtils.base64URLEncode(UTF8_ENCODER.encode(JSON.stringify(header)), {
        pad: false,
      })
    );
    const encrypted = await crypto.subtle.encrypt(
      {
        name: "AES-GCM",
        iv,
        additionalData,
        tagLength: AES_TAG_LEN,
      },
      contentKey,
      data
    );
    // JWE needs the authentication tag as a separate string.
    const tagIdx = encrypted.byteLength - ((AES_TAG_LEN + 7) >> 3);
    let ciphertext = encrypted.slice(0, tagIdx);
    let tag = encrypted.slice(tagIdx);
    // JWE serialization in compact format.
    header = UTF8_ENCODER.encode(JSON.stringify(header));
    header = ChromeUtils.base64URLEncode(header, { pad: false });
    tag = ChromeUtils.base64URLEncode(tag, { pad: false });
    ciphertext = ChromeUtils.base64URLEncode(ciphertext, { pad: false });
    iv = ChromeUtils.base64URLEncode(iv, { pad: false });
    return `${header}..${iv}.${ciphertext}.${tag}`; // No CEK
  }

  /**
   * Decrypts the given JWE using AES-256-GCM content encryption into a byte array.
   * This function does the opposite of `JWCrypto.generateJWE`.
   * The only supported content encryption algorithm is enc="A256GCM" [1]
   * and the only supported key encryption algorithm is alg="ECDH-ES" [2].
   *
   * @param {"ECDH-ES"} algorithm
   * @param {CryptoKey} key Local private key
   *
   * [1] https://tools.ietf.org/html/rfc7518#section-5.3
   * [2] https://tools.ietf.org/html/rfc7518#section-4.6
   *
   * @returns {Promise<Uint8Array>}
   */
  async decryptJWE(jwe, key) {
    let [header, cek, iv, ciphertext, authTag] = jwe.split(".");
    const additionalData = UTF8_ENCODER.encode(header);
    header = JSON.parse(
      UTF8_DECODER.decode(
        ChromeUtils.base64URLDecode(header, { padding: "reject" })
      )
    );
    if (!!cek.length || header.enc !== "A256GCM" || header.alg !== "ECDH-ES") {
      throw new Error("Unknown algorithm.");
    }
    if ("apu" in header || "apv" in header) {
      throw new Error("apu and apv header values are not supported.");
    }
    const peerPublicKey = await crypto.subtle.importKey(
      "jwk",
      header.epk,
      ECDH_PARAMS,
      false,
      ["deriveKey"]
    );
    // Do ECDH agreement to get the content encryption key.
    const contentKey = await deriveECDHSharedAESKey(key, peerPublicKey, [
      "decrypt",
    ]);
    iv = ChromeUtils.base64URLDecode(iv, { padding: "reject" });
    ciphertext = new Uint8Array(
      ChromeUtils.base64URLDecode(ciphertext, { padding: "reject" })
    );
    authTag = new Uint8Array(
      ChromeUtils.base64URLDecode(authTag, { padding: "reject" })
    );
    const bundle = new Uint8Array([...ciphertext, ...authTag]);

    const decrypted = await crypto.subtle.decrypt(
      {
        name: "AES-GCM",
        iv,
        tagLength: AES_TAG_LEN,
        additionalData,
      },
      contentKey,
      bundle
    );
    return new Uint8Array(decrypted);
  }
}

/**
 * Do an ECDH agreement between a public and private key,
 * returning the derived encryption key as specced by
 * JWA RFC.
 * The raw ECDH secret is derived into a key using
 * Concat KDF, as defined in Section 5.8.1 of [NIST.800-56A].
 * @param {CryptoKey} privateKey
 * @param {CryptoKey} publicKey
 * @param {String[]} keyUsages See `SubtleCrypto.deriveKey` 5th paramater documentation.
 * @returns {Promise<CryptoKey>}
 */
async function deriveECDHSharedAESKey(privateKey, publicKey, keyUsages) {
  const params = { ...ECDH_PARAMS, ...{ public: publicKey } };
  const sharedKey = await crypto.subtle.deriveKey(
    params,
    privateKey,
    AES_PARAMS,
    true,
    keyUsages
  );
  // This is the NIST Concat KDF specialized to a specific set of parameters,
  // which basically turn it into a single application of SHA256.
  // The details are from the JWA RFC.
  let sharedKeyBytes = await crypto.subtle.exportKey("raw", sharedKey);
  sharedKeyBytes = new Uint8Array(sharedKeyBytes);
  const info = [
    "\x00\x00\x00\x07A256GCM", // 7-byte algorithm identifier
    "\x00\x00\x00\x00", // empty PartyUInfo
    "\x00\x00\x00\x00", // empty PartyVInfo
    "\x00\x00\x01\x00", // keylen == 256
  ].join("");
  const pkcs = `\x00\x00\x00\x01${String.fromCharCode.apply(
    null,
    sharedKeyBytes
  )}${info}`;
  const pkcsBuf = Uint8Array.from(
    Array.prototype.map.call(pkcs, c => c.charCodeAt(0))
  );
  const derivedKeyBytes = await crypto.subtle.digest(
    {
      name: "SHA-256",
    },
    pkcsBuf
  );
  return crypto.subtle.importKey(
    "raw",
    derivedKeyBytes,
    AES_PARAMS,
    false,
    keyUsages
  );
}

const jwcrypto = new JWCrypto();
