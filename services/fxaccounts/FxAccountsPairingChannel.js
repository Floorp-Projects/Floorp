/*!
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * The following bundle is from an external repository at github.com/mozilla/fxa-pairing-channel,
 * it implements a shared library for two javascript environments to create an encrypted and authenticated
 * communication channel by sharing a secret key and by relaying messages through a websocket server.
 * 
 * It is used by the Firefox Accounts pairing flow, with one side of the channel being web
 * content from https://accounts.firefox.com and the other side of the channel being chrome native code.
 * 
 * This uses the event-target-shim node library published under the MIT license:
 * https://github.com/mysticatea/event-target-shim/blob/master/LICENSE
 * 
 * Bundle generated from https://github.com/mozilla/fxa-pairing-channel.git. Hash:c8ec3119920b4ffa833b, Chunkhash:378a5f51445e7aa7630e.
 * 
 */

// This header provides a little bit of plumbing to use `FxAccountsPairingChannel`
// from Firefox browser code, hence the presence of these privileged browser APIs.
// If you're trying to use this from ordinary web content you're in for a bad time.

const {setTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");
// We cannot use WebSocket from chrome code without a window,
// see https://bugzilla.mozilla.org/show_bug.cgi?id=784686
const browser = Services.appShell.createWindowlessBrowser(true);
const {WebSocket} = browser.document.ownerGlobal;

const EXPORTED_SYMBOLS = ["FxAccountsPairingChannel"];

var FxAccountsPairingChannel =
/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, { enumerable: true, get: getter });
/******/ 		}
/******/ 	};
/******/
/******/ 	// define __esModule on exports
/******/ 	__webpack_require__.r = function(exports) {
/******/ 		if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 			Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 		}
/******/ 		Object.defineProperty(exports, '__esModule', { value: true });
/******/ 	};
/******/
/******/ 	// create a fake namespace object
/******/ 	// mode & 1: value is a module id, require it
/******/ 	// mode & 2: merge all properties of value into the ns
/******/ 	// mode & 4: return value when already ns object
/******/ 	// mode & 8|1: behave like require
/******/ 	__webpack_require__.t = function(value, mode) {
/******/ 		if(mode & 1) value = __webpack_require__(value);
/******/ 		if(mode & 8) return value;
/******/ 		if((mode & 4) && typeof value === 'object' && value && value.__esModule) return value;
/******/ 		var ns = Object.create(null);
/******/ 		__webpack_require__.r(ns);
/******/ 		Object.defineProperty(ns, 'default', { enumerable: true, value: value });
/******/ 		if(mode & 2 && typeof value != 'string') for(var key in value) __webpack_require__.d(ns, key, function(key) { return value[key]; }.bind(null, key));
/******/ 		return ns;
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
// ESM COMPAT FLAG
__webpack_require__.r(__webpack_exports__);

// EXPORTS
__webpack_require__.d(__webpack_exports__, "PairingChannel", function() { return /* binding */ src_PairingChannel; });
__webpack_require__.d(__webpack_exports__, "base64urlToBytes", function() { return /* reexport */ base64urlToBytes; });
__webpack_require__.d(__webpack_exports__, "bytesToBase64url", function() { return /* reexport */ bytesToBase64url; });
__webpack_require__.d(__webpack_exports__, "bytesToHex", function() { return /* reexport */ bytesToHex; });
__webpack_require__.d(__webpack_exports__, "bytesToUtf8", function() { return /* reexport */ bytesToUtf8; });
__webpack_require__.d(__webpack_exports__, "hexToBytes", function() { return /* reexport */ hexToBytes; });
__webpack_require__.d(__webpack_exports__, "TLSCloseNotify", function() { return /* reexport */ TLSCloseNotify; });
__webpack_require__.d(__webpack_exports__, "TLSError", function() { return /* reexport */ TLSError; });
__webpack_require__.d(__webpack_exports__, "utf8ToBytes", function() { return /* reexport */ utf8ToBytes; });
__webpack_require__.d(__webpack_exports__, "_internals", function() { return /* binding */ _internals; });

// CONCATENATED MODULE: ./src/alerts.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable sorting/sort-object-props */
const ALERT_LEVEL = {
  WARNING: 1,
  FATAL: 2
};

const ALERT_DESCRIPTION = {
  CLOSE_NOTIFY: 0,
  UNEXPECTED_MESSAGE: 10,
  BAD_RECORD_MAC: 20,
  RECORD_OVERFLOW: 22,
  HANDSHAKE_FAILURE: 40,
  ILLEGAL_PARAMETER: 47,
  DECODE_ERROR: 50,
  DECRYPT_ERROR: 51,
  PROTOCOL_VERSION: 70,
  INTERNAL_ERROR: 80,
  MISSING_EXTENSION: 109,
  UNSUPPORTED_EXTENSION: 110,
  UNKNOWN_PSK_IDENTITY: 115,
  NO_APPLICATION_PROTOCOL: 120,
};
/* eslint-enable sorting/sort-object-props */

function alertTypeToName(type) {
  for (const name in ALERT_DESCRIPTION) {
    if (ALERT_DESCRIPTION[name] === type) {
      return `${name} (${type})`;
    }
  }
  return `UNKNOWN (${type})`;
}

class TLSAlert extends Error {
  constructor(description, level) {
    super(`TLS Alert: ${alertTypeToName(description)}`);
    this.description = description;
    this.level = level;
  }

  static fromBytes(bytes) {
    if (bytes.byteLength !== 2) {
      throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
    }
    switch (bytes[1]) {
      case ALERT_DESCRIPTION.CLOSE_NOTIFY:
        if (bytes[0] !== ALERT_LEVEL.WARNING) {
          // Close notifications should be fatal.
          throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
        }
        return new TLSCloseNotify();
      default:
        return new TLSError(bytes[1]);
    }
  }

  toBytes() {
    return new Uint8Array([this.level, this.description]);
  }
}

class TLSCloseNotify extends TLSAlert {
  constructor() {
    super(ALERT_DESCRIPTION.CLOSE_NOTIFY, ALERT_LEVEL.WARNING);
  }
}

class TLSError extends TLSAlert {
  constructor(description = ALERT_DESCRIPTION.INTERNAL_ERROR) {
    super(description, ALERT_LEVEL.FATAL);
  }
}

// CONCATENATED MODULE: ./src/utils.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */



//
// Various low-level utility functions.
//
// These are mostly conveniences for working with Uint8Arrays as
// the primitive "bytes" type.
//

const UTF8_ENCODER = new TextEncoder();
const UTF8_DECODER = new TextDecoder();

function noop() {}

function assert(cond, msg) {
  if (! cond) {
    throw new Error('assert failed: ' + msg);
  }
}

function assertIsBytes(value, msg = 'value must be a Uint8Array') {
  // Using `value instanceof Uint8Array` seems to fail in Firefox chrome code
  // for inscrutable reasons, so we do a less direct check.
  assert(ArrayBuffer.isView(value), msg);
  assert(value.BYTES_PER_ELEMENT === 1, msg);
  return value;
}

const EMPTY = new Uint8Array(0);

function zeros(n) {
  return new Uint8Array(n);
}

function arrayToBytes(value) {
  return new Uint8Array(value);
}

function bytesToHex(bytes) {
  return Array.prototype.map.call(bytes, byte => {
    let s = byte.toString(16);
    if (s.length === 1) {
      s = '0' + s;
    }
    return s;
  }).join('');
}

function hexToBytes(hexstr) {
  assert(hexstr.length % 2 === 0, 'hexstr.length must be even');
  return new Uint8Array(Array.prototype.map.call(hexstr, (c, n) => {
    if (n % 2 === 1) {
      return hexstr[n - 1] + c;
    } else {
      return '';
    }
  }).filter(s => {
    return !! s;
  }).map(s => {
    return parseInt(s, 16);
  }));
}

function bytesToUtf8(bytes) {
  return UTF8_DECODER.decode(bytes);
}

function utf8ToBytes(str) {
  return UTF8_ENCODER.encode(str);
}

function bytesToBase64url(bytes) {
  // XXX TODO: try to use something constant-time, in case calling code
  // uses it to encode secrets?
  const charCodes = String.fromCharCode.apply(String, bytes);
  return btoa(charCodes).replace(/\+/g, '-').replace(/\//g, '_');
}

function base64urlToBytes(str) {
  // XXX TODO: try to use something constant-time, in case calling code
  // uses it to decode secrets?
  str = atob(str.replace(/-/g, '+').replace(/_/g, '/'));
  const bytes = new Uint8Array(str.length);
  for (let i = 0; i < str.length; i++) {
    bytes[i] = str.charCodeAt(i);
  }
  return bytes;
}

function bytesAreEqual(v1, v2) {
  assertIsBytes(v1);
  assertIsBytes(v2);
  if (v1.length !== v2.length) {
    return false;
  }
  for (let i = 0; i < v1.length; i++) {
    if (v1[i] !== v2[i]) {
      return false;
    }
  }
  return true;
}

// The `BufferReader` and `BufferWriter` classes are helpers for dealing with the
// binary struct format that's used for various TLS message.  Think of them as a
// buffer with a pointer to the "current position" and a bunch of helper methods
// to read/write structured data and advance said pointer.

class utils_BufferWithPointer {
  constructor(buf) {
    this._buffer = buf;
    this._dataview = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    this._pos = 0;
  }

  length() {
    return this._buffer.byteLength;
  }

  tell() {
    return this._pos;
  }

  seek(pos) {
    if (pos < 0) {
      throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
    }
    if (pos > this.length()) {
      throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
    }
    this._pos = pos;
  }

  incr(offset) {
    this.seek(this._pos + offset);
  }
}

// The `BufferReader` class helps you read structured data from a byte array.
// It offers methods for reading both primitive values, and the variable-length
// vector structures defined in https://tools.ietf.org/html/rfc8446#section-3.4.
//
// Such vectors are represented as a length followed by the concatenated
// bytes of each item, and the size of the length field is determined by
// the maximum allowed number of bytes in the vector.  For example
// to read a vector that may contain up to 65535 bytes, use `readVector16`.
//
// To read a variable-length vector of between 1 and 100 uint16 values,
// defined in the RFC like this:
//
//    uint16 items<2..200>;
//
// You would do something like this:
//
//    const items = []
//    buf.readVector8(buf => {
//      items.push(buf.readUint16())
//    })
//
// The various `read` will throw `DECODE_ERROR` if you attempt to read path
// the end of the buffer, or past the end of a variable-length list.
//
class utils_BufferReader extends utils_BufferWithPointer {

  hasMoreBytes() {
    return this.tell() < this.length();
  }

  readBytes(length) {
    // This avoids copies by returning a view onto the existing buffer.
    const start = this._buffer.byteOffset + this.tell();
    this.incr(length);
    return new Uint8Array(this._buffer.buffer, start, length);
  }

  _rangeErrorToAlert(cb) {
    try {
      return cb(this);
    } catch (err) {
      if (err instanceof RangeError) {
        throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
      }
      throw err;
    }
  }

  readUint8() {
    return this._rangeErrorToAlert(() => {
      const n = this._dataview.getUint8(this._pos);
      this.incr(1);
      return n;
    });
  }

  readUint16() {
    return this._rangeErrorToAlert(() => {
      const n = this._dataview.getUint16(this._pos);
      this.incr(2);
      return n;
    });
  }

  readUint24() {
    return this._rangeErrorToAlert(() => {
      let n = this._dataview.getUint16(this._pos);
      n = (n << 8) | this._dataview.getUint8(this._pos + 2);
      this.incr(3);
      return n;
    });
  }

  readUint32() {
    return this._rangeErrorToAlert(() => {
      const n = this._dataview.getUint32(this._pos);
      this.incr(4);
      return n;
    });
  }

  _readVector(length, cb) {
    const contentsBuf = new utils_BufferReader(this.readBytes(length));
    const expectedEnd = this.tell();
    // Keep calling the callback until we've consumed the expected number of bytes.
    let n = 0;
    while (contentsBuf.hasMoreBytes()) {
      const prevPos = contentsBuf.tell();
      cb(contentsBuf, n);
      // Check that the callback made forward progress, otherwise we'll infinite loop.
      if (contentsBuf.tell() <= prevPos) {
        throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
      }
      n += 1;
    }
    // Check that the callback correctly consumed the vector's entire contents.
    if (this.tell() !== expectedEnd) {
      throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
    }
  }

  readVector8(cb) {
    const length = this.readUint8();
    return this._readVector(length, cb);
  }

  readVector16(cb) {
    const length = this.readUint16();
    return this._readVector(length, cb);
  }

  readVector24(cb) {
    const length = this.readUint24();
    return this._readVector(length, cb);
  }

  readVectorBytes8() {
    return this.readBytes(this.readUint8());
  }

  readVectorBytes16() {
    return this.readBytes(this.readUint16());
  }

  readVectorBytes24() {
    return this.readBytes(this.readUint24());
  }
}


class utils_BufferWriter extends utils_BufferWithPointer {
  constructor(size = 1024) {
    super(new Uint8Array(size));
  }

  _maybeGrow(n) {
    const curSize = this._buffer.byteLength;
    const newPos = this._pos + n;
    const shortfall = newPos - curSize;
    if (shortfall > 0) {
      // Classic grow-by-doubling, up to 4kB max increment.
      // This formula was not arrived at by any particular science.
      const incr = Math.min(curSize, 4 * 1024);
      const newbuf = new Uint8Array(curSize + Math.ceil(shortfall / incr) * incr);
      newbuf.set(this._buffer, 0);
      this._buffer = newbuf;
      this._dataview = new DataView(newbuf.buffer, newbuf.byteOffset, newbuf.byteLength);
    }
  }

  slice(start = 0, end = this.tell()) {
    if (end < 0) {
      end = this.tell() + end;
    }
    if (start < 0) {
      throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    if (end < 0) {
      throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    if (end > this.length()) {
      throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    return this._buffer.slice(start, end);
  }

  flush() {
    const slice = this.slice();
    this.seek(0);
    return slice;
  }

  writeBytes(data) {
    this._maybeGrow(data.byteLength);
    this._buffer.set(data, this.tell());
    this.incr(data.byteLength);
  }

  writeUint8(n) {
    this._maybeGrow(1);
    this._dataview.setUint8(this._pos, n);
    this.incr(1);
  }

  writeUint16(n) {
    this._maybeGrow(2);
    this._dataview.setUint16(this._pos, n);
    this.incr(2);
  }

  writeUint24(n) {
    this._maybeGrow(3);
    this._dataview.setUint16(this._pos, n >> 8);
    this._dataview.setUint8(this._pos + 2, n & 0xFF);
    this.incr(3);
  }

  writeUint32(n) {
    this._maybeGrow(4);
    this._dataview.setUint32(this._pos, n);
    this.incr(4);
  }

  // These are helpers for writing the variable-length vector structure
  // defined in https://tools.ietf.org/html/rfc8446#section-3.4.
  //
  // Such vectors are represented as a length followed by the concatenated
  // bytes of each item, and the size of the length field is determined by
  // the maximum allowed size of the vector.  For example to write a vector
  // that may contain up to 65535 bytes, use `writeVector16`.
  //
  // To write a variable-length vector of between 1 and 100 uint16 values,
  // defined in the RFC like this:
  //
  //    uint16 items<2..200>;
  //
  // You would do something like this:
  //
  //    buf.writeVector8(buf => {
  //      for (let item of items) {
  //          buf.writeUint16(item)
  //      }
  //    })
  //
  // The helper will automatically take care of writing the appropriate
  // length field once the callback completes.

  _writeVector(maxLength, writeLength, cb) {
    // Initially, write the length field as zero.
    const lengthPos = this.tell();
    writeLength(0);
    // Call the callback to write the vector items.
    const bodyPos = this.tell();
    cb(this);
    const length = this.tell() - bodyPos;
    if (length >= maxLength) {
      throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    // Backfill the actual length field.
    this.seek(lengthPos);
    writeLength(length);
    this.incr(length);
    return length;
  }

  writeVector8(cb) {
    return this._writeVector(Math.pow(2, 8), len => this.writeUint8(len), cb);
  }

  writeVector16(cb) {
    return this._writeVector(Math.pow(2, 16), len => this.writeUint16(len), cb);
  }

  writeVector24(cb) {
    return this._writeVector(Math.pow(2, 24), len => this.writeUint24(len), cb);
  }

  writeVectorBytes8(bytes) {
    return this.writeVector8(buf => {
      buf.writeBytes(bytes);
    });
  }

  writeVectorBytes16(bytes) {
    return this.writeVector16(buf => {
      buf.writeBytes(bytes);
    });
  }

  writeVectorBytes24(bytes) {
    return this.writeVector24(buf => {
      buf.writeBytes(bytes);
    });
  }
}

// CONCATENATED MODULE: ./src/crypto.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Low-level crypto primitives.
//
// This file implements the AEAD encrypt/decrypt and hashing routines
// for the TLS_AES_128_GCM_SHA256 ciphersuite. They are (thankfully)
// fairly light-weight wrappers around what's available via the WebCrypto
// API.
//




const AEAD_SIZE_INFLATION = 16;
const KEY_LENGTH = 16;
const IV_LENGTH = 12;
const HASH_LENGTH = 32;

async function prepareKey(key, mode) {
  return crypto.subtle.importKey('raw', key, { name: 'AES-GCM' }, false, [mode]);
}

async function encrypt(key, iv, plaintext, additionalData) {
  const ciphertext = await crypto.subtle.encrypt({
    additionalData,
    iv,
    name: 'AES-GCM',
    tagLength: AEAD_SIZE_INFLATION * 8
  }, key, plaintext);
  return new Uint8Array(ciphertext);
}

async function decrypt(key, iv, ciphertext, additionalData) {
  try {
    const plaintext = await crypto.subtle.decrypt({
      additionalData,
      iv,
      name: 'AES-GCM',
      tagLength: AEAD_SIZE_INFLATION * 8
    }, key, ciphertext);
    return new Uint8Array(plaintext);
  } catch (err) {
    // Yes, we really do throw 'decrypt_error' when failing to verify a HMAC,
    // and a 'bad_record_mac' error when failing to decrypt.
    throw new TLSError(ALERT_DESCRIPTION.BAD_RECORD_MAC);
  }
}

async function hash(message) {
  return new Uint8Array(await crypto.subtle.digest({ name: 'SHA-256' }, message));
}

async function hmac(keyBytes, message) {
  const key = await crypto.subtle.importKey('raw', keyBytes, {
    hash: { name: 'SHA-256' },
    name: 'HMAC',
  }, false, ['sign']);
  const sig = await crypto.subtle.sign({ name: 'HMAC' }, key, message);
  return new Uint8Array(sig);
}

async function verifyHmac(keyBytes, signature, message) {
  const key = await crypto.subtle.importKey('raw', keyBytes, {
    hash: { name: 'SHA-256' },
    name: 'HMAC',
  }, false, ['verify']);
  if (! await crypto.subtle.verify({ name: 'HMAC' }, key, signature, message)) {
    // Yes, we really do throw 'decrypt_error' when failing to verify a HMAC,
    // and a 'bad_record_mac' error when failing to decrypt.
    throw new TLSError(ALERT_DESCRIPTION.DECRYPT_ERROR);
  }
}

async function hkdfExtract(salt, ikm) {
  // Ref https://tools.ietf.org/html/rfc5869#section-2.2
  return await hmac(salt, ikm);
}

async function hkdfExpand(prk, info, length) {
  // Ref https://tools.ietf.org/html/rfc5869#section-2.3
  const N = Math.ceil(length / HASH_LENGTH);
  if (N <= 0) {
    throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
  }
  if (N >= 255) {
    throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
  }
  const input = new utils_BufferWriter();
  const output = new utils_BufferWriter();
  let T = new Uint8Array(0);
  for (let i = 1; i <= N; i++) {
    input.writeBytes(T);
    input.writeBytes(info);
    input.writeUint8(i);
    T = await hmac(prk, input.flush());
    output.writeBytes(T);
  }
  return output.slice(0, length);
}

async function hkdfExpandLabel(secret, label, context, length) {
  //  struct {
  //    uint16 length = Length;
  //    opaque label < 7..255 > = "tls13 " + Label;
  //    opaque context < 0..255 > = Context;
  //  } HkdfLabel;
  const hkdfLabel = new utils_BufferWriter();
  hkdfLabel.writeUint16(length);
  hkdfLabel.writeVectorBytes8(utf8ToBytes('tls13 ' + label));
  hkdfLabel.writeVectorBytes8(context);
  return hkdfExpand(secret, hkdfLabel.flush(), length);
}

async function getRandomBytes(size) {
  const bytes = new Uint8Array(size);
  crypto.getRandomValues(bytes);
  return bytes;
}

// CONCATENATED MODULE: ./src/extensions.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Extension parsing.
//
// This file contains some helpers for reading/writing the various kinds
// of Extension that might appear in a HandshakeMessage.
//
// "Extensions" are how TLS signals the presence of particular bits of optional
// functionality in the protocol. Lots of parts of TLS1.3 that don't seem like
// they're optional are implemented in terms of an extension, IIUC because that's
// what was needed for a clean deployment in amongst earlier versions of the protocol.
//





/* eslint-disable sorting/sort-object-props */
const EXTENSION_TYPE = {
  PRE_SHARED_KEY: 41,
  SUPPORTED_VERSIONS: 43,
  PSK_KEY_EXCHANGE_MODES: 45,
};
/* eslint-enable sorting/sort-object-props */

// Base class for generic reading/writing of extensions,
// which are all uniformly formatted as:
//
//   struct {
//     ExtensionType extension_type;
//     opaque extension_data<0..2^16-1>;
//   } Extension;
//
// Extensions always appear inside of a handshake message,
// and their internal structure may differ based on the
// type of that message.

class extensions_Extension {

  get TYPE_TAG() {
    throw new Error('not implemented');
  }

  static read(messageType, buf) {
    const type = buf.readUint16();
    let ext = {
      TYPE_TAG: type,
    };
    buf.readVector16(buf => {
      switch (type) {
        case EXTENSION_TYPE.PRE_SHARED_KEY:
          ext = extensions_PreSharedKeyExtension._read(messageType, buf);
          break;
        case EXTENSION_TYPE.SUPPORTED_VERSIONS:
          ext = extensions_SupportedVersionsExtension._read(messageType, buf);
          break;
        case EXTENSION_TYPE.PSK_KEY_EXCHANGE_MODES:
          ext = extensions_PskKeyExchangeModesExtension._read(messageType, buf);
          break;
        default:
          // Skip over unrecognised extensions.
          buf.incr(buf.length());
      }
      if (buf.hasMoreBytes()) {
        throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
      }
    });
    return ext;
  }

  write(messageType, buf) {
    buf.writeUint16(this.TYPE_TAG);
    buf.writeVector16(buf => {
      this._write(messageType, buf);
    });
  }

  static _read(messageType, buf) {
    throw new Error('not implemented');
  }

  static _write(messageType, buf) {
    throw new Error('not implemented');
  }
}

// The PreSharedKey extension:
//
//  struct {
//    opaque identity<1..2^16-1>;
//    uint32 obfuscated_ticket_age;
//  } PskIdentity;
//  opaque PskBinderEntry<32..255>;
//  struct {
//    PskIdentity identities<7..2^16-1>;
//    PskBinderEntry binders<33..2^16-1>;
//  } OfferedPsks;
//  struct {
//    select(Handshake.msg_type) {
//      case client_hello: OfferedPsks;
//      case server_hello: uint16 selected_identity;
//    };
//  } PreSharedKeyExtension;

class extensions_PreSharedKeyExtension extends extensions_Extension {
  constructor(identities, binders, selectedIdentity) {
    super();
    this.identities = identities;
    this.binders = binders;
    this.selectedIdentity = selectedIdentity;
  }

  get TYPE_TAG() {
    return EXTENSION_TYPE.PRE_SHARED_KEY;
  }

  static _read(messageType, buf) {
    let identities = null, binders = null, selectedIdentity = null;
    switch (messageType) {
      case HANDSHAKE_TYPE.CLIENT_HELLO:
        identities = []; binders = [];
        buf.readVector16(buf => {
          const identity = buf.readVectorBytes16();
          buf.readBytes(4); // Skip over the ticket age.
          identities.push(identity);
        });
        buf.readVector16(buf => {
          const binder = buf.readVectorBytes8();
          if (binder.byteLength < HASH_LENGTH) {
            throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
          }
          binders.push(binder);
        });
        if (identities.length !== binders.length) {
          throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
        }
        break;
      case HANDSHAKE_TYPE.SERVER_HELLO:
        selectedIdentity = buf.readUint16();
        break;
      default:
        throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    return new this(identities, binders, selectedIdentity);
  }

  _write(messageType, buf) {
    switch (messageType) {
      case HANDSHAKE_TYPE.CLIENT_HELLO:
        buf.writeVector16(buf => {
          this.identities.forEach(pskId => {
            buf.writeVectorBytes16(pskId);
            buf.writeUint32(0); // Zero for "tag age" field.
          });
        });
        buf.writeVector16(buf => {
          this.binders.forEach(pskBinder => {
            buf.writeVectorBytes8(pskBinder);
          });
        });
        break;
      case HANDSHAKE_TYPE.SERVER_HELLO:
        buf.writeUint16(this.selectedIdentity);
        break;
      default:
        throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
  }
}


// The SupportedVersions extension:
//
//  struct {
//    select(Handshake.msg_type) {
//      case client_hello:
//        ProtocolVersion versions < 2..254 >;
//      case server_hello:
//        ProtocolVersion selected_version;
//    };
//  } SupportedVersions;

class extensions_SupportedVersionsExtension extends extensions_Extension {
  constructor(versions, selectedVersion) {
    super();
    this.versions = versions;
    this.selectedVersion = selectedVersion;
  }

  get TYPE_TAG() {
    return EXTENSION_TYPE.SUPPORTED_VERSIONS;
  }

  static _read(messageType, buf) {
    let versions = null, selectedVersion = null;
    switch (messageType) {
      case HANDSHAKE_TYPE.CLIENT_HELLO:
        versions = [];
        buf.readVector8(buf => {
          versions.push(buf.readUint16());
        });
        break;
      case HANDSHAKE_TYPE.SERVER_HELLO:
        selectedVersion = buf.readUint16();
        break;
      default:
        throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    return new this(versions, selectedVersion);
  }

  _write(messageType, buf) {
    switch (messageType) {
      case HANDSHAKE_TYPE.CLIENT_HELLO:
        buf.writeVector8(buf => {
          this.versions.forEach(version => {
            buf.writeUint16(version);
          });
        });
        break;
      case HANDSHAKE_TYPE.SERVER_HELLO:
        buf.writeUint16(this.selectedVersion);
        break;
      default:
        throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
  }
}


class extensions_PskKeyExchangeModesExtension extends extensions_Extension {
  constructor(modes) {
    super();
    this.modes = modes;
  }

  get TYPE_TAG() {
    return EXTENSION_TYPE.PSK_KEY_EXCHANGE_MODES;
  }

  static _read(messageType, buf) {
    const modes = [];
    switch (messageType) {
      case HANDSHAKE_TYPE.CLIENT_HELLO:
        buf.readVector8(buf => {
          modes.push(buf.readUint8());
        });
        break;
      default:
        throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    return new this(modes);
  }

  _write(messageType, buf) {
    switch (messageType) {
      case HANDSHAKE_TYPE.CLIENT_HELLO:
        buf.writeVector8(buf => {
          this.modes.forEach(mode => {
            buf.writeUint8(mode);
          });
        });
        break;
      default:
        throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
  }
}

// CONCATENATED MODULE: ./src/constants.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const VERSION_TLS_1_0 = 0x0301;
const VERSION_TLS_1_2 = 0x0303;
const VERSION_TLS_1_3 = 0x0304;
const TLS_AES_128_GCM_SHA256 = 0x1301;
const PSK_MODE_KE = 0;

// CONCATENATED MODULE: ./src/messages.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Message parsing.
//
// Herein we have code for reading and writing the various Handshake
// messages involved in the TLS protocol.
//







/* eslint-disable sorting/sort-object-props */
const HANDSHAKE_TYPE = {
  CLIENT_HELLO: 1,
  SERVER_HELLO: 2,
  NEW_SESSION_TICKET: 4,
  ENCRYPTED_EXTENSIONS: 8,
  FINISHED: 20,
};
/* eslint-enable sorting/sort-object-props */

// Base class for generic reading/writing of handshake messages,
// which are all uniformly formatted as:
//
//  struct {
//    HandshakeType msg_type;    /* handshake type */
//    uint24 length;             /* bytes in message */
//    select(Handshake.msg_type) {
//        ... type specific cases here ...
//    };
//  } Handshake;

class messages_HandshakeMessage {

  get TYPE_TAG() {
    throw new Error('not implemented');
  }

  static fromBytes(bytes) {
    // Each handshake message has a type and length prefix, per
    // https://tools.ietf.org/html/rfc8446#appendix-B.3
    const buf = new utils_BufferReader(bytes);
    const msg = this.read(buf);
    if (buf.hasMoreBytes()) {
      throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
    }
    return msg;
  }

  toBytes() {
    const buf = new utils_BufferWriter();
    this.write(buf);
    return buf.flush();
  }

  static read(buf) {
    const type = buf.readUint8();
    let msg = null;
    buf.readVector24(buf => {
      switch (type) {
        case HANDSHAKE_TYPE.CLIENT_HELLO:
          msg = messages_ClientHello._read(buf);
          break;
        case HANDSHAKE_TYPE.SERVER_HELLO:
          msg = messages_ServerHello._read(buf);
          break;
        case HANDSHAKE_TYPE.NEW_SESSION_TICKET:
          msg = messages_NewSessionTicket._read(buf);
          break;
        case HANDSHAKE_TYPE.ENCRYPTED_EXTENSIONS:
          msg = EncryptedExtensions._read(buf);
          break;
        case HANDSHAKE_TYPE.FINISHED:
          msg = messages_Finished._read(buf);
          break;
      }
      if (buf.hasMoreBytes()) {
        throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
      }
    });
    if (msg === null) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    return msg;
  }

  write(buf) {
    buf.writeUint8(this.TYPE_TAG);
    buf.writeVector24(buf => {
      this._write(buf);
    });
  }

  static _read(buf) {
    throw new Error('not implemented');
  }

  _write(buf) {
    throw new Error('not implemented');
  }

  // Some little helpers for reading a list of extensions,
  // which is uniformly represented as:
  //
  //   Extension extensions<8..2^16-1>;
  //
  // Recognized extensions are returned as a Map from extension type
  // to extension data object, with a special `lastSeenExtension`
  // property to make it easy to check which one came last.

  static _readExtensions(messageType, buf) {
    const extensions = new Map();
    buf.readVector16(buf => {
      const ext = extensions_Extension.read(messageType, buf);
      if (extensions.has(ext.TYPE_TAG)) {
        throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
      }
      extensions.set(ext.TYPE_TAG, ext);
      extensions.lastSeenExtension = ext.TYPE_TAG;
    });
    return extensions;
  }

  _writeExtensions(buf, extensions) {
    buf.writeVector16(buf => {
      extensions.forEach(ext => {
        ext.write(this.TYPE_TAG, buf);
      });
    });
  }
}


// The ClientHello message:
//
// struct {
//   ProtocolVersion legacy_version = 0x0303;
//   Random random;
//   opaque legacy_session_id<0..32>;
//   CipherSuite cipher_suites<2..2^16-2>;
//   opaque legacy_compression_methods<1..2^8-1>;
//   Extension extensions<8..2^16-1>;
// } ClientHello;

class messages_ClientHello extends messages_HandshakeMessage {

  constructor(random, sessionId, extensions) {
    super();
    this.random = random;
    this.sessionId = sessionId;
    this.extensions = extensions;
  }

  get TYPE_TAG() {
    return HANDSHAKE_TYPE.CLIENT_HELLO;
  }

  static _read(buf) {
    // The legacy_version field may indicate an earlier version of TLS
    // for backwards compatibility, but must not predate TLS 1.0!
    if (buf.readUint16() < VERSION_TLS_1_0) {
      throw new TLSError(ALERT_DESCRIPTION.PROTOCOL_VERSION);
    }
    // The random bytes provided by the peer.
    const random = buf.readBytes(32);
    // Read legacy_session_id, so the server can echo it.
    const sessionId = buf.readVectorBytes8();
    // We only support a single ciphersuite, but the peer may offer several.
    // Scan the list to confirm that the one we want is present.
    let found = false;
    buf.readVector16(buf => {
      const cipherSuite = buf.readUint16();
      if (cipherSuite === TLS_AES_128_GCM_SHA256) {
        found = true;
      }
    });
    if (! found) {
      throw new TLSError(ALERT_DESCRIPTION.HANDSHAKE_FAILURE);
    }
    // legacy_compression_methods must be a single zero byte for TLS1.3 ClientHellos.
    // It can be non-zero in previous versions of TLS, but we're not going to
    // make a successful handshake with such versions, so better to just bail out now.
    const legacyCompressionMethods = buf.readVectorBytes8();
    if (legacyCompressionMethods.byteLength !== 1) {
      throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    if (legacyCompressionMethods[0] !== 0x00) {
      throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    // Read and check the extensions.
    const extensions = this._readExtensions(HANDSHAKE_TYPE.CLIENT_HELLO, buf);
    if (! extensions.has(EXTENSION_TYPE.SUPPORTED_VERSIONS)) {
      throw new TLSError(ALERT_DESCRIPTION.MISSING_EXTENSION);
    }
    if (extensions.get(EXTENSION_TYPE.SUPPORTED_VERSIONS).versions.indexOf(VERSION_TLS_1_3) === -1) {
      throw new TLSError(ALERT_DESCRIPTION.PROTOCOL_VERSION);
    }
    // Was the PreSharedKey extension the last one?
    if (extensions.has(EXTENSION_TYPE.PRE_SHARED_KEY)) {
      if (extensions.lastSeenExtension !== EXTENSION_TYPE.PRE_SHARED_KEY) {
        throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
      }
    }
    return new this(random, sessionId, extensions);
  }

  _write(buf) {
    buf.writeUint16(VERSION_TLS_1_2);
    buf.writeBytes(this.random);
    buf.writeVectorBytes8(this.sessionId);
    // Our single supported ciphersuite
    buf.writeVector16(buf => {
      buf.writeUint16(TLS_AES_128_GCM_SHA256);
    });
    // A single zero byte for legacy_compression_methods
    buf.writeVectorBytes8(new Uint8Array(1));
    this._writeExtensions(buf, this.extensions);
  }
}


// The ServerHello message:
//
//  struct {
//      ProtocolVersion legacy_version = 0x0303;    /* TLS v1.2 */
//      Random random;
//      opaque legacy_session_id_echo<0..32>;
//      CipherSuite cipher_suite;
//      uint8 legacy_compression_method = 0;
//      Extension extensions < 6..2 ^ 16 - 1 >;
//  } ServerHello;

class messages_ServerHello extends messages_HandshakeMessage {

  constructor(random, sessionId, extensions) {
    super();
    this.random = random;
    this.sessionId = sessionId;
    this.extensions = extensions;
  }

  get TYPE_TAG() {
    return HANDSHAKE_TYPE.SERVER_HELLO;
  }

  static _read(buf) {
    // Fixed value for legacy_version.
    if (buf.readUint16() !== VERSION_TLS_1_2) {
      throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    // Random bytes from the server.
    const random = buf.readBytes(32);
    // It should have echoed our vector for legacy_session_id.
    const sessionId = buf.readVectorBytes8();
    // It should have selected our single offered ciphersuite.
    if (buf.readUint16() !== TLS_AES_128_GCM_SHA256) {
      throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    // legacy_compression_method must be zero.
    if (buf.readUint8() !== 0) {
      throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    const extensions = this._readExtensions(HANDSHAKE_TYPE.SERVER_HELLO, buf);
    if (! extensions.has(EXTENSION_TYPE.SUPPORTED_VERSIONS)) {
      throw new TLSError(ALERT_DESCRIPTION.MISSING_EXTENSION);
    }
    if (extensions.get(EXTENSION_TYPE.SUPPORTED_VERSIONS).selectedVersion !== VERSION_TLS_1_3) {
      throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    return new this(random, sessionId, extensions);
  }

  _write(buf) {
    buf.writeUint16(VERSION_TLS_1_2);
    buf.writeBytes(this.random);
    buf.writeVectorBytes8(this.sessionId);
    // Our single supported ciphersuite
    buf.writeUint16(TLS_AES_128_GCM_SHA256);
    // A single zero byte for legacy_compression_method
    buf.writeUint8(0);
    this._writeExtensions(buf, this.extensions);
  }
}


// The EncryptedExtensions message:
//
//  struct {
//    Extension extensions < 0..2 ^ 16 - 1 >;
//  } EncryptedExtensions;
//
// We don't actually send any EncryptedExtensions,
// but still have to send an empty message.

class EncryptedExtensions extends messages_HandshakeMessage {
  constructor(extensions) {
    super();
    this.extensions = extensions;
  }

  get TYPE_TAG() {
    return HANDSHAKE_TYPE.ENCRYPTED_EXTENSIONS;
  }

  static _read(buf) {
    const extensions = this._readExtensions(HANDSHAKE_TYPE.ENCRYPTED_EXTENSIONS, buf);
    return new this(extensions);
  }

  _write(buf) {
    this._writeExtensions(buf, this.extensions);
  }
}


// The Finished message:
//
// struct {
//   opaque verify_data[Hash.length];
// } Finished;

class messages_Finished extends messages_HandshakeMessage {

  constructor(verifyData) {
    super();
    this.verifyData = verifyData;
  }

  get TYPE_TAG() {
    return HANDSHAKE_TYPE.FINISHED;
  }

  static _read(buf) {
    const verifyData = buf.readBytes(HASH_LENGTH);
    return new this(verifyData);
  }

  _write(buf) {
    buf.writeBytes(this.verifyData);
  }
}


// The NewSessionTicket message:
//
//   struct {
//    uint32 ticket_lifetime;
//    uint32 ticket_age_add;
//    opaque ticket_nonce < 0..255 >;
//    opaque ticket < 1..2 ^ 16 - 1 >;
//    Extension extensions < 0..2 ^ 16 - 2 >;
//  } NewSessionTicket;
//
// We don't actually make use of these, but we need to be able
// to accept them and do basic validation.

class messages_NewSessionTicket extends messages_HandshakeMessage {
  constructor(ticketLifetime, ticketAgeAdd, ticketNonce, ticket, extensions) {
    super();
    this.ticketLifetime = ticketLifetime;
    this.ticketAgeAdd = ticketAgeAdd;
    this.ticketNonce = ticketNonce;
    this.ticket = ticket;
    this.extensions = extensions;
  }

  get TYPE_TAG() {
    return HANDSHAKE_TYPE.NEW_SESSION_TICKET;
  }

  static _read(buf) {
    const ticketLifetime = buf.readUint32();
    const ticketAgeAdd = buf.readUint32();
    const ticketNonce = buf.readVectorBytes8();
    const ticket = buf.readVectorBytes16();
    if (ticket.byteLength < 1) {
      throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
    }
    const extensions = this._readExtensions(HANDSHAKE_TYPE.NEW_SESSION_TICKET, buf);
    return new this(ticketLifetime, ticketAgeAdd, ticketNonce, ticket, extensions);
  }

  _write(buf) {
    buf.writeUint32(this.ticketLifetime);
    buf.writeUint32(this.ticketAgeAdd);
    buf.writeVectorBytes8(this.ticketNonce);
    buf.writeVectorBytes16(this.ticket);
    this._writeExtensions(buf, this.extensions);
  }
}

// CONCATENATED MODULE: ./src/states.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */








//
// State-machine for TLS Handshake Management.
//
// Internally, we manage the TLS connection by explicitly modelling the
// client and server state-machines from RFC8446.  You can think of
// these `State` objects as little plugins for the `Connection` class
// that provide different behaviours of `send` and `receive` depending
// on the state of the connection.
//

class states_State {

  constructor(conn) {
    this.conn = conn;
  }

  async initialize() {
    // By default, nothing to do when entering the state.
  }

  async sendApplicationData(bytes) {
    // By default, assume we're not ready to send yet and the caller
    // should be blocking on the connection promise before reaching here.
    throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
  }

  async recvApplicationData(bytes) {
    throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
  }

  async recvHandshakeMessage(msg) {
    throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
  }

  async recvAlertMessage(alert) {
    switch (alert.description) {
      case ALERT_DESCRIPTION.CLOSE_NOTIFY:
        this.conn._closeForRecv(alert);
        throw alert;
      default:
        return await this.handleErrorAndRethrow(alert);
    }
  }

  async recvChangeCipherSpec(bytes) {
    throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
  }

  async handleErrorAndRethrow(err) {
    let alert = err;
    if (! (alert instanceof TLSAlert)) {
      alert = new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    // Try to send error alert to the peer, but we may not
    // be able to if the outgoing connection was already closed.
    try {
      await this.conn._sendAlertMessage(alert);
    } catch (_) { }
    await this.conn._transition(ERROR, err);
    throw err;
  }

  async close() {
    const alert = new TLSCloseNotify();
    await this.conn._sendAlertMessage(alert);
    this.conn._closeForSend(alert);
  }

}

// A special "guard" state to prevent us from using
// an improperly-initialized Connection.

class UNINITIALIZED extends states_State {
  async initialize() {
    throw new Error('uninitialized state');
  }
  async sendApplicationData(bytes) {
    throw new Error('uninitialized state');
  }
  async recvApplicationData(bytes) {
    throw new Error('uninitialized state');
  }
  async recvHandshakeMessage(msg) {
    throw new Error('uninitialized state');
  }
  async recvChangeCipherSpec(bytes) {
    throw new Error('uninitialized state');
  }
  async handleErrorAndRethrow(err) {
    throw err;
  }
  async close() {
    throw new Error('uninitialized state');
  }
}

// A special "error" state for when something goes wrong.
// This state never transitions to another state, effectively
// terminating the connection.

class ERROR extends states_State {
  async initialize(err) {
    this.error = err;
    this.conn._setConnectionFailure(err);
    // Unceremoniously shut down the record layer on error.
    this.conn._recordlayer.setSendError(err);
    this.conn._recordlayer.setRecvError(err);
  }
  async sendApplicationData(bytes) {
    throw this.error;
  }
  async recvApplicationData(bytes) {
    throw this.error;
  }
  async recvHandshakeMessage(msg) {
    throw this.error;
  }
  async recvAlertMessage(err) {
    throw this.error;
  }
  async recvChangeCipherSpec(bytes) {
    throw this.error;
  }
  async handleErrorAndRethrow(err) {
    throw err;
  }
  async close() {
    throw this.error;
  }
}

// The "connected" state, for when the handshake is complete
// and we're ready to send application-level data.
// The logic for this is largely symmetric between client and server.

class states_CONNECTED extends states_State {
  async initialize() {
    this.conn._setConnectionSuccess();
  }
  async sendApplicationData(bytes) {
    await this.conn._sendApplicationData(bytes);
  }
  async recvApplicationData(bytes) {
    return bytes;
  }
  async recvChangeCipherSpec(bytes) {
    throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
  }
}

// A base class for states that occur in the middle of the handshake
// (that is, between ClientHello and Finished).  These states may receive
// CHANGE_CIPHER_SPEC records for b/w compat reasons, which must contain
// exactly a single 0x01 byte and must otherwise be ignored.

class states_MidHandshakeState extends states_State {
  async recvChangeCipherSpec(bytes) {
    if (this.conn._hasSeenChangeCipherSpec) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    if (bytes.byteLength !== 1 || bytes[0] !== 1) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    this.conn._hasSeenChangeCipherSpec = true;
  }
}

// These states implement (part of) the client state-machine from
// https://tools.ietf.org/html/rfc8446#appendix-A.1
//
// Since we're only implementing a small subset of TLS1.3,
// we only need a small subset of the handshake.  It basically goes:
//
//   * send ClientHello
//   * receive ServerHello
//   * receive EncryptedExtensions
//   * receive server Finished
//   * send client Finished
//
// We include some unused states for completeness, so that it's easier
// to check the implementation against the diagrams in the RFC.

class states_CLIENT_START extends states_State {
  async initialize() {
    const keyschedule = this.conn._keyschedule;
    await keyschedule.addPSK(this.conn.psk);
    // Construct a ClientHello message with our single PSK.
    // We can't know the PSK binder value yet, so we initially write zeros.
    const clientHello = new messages_ClientHello(
      // Client random salt.
      await getRandomBytes(32),
      // Random legacy_session_id; we *could* send an empty string here,
      // but sending a random one makes it easier to be compatible with
      // the data emitted by tlslite-ng for test-case generation.
      await getRandomBytes(32),
      [
        new extensions_SupportedVersionsExtension([VERSION_TLS_1_3]),
        new extensions_PskKeyExchangeModesExtension([PSK_MODE_KE]),
        new extensions_PreSharedKeyExtension([this.conn.pskId], [zeros(HASH_LENGTH)]),
      ],
    );
    const buf = new utils_BufferWriter();
    clientHello.write(buf);
    // Now that we know what the ClientHello looks like,
    // go back and calculate the appropriate PSK binder value.
    // We only support a single PSK, so the length of the binders field is the
    // length of the hash plus one for rendering it as a variable-length byte array,
    // plus two for rendering the variable-length list of PSK binders.
    const PSK_BINDERS_SIZE = HASH_LENGTH + 1 + 2;
    const truncatedTranscript = buf.slice(0, buf.tell() - PSK_BINDERS_SIZE);
    const pskBinder = await keyschedule.calculateFinishedMAC(keyschedule.extBinderKey, truncatedTranscript);
    buf.incr(-HASH_LENGTH);
    buf.writeBytes(pskBinder);
    await this.conn._sendHandshakeMessageBytes(buf.flush());
    await this.conn._transition(states_CLIENT_WAIT_SH, clientHello.sessionId);
  }
}

class states_CLIENT_WAIT_SH extends states_State {
  async initialize(sessionId) {
    this._sessionId = sessionId;
  }
  async recvHandshakeMessage(msg) {
    if (! (msg instanceof messages_ServerHello)) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    if (! bytesAreEqual(msg.sessionId, this._sessionId)) {
      throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    const pskExt = msg.extensions.get(EXTENSION_TYPE.PRE_SHARED_KEY);
    if (! pskExt) {
      throw new TLSError(ALERT_DESCRIPTION.MISSING_EXTENSION);
    }
    // We expect only the SUPPORTED_VERSIONS and PRE_SHARED_KEY extensions.
    if (msg.extensions.size !== 2) {
      throw new TLSError(ALERT_DESCRIPTION.UNSUPPORTED_EXTENSION);
    }
    if (pskExt.selectedIdentity !== 0) {
      throw new TLSError(ALERT_DESCRIPTION.ILLEGAL_PARAMETER);
    }
    await this.conn._keyschedule.addECDHE(null);
    await this.conn._setSendKey(this.conn._keyschedule.clientHandshakeTrafficSecret);
    await this.conn._setRecvKey(this.conn._keyschedule.serverHandshakeTrafficSecret);
    await this.conn._transition(states_CLIENT_WAIT_EE);
  }
}

class states_CLIENT_WAIT_EE extends states_MidHandshakeState {
  async recvHandshakeMessage(msg) {
    // We don't make use of any encrypted extensions, but we still
    // have to wait for the server to send the (empty) list of them.
    if (! (msg instanceof EncryptedExtensions)) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    // We do not support any EncryptedExtensions.
    if (msg.extensions.size !== 0) {
      throw new TLSError(ALERT_DESCRIPTION.UNSUPPORTED_EXTENSION);
    }
    const keyschedule = this.conn._keyschedule;
    const serverFinishedTranscript = keyschedule.getTranscript();
    await this.conn._transition(states_CLIENT_WAIT_FINISHED, serverFinishedTranscript);
  }
}

class states_CLIENT_WAIT_FINISHED extends states_State {
  async initialize(serverFinishedTranscript) {
    this._serverFinishedTranscript = serverFinishedTranscript;
  }
  async recvHandshakeMessage(msg) {
    if (! (msg instanceof messages_Finished)) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    // Verify server Finished MAC.
    const keyschedule = this.conn._keyschedule;
    await keyschedule.verifyFinishedMAC(keyschedule.serverHandshakeTrafficSecret, msg.verifyData, this._serverFinishedTranscript);
    // Send our own Finished message in return.
    // This must be encrypted with the handshake traffic key,
    // but must not appear in the transcript used to calculate the application keys.
    const clientFinishedMAC = await keyschedule.calculateFinishedMAC(keyschedule.clientHandshakeTrafficSecret);
    await keyschedule.finalize();
    await this.conn._sendHandshakeMessage(new messages_Finished(clientFinishedMAC));
    await this.conn._setSendKey(keyschedule.clientApplicationTrafficSecret);
    await this.conn._setRecvKey(keyschedule.serverApplicationTrafficSecret);
    await this.conn._transition(states_CLIENT_CONNECTED);
  }
}

class states_CLIENT_CONNECTED extends states_CONNECTED {
  async recvHandshakeMessage(msg) {
    // A connected client must be prepared to accept NewSessionTicket
    // messages.  We never use them, but other server implementations
    // might send them.
    if (! (msg instanceof messages_NewSessionTicket)) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
  }
}

// These states implement (part of) the server state-machine from
// https://tools.ietf.org/html/rfc8446#appendix-A.2
//
// Since we're only implementing a small subset of TLS1.3,
// we only need a small subset of the handshake.  It basically goes:
//
//   * receive ClientHello
//   * send ServerHello
//   * send empty EncryptedExtensions
//   * send server Finished
//   * receive client Finished
//
// We include some unused states for completeness, so that it's easier
// to check the implementation against the diagrams in the RFC.

class states_SERVER_START extends states_State {
  async recvHandshakeMessage(msg) {
    if (! (msg instanceof messages_ClientHello)) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    // In the spec, this is where we select connection parameters, and maybe
    // tell the client to try again if we can't find a compatible set.
    // Since we only support a fixed cipherset, the only thing to "negotiate"
    // is whether they provided an acceptable PSK.
    const pskExt = msg.extensions.get(EXTENSION_TYPE.PRE_SHARED_KEY);
    const pskModesExt = msg.extensions.get(EXTENSION_TYPE.PSK_KEY_EXCHANGE_MODES);
    if (! pskExt || ! pskModesExt) {
      throw new TLSError(ALERT_DESCRIPTION.MISSING_EXTENSION);
    }
    if (pskModesExt.modes.indexOf(PSK_MODE_KE) === -1) {
      throw new TLSError(ALERT_DESCRIPTION.HANDSHAKE_FAILURE);
    }
    const pskIndex = pskExt.identities.findIndex(pskId => bytesAreEqual(pskId, this.conn.pskId));
    if (pskIndex === -1) {
      throw new TLSError(ALERT_DESCRIPTION.UNKNOWN_PSK_IDENTITY);
    }
    await this.conn._keyschedule.addPSK(this.conn.psk);
    // Validate the PSK binder.
    const keyschedule = this.conn._keyschedule;
    const transcript = keyschedule.getTranscript();
    // Calculate size occupied by the PSK binders.
    let pskBindersSize = 2; // Vector16 representation overhead.
    for (const binder of pskExt.binders) {
      pskBindersSize += binder.byteLength + 1; // Vector8 representation overhead.
    }
    await keyschedule.verifyFinishedMAC(keyschedule.extBinderKey, pskExt.binders[pskIndex], transcript.slice(0, -pskBindersSize));
    await this.conn._transition(states_SERVER_NEGOTIATED, msg.sessionId, pskIndex);
  }
}

class states_SERVER_NEGOTIATED extends states_MidHandshakeState {
  async initialize(sessionId, pskIndex) {
    await this.conn._sendHandshakeMessage(new messages_ServerHello(
      // Server random
      await getRandomBytes(32),
      sessionId,
      [
        new extensions_SupportedVersionsExtension(null, VERSION_TLS_1_3),
        new extensions_PreSharedKeyExtension(null, null, pskIndex),
      ]
    ));
    // If the client sent a non-empty sessionId, the server *must* send a change-cipher-spec for b/w compat.
    if (sessionId.byteLength > 0) {
      await this.conn._sendChangeCipherSpec();
    }
    // We can now transition to the encrypted part of the handshake.
    const keyschedule = this.conn._keyschedule;
    await keyschedule.addECDHE(null);
    await this.conn._setSendKey(keyschedule.serverHandshakeTrafficSecret);
    await this.conn._setRecvKey(keyschedule.clientHandshakeTrafficSecret);
    // Send an empty EncryptedExtensions message.
    await this.conn._sendHandshakeMessage(new EncryptedExtensions([]));
    // Send the Finished message.
    const serverFinishedMAC = await keyschedule.calculateFinishedMAC(keyschedule.serverHandshakeTrafficSecret);
    await this.conn._sendHandshakeMessage(new messages_Finished(serverFinishedMAC));
    // We can now *send* using the application traffic key,
    // but have to wait to receive the client Finished before receiving under that key.
    // We need to remember the handshake state from before the client Finished
    // in order to successfully verify the client Finished.
    const clientFinishedTranscript = await keyschedule.getTranscript();
    const clientHandshakeTrafficSecret = keyschedule.clientHandshakeTrafficSecret;
    await keyschedule.finalize();
    await this.conn._setSendKey(keyschedule.serverApplicationTrafficSecret);
    await this.conn._transition(states_SERVER_WAIT_FINISHED, clientHandshakeTrafficSecret, clientFinishedTranscript);
  }
}

class states_SERVER_WAIT_FINISHED extends states_MidHandshakeState {
  async initialize(clientHandshakeTrafficSecret, clientFinishedTranscript) {
    this._clientHandshakeTrafficSecret = clientHandshakeTrafficSecret;
    this._clientFinishedTranscript = clientFinishedTranscript;
  }
  async recvHandshakeMessage(msg) {
    if (! (msg instanceof messages_Finished)) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    const keyschedule = this.conn._keyschedule;
    await keyschedule.verifyFinishedMAC(this._clientHandshakeTrafficSecret, msg.verifyData, this._clientFinishedTranscript);
    this._clientHandshakeTrafficSecret = this._clientFinishedTranscript = null;
    await this.conn._setRecvKey(keyschedule.clientApplicationTrafficSecret);
    await this.conn._transition(states_CONNECTED);
  }
}

// CONCATENATED MODULE: ./src/keyschedule.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TLS1.3 Key Schedule.
//
// In this file we implement the "key schedule" from
// https://tools.ietf.org/html/rfc8446#section-7.1, which
// defines how to calculate various keys as the handshake
// state progresses.







// The `KeySchedule` class progresses through three stages corresponding
// to the three phases of the TLS1.3 key schedule:
//
//   UNINITIALIZED
//       |
//       | addPSK()
//       v
//   EARLY_SECRET
//       |
//       | addECDHE()
//       v
//   HANDSHAKE_SECRET
//       |
//       | finalize()
//       v
//   MASTER_SECRET
//
// It will error out if the calling code attempts to add key material
// in the wrong order.

const STAGE_UNINITIALIZED = 0;
const STAGE_EARLY_SECRET = 1;
const STAGE_HANDSHAKE_SECRET = 2;
const STAGE_MASTER_SECRET = 3;

class keyschedule_KeySchedule {
  constructor() {
    this.stage = STAGE_UNINITIALIZED;
    // WebCrypto doesn't support a rolling hash construct, so we have to
    // keep the entire message transcript in memory.
    this.transcript = new utils_BufferWriter();
    // This tracks the main secret from with other keys are derived at each stage.
    this.secret = null;
    // And these are all the various keys we'll derive as the handshake progresses.
    this.extBinderKey = null;
    this.clientHandshakeTrafficSecret = null;
    this.serverHandshakeTrafficSecret = null;
    this.clientApplicationTrafficSecret = null;
    this.serverApplicationTrafficSecret = null;
  }

  async addPSK(psk) {
    // Use the selected PSK (if any) to calculate the "early secret".
    if (psk === null) {
      psk = zeros(HASH_LENGTH);
    }
    if (this.stage !== STAGE_UNINITIALIZED) {
      throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    this.stage = STAGE_EARLY_SECRET;
    this.secret = await hkdfExtract(zeros(HASH_LENGTH), psk);
    this.extBinderKey = await this.deriveSecret('ext binder', EMPTY);
    this.secret = await this.deriveSecret('derived', EMPTY);
  }

  async addECDHE(ecdhe) {
    // Mix in the ECDHE output (if any) to calculate the "handshake secret".
    if (ecdhe === null) {
      ecdhe = zeros(HASH_LENGTH);
    }
    if (this.stage !== STAGE_EARLY_SECRET) {
      throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    this.stage = STAGE_HANDSHAKE_SECRET;
    this.extBinderKey = null;
    this.secret = await hkdfExtract(this.secret, ecdhe);
    this.clientHandshakeTrafficSecret = await this.deriveSecret('c hs traffic');
    this.serverHandshakeTrafficSecret = await this.deriveSecret('s hs traffic');
    this.secret = await this.deriveSecret('derived', EMPTY);
  }

  async finalize() {
    if (this.stage !== STAGE_HANDSHAKE_SECRET) {
      throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    this.stage = STAGE_MASTER_SECRET;
    this.clientHandshakeTrafficSecret = null;
    this.serverHandshakeTrafficSecret = null;
    this.secret = await hkdfExtract(this.secret, zeros(HASH_LENGTH));
    this.clientApplicationTrafficSecret = await this.deriveSecret('c ap traffic');
    this.serverApplicationTrafficSecret = await this.deriveSecret('s ap traffic');
    this.secret = null;
  }

  addToTranscript(bytes) {
    this.transcript.writeBytes(bytes);
  }

  getTranscript() {
    return this.transcript.slice();
  }

  async deriveSecret(label, transcript = undefined) {
    transcript = transcript || this.getTranscript();
    return await hkdfExpandLabel(this.secret, label, await hash(transcript), HASH_LENGTH);
  }

  async calculateFinishedMAC(baseKey, transcript = undefined) {
    transcript = transcript || this.getTranscript();
    const finishedKey = await hkdfExpandLabel(baseKey, 'finished', EMPTY, HASH_LENGTH);
    return await hmac(finishedKey, await hash(transcript));
  }

  async verifyFinishedMAC(baseKey, mac, transcript = undefined) {
    transcript = transcript || this.getTranscript();
    const finishedKey = await hkdfExpandLabel(baseKey, 'finished', EMPTY, HASH_LENGTH);
    await verifyHmac(finishedKey, mac, await hash(transcript));
  }
}

// CONCATENATED MODULE: ./src/recordlayer.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// This file implements the "record layer" for TLS1.3, as defined in
// https://tools.ietf.org/html/rfc8446#section-5.
//
// The record layer is responsible for encrypting/decrypting bytes to be
// sent over the wire, including stateful management of sequence numbers
// for the incoming and outgoing stream.
//
// The main interface is the RecordLayer class, which takes a callback function
// sending data and can be used like so:
//
//    rl = new RecordLayer(async function send_encrypted_data(data) {
//      // application-specific sending logic here.
//    });
//
//    // Records are sent and received in plaintext by default,
//    // until you specify the key to use.
//    await rl.setSendKey(key)
//
//    // Send some data by specifying the record type and the bytes.
//    // Where allowed by the record type, it will be buffered until
//    // explicitly flushed, and then sent by calling the callback.
//    await rl.send(RECORD_TYPE.HANDSHAKE, <bytes for a handshake message>)
//    await rl.send(RECORD_TYPE.HANDSHAKE, <bytes for another handshake message>)
//    await rl.flush()
//
//    // Separate keys are used for sending and receiving.
//    rl.setRecvKey(key);
//
//    // When data is received, push it into the RecordLayer
//    // and pass a callback that will be called with a [type, bytes]
//    // pair for each message parsed from the data.
//    rl.recv(dataReceivedFromPeer, async (type, bytes) => {
//      switch (type) {
//        case RECORD_TYPE.APPLICATION_DATA:
//          // do something with application data
//        case RECORD_TYPE.HANDSHAKE:
//          // do something with a handshake message
//        default:
//          // etc...
//      }
//    });
//







/* eslint-disable sorting/sort-object-props */
const RECORD_TYPE = {
  CHANGE_CIPHER_SPEC: 20,
  ALERT: 21,
  HANDSHAKE: 22,
  APPLICATION_DATA: 23,
};
/* eslint-enable sorting/sort-object-props */

// Encrypting at most 2^24 records will force us to stay
// below data limits on AES-GCM encryption key use, and also
// means we can accurately represent the sequence number as
// a javascript double.
const MAX_SEQUENCE_NUMBER = Math.pow(2, 24);
const MAX_RECORD_SIZE = Math.pow(2, 14);
const MAX_ENCRYPTED_RECORD_SIZE = MAX_RECORD_SIZE + 256;
const RECORD_HEADER_SIZE = 5;

// These are some helper classes to manage the encryption/decryption state
// for a particular key.

class recordlayer_CipherState {
  constructor(key, iv) {
    this.key = key;
    this.iv = iv;
    this.seqnum = 0;
  }

  static async create(baseKey, mode) {
    // Derive key and iv per https://tools.ietf.org/html/rfc8446#section-7.3
    const key = await prepareKey(await hkdfExpandLabel(baseKey, 'key', EMPTY, KEY_LENGTH), mode);
    const iv = await hkdfExpandLabel(baseKey, 'iv', EMPTY, IV_LENGTH);
    return new this(key, iv);
  }

  nonce() {
    // Ref https://tools.ietf.org/html/rfc8446#section-5.3:
    // * left-pad the sequence number with zeros to IV_LENGTH
    // * xor with the provided iv
    // Our sequence numbers are always less than 2^24, so fit in a Uint32
    // in the last 4 bytes of the nonce.
    const nonce = this.iv.slice();
    const dv = new DataView(nonce.buffer, nonce.byteLength - 4, 4);
    dv.setUint32(0, dv.getUint32(0) ^ this.seqnum);
    this.seqnum += 1;
    if (this.seqnum > MAX_SEQUENCE_NUMBER) {
      throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    return nonce;
  }
}

class recordlayer_EncryptionState extends recordlayer_CipherState {
  static async create(key) {
    return super.create(key, 'encrypt');
  }

  async encrypt(plaintext, additionalData) {
    return await encrypt(this.key, this.nonce(), plaintext, additionalData);
  }
}

class recordlayer_DecryptionState extends recordlayer_CipherState {
  static async create(key) {
    return super.create(key, 'decrypt');
  }

  async decrypt(ciphertext, additionalData) {
    return await decrypt(this.key, this.nonce(), ciphertext, additionalData);
  }
}

// The main RecordLayer class.

class recordlayer_RecordLayer {
  constructor(sendCallback) {
    this.sendCallback = sendCallback;
    this._sendEncryptState = null;
    this._sendError = null;
    this._recvDecryptState = null;
    this._recvError = null;
    this._pendingRecordType = 0;
    this._pendingRecordBuf = null;
  }

  async setSendKey(key) {
    await this.flush();
    this._sendEncryptState = await recordlayer_EncryptionState.create(key);
  }

  async setRecvKey(key) {
    this._recvDecryptState = await recordlayer_DecryptionState.create(key);
  }

  async setSendError(err) {
    this._sendError = err;
  }

  async setRecvError(err) {
    this._recvError = err;
  }

  async send(type, data) {
    if (this._sendError !== null) {
      throw this._sendError;
    }
    // Forbid sending data that doesn't fit into a single record.
    // We do not support fragmentation over multiple records.
    if (data.byteLength > MAX_RECORD_SIZE) {
      throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
    }
    // Flush if we're switching to a different record type.
    if (this._pendingRecordType && this._pendingRecordType !== type) {
      await this.flush();
    }
    // Flush if we would overflow the max size of a record.
    if (this._pendingRecordBuf !== null) {
      if (this._pendingRecordBuf.tell() + data.byteLength > MAX_RECORD_SIZE) {
        await this.flush();
      }
    }
    // Start a new pending record if necessary.
    // We reserve space at the start of the buffer for the record header,
    // which is conveniently always a fixed size.
    if (this._pendingRecordBuf === null) {
      this._pendingRecordType = type;
      this._pendingRecordBuf = new utils_BufferWriter();
      this._pendingRecordBuf.incr(RECORD_HEADER_SIZE);
    }
    this._pendingRecordBuf.writeBytes(data);
  }

  async flush() {
    // If there's nothing to flush, bail out early.
    // Don't throw `_sendError` if we're not sending anything, because `flush()`
    // can be called when we're trying to transition into an error state.
    const buf = this._pendingRecordBuf;
    let type = this._pendingRecordType;
    if (! type) {
      if (buf !== null) {
        throw new TLSError(ALERT_DESCRIPTION.INTERNAL_ERROR);
      }
      return;
    }
    if (this._sendError !== null) {
      throw this._sendError;
    }
    // If we're encrypting, turn the existing buffer contents into a `TLSInnerPlaintext` by
    // appending the type. We don't do any zero-padding, although the spec allows it.
    let inflation = 0, innerPlaintext = null;
    if (this._sendEncryptState !== null) {
      buf.writeUint8(type);
      innerPlaintext = buf.slice(RECORD_HEADER_SIZE);
      inflation = AEAD_SIZE_INFLATION;
      type = RECORD_TYPE.APPLICATION_DATA;
    }
    // Write the common header for either `TLSPlaintext` or `TLSCiphertext` record.
    const length = buf.tell() - RECORD_HEADER_SIZE + inflation;
    buf.seek(0);
    buf.writeUint8(type);
    buf.writeUint16(VERSION_TLS_1_2);
    buf.writeUint16(length);
    // Followed by different payload depending on encryption status.
    if (this._sendEncryptState !== null) {
      const additionalData = buf.slice(0, RECORD_HEADER_SIZE);
      const ciphertext = await this._sendEncryptState.encrypt(innerPlaintext, additionalData);
      buf.writeBytes(ciphertext);
    } else {
      buf.incr(length);
    }
    this._pendingRecordBuf = null;
    this._pendingRecordType = 0;
    await this.sendCallback(buf.flush());
  }

  async recv(data) {
    if (this._recvError !== null) {
      throw this._recvError;
    }
    // For simplicity, we assume that the given data contains exactly one record.
    // Peers using this library will send one record at a time over the websocket
    // connection, and we can assume that the server-side websocket bridge will split
    // up any traffic into individual records if we ever start interoperating with
    // peers using a different TLS implementation.
    // Similarly, we assume that handshake messages will not be fragmented across
    // multiple records. This should be trivially true for the PSK-only mode used
    // by this library, but we may want to relax it in future for interoperability
    // with e.g. large ClientHello messages that contain lots of different options.
    const buf = new utils_BufferReader(data);
    // The data to read is either a TLSPlaintext or TLSCiphertext struct,
    // depending on whether record protection has been enabled yet:
    //
    //    struct {
    //        ContentType type;
    //        ProtocolVersion legacy_record_version;
    //        uint16 length;
    //        opaque fragment[TLSPlaintext.length];
    //    } TLSPlaintext;
    //
    //    struct {
    //        ContentType opaque_type = application_data; /* 23 */
    //        ProtocolVersion legacy_record_version = 0x0303; /* TLS v1.2 */
    //        uint16 length;
    //        opaque encrypted_record[TLSCiphertext.length];
    //    } TLSCiphertext;
    //
    let type = buf.readUint8();
    // The spec says legacy_record_version "MUST be ignored for all purposes",
    // but we know TLS1.3 implementations will only ever emit two possible values,
    // so it seems useful to bail out early if we receive anything else.
    const version = buf.readUint16();
    if (version !== VERSION_TLS_1_2) {
      // TLS1.0 is only acceptable on initial plaintext records.
      if (this._recvDecryptState !== null || version !== VERSION_TLS_1_0) {
        throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
      }
    }
    const length = buf.readUint16();
    let plaintext;
    if (this._recvDecryptState === null || type === RECORD_TYPE.CHANGE_CIPHER_SPEC) {
      [type, plaintext] = await this._readPlaintextRecord(type, length, buf);
    } else {
      [type, plaintext] = await this._readEncryptedRecord(type, length, buf);
    }
    // Sanity-check that we received exactly one record.
    if (buf.hasMoreBytes()) {
      throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
    }
    return [type, plaintext];
  }

  // Helper to read an unencrypted `TLSPlaintext` struct

  async _readPlaintextRecord(type, length, buf) {
    if (length > MAX_RECORD_SIZE) {
      throw new TLSError(ALERT_DESCRIPTION.RECORD_OVERFLOW);
    }
    return [type, buf.readBytes(length)];
  }

  // Helper to read an encrypted `TLSCiphertext` struct,
  // decrypting it into plaintext.

  async _readEncryptedRecord(type, length, buf) {
    if (length > MAX_ENCRYPTED_RECORD_SIZE) {
      throw new TLSError(ALERT_DESCRIPTION.RECORD_OVERFLOW);
    }
    // The outer type for encrypted records is always APPLICATION_DATA.
    if (type !== RECORD_TYPE.APPLICATION_DATA) {
      throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
    }
    // Decrypt and decode the contained `TLSInnerPlaintext` struct:
    //
    //    struct {
    //        opaque content[TLSPlaintext.length];
    //        ContentType type;
    //        uint8 zeros[length_of_padding];
    //    } TLSInnerPlaintext;
    //
    // The additional data for the decryption is the `TLSCiphertext` record
    // header, which is a fixed size and immediately prior to current buffer position.
    buf.incr(-RECORD_HEADER_SIZE);
    const additionalData = buf.readBytes(RECORD_HEADER_SIZE);
    const ciphertext = buf.readBytes(length);
    const paddedPlaintext = await this._recvDecryptState.decrypt(ciphertext, additionalData);
    // We have to scan backwards over the zero padding at the end of the struct
    // in order to find the non-zero `type` byte.
    let i;
    for (i = paddedPlaintext.byteLength - 1; i >= 0; i--) {
      if (paddedPlaintext[i] !== 0) {
        break;
      }
    }
    if (i < 0) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    type = paddedPlaintext[i];
    // `change_cipher_spec` records must always be plaintext.
    if (type === RECORD_TYPE.CHANGE_CIPHER_SPEC) {
      throw new TLSError(ALERT_DESCRIPTION.DECODE_ERROR);
    }
    return [type, paddedPlaintext.slice(0, i)];
  }
}

// CONCATENATED MODULE: ./src/tlsconnection.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The top-level APIs offered by this module are `ClientConnection` and
// `ServerConnection` classes, which provide authenticated and encrypted
// communication via the "externally-provisioned PSK" mode of TLS1.3.
// They each take a callback to be used for sending data to the remote peer,
// and operate like this:
//
//    conn = await ClientConnection.create(psk, pskId, async function send_data_to_server(data) {
//      // application-specific sending logic here.
//    })
//
//    // Send data to the server by calling `send`,
//    // which will use the callback provided in the constructor.
//    // A single `send()` by the application may result in multiple
//    // invokations of the callback.
//
//    await conn.send('application-level data')
//
//    // When data is received from the server, push it into
//    // the connection and let it return any decrypted app-level data.
//    // There might not be any app-level data if it was a protocol control
//    //  message, and the receipt of the data might trigger additional calls
//    // to the send callback for protocol control purposes.
//
//    serverSocket.on('data', async encrypted_data => {
//      const plaintext = await conn.recv(data)
//      if (plaintext !== null) {
//        do_something_with_app_level_data(plaintext)
//      }
//    })
//
//    // It's good practice to explicitly close the connection
//    // when finished.  This will send a "closed" notification
//    // to the server.
//
//    await conn.close()
//
//    // When the peer sends a "closed" notification it will show up
//    // as a `TLSCloseNotify` exception from recv:
//
//    try {
//      data = await conn.recv(data);
//    } catch (err) {
//      if (! (err instanceof TLSCloseNotify) { throw err }
//      do_something_to_cleanly_close_data_connection();
//    }
//
// The `ServerConnection` API operates similarly; the distinction is mainly
// in which side is expected to send vs receieve during the protocol handshake.










class tlsconnection_Connection {
  constructor(psk, pskId, sendCallback) {
    this.psk = assertIsBytes(psk);
    this.pskId = assertIsBytes(pskId);
    this.connected = new Promise((resolve, reject) => {
      this._onConnectionSuccess = resolve;
      this._onConnectionFailure = reject;
    });
    this._state = new UNINITIALIZED(this);
    this._handshakeRecvBuffer = null;
    this._hasSeenChangeCipherSpec = false;
    this._recordlayer = new recordlayer_RecordLayer(sendCallback);
    this._keyschedule = new keyschedule_KeySchedule();
    this._lastPromise = Promise.resolve();
  }

  // Subclasses will override this with some async initialization logic.
  static async create(psk, pskId, sendCallback) {
    return new this(psk, pskId, sendCallback);
  }

  // These are the three public API methods that consumers can use
  // to send and receive data encrypted with TLS1.3.

  async send(data) {
    assertIsBytes(data);
    await this.connected;
    await this._synchronized(async () => {
      await this._state.sendApplicationData(data);
    });
  }

  async recv(data) {
    assertIsBytes(data);
    return await this._synchronized(async () => {
      // Decrypt the data using the record layer.
      // We expect to receive precisely one record at a time.
      const [type, bytes] = await this._recordlayer.recv(data);
      // Dispatch based on the type of the record.
      switch (type) {
        case RECORD_TYPE.CHANGE_CIPHER_SPEC:
          await this._state.recvChangeCipherSpec(bytes);
          return null;
        case RECORD_TYPE.ALERT:
          await this._state.recvAlertMessage(TLSAlert.fromBytes(bytes));
          return null;
        case RECORD_TYPE.APPLICATION_DATA:
          return await this._state.recvApplicationData(bytes);
        case RECORD_TYPE.HANDSHAKE:
          // Multiple handshake messages may be coalesced into a single record.
          // Store the in-progress record buffer on `this` so that we can guard
          // against handshake messages that span a change in keys.
          this._handshakeRecvBuffer = new utils_BufferReader(bytes);
          if (! this._handshakeRecvBuffer.hasMoreBytes()) {
            throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
          }
          do {
            // Each handshake messages has a type and length prefix, per
            // https://tools.ietf.org/html/rfc8446#appendix-B.3
            this._handshakeRecvBuffer.incr(1);
            const mlength = this._handshakeRecvBuffer.readUint24();
            this._handshakeRecvBuffer.incr(-4);
            const messageBytes = this._handshakeRecvBuffer.readBytes(mlength + 4);
            this._keyschedule.addToTranscript(messageBytes);
            await this._state.recvHandshakeMessage(messages_HandshakeMessage.fromBytes(messageBytes));
          } while (this._handshakeRecvBuffer.hasMoreBytes());
          this._handshakeRecvBuffer = null;
          return null;
        default:
          throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
      }
    });
  }

  async close() {
    await this._synchronized(async () => {
      await this._state.close();
    });
  }

  // Ensure that async functions execute one at a time,
  // by waiting for the previous call to `_synchronized()` to complete
  // before starting a new one.  This helps ensure that we complete
  // one state-machine transition before starting to do the next.
  // It's also a convenient place to catch and alert on errors.

  _synchronized(cb) {
    const nextPromise = this._lastPromise.then(() => {
      return cb();
    }).catch(async err => {
      if (err instanceof TLSCloseNotify) {
        throw err;
      }
      await this._state.handleErrorAndRethrow(err);
    });
    // We don't want to hold on to the return value or error,
    // just synchronize on the fact that it completed.
    this._lastPromise = nextPromise.then(noop, noop);
    return nextPromise;
  }

  // This drives internal transition of the state-machine,
  // ensuring that the new state is properly initialized.

  async _transition(State, ...args) {
    this._state = new State(this);
    await this._state.initialize(...args);
    await this._recordlayer.flush();
  }

  // These are helpers to allow the State to manipulate the recordlayer
  // and send out various types of data.

  async _sendApplicationData(bytes) {
    await this._recordlayer.send(RECORD_TYPE.APPLICATION_DATA, bytes);
    await this._recordlayer.flush();
  }

  async _sendHandshakeMessage(msg) {
    await this._sendHandshakeMessageBytes(msg.toBytes());
  }

  async _sendHandshakeMessageBytes(bytes) {
    this._keyschedule.addToTranscript(bytes);
    await this._recordlayer.send(RECORD_TYPE.HANDSHAKE, bytes);
    // Don't flush after each handshake message, since we can probably
    // coalesce multiple messages into a single record.
  }

  async _sendAlertMessage(err) {
    await this._recordlayer.send(RECORD_TYPE.ALERT, err.toBytes());
    await this._recordlayer.flush();
  }

  async _sendChangeCipherSpec() {
    await this._recordlayer.send(RECORD_TYPE.CHANGE_CIPHER_SPEC, new Uint8Array([0x01]));
    await this._recordlayer.flush();
  }

  async _setSendKey(key) {
    return await this._recordlayer.setSendKey(key);
  }

  async _setRecvKey(key) {
    // Handshake messages that change keys must be on a record boundary.
    if (this._handshakeRecvBuffer && this._handshakeRecvBuffer.hasMoreBytes()) {
      throw new TLSError(ALERT_DESCRIPTION.UNEXPECTED_MESSAGE);
    }
    return await this._recordlayer.setRecvKey(key);
  }

  _setConnectionSuccess() {
    if (this._onConnectionSuccess !== null) {
      this._onConnectionSuccess();
      this._onConnectionSuccess = null;
      this._onConnectionFailure = null;
    }
  }

  _setConnectionFailure(err) {
    if (this._onConnectionFailure !== null) {
      this._onConnectionFailure(err);
      this._onConnectionSuccess = null;
      this._onConnectionFailure = null;
    }
  }

  _closeForSend(alert) {
    this._recordlayer.setSendError(alert);
  }

  _closeForRecv(alert) {
    this._recordlayer.setRecvError(alert);
  }
}

class tlsconnection_ClientConnection extends tlsconnection_Connection {
  static async create(psk, pskId, sendCallback) {
    const instance = await super.create(psk, pskId, sendCallback);
    await instance._transition(states_CLIENT_START);
    return instance;
  }
}

class tlsconnection_ServerConnection extends tlsconnection_Connection {
  static async create(psk, pskId, sendCallback) {
    const instance = await super.create(psk, pskId, sendCallback);
    await instance._transition(states_SERVER_START);
    return instance;
  }
}

// CONCATENATED MODULE: ./node_modules/event-target-shim/dist/event-target-shim.mjs
/**
 * @author Toru Nagashima <https://github.com/mysticatea>
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
/**
 * @typedef {object} PrivateData
 * @property {EventTarget} eventTarget The event target.
 * @property {{type:string}} event The original event object.
 * @property {number} eventPhase The current event phase.
 * @property {EventTarget|null} currentTarget The current event target.
 * @property {boolean} canceled The flag to prevent default.
 * @property {boolean} stopped The flag to stop propagation.
 * @property {boolean} immediateStopped The flag to stop propagation immediately.
 * @property {Function|null} passiveListener The listener if the current listener is passive. Otherwise this is null.
 * @property {number} timeStamp The unix time.
 * @private
 */

/**
 * Private data for event wrappers.
 * @type {WeakMap<Event, PrivateData>}
 * @private
 */
const privateData = new WeakMap();

/**
 * Cache for wrapper classes.
 * @type {WeakMap<Object, Function>}
 * @private
 */
const wrappers = new WeakMap();

/**
 * Get private data.
 * @param {Event} event The event object to get private data.
 * @returns {PrivateData} The private data of the event.
 * @private
 */
function pd(event) {
    const retv = privateData.get(event);
    console.assert(
        retv != null,
        "'this' is expected an Event object, but got",
        event
    );
    return retv
}

/**
 * https://dom.spec.whatwg.org/#set-the-canceled-flag
 * @param data {PrivateData} private data.
 */
function setCancelFlag(data) {
    if (data.passiveListener != null) {
        if (
            typeof console !== "undefined" &&
            typeof console.error === "function"
        ) {
            console.error(
                "Unable to preventDefault inside passive event listener invocation.",
                data.passiveListener
            );
        }
        return
    }
    if (!data.event.cancelable) {
        return
    }

    data.canceled = true;
    if (typeof data.event.preventDefault === "function") {
        data.event.preventDefault();
    }
}

/**
 * @see https://dom.spec.whatwg.org/#interface-event
 * @private
 */
/**
 * The event wrapper.
 * @constructor
 * @param {EventTarget} eventTarget The event target of this dispatching.
 * @param {Event|{type:string}} event The original event to wrap.
 */
function Event(eventTarget, event) {
    privateData.set(this, {
        eventTarget,
        event,
        eventPhase: 2,
        currentTarget: eventTarget,
        canceled: false,
        stopped: false,
        immediateStopped: false,
        passiveListener: null,
        timeStamp: event.timeStamp || Date.now(),
    });

    // https://heycam.github.io/webidl/#Unforgeable
    Object.defineProperty(this, "isTrusted", { value: false, enumerable: true });

    // Define accessors
    const keys = Object.keys(event);
    for (let i = 0; i < keys.length; ++i) {
        const key = keys[i];
        if (!(key in this)) {
            Object.defineProperty(this, key, defineRedirectDescriptor(key));
        }
    }
}

// Should be enumerable, but class methods are not enumerable.
Event.prototype = {
    /**
     * The type of this event.
     * @type {string}
     */
    get type() {
        return pd(this).event.type
    },

    /**
     * The target of this event.
     * @type {EventTarget}
     */
    get target() {
        return pd(this).eventTarget
    },

    /**
     * The target of this event.
     * @type {EventTarget}
     */
    get currentTarget() {
        return pd(this).currentTarget
    },

    /**
     * @returns {EventTarget[]} The composed path of this event.
     */
    composedPath() {
        const currentTarget = pd(this).currentTarget;
        if (currentTarget == null) {
            return []
        }
        return [currentTarget]
    },

    /**
     * Constant of NONE.
     * @type {number}
     */
    get NONE() {
        return 0
    },

    /**
     * Constant of CAPTURING_PHASE.
     * @type {number}
     */
    get CAPTURING_PHASE() {
        return 1
    },

    /**
     * Constant of AT_TARGET.
     * @type {number}
     */
    get AT_TARGET() {
        return 2
    },

    /**
     * Constant of BUBBLING_PHASE.
     * @type {number}
     */
    get BUBBLING_PHASE() {
        return 3
    },

    /**
     * The target of this event.
     * @type {number}
     */
    get eventPhase() {
        return pd(this).eventPhase
    },

    /**
     * Stop event bubbling.
     * @returns {void}
     */
    stopPropagation() {
        const data = pd(this);

        data.stopped = true;
        if (typeof data.event.stopPropagation === "function") {
            data.event.stopPropagation();
        }
    },

    /**
     * Stop event bubbling.
     * @returns {void}
     */
    stopImmediatePropagation() {
        const data = pd(this);

        data.stopped = true;
        data.immediateStopped = true;
        if (typeof data.event.stopImmediatePropagation === "function") {
            data.event.stopImmediatePropagation();
        }
    },

    /**
     * The flag to be bubbling.
     * @type {boolean}
     */
    get bubbles() {
        return Boolean(pd(this).event.bubbles)
    },

    /**
     * The flag to be cancelable.
     * @type {boolean}
     */
    get cancelable() {
        return Boolean(pd(this).event.cancelable)
    },

    /**
     * Cancel this event.
     * @returns {void}
     */
    preventDefault() {
        setCancelFlag(pd(this));
    },

    /**
     * The flag to indicate cancellation state.
     * @type {boolean}
     */
    get defaultPrevented() {
        return pd(this).canceled
    },

    /**
     * The flag to be composed.
     * @type {boolean}
     */
    get composed() {
        return Boolean(pd(this).event.composed)
    },

    /**
     * The unix time of this event.
     * @type {number}
     */
    get timeStamp() {
        return pd(this).timeStamp
    },

    /**
     * The target of this event.
     * @type {EventTarget}
     * @deprecated
     */
    get srcElement() {
        return pd(this).eventTarget
    },

    /**
     * The flag to stop event bubbling.
     * @type {boolean}
     * @deprecated
     */
    get cancelBubble() {
        return pd(this).stopped
    },
    set cancelBubble(value) {
        if (!value) {
            return
        }
        const data = pd(this);

        data.stopped = true;
        if (typeof data.event.cancelBubble === "boolean") {
            data.event.cancelBubble = true;
        }
    },

    /**
     * The flag to indicate cancellation state.
     * @type {boolean}
     * @deprecated
     */
    get returnValue() {
        return !pd(this).canceled
    },
    set returnValue(value) {
        if (!value) {
            setCancelFlag(pd(this));
        }
    },

    /**
     * Initialize this event object. But do nothing under event dispatching.
     * @param {string} type The event type.
     * @param {boolean} [bubbles=false] The flag to be possible to bubble up.
     * @param {boolean} [cancelable=false] The flag to be possible to cancel.
     * @deprecated
     */
    initEvent() {
        // Do nothing.
    },
};

// `constructor` is not enumerable.
Object.defineProperty(Event.prototype, "constructor", {
    value: Event,
    configurable: true,
    writable: true,
});

// Ensure `event instanceof window.Event` is `true`.
if (typeof window !== "undefined" && typeof window.Event !== "undefined") {
    Object.setPrototypeOf(Event.prototype, window.Event.prototype);

    // Make association for wrappers.
    wrappers.set(window.Event.prototype, Event);
}

/**
 * Get the property descriptor to redirect a given property.
 * @param {string} key Property name to define property descriptor.
 * @returns {PropertyDescriptor} The property descriptor to redirect the property.
 * @private
 */
function defineRedirectDescriptor(key) {
    return {
        get() {
            return pd(this).event[key]
        },
        set(value) {
            pd(this).event[key] = value;
        },
        configurable: true,
        enumerable: true,
    }
}

/**
 * Get the property descriptor to call a given method property.
 * @param {string} key Property name to define property descriptor.
 * @returns {PropertyDescriptor} The property descriptor to call the method property.
 * @private
 */
function defineCallDescriptor(key) {
    return {
        value() {
            const event = pd(this).event;
            return event[key].apply(event, arguments)
        },
        configurable: true,
        enumerable: true,
    }
}

/**
 * Define new wrapper class.
 * @param {Function} BaseEvent The base wrapper class.
 * @param {Object} proto The prototype of the original event.
 * @returns {Function} The defined wrapper class.
 * @private
 */
function defineWrapper(BaseEvent, proto) {
    const keys = Object.keys(proto);
    if (keys.length === 0) {
        return BaseEvent
    }

    /** CustomEvent */
    function CustomEvent(eventTarget, event) {
        BaseEvent.call(this, eventTarget, event);
    }

    CustomEvent.prototype = Object.create(BaseEvent.prototype, {
        constructor: { value: CustomEvent, configurable: true, writable: true },
    });

    // Define accessors.
    for (let i = 0; i < keys.length; ++i) {
        const key = keys[i];
        if (!(key in BaseEvent.prototype)) {
            const descriptor = Object.getOwnPropertyDescriptor(proto, key);
            const isFunc = typeof descriptor.value === "function";
            Object.defineProperty(
                CustomEvent.prototype,
                key,
                isFunc
                    ? defineCallDescriptor(key)
                    : defineRedirectDescriptor(key)
            );
        }
    }

    return CustomEvent
}

/**
 * Get the wrapper class of a given prototype.
 * @param {Object} proto The prototype of the original event to get its wrapper.
 * @returns {Function} The wrapper class.
 * @private
 */
function getWrapper(proto) {
    if (proto == null || proto === Object.prototype) {
        return Event
    }

    let wrapper = wrappers.get(proto);
    if (wrapper == null) {
        wrapper = defineWrapper(getWrapper(Object.getPrototypeOf(proto)), proto);
        wrappers.set(proto, wrapper);
    }
    return wrapper
}

/**
 * Wrap a given event to management a dispatching.
 * @param {EventTarget} eventTarget The event target of this dispatching.
 * @param {Object} event The event to wrap.
 * @returns {Event} The wrapper instance.
 * @private
 */
function wrapEvent(eventTarget, event) {
    const Wrapper = getWrapper(Object.getPrototypeOf(event));
    return new Wrapper(eventTarget, event)
}

/**
 * Get the immediateStopped flag of a given event.
 * @param {Event} event The event to get.
 * @returns {boolean} The flag to stop propagation immediately.
 * @private
 */
function isStopped(event) {
    return pd(event).immediateStopped
}

/**
 * Set the current event phase of a given event.
 * @param {Event} event The event to set current target.
 * @param {number} eventPhase New event phase.
 * @returns {void}
 * @private
 */
function setEventPhase(event, eventPhase) {
    pd(event).eventPhase = eventPhase;
}

/**
 * Set the current target of a given event.
 * @param {Event} event The event to set current target.
 * @param {EventTarget|null} currentTarget New current target.
 * @returns {void}
 * @private
 */
function setCurrentTarget(event, currentTarget) {
    pd(event).currentTarget = currentTarget;
}

/**
 * Set a passive listener of a given event.
 * @param {Event} event The event to set current target.
 * @param {Function|null} passiveListener New passive listener.
 * @returns {void}
 * @private
 */
function setPassiveListener(event, passiveListener) {
    pd(event).passiveListener = passiveListener;
}

/**
 * @typedef {object} ListenerNode
 * @property {Function} listener
 * @property {1|2|3} listenerType
 * @property {boolean} passive
 * @property {boolean} once
 * @property {ListenerNode|null} next
 * @private
 */

/**
 * @type {WeakMap<object, Map<string, ListenerNode>>}
 * @private
 */
const listenersMap = new WeakMap();

// Listener types
const CAPTURE = 1;
const BUBBLE = 2;
const ATTRIBUTE = 3;

/**
 * Check whether a given value is an object or not.
 * @param {any} x The value to check.
 * @returns {boolean} `true` if the value is an object.
 */
function isObject(x) {
    return x !== null && typeof x === "object" //eslint-disable-line no-restricted-syntax
}

/**
 * Get listeners.
 * @param {EventTarget} eventTarget The event target to get.
 * @returns {Map<string, ListenerNode>} The listeners.
 * @private
 */
function getListeners(eventTarget) {
    const listeners = listenersMap.get(eventTarget);
    if (listeners == null) {
        throw new TypeError(
            "'this' is expected an EventTarget object, but got another value."
        )
    }
    return listeners
}

/**
 * Get the property descriptor for the event attribute of a given event.
 * @param {string} eventName The event name to get property descriptor.
 * @returns {PropertyDescriptor} The property descriptor.
 * @private
 */
function defineEventAttributeDescriptor(eventName) {
    return {
        get() {
            const listeners = getListeners(this);
            let node = listeners.get(eventName);
            while (node != null) {
                if (node.listenerType === ATTRIBUTE) {
                    return node.listener
                }
                node = node.next;
            }
            return null
        },

        set(listener) {
            if (typeof listener !== "function" && !isObject(listener)) {
                listener = null; // eslint-disable-line no-param-reassign
            }
            const listeners = getListeners(this);

            // Traverse to the tail while removing old value.
            let prev = null;
            let node = listeners.get(eventName);
            while (node != null) {
                if (node.listenerType === ATTRIBUTE) {
                    // Remove old value.
                    if (prev !== null) {
                        prev.next = node.next;
                    } else if (node.next !== null) {
                        listeners.set(eventName, node.next);
                    } else {
                        listeners.delete(eventName);
                    }
                } else {
                    prev = node;
                }

                node = node.next;
            }

            // Add new value.
            if (listener !== null) {
                const newNode = {
                    listener,
                    listenerType: ATTRIBUTE,
                    passive: false,
                    once: false,
                    next: null,
                };
                if (prev === null) {
                    listeners.set(eventName, newNode);
                } else {
                    prev.next = newNode;
                }
            }
        },
        configurable: true,
        enumerable: true,
    }
}

/**
 * Define an event attribute (e.g. `eventTarget.onclick`).
 * @param {Object} eventTargetPrototype The event target prototype to define an event attrbite.
 * @param {string} eventName The event name to define.
 * @returns {void}
 */
function defineEventAttribute(eventTargetPrototype, eventName) {
    Object.defineProperty(
        eventTargetPrototype,
        `on${eventName}`,
        defineEventAttributeDescriptor(eventName)
    );
}

/**
 * Define a custom EventTarget with event attributes.
 * @param {string[]} eventNames Event names for event attributes.
 * @returns {EventTarget} The custom EventTarget.
 * @private
 */
function defineCustomEventTarget(eventNames) {
    /** CustomEventTarget */
    function CustomEventTarget() {
        EventTarget.call(this);
    }

    CustomEventTarget.prototype = Object.create(EventTarget.prototype, {
        constructor: {
            value: CustomEventTarget,
            configurable: true,
            writable: true,
        },
    });

    for (let i = 0; i < eventNames.length; ++i) {
        defineEventAttribute(CustomEventTarget.prototype, eventNames[i]);
    }

    return CustomEventTarget
}

/**
 * EventTarget.
 *
 * - This is constructor if no arguments.
 * - This is a function which returns a CustomEventTarget constructor if there are arguments.
 *
 * For example:
 *
 *     class A extends EventTarget {}
 *     class B extends EventTarget("message") {}
 *     class C extends EventTarget("message", "error") {}
 *     class D extends EventTarget(["message", "error"]) {}
 */
function EventTarget() {
    /*eslint-disable consistent-return */
    if (this instanceof EventTarget) {
        listenersMap.set(this, new Map());
        return
    }
    if (arguments.length === 1 && Array.isArray(arguments[0])) {
        return defineCustomEventTarget(arguments[0])
    }
    if (arguments.length > 0) {
        const types = new Array(arguments.length);
        for (let i = 0; i < arguments.length; ++i) {
            types[i] = arguments[i];
        }
        return defineCustomEventTarget(types)
    }
    throw new TypeError("Cannot call a class as a function")
    /*eslint-enable consistent-return */
}

// Should be enumerable, but class methods are not enumerable.
EventTarget.prototype = {
    /**
     * Add a given listener to this event target.
     * @param {string} eventName The event name to add.
     * @param {Function} listener The listener to add.
     * @param {boolean|{capture?:boolean,passive?:boolean,once?:boolean}} [options] The options for this listener.
     * @returns {void}
     */
    addEventListener(eventName, listener, options) {
        if (listener == null) {
            return
        }
        if (typeof listener !== "function" && !isObject(listener)) {
            throw new TypeError("'listener' should be a function or an object.")
        }

        const listeners = getListeners(this);
        const optionsIsObj = isObject(options);
        const capture = optionsIsObj
            ? Boolean(options.capture)
            : Boolean(options);
        const listenerType = capture ? CAPTURE : BUBBLE;
        const newNode = {
            listener,
            listenerType,
            passive: optionsIsObj && Boolean(options.passive),
            once: optionsIsObj && Boolean(options.once),
            next: null,
        };

        // Set it as the first node if the first node is null.
        let node = listeners.get(eventName);
        if (node === undefined) {
            listeners.set(eventName, newNode);
            return
        }

        // Traverse to the tail while checking duplication..
        let prev = null;
        while (node != null) {
            if (
                node.listener === listener &&
                node.listenerType === listenerType
            ) {
                // Should ignore duplication.
                return
            }
            prev = node;
            node = node.next;
        }

        // Add it.
        prev.next = newNode;
    },

    /**
     * Remove a given listener from this event target.
     * @param {string} eventName The event name to remove.
     * @param {Function} listener The listener to remove.
     * @param {boolean|{capture?:boolean,passive?:boolean,once?:boolean}} [options] The options for this listener.
     * @returns {void}
     */
    removeEventListener(eventName, listener, options) {
        if (listener == null) {
            return
        }

        const listeners = getListeners(this);
        const capture = isObject(options)
            ? Boolean(options.capture)
            : Boolean(options);
        const listenerType = capture ? CAPTURE : BUBBLE;

        let prev = null;
        let node = listeners.get(eventName);
        while (node != null) {
            if (
                node.listener === listener &&
                node.listenerType === listenerType
            ) {
                if (prev !== null) {
                    prev.next = node.next;
                } else if (node.next !== null) {
                    listeners.set(eventName, node.next);
                } else {
                    listeners.delete(eventName);
                }
                return
            }

            prev = node;
            node = node.next;
        }
    },

    /**
     * Dispatch a given event.
     * @param {Event|{type:string}} event The event to dispatch.
     * @returns {boolean} `false` if canceled.
     */
    dispatchEvent(event) {
        if (event == null || typeof event.type !== "string") {
            throw new TypeError('"event.type" should be a string.')
        }

        // If listeners aren't registered, terminate.
        const listeners = getListeners(this);
        const eventName = event.type;
        let node = listeners.get(eventName);
        if (node == null) {
            return true
        }

        // Since we cannot rewrite several properties, so wrap object.
        const wrappedEvent = wrapEvent(this, event);

        // This doesn't process capturing phase and bubbling phase.
        // This isn't participating in a tree.
        let prev = null;
        while (node != null) {
            // Remove this listener if it's once
            if (node.once) {
                if (prev !== null) {
                    prev.next = node.next;
                } else if (node.next !== null) {
                    listeners.set(eventName, node.next);
                } else {
                    listeners.delete(eventName);
                }
            } else {
                prev = node;
            }

            // Call this listener
            setPassiveListener(
                wrappedEvent,
                node.passive ? node.listener : null
            );
            if (typeof node.listener === "function") {
                try {
                    node.listener.call(this, wrappedEvent);
                } catch (err) {
                    if (
                        typeof console !== "undefined" &&
                        typeof console.error === "function"
                    ) {
                        console.error(err);
                    }
                }
            } else if (
                node.listenerType !== ATTRIBUTE &&
                typeof node.listener.handleEvent === "function"
            ) {
                node.listener.handleEvent(wrappedEvent);
            }

            // Break if `event.stopImmediatePropagation` was called.
            if (isStopped(wrappedEvent)) {
                break
            }

            node = node.next;
        }
        setPassiveListener(wrappedEvent, null);
        setEventPhase(wrappedEvent, 0);
        setCurrentTarget(wrappedEvent, null);

        return !wrappedEvent.defaultPrevented
    },
};

// `constructor` is not enumerable.
Object.defineProperty(EventTarget.prototype, "constructor", {
    value: EventTarget,
    configurable: true,
    writable: true,
});

// Ensure `eventTarget instanceof window.EventTarget` is `true`.
if (
    typeof window !== "undefined" &&
    typeof window.EventTarget !== "undefined"
) {
    Object.setPrototypeOf(EventTarget.prototype, window.EventTarget.prototype);
}

/* harmony default export */ var event_target_shim = (EventTarget);


// CONCATENATED MODULE: ./src/index.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A wrapper that combines a WebSocket to the channelserver
// with some client-side encryption for securing the channel.
//
// This code is responsible for the event handling and the consumer API.
// All the details of encrypting the messages are delegated to`./tlsconnection.js`.







const CLOSE_FLUSH_BUFFER_INTERVAL_MS = 200;
const CLOSE_FLUSH_BUFFER_MAX_TRIES = 5;

class src_PairingChannel extends EventTarget {
  constructor(channelId, channelKey, socket, connection) {
    super();
    this._channelId = channelId;
    this._channelKey = channelKey;
    this._socket = socket;
    this._connection = connection;
    this._selfClosed = false;
    this._peerClosed = false;
    this._setupListeners();
  }

  /**
   * Create a new pairing channel.
   *
   * This will open a channel on the channelserver, and generate a random client-side
   * encryption key. When the promise resolves, `this.channelId` and `this.channelKey`
   * can be transferred to another client to allow it to securely connect to the channel.
   *
   * @returns Promise<PairingChannel>
   */
  static create(channelServerURI) {
    const wsURI = new URL('/v1/ws/', channelServerURI).href;
    const channelKey = crypto.getRandomValues(new Uint8Array(32));
    // The one who creates the channel plays the role of 'server' in the underlying TLS exchange.
    return this._makePairingChannel(wsURI, tlsconnection_ServerConnection, channelKey);
  }

  /**
   * Connect to an existing pairing channel.
   *
   * This will connect to a channel on the channelserver previously established by
   * another client calling `create`. The `channelId` and `channelKey` must have been
   * obtained via some out-of-band mechanism (such as by scanning from a QR code).
   *
   * @returns Promise<PairingChannel>
   */
  static connect(channelServerURI, channelId, channelKey) {
    const wsURI = new URL(`/v1/ws/${channelId}`, channelServerURI).href;
    // The one who connects to an existing channel plays the role of 'client'
    // in the underlying TLS exchange.
    return this._makePairingChannel(wsURI, tlsconnection_ClientConnection, channelKey);
  }

  static _makePairingChannel(wsUri, ConnectionClass, psk) {
    const socket = new WebSocket(wsUri);
    return new Promise((resolve, reject) => {
      // eslint-disable-next-line prefer-const
      let stopListening;
      const onConnectionError = async () => {
        stopListening();
        reject(new Error('Error while creating the pairing channel'));
      };
      const onFirstMessage = async event => {
        stopListening();
        try {
          // The channelserver echos back the channel id, and we use it as an
          // additional input to the TLS handshake via the "psk id" field.
          const {channelid: channelId} = JSON.parse(event.data);
          const pskId = utf8ToBytes(channelId);
          const connection = await ConnectionClass.create(psk, pskId, data => {
            // Send data by forwarding it via the channelserver websocket.
            // The TLS connection gives us `data` as raw bytes, but channelserver
            // expects b64urlsafe strings, because it wraps them in a JSON object envelope.
            socket.send(bytesToBase64url(data));
          });
          const instance = new this(channelId, psk, socket, connection);
          resolve(instance);
        } catch (err) {
          reject(err);
        }
      };
      stopListening = () => {
        socket.removeEventListener('close', onConnectionError);
        socket.removeEventListener('error', onConnectionError);
        socket.removeEventListener('message', onFirstMessage);
      };
      socket.addEventListener('close', onConnectionError);
      socket.addEventListener('error', onConnectionError);
      socket.addEventListener('message', onFirstMessage);
    });
  }

  _setupListeners() {
    this._socket.addEventListener('message', async event => {
      try {
        // When we receive data from the channelserver, pump it through the TLS connection
        // to decrypt it, then echo it back out to consumers as an event.
        const channelServerEnvelope = JSON.parse(event.data);
        const payload = await this._connection.recv(base64urlToBytes(channelServerEnvelope.message));
        if (payload !== null) {
          const data = JSON.parse(bytesToUtf8(payload));
          this.dispatchEvent(new CustomEvent('message', {
            detail: {
              data,
              sender: channelServerEnvelope.sender,
            },
          }));
        }
      } catch (error) {
        let event;
        // The underlying TLS connection will signal a clean shutdown of the channel
        // by throwing a special error, because it doesn't really have a better
        // signally mechanism available.
        if (error instanceof TLSCloseNotify) {
          this._peerClosed = true;
          if (this._selfClosed) {
            this._shutdown();
          }
          event = new CustomEvent('close');
        } else {
          event = new CustomEvent('error', {
            detail: {
              error,
            }
          });
        }
        this.dispatchEvent(event);
      }
    });
    // Relay the WebSocket events.
    this._socket.addEventListener('error', () => {
      this._shutdown();
      // The dispatched event that we receive has no useful information.
      this.dispatchEvent(new CustomEvent('error', {
        detail: {
          error: new Error('WebSocket error.'),
        },
      }));
    });
    // In TLS, the peer has to explicitly send a close notification,
    // which we dispatch above.  Unexpected socket close is an error.
    this._socket.addEventListener('close', () => {
      this._shutdown();
      if (! this._peerClosed) {
        this.dispatchEvent(new CustomEvent('error', {
          detail: {
            error: new Error('WebSocket unexpectedly closed'),
          }
        }));
      }
    });
  }

  /**
   * @param {Object} data
   */
  async send(data) {
    const payload = utf8ToBytes(JSON.stringify(data));
    await this._connection.send(payload);
  }

  async close() {
    this._selfClosed = true;
    await this._connection.close();
    try {
      // Ensure all queued bytes have been sent before closing the connection.
      let tries = 0;
      while (this._socket.bufferedAmount > 0) {
        if (++tries > CLOSE_FLUSH_BUFFER_MAX_TRIES) {
          throw new Error('Could not flush the outgoing buffer in time.');
        }
        await new Promise(res => setTimeout(res, CLOSE_FLUSH_BUFFER_INTERVAL_MS));
      }
    } finally {
      // If the peer hasn't closed, we might still receive some data.
      if (this._peerClosed) {
        this._shutdown();
      }
    }
  }

  _shutdown() {
    if (this._socket) {
      this._socket.close();
      this._socket = null;
      this._connection = null;
    }
  }

  get closed() {
    return (! this._socket) || (this._socket.readyState === 3);
  }

  get channelId() {
    return this._channelId;
  }

  get channelKey() {
    return this._channelKey;
  }
}

// Re-export helpful utilities for calling code to use.


// For running tests using the built bundle,
// expose a bunch of implementation details.







const _internals = {
  arrayToBytes: arrayToBytes,
  BufferReader: utils_BufferReader,
  BufferWriter: utils_BufferWriter,
  bytesAreEqual: bytesAreEqual,
  bytesToHex: bytesToHex,
  bytesToUtf8: bytesToUtf8,
  ClientConnection: tlsconnection_ClientConnection,
  Connection: tlsconnection_Connection,
  DecryptionState: recordlayer_DecryptionState,
  EncryptedExtensions: EncryptedExtensions,
  EncryptionState: recordlayer_EncryptionState,
  Finished: messages_Finished,
  HASH_LENGTH: HASH_LENGTH,
  hexToBytes: hexToBytes,
  hkdfExpand: hkdfExpand,
  KeySchedule: keyschedule_KeySchedule,
  NewSessionTicket: messages_NewSessionTicket,
  RecordLayer: recordlayer_RecordLayer,
  ServerConnection: tlsconnection_ServerConnection,
  utf8ToBytes: utf8ToBytes,
  zeros: zeros,
};


/***/ })
/******/ ])["PairingChannel"];
