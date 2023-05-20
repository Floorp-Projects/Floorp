/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { CommonUtils } from "resource://services-common/utils.sys.mjs";

import { Log } from "resource://gre/modules/Log.sys.mjs";

import { Weave } from "resource://services-sync/main.sys.mjs";

/**
 * Represents a pair of keys.
 *
 * Each key stored in a key bundle is 256 bits. One key is used for symmetric
 * encryption. The other is used for HMAC.
 *
 * A KeyBundle by itself is just an anonymous pair of keys. Other types
 * deriving from this one add semantics, such as associated collections or
 * generating a key bundle via HKDF from another key.
 */
function KeyBundle() {
  this._encrypt = null;
  this._encryptB64 = null;
  this._hmac = null;
  this._hmacB64 = null;
}
KeyBundle.prototype = {
  _encrypt: null,
  _encryptB64: null,
  _hmac: null,
  _hmacB64: null,

  equals: function equals(bundle) {
    return (
      bundle &&
      bundle.hmacKey == this.hmacKey &&
      bundle.encryptionKey == this.encryptionKey
    );
  },

  /*
   * Accessors for the two keys.
   */
  get encryptionKey() {
    return this._encrypt;
  },

  set encryptionKey(value) {
    if (!value || typeof value != "string") {
      throw new Error("Encryption key can only be set to string values.");
    }

    if (value.length < 16) {
      throw new Error("Encryption key must be at least 128 bits long.");
    }

    this._encrypt = value;
    this._encryptB64 = btoa(value);
  },

  get encryptionKeyB64() {
    return this._encryptB64;
  },

  get hmacKey() {
    return this._hmac;
  },

  set hmacKey(value) {
    if (!value || typeof value != "string") {
      throw new Error("HMAC key can only be set to string values.");
    }

    if (value.length < 16) {
      throw new Error("HMAC key must be at least 128 bits long.");
    }

    this._hmac = value;
    this._hmacB64 = btoa(value);
  },

  get hmacKeyB64() {
    return this._hmacB64;
  },

  /**
   * Populate this key pair with 2 new, randomly generated keys.
   */
  async generateRandom() {
    // Compute both at that same time
    let [generatedHMAC, generatedEncr] = await Promise.all([
      Weave.Crypto.generateRandomKey(),
      Weave.Crypto.generateRandomKey(),
    ]);
    this.keyPairB64 = [generatedEncr, generatedHMAC];
  },
};

/**
 * Represents a KeyBundle associated with a collection.
 *
 * This is just a KeyBundle with a collection attached.
 */
export function BulkKeyBundle(collection) {
  let log = Log.repository.getLogger("Sync.BulkKeyBundle");
  log.info("BulkKeyBundle being created for " + collection);
  KeyBundle.call(this);

  this._collection = collection;
}

BulkKeyBundle.fromHexKey = function (hexKey) {
  let key = CommonUtils.hexToBytes(hexKey);
  let bundle = new BulkKeyBundle();
  // [encryptionKey, hmacKey]
  bundle.keyPair = [key.slice(0, 32), key.slice(32, 64)];
  return bundle;
};

BulkKeyBundle.fromJWK = function (jwk) {
  if (!jwk || !jwk.k || jwk.kty !== "oct") {
    throw new Error("Invalid JWK provided to BulkKeyBundle.fromJWK");
  }
  return BulkKeyBundle.fromHexKey(CommonUtils.base64urlToHex(jwk.k));
};

BulkKeyBundle.prototype = {
  get collection() {
    return this._collection;
  },

  /**
   * Obtain the key pair in this key bundle.
   *
   * The returned keys are represented as raw byte strings.
   */
  get keyPair() {
    return [this.encryptionKey, this.hmacKey];
  },

  set keyPair(value) {
    if (!Array.isArray(value) || value.length != 2) {
      throw new Error("BulkKeyBundle.keyPair value must be array of 2 keys.");
    }

    this.encryptionKey = value[0];
    this.hmacKey = value[1];
  },

  get keyPairB64() {
    return [this.encryptionKeyB64, this.hmacKeyB64];
  },

  set keyPairB64(value) {
    if (!Array.isArray(value) || value.length != 2) {
      throw new Error(
        "BulkKeyBundle.keyPairB64 value must be an array of 2 keys."
      );
    }

    this.encryptionKey = CommonUtils.safeAtoB(value[0]);
    this.hmacKey = CommonUtils.safeAtoB(value[1]);
  },
};

Object.setPrototypeOf(BulkKeyBundle.prototype, KeyBundle.prototype);
