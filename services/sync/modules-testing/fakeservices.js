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

var {utils: Cu} = Components;

Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");

this.FakeFilesystemService = function FakeFilesystemService(contents) {
  this.fakeContents = contents;
  let self = this;

  // Save away the unmocked versions of the functions we replace here for tests
  // that really want the originals. As this may be called many times per test,
  // we must be careful to not replace them with ones we previously replaced.
  // (And WTF are we bothering with these mocks in the first place? Is the
  // performance of the filesystem *really* such that it outweighs the downside
  // of not running our real JSON functions in the tests? Eg, these mocks don't
  // always throw exceptions when the real ones do. Anyway...)
  for (let name of ["jsonSave", "jsonLoad", "jsonMove", "jsonRemove"]) {
    let origName = "_real_" + name;
    if (!Utils[origName]) {
      Utils[origName] = Utils[name];
    }
  }

  Utils.jsonSave = function jsonSave(filePath, that, obj, callback) {
    let json = typeof obj == "function" ? obj.call(that) : obj;
    self.fakeContents["weave/" + filePath + ".json"] = JSON.stringify(json);
    if (callback) {
      callback.call(that);
    }
    return Promise.resolve();
  };

  Utils.jsonLoad = function jsonLoad(filePath, that, cb) {
    let obj;
    let json = self.fakeContents["weave/" + filePath + ".json"];
    if (json) {
      obj = JSON.parse(json);
    }
    if (cb) {
      cb.call(that, obj);
    }
    return Promise.resolve(obj);
  };

  Utils.jsonMove = function jsonMove(aFrom, aTo, that) {
    const fromPath = "weave/" + aFrom + ".json";
    self.fakeContents["weave/" + aTo + ".json"] = self.fakeContents[fromPath];
    delete self.fakeContents[fromPath];
    return Promise.resolve();
  };

  Utils.jsonRemove = function jsonRemove(filePath, that) {
    delete self.fakeContents["weave/" + filePath + ".json"];
    return Promise.resolve();
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
    // ensure that this always returns a unique 12 character string
    let nextGUID = "fake-guid-" + String(latestGUID++).padStart(2, "0");
    return nextGUID.slice(nextGUID.length - 12, nextGUID.length);
  };
}

/*
 * Mock implementation of WeaveCrypto. It does not encrypt or
 * decrypt, merely returning the input verbatim.
 */
this.FakeCryptoService = function FakeCryptoService() {
  this.counter = 0;

  delete Weave.Crypto;  // get rid of the getter first
  Weave.Crypto = this;

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

  generateRandomBytes: function generateRandomBytes(byteCount) {
    return "not-so-random-now-are-we-HA-HA-HA! >:)".slice(byteCount);
  }
};

