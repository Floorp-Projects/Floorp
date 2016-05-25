/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["WeaveCrypto"];

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/async.js");

Cu.importGlobalProperties(['crypto']);

const CRYPT_ALGO        = "AES-CBC";
const CRYPT_ALGO_LENGTH = 256;
const AES_CBC_IV_SIZE   = 16;
const OPERATIONS        = { ENCRYPT: 0, DECRYPT: 1 };
const UTF_LABEL          = "utf-8";

const KEY_DERIVATION_ALGO         = "PBKDF2";
const KEY_DERIVATION_HASHING_ALGO = "SHA-1";
const KEY_DERIVATION_ITERATIONS   = 4096; // PKCS#5 recommends at least 1000.
const DERIVED_KEY_ALGO            = CRYPT_ALGO;

this.WeaveCrypto = function WeaveCrypto() {
    this.init();
};

WeaveCrypto.prototype = {
    prefBranch : null,
    debug      : true,  // services.sync.log.cryptoDebug

    observer : {
        _self : null,

        QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                                Ci.nsISupportsWeakReference]),

        observe(subject, topic, data) {
            let self = this._self;
            self.log("Observed " + topic + " topic.");
            if (topic == "nsPref:changed") {
                self.debug = self.prefBranch.getBoolPref("cryptoDebug");
            }
        }
    },

    init() {
        // Preferences. Add observer so we get notified of changes.
        this.prefBranch = Services.prefs.getBranch("services.sync.log.");
        this.prefBranch.addObserver("cryptoDebug", this.observer, false);
        this.observer._self = this;
        try {
          this.debug = this.prefBranch.getBoolPref("cryptoDebug");
        } catch (x) {
          this.debug = false;
        }
        XPCOMUtils.defineLazyGetter(this, 'encoder', () => new TextEncoder(UTF_LABEL));
        XPCOMUtils.defineLazyGetter(this, 'decoder', () => new TextDecoder(UTF_LABEL, { fatal: true }));
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

    encrypt(clearTextUCS2, symmetricKey, iv) {
        this.log("encrypt() called");
        let clearTextBuffer = this.encoder.encode(clearTextUCS2).buffer;
        let encrypted = this._commonCrypt(clearTextBuffer, symmetricKey, iv, OPERATIONS.ENCRYPT);
        return this.encodeBase64(encrypted);
    },

    decrypt(cipherText, symmetricKey, iv) {
        this.log("decrypt() called");
        if (cipherText.length) {
            cipherText = atob(cipherText);
        }
        let cipherTextBuffer = this.byteCompressInts(cipherText);
        let decrypted = this._commonCrypt(cipherTextBuffer, symmetricKey, iv, OPERATIONS.DECRYPT);
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
    _commonCrypt(data, symKeyStr, ivStr, operation) {
        this.log("_commonCrypt() called");
        ivStr = atob(ivStr);

        if (operation !== OPERATIONS.ENCRYPT && operation !== OPERATIONS.DECRYPT) {
            throw new Error("Unsupported operation in _commonCrypt.");
        }
        // We never want an IV longer than the block size, which is 16 bytes
        // for AES, neither do we want one smaller; throw in both cases.
        if (ivStr.length !== AES_CBC_IV_SIZE) {
            throw "Invalid IV size; must be " + AES_CBC_IV_SIZE + " bytes.";
        }

        let iv = this.byteCompressInts(ivStr);
        let symKey = this.importSymKey(symKeyStr, operation);
        let cryptMethod = (operation === OPERATIONS.ENCRYPT
                           ? crypto.subtle.encrypt
                           : crypto.subtle.decrypt)
                          .bind(crypto.subtle);
        let algo = { name: CRYPT_ALGO, iv: iv };


        return Async.promiseSpinningly(
            cryptMethod(algo, symKey, data)
            .then(keyBytes => new Uint8Array(keyBytes))
        );
    },


    generateRandomKey() {
        this.log("generateRandomKey() called");
        let algo = {
            name: CRYPT_ALGO,
            length: CRYPT_ALGO_LENGTH
        };
        return Async.promiseSpinningly(
            crypto.subtle.generateKey(algo, true, [])
            .then(key => crypto.subtle.exportKey("raw", key))
            .then(keyBytes => {
                keyBytes = new Uint8Array(keyBytes);
                return this.encodeBase64(keyBytes);
            })
        );
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
    importSymKey(encodedKeyString, operation) {
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
                throw "Unsupported operation in importSymKey.";
        }

        if (encodedKeyString in memo)
            return memo[encodedKeyString];

        let symmetricKeyBuffer = this.makeUint8Array(encodedKeyString, true);
        let algo = { name: CRYPT_ALGO };
        let usages = [operation === OPERATIONS.ENCRYPT ? "encrypt" : "decrypt"];

        return Async.promiseSpinningly(
            crypto.subtle.importKey("raw", symmetricKeyBuffer, algo, false, usages)
            .then(symKey => {
                memo[encodedKeyString] = symKey;
                return symKey;
            })
        );
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
            arrayBuffer[i] = str.charCodeAt(i) & 0xFF;
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

    /**
     * Returns the expanded data string for the derived key.
     */
    deriveKeyFromPassphrase(passphrase, saltStr, keyLength = 32) {
        this.log("deriveKeyFromPassphrase() called.");
        let keyData = this.makeUint8Array(passphrase, false);
        let salt = this.makeUint8Array(saltStr, true);
        let importAlgo = { name: KEY_DERIVATION_ALGO };
        let deriveAlgo = {
            name: KEY_DERIVATION_ALGO,
            salt: salt,
            iterations: KEY_DERIVATION_ITERATIONS,
            hash: { name: KEY_DERIVATION_HASHING_ALGO },
        };
        let derivedKeyType = {
            name: DERIVED_KEY_ALGO,
            length: keyLength * 8,
        };
        return Async.promiseSpinningly(
            crypto.subtle.importKey("raw", keyData, importAlgo, false, ["deriveKey"])
            .then(key => crypto.subtle.deriveKey(deriveAlgo, key, derivedKeyType, true, []))
            .then(derivedKey => crypto.subtle.exportKey("raw", derivedKey))
            .then(keyBytes => {
                keyBytes = new Uint8Array(keyBytes);
                return this.expandData(keyBytes);
            })
        );
    },
};
