/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ctypes", "resource://gre/modules/ctypes.jsm");

function OSCrypto() {
  this._structs = {};
  this._functions = new Map();
  this._libs = new Map();
  this._structs.DATA_BLOB = new ctypes.StructType("DATA_BLOB",
                                                  [
                                                    {cbData: ctypes.uint32_t},
                                                    {pbData: ctypes.uint8_t.ptr}
                                                  ]);

  try {

    this._libs.set("crypt32", ctypes.open("Crypt32"));
    this._libs.set("kernel32", ctypes.open("Kernel32"));

    this._functions.set("CryptProtectData",
                        this._libs.get("crypt32").declare("CryptProtectData",
                                                          ctypes.winapi_abi,
                                                          ctypes.uint32_t,
                                                          this._structs.DATA_BLOB.ptr,
                                                          ctypes.voidptr_t,
                                                          ctypes.voidptr_t,
                                                          ctypes.voidptr_t,
                                                          ctypes.voidptr_t,
                                                          ctypes.uint32_t,
                                                          this._structs.DATA_BLOB.ptr));

    this._functions.set("CryptUnprotectData",
                        this._libs.get("crypt32").declare("CryptUnprotectData",
                                                          ctypes.winapi_abi,
                                                          ctypes.uint32_t,
                                                          this._structs.DATA_BLOB.ptr,
                                                          ctypes.voidptr_t,
                                                          ctypes.voidptr_t,
                                                          ctypes.voidptr_t,
                                                          ctypes.voidptr_t,
                                                          ctypes.uint32_t,
                                                          this._structs.DATA_BLOB.ptr));
    this._functions.set("LocalFree",
                        this._libs.get("kernel32").declare("LocalFree",
                                                           ctypes.winapi_abi,
                                                           ctypes.uint32_t,
                                                           ctypes.voidptr_t));
  } catch (ex) {
    Cu.reportError(ex);
    this.finalize();
    throw ex;
  }
}
OSCrypto.prototype = {
  /**
   * Decrypt an array of numbers using the windows CryptUnprotectData API.
   * @param {number[]} array - the encrypted array that needs to be decrypted.
   * @returns {string} the decryption of the array.
   */
  decryptData(array) {
    let decryptedData = "";
    let encryptedData = ctypes.uint8_t.array(array.length)(array);
    let inData = new this._structs.DATA_BLOB(encryptedData.length, encryptedData);
    let outData = new this._structs.DATA_BLOB();
    let status = this._functions.get("CryptUnprotectData")(inData.address(), null,
                                                           null, null, null, 0,
                                                           outData.address());
    if (status === 0) {
      throw new Error("decryptData failed: " + status);
    }

    // convert byte array to JS string.
    let len = outData.cbData;
    let decrypted = ctypes.cast(outData.pbData,
                                ctypes.uint8_t.array(len).ptr).contents;
    for (let i = 0; i < decrypted.length; i++) {
      decryptedData += String.fromCharCode(decrypted[i]);
    }

    this._functions.get("LocalFree")(outData.pbData);
    return decryptedData;
  },

  /**
   * Encrypt a string using the windows CryptProtectData API.
   * @param {string} string - the string that is going to be encrypted.
   * @returns {number[]} the encrypted string encoded as an array of numbers.
   */
  encryptData(string) {
    let encryptedData = [];
    let decryptedData = ctypes.uint8_t.array(string.length)();

    for (let i = 0; i < string.length; i++) {
      decryptedData[i] = string.charCodeAt(i);
    }

    let inData = new this._structs.DATA_BLOB(string.length, decryptedData);
    let outData = new this._structs.DATA_BLOB();
    let status = this._functions.get("CryptProtectData")(inData.address(), null,
                                                         null, null, null, 0,
                                                         outData.address());
    if (status === 0) {
      throw new Error("encryptData failed: " + status);
    }

    // convert byte array to JS string.
    let len = outData.cbData;
    let encrypted = ctypes.cast(outData.pbData,
                                ctypes.uint8_t.array(len).ptr).contents;

    for (let i = 0; i < len; i++) {
      encryptedData.push(encrypted[i]);
    }

    this._functions.get("LocalFree")(outData.pbData);
    return encryptedData;
  },

  /**
   * Must be invoked once after last use of any of the provided helpers.
   */
  finalize() {
    this._structs = {};
    this._functions.clear();
    for (let lib of this._libs.values()) {
      try {
        lib.close();
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
    this._libs.clear();
  },
};
