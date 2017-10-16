/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "BulkKeyBundle",
];

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/util.js");

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
  this._hmacObj = null;
  this._sha256HMACHasher = null;
}
KeyBundle.prototype = {
  _encrypt: null,
  _encryptB64: null,
  _hmac: null,
  _hmacB64: null,
  _hmacObj: null,
  _sha256HMACHasher: null,

  equals: function equals(bundle) {
    return bundle &&
           (bundle.hmacKey == this.hmacKey) &&
           (bundle.encryptionKey == this.encryptionKey);
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
    this._hmacObj = value ? Utils.makeHMACKey(value) : null;
    this._sha256HMACHasher = value ? Utils.makeHMACHasher(
      Ci.nsICryptoHMAC.SHA256, this._hmacObj) : null;
  },

  get hmacKeyB64() {
    return this._hmacB64;
  },

  get hmacKeyObject() {
    return this._hmacObj;
  },

  get sha256HMACHasher() {
    return this._sha256HMACHasher;
  },

  /**
   * Populate this key pair with 2 new, randomly generated keys.
   */
  async generateRandom() {
    // Compute both at that same time
    let [generatedHMAC, generatedEncr] = await Promise.all([
      Weave.Crypto.generateRandomKey(),
      Weave.Crypto.generateRandomKey()
    ]);
    this.keyPairB64 = [generatedEncr, generatedHMAC];
  },

};

/**
 * Represents a KeyBundle associated with a collection.
 *
 * This is just a KeyBundle with a collection attached.
 */
this.BulkKeyBundle = function BulkKeyBundle(collection) {
  let log = Log.repository.getLogger("Sync.BulkKeyBundle");
  log.info("BulkKeyBundle being created for " + collection);
  KeyBundle.call(this);

  this._collection = collection;
};

BulkKeyBundle.prototype = {
  __proto__: KeyBundle.prototype,

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
    this.hmacKey       = value[1];
  },

  get keyPairB64() {
    return [this.encryptionKeyB64, this.hmacKeyB64];
  },

  set keyPairB64(value) {
    if (!Array.isArray(value) || value.length != 2) {
      throw new Error("BulkKeyBundle.keyPairB64 value must be an array of 2 " +
                      "keys.");
    }

    this.encryptionKey  = CommonUtils.safeAtoB(value[0]);
    this.hmacKey        = CommonUtils.safeAtoB(value[1]);
  },
};
