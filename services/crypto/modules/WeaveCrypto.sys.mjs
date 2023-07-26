/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CRYPT_ALGO = "AES-CBC";
const CRYPT_ALGO_LENGTH = 256;
const CRYPT_ALGO_USAGES = ["encrypt", "decrypt"];
const AES_CBC_IV_SIZE = 16;
const OPERATIONS = { ENCRYPT: 0, DECRYPT: 1 };
const UTF_LABEL = "utf-8";

export function WeaveCrypto() {
  this.init();
}

WeaveCrypto.prototype = {
  prefBranch: null,
  debug: true, // services.sync.log.cryptoDebug

  observer: {
    _self: null,

    QueryInterface: ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]),

    observe(subject, topic, data) {
      let self = this._self;
      self.log("Observed " + topic + " topic.");
      if (topic == "nsPref:changed") {
        self.debug = self.prefBranch.getBoolPref("cryptoDebug");
      }
    },
  },

  init() {
    // Preferences. Add observer so we get notified of changes.
    this.prefBranch = Services.prefs.getBranch("services.sync.log.");
    this.prefBranch.addObserver("cryptoDebug", this.observer);
    this.observer._self = this;
    this.debug = this.prefBranch.getBoolPref("cryptoDebug", false);
    ChromeUtils.defineLazyGetter(
      this,
      "encoder",
      () => new TextEncoder(UTF_LABEL)
    );
    ChromeUtils.defineLazyGetter(
      this,
      "decoder",
      () => new TextDecoder(UTF_LABEL, { fatal: true })
    );
  },

  log(message) {
    if (!this.debug) {
      return;
    }
    dump("WeaveCrypto: " + message + "\n");
    Services.console.logStringMessage("WeaveCrypto: " + message);
  },

  // /!\ Only use this for tests! /!\
  _getCrypto() {
    return crypto;
  },

  async encrypt(clearTextUCS2, symmetricKey, iv) {
    this.log("encrypt() called");
    let clearTextBuffer = this.encoder.encode(clearTextUCS2).buffer;
    let encrypted = await this._commonCrypt(
      clearTextBuffer,
      symmetricKey,
      iv,
      OPERATIONS.ENCRYPT
    );
    return this.encodeBase64(encrypted);
  },

  async decrypt(cipherText, symmetricKey, iv) {
    this.log("decrypt() called");
    if (cipherText.length) {
      cipherText = atob(cipherText);
    }
    let cipherTextBuffer = this.byteCompressInts(cipherText);
    let decrypted = await this._commonCrypt(
      cipherTextBuffer,
      symmetricKey,
      iv,
      OPERATIONS.DECRYPT
    );
    return this.decoder.decode(decrypted);
  },

  /**
   * _commonCrypt
   *
   * @args
   * data: data to encrypt/decrypt (ArrayBuffer)
   * symKeyStr: symmetric key (Base64 String)
   * ivStr: initialization vector (Base64 String)
   * operation: operation to apply (either OPERATIONS.ENCRYPT or OPERATIONS.DECRYPT)
   * @returns
   * the encrypted/decrypted data (ArrayBuffer)
   */
  async _commonCrypt(data, symKeyStr, ivStr, operation) {
    this.log("_commonCrypt() called");
    ivStr = atob(ivStr);

    if (operation !== OPERATIONS.ENCRYPT && operation !== OPERATIONS.DECRYPT) {
      throw new Error("Unsupported operation in _commonCrypt.");
    }
    // We never want an IV longer than the block size, which is 16 bytes
    // for AES, neither do we want one smaller; throw in both cases.
    if (ivStr.length !== AES_CBC_IV_SIZE) {
      throw new Error(`Invalid IV size; must be ${AES_CBC_IV_SIZE} bytes.`);
    }

    let iv = this.byteCompressInts(ivStr);
    let symKey = await this.importSymKey(symKeyStr, operation);
    let cryptMethod = (
      operation === OPERATIONS.ENCRYPT
        ? crypto.subtle.encrypt
        : crypto.subtle.decrypt
    ).bind(crypto.subtle);
    let algo = { name: CRYPT_ALGO, iv };

    let keyBytes = await cryptMethod.call(crypto.subtle, algo, symKey, data);
    return new Uint8Array(keyBytes);
  },

  async generateRandomKey() {
    this.log("generateRandomKey() called");
    let algo = {
      name: CRYPT_ALGO,
      length: CRYPT_ALGO_LENGTH,
    };
    let key = await crypto.subtle.generateKey(algo, true, CRYPT_ALGO_USAGES);
    let keyBytes = await crypto.subtle.exportKey("raw", key);
    return this.encodeBase64(new Uint8Array(keyBytes));
  },

  generateRandomIV() {
    return this.generateRandomBytes(AES_CBC_IV_SIZE);
  },

  generateRandomBytes(byteCount) {
    this.log("generateRandomBytes() called");

    let randBytes = new Uint8Array(byteCount);
    crypto.getRandomValues(randBytes);

    return this.encodeBase64(randBytes);
  },

  //
  // SymKey CryptoKey memoization.
  //

  // Memoize the import of symmetric keys. We do this by using the base64
  // string itself as a key.
  _encryptionSymKeyMemo: {},
  _decryptionSymKeyMemo: {},
  async importSymKey(encodedKeyString, operation) {
    let memo;

    // We use two separate memos for thoroughness: operation is an input to
    // key import.
    switch (operation) {
      case OPERATIONS.ENCRYPT:
        memo = this._encryptionSymKeyMemo;
        break;
      case OPERATIONS.DECRYPT:
        memo = this._decryptionSymKeyMemo;
        break;
      default:
        throw new Error("Unsupported operation in importSymKey.");
    }

    if (encodedKeyString in memo) {
      return memo[encodedKeyString];
    }

    let symmetricKeyBuffer = this.makeUint8Array(encodedKeyString, true);
    let algo = { name: CRYPT_ALGO };
    let usages = [operation === OPERATIONS.ENCRYPT ? "encrypt" : "decrypt"];
    let symKey = await crypto.subtle.importKey(
      "raw",
      symmetricKeyBuffer,
      algo,
      false,
      usages
    );
    memo[encodedKeyString] = symKey;
    return symKey;
  },

  //
  // Utility functions
  //

  /**
   * Returns an Uint8Array filled with a JS string,
   * which means we only keep utf-16 characters from 0x00 to 0xFF.
   */
  byteCompressInts(str) {
    let arrayBuffer = new Uint8Array(str.length);
    for (let i = 0; i < str.length; i++) {
      arrayBuffer[i] = str.charCodeAt(i) & 0xff;
    }
    return arrayBuffer;
  },

  expandData(data) {
    let expanded = "";
    for (let i = 0; i < data.length; i++) {
      expanded += String.fromCharCode(data[i]);
    }
    return expanded;
  },

  encodeBase64(data) {
    return btoa(this.expandData(data));
  },

  makeUint8Array(input, isEncoded) {
    if (isEncoded) {
      input = atob(input);
    }
    return this.byteCompressInts(input);
  },
};
