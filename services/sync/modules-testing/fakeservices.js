/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "FakeCryptoService",
  "FakeFilesystemService",
  "FakeGUIDService",
  "fakeSHA256HMAC",
];

const {utils: Cu} = Components;

Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");

let btoa = Cu.import("resource://services-common/log4moz.js").btoa;

this.FakeFilesystemService = function FakeFilesystemService(contents) {
  this.fakeContents = contents;
  let self = this;

  Utils.jsonSave = function jsonSave(filePath, that, obj, callback) {
    let json = typeof obj == "function" ? obj.call(that) : obj;
    self.fakeContents["weave/" + filePath + ".json"] = JSON.stringify(json);
    callback.call(that);
  };

  Utils.jsonLoad = function jsonLoad(filePath, that, cb) {
    let obj;
    let json = self.fakeContents["weave/" + filePath + ".json"];
    if (json) {
      obj = JSON.parse(json);
    }
    cb.call(that, obj);
  };
};

this.fakeSHA256HMAC = function fakeSHA256HMAC(message) {
   message = message.substr(0, 64);
   while (message.length < 64) {
     message += " ";
   }
   return message;
}

this.FakeGUIDService = function FakeGUIDService() {
  let latestGUID = 0;

  Utils.makeGUID = function makeGUID() {
    return "fake-guid-" + latestGUID++;
  };
}

/*
 * Mock implementation of WeaveCrypto. It does not encrypt or
 * decrypt, merely returning the input verbatim.
 */
this.FakeCryptoService = function FakeCryptoService() {
  this.counter = 0;

  delete Svc.Crypto;  // get rid of the getter first
  Svc.Crypto = this;

  CryptoWrapper.prototype.ciphertextHMAC = function ciphertextHMAC(keyBundle) {
    return fakeSHA256HMAC(this.ciphertext);
  };
}
FakeCryptoService.prototype = {

  encrypt: function encrypt(clearText, symmetricKey, iv) {
    return clearText;
  },

  decrypt: function decrypt(cipherText, symmetricKey, iv) {
    return cipherText;
  },

  generateRandomKey: function generateRandomKey() {
    return btoa("fake-symmetric-key-" + this.counter++);
  },

  generateRandomIV: function generateRandomIV() {
    // A base64-encoded IV is 24 characters long
    return btoa("fake-fake-fake-random-iv");
  },

  expandData: function expandData(data, len) {
    return data;
  },

  deriveKeyFromPassphrase: function deriveKeyFromPassphrase(passphrase,
                                                            salt, keyLength) {
    return "some derived key string composed of bytes";
  },

  generateRandomBytes: function generateRandomBytes(byteCount) {
    return "not-so-random-now-are-we-HA-HA-HA! >:)".slice(byteCount);
  }
};

