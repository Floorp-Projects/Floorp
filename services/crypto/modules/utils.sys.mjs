/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Observers } from "resource://services-common/observers.sys.mjs";

import { CommonUtils } from "resource://services-common/utils.sys.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "textEncoder", function () {
  return new TextEncoder();
});

/**
 * A number of `Legacy` suffixed functions are exposed by CryptoUtils.
 * They work with octet strings, which were used before Javascript
 * got ArrayBuffer and friends.
 */
export var CryptoUtils = {
  xor(a, b) {
    let bytes = [];

    if (a.length != b.length) {
      throw new Error(
        "can't xor unequal length strings: " + a.length + " vs " + b.length
      );
    }

    for (let i = 0; i < a.length; i++) {
      bytes[i] = a.charCodeAt(i) ^ b.charCodeAt(i);
    }

    return String.fromCharCode.apply(String, bytes);
  },

  /**
   * Generate a string of random bytes.
   * @returns {String} Octet string
   */
  generateRandomBytesLegacy(length) {
    let bytes = CryptoUtils.generateRandomBytes(length);
    return CommonUtils.arrayBufferToByteString(bytes);
  },

  generateRandomBytes(length) {
    return crypto.getRandomValues(new Uint8Array(length));
  },

  /**
   * UTF8-encode a message and hash it with the given hasher. Returns a
   * string containing bytes.
   */
  digestUTF8(message, hasher) {
    let data = lazy.textEncoder.encode(message);
    hasher.update(data, data.length);
    let result = hasher.finish(false);
    return result;
  },

  /**
   * Treat the given message as a bytes string (if necessary) and hash it with
   * the given hasher. Returns a string containing bytes.
   */
  digestBytes(bytes, hasher) {
    if (typeof bytes == "string" || bytes instanceof String) {
      bytes = CommonUtils.byteStringToArrayBuffer(bytes);
    }
    return CryptoUtils.digestBytesArray(bytes, hasher);
  },

  digestBytesArray(bytes, hasher) {
    hasher.update(bytes, bytes.length);
    let result = hasher.finish(false);
    return result;
  },

  /**
   * Encode the message into UTF-8 and feed the resulting bytes into the
   * given hasher. Does not return a hash. This can be called multiple times
   * with a single hasher, but eventually you must extract the result
   * yourself.
   */
  updateUTF8(message, hasher) {
    let bytes = lazy.textEncoder.encode(message);
    hasher.update(bytes, bytes.length);
  },

  sha256(message) {
    let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
      Ci.nsICryptoHash
    );
    hasher.init(hasher.SHA256);
    return CommonUtils.bytesAsHex(CryptoUtils.digestUTF8(message, hasher));
  },

  sha256Base64(message) {
    let data = lazy.textEncoder.encode(message);
    let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
      Ci.nsICryptoHash
    );
    hasher.init(hasher.SHA256);
    hasher.update(data, data.length);
    return hasher.finish(true);
  },

  /**
   * @param {string} alg Hash algorithm (common values are SHA-1 or SHA-256)
   * @param {string} key Key as an octet string.
   * @param {string} data Data as an octet string.
   */
  async hmacLegacy(alg, key, data) {
    if (!key || !key.length) {
      key = "\0";
    }
    data = CommonUtils.byteStringToArrayBuffer(data);
    key = CommonUtils.byteStringToArrayBuffer(key);
    const result = await CryptoUtils.hmac(alg, key, data);
    return CommonUtils.arrayBufferToByteString(result);
  },

  /**
   * @param {string} ikm IKM as an octet string.
   * @param {string} salt Salt as an Hex string.
   * @param {string} info Info as a regular string.
   * @param {Number} len Desired output length in bytes.
   */
  async hkdfLegacy(ikm, xts, info, len) {
    ikm = CommonUtils.byteStringToArrayBuffer(ikm);
    xts = CommonUtils.byteStringToArrayBuffer(xts);
    info = lazy.textEncoder.encode(info);
    const okm = await CryptoUtils.hkdf(ikm, xts, info, len);
    return CommonUtils.arrayBufferToByteString(okm);
  },

  /**
   * @param {String} alg Hash algorithm (common values are SHA-1 or SHA-256)
   * @param {ArrayBuffer} key
   * @param {ArrayBuffer} data
   * @param {Number} len Desired output length in bytes.
   * @returns {Uint8Array}
   */
  async hmac(alg, key, data) {
    const hmacKey = await crypto.subtle.importKey(
      "raw",
      key,
      { name: "HMAC", hash: alg },
      false,
      ["sign"]
    );
    const result = await crypto.subtle.sign("HMAC", hmacKey, data);
    return new Uint8Array(result);
  },

  /**
   * @param {ArrayBuffer} ikm
   * @param {ArrayBuffer} salt
   * @param {ArrayBuffer} info
   * @param {Number} len Desired output length in bytes.
   * @returns {Uint8Array}
   */
  async hkdf(ikm, salt, info, len) {
    const key = await crypto.subtle.importKey(
      "raw",
      ikm,
      { name: "HKDF" },
      false,
      ["deriveBits"]
    );
    const okm = await crypto.subtle.deriveBits(
      {
        name: "HKDF",
        hash: "SHA-256",
        salt,
        info,
      },
      key,
      len * 8
    );
    return new Uint8Array(okm);
  },

  /**
   * PBKDF2 password stretching with SHA-256 hmac.
   *
   * @param {string} passphrase Passphrase as an octet string.
   * @param {string} salt Salt as an octet string.
   * @param {string} iterations Number of iterations, a positive integer.
   * @param {string} len Desired output length in bytes.
   */
  async pbkdf2Generate(passphrase, salt, iterations, len) {
    passphrase = CommonUtils.byteStringToArrayBuffer(passphrase);
    salt = CommonUtils.byteStringToArrayBuffer(salt);
    const key = await crypto.subtle.importKey(
      "raw",
      passphrase,
      { name: "PBKDF2" },
      false,
      ["deriveBits"]
    );
    const output = await crypto.subtle.deriveBits(
      {
        name: "PBKDF2",
        hash: "SHA-256",
        salt,
        iterations,
      },
      key,
      len * 8
    );
    return CommonUtils.arrayBufferToByteString(new Uint8Array(output));
  },

  /**
   * Compute the HTTP MAC SHA-1 for an HTTP request.
   *
   * @param  identifier
   *         (string) MAC Key Identifier.
   * @param  key
   *         (string) MAC Key.
   * @param  method
   *         (string) HTTP request method.
   * @param  URI
   *         (nsIURI) HTTP request URI.
   * @param  extra
   *         (object) Optional extra parameters. Valid keys are:
   *           nonce_bytes - How many bytes the nonce should be. This defaults
   *             to 8. Note that this many bytes are Base64 encoded, so the
   *             string length of the nonce will be longer than this value.
   *           ts - Timestamp to use. Should only be defined for testing.
   *           nonce - String nonce. Should only be defined for testing as this
   *             function will generate a cryptographically secure random one
   *             if not defined.
   *           ext - Extra string to be included in MAC. Per the HTTP MAC spec,
   *             the format is undefined and thus application specific.
   * @returns
   *         (object) Contains results of operation and input arguments (for
   *           symmetry). The object has the following keys:
   *
   *           identifier - (string) MAC Key Identifier (from arguments).
   *           key - (string) MAC Key (from arguments).
   *           method - (string) HTTP request method (from arguments).
   *           hostname - (string) HTTP hostname used (derived from arguments).
   *           port - (string) HTTP port number used (derived from arguments).
   *           mac - (string) Raw HMAC digest bytes.
   *           getHeader - (function) Call to obtain the string Authorization
   *             header value for this invocation.
   *           nonce - (string) Nonce value used.
   *           ts - (number) Integer seconds since Unix epoch that was used.
   */
  async computeHTTPMACSHA1(identifier, key, method, uri, extra) {
    let ts = extra && extra.ts ? extra.ts : Math.floor(Date.now() / 1000);
    let nonce_bytes = extra && extra.nonce_bytes > 0 ? extra.nonce_bytes : 8;

    // We are allowed to use more than the Base64 alphabet if we want.
    let nonce =
      extra && extra.nonce
        ? extra.nonce
        : btoa(CryptoUtils.generateRandomBytesLegacy(nonce_bytes));

    let host = uri.asciiHost;
    let port;
    let usedMethod = method.toUpperCase();

    if (uri.port != -1) {
      port = uri.port;
    } else if (uri.scheme == "http") {
      port = "80";
    } else if (uri.scheme == "https") {
      port = "443";
    } else {
      throw new Error("Unsupported URI scheme: " + uri.scheme);
    }

    let ext = extra && extra.ext ? extra.ext : "";

    let requestString =
      ts.toString(10) +
      "\n" +
      nonce +
      "\n" +
      usedMethod +
      "\n" +
      uri.pathQueryRef +
      "\n" +
      host +
      "\n" +
      port +
      "\n" +
      ext +
      "\n";

    const mac = await CryptoUtils.hmacLegacy("SHA-1", key, requestString);

    function getHeader() {
      return CryptoUtils.getHTTPMACSHA1Header(
        this.identifier,
        this.ts,
        this.nonce,
        this.mac,
        this.ext
      );
    }

    return {
      identifier,
      key,
      method: usedMethod,
      hostname: host,
      port,
      mac,
      nonce,
      ts,
      ext,
      getHeader,
    };
  },

  /**
   * Obtain the HTTP MAC Authorization header value from fields.
   *
   * @param  identifier
   *         (string) MAC key identifier.
   * @param  ts
   *         (number) Integer seconds since Unix epoch.
   * @param  nonce
   *         (string) Nonce value.
   * @param  mac
   *         (string) Computed HMAC digest (raw bytes).
   * @param  ext
   *         (optional) (string) Extra string content.
   * @returns
   *         (string) Value to put in Authorization header.
   */
  getHTTPMACSHA1Header: function getHTTPMACSHA1Header(
    identifier,
    ts,
    nonce,
    mac,
    ext
  ) {
    let header =
      'MAC id="' +
      identifier +
      '", ' +
      'ts="' +
      ts +
      '", ' +
      'nonce="' +
      nonce +
      '", ' +
      'mac="' +
      btoa(mac) +
      '"';

    if (!ext) {
      return header;
    }

    return (header += ', ext="' + ext + '"');
  },

  /**
   * Given an HTTP header value, strip out any attributes.
   */

  stripHeaderAttributes(value) {
    value = value || "";
    let i = value.indexOf(";");
    return value
      .substring(0, i >= 0 ? i : undefined)
      .trim()
      .toLowerCase();
  },

  /**
   * Compute the HAWK client values (mostly the header) for an HTTP request.
   *
   * @param  URI
   *         (nsIURI) HTTP request URI.
   * @param  method
   *         (string) HTTP request method.
   * @param  options
   *         (object) extra parameters (all but "credentials" are optional):
   *           credentials - (object, mandatory) HAWK credentials object.
   *             All three keys are required:
   *             id - (string) key identifier
   *             key - (string) raw key bytes
   *           ext - (string) application-specific data, included in MAC
   *           localtimeOffsetMsec - (number) local clock offset (vs server)
   *           payload - (string) payload to include in hash, containing the
   *                     HTTP request body. If not provided, the HAWK hash
   *                     will not cover the request body, and the server
   *                     should not check it either. This will be UTF-8
   *                     encoded into bytes before hashing. This function
   *                     cannot handle arbitrary binary data, sorry (the
   *                     UTF-8 encoding process will corrupt any codepoints
   *                     between U+0080 and U+00FF). Callers must be careful
   *                     to use an HTTP client function which encodes the
   *                     payload exactly the same way, otherwise the hash
   *                     will not match.
   *           contentType - (string) payload Content-Type. This is included
   *                         (without any attributes like "charset=") in the
   *                         HAWK hash. It does *not* affect interpretation
   *                         of the "payload" property.
   *           hash - (base64 string) pre-calculated payload hash. If
   *                  provided, "payload" is ignored.
   *           ts - (number) pre-calculated timestamp, secs since epoch
   *           now - (number) current time, ms-since-epoch, for tests
   *           nonce - (string) pre-calculated nonce. Should only be defined
   *                   for testing as this function will generate a
   *                   cryptographically secure random one if not defined.
   * @returns
   *         Promise<Object> Contains results of operation. The object has the
   *         following keys:
   *           field - (string) HAWK header, to use in Authorization: header
   *           artifacts - (object) other generated values:
   *             ts - (number) timestamp, in seconds since epoch
   *             nonce - (string)
   *             method - (string)
   *             resource - (string) path plus querystring
   *             host - (string)
   *             port - (number)
   *             hash - (string) payload hash (base64)
   *             ext - (string) app-specific data
   *             MAC - (string) request MAC (base64)
   */
  async computeHAWK(uri, method, options) {
    let credentials = options.credentials;
    let ts =
      options.ts ||
      Math.floor(
        ((options.now || Date.now()) + (options.localtimeOffsetMsec || 0)) /
          1000
      );
    let port;
    if (uri.port != -1) {
      port = uri.port;
    } else if (uri.scheme == "http") {
      port = 80;
    } else if (uri.scheme == "https") {
      port = 443;
    } else {
      throw new Error("Unsupported URI scheme: " + uri.scheme);
    }

    let artifacts = {
      ts,
      nonce: options.nonce || btoa(CryptoUtils.generateRandomBytesLegacy(8)),
      method: method.toUpperCase(),
      resource: uri.pathQueryRef, // This includes both path and search/queryarg.
      host: uri.asciiHost.toLowerCase(), // This includes punycoding.
      port: port.toString(10),
      hash: options.hash,
      ext: options.ext,
    };

    let contentType = CryptoUtils.stripHeaderAttributes(options.contentType);

    if (
      !artifacts.hash &&
      options.hasOwnProperty("payload") &&
      options.payload
    ) {
      const buffer = lazy.textEncoder.encode(
        `hawk.1.payload\n${contentType}\n${options.payload}\n`
      );
      const hash = await crypto.subtle.digest("SHA-256", buffer);
      // HAWK specifies this .hash to use +/ (not _-) and include the
      // trailing "==" padding.
      artifacts.hash = ChromeUtils.base64URLEncode(hash, { pad: true })
        .replace(/-/g, "+")
        .replace(/_/g, "/");
    }

    let requestString =
      "hawk.1.header\n" +
      artifacts.ts.toString(10) +
      "\n" +
      artifacts.nonce +
      "\n" +
      artifacts.method +
      "\n" +
      artifacts.resource +
      "\n" +
      artifacts.host +
      "\n" +
      artifacts.port +
      "\n" +
      (artifacts.hash || "") +
      "\n";
    if (artifacts.ext) {
      requestString += artifacts.ext.replace("\\", "\\\\").replace("\n", "\\n");
    }
    requestString += "\n";

    const hash = await CryptoUtils.hmacLegacy(
      "SHA-256",
      credentials.key,
      requestString
    );
    artifacts.mac = btoa(hash);
    // The output MAC uses "+" and "/", and padded== .

    function escape(attribute) {
      // This is used for "x=y" attributes inside HTTP headers.
      return attribute.replace(/\\/g, "\\\\").replace(/\"/g, '\\"');
    }
    let header =
      'Hawk id="' +
      credentials.id +
      '", ' +
      'ts="' +
      artifacts.ts +
      '", ' +
      'nonce="' +
      artifacts.nonce +
      '", ' +
      (artifacts.hash ? 'hash="' + artifacts.hash + '", ' : "") +
      (artifacts.ext ? 'ext="' + escape(artifacts.ext) + '", ' : "") +
      'mac="' +
      artifacts.mac +
      '"';
    return {
      artifacts,
      field: header,
    };
  },
};

var Svc = {};

Observers.add("xpcom-shutdown", function unloadServices() {
  Observers.remove("xpcom-shutdown", unloadServices);

  for (let k in Svc) {
    delete Svc[k];
  }
});
