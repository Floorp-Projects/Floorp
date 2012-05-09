/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

const EXPORTED_SYMBOLS = ["CryptoUtils"];

Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let CryptoUtils = {
  /**
   * Generate a string of random bytes.
   */
  generateRandomBytes: function generateRandomBytes(length) {
    let rng = Cc["@mozilla.org/security/random-generator;1"]
                .createInstance(Ci.nsIRandomGenerator);
    let bytes = rng.generateRandomBytes(length);
    return CommonUtils.byteArrayToString(bytes);
  },

  /**
   * UTF8-encode a message and hash it with the given hasher. Returns a
   * string containing bytes. The hasher is reset if it's an HMAC hasher.
   */
  digestUTF8: function digestUTF8(message, hasher) {
    let data = this._utf8Converter.convertToByteArray(message, {});
    hasher.update(data, data.length);
    let result = hasher.finish(false);
    if (hasher instanceof Ci.nsICryptoHMAC) {
      hasher.reset();
    }
    return result;
  },

  /**
   * Treat the given message as a bytes string and hash it with the given
   * hasher. Returns a string containing bytes. The hasher is reset if it's
   * an HMAC hasher.
   */
  digestBytes: function digestBytes(message, hasher) {
    // No UTF-8 encoding for you, sunshine.
    let bytes = [b.charCodeAt() for each (b in message)];
    hasher.update(bytes, bytes.length);
    let result = hasher.finish(false);
    if (hasher instanceof Ci.nsICryptoHMAC) {
      hasher.reset();
    }
    return result;
  },

  /**
   * UTF-8 encode a message and perform a SHA-1 over it.
   *
   * @param message
   *        (string) Buffer to perform operation on. Should be a JS string.
   *                 It is possible to pass in a string representing an array
   *                 of bytes. But, you probably don't want to UTF-8 encode
   *                 such data and thus should not be using this function.
   *
   * @return string
   *         Raw bytes constituting SHA-1 hash. Value is a JS string. Each
   *         character is the byte value for that offset. Returned string
   *         always has .length == 20.
   */
  UTF8AndSHA1: function UTF8AndSHA1(message) {
    let hasher = Cc["@mozilla.org/security/hash;1"]
                 .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA1);

    return CryptoUtils.digestUTF8(message, hasher);
  },

  sha1: function sha1(message) {
    return CommonUtils.bytesAsHex(CryptoUtils.UTF8AndSHA1(message));
  },

  sha1Base32: function sha1Base32(message) {
    return CommonUtils.encodeBase32(CryptoUtils.UTF8AndSHA1(message));
  },

  /**
   * Produce an HMAC key object from a key string.
   */
  makeHMACKey: function makeHMACKey(str) {
    return Svc.KeyFactory.keyFromString(Ci.nsIKeyObject.HMAC, str);
  },

  /**
   * Produce an HMAC hasher and initialize it with the given HMAC key.
   */
  makeHMACHasher: function makeHMACHasher(type, key) {
    let hasher = Cc["@mozilla.org/security/hmac;1"]
                   .createInstance(Ci.nsICryptoHMAC);
    hasher.init(type, key);
    return hasher;
  },

  /**
   * HMAC-based Key Derivation Step 2 according to RFC 5869.
   */
  hkdfExpand: function hkdfExpand(prk, info, len) {
    const BLOCKSIZE = 256 / 8;
    let h = CryptoUtils.makeHMACHasher(Ci.nsICryptoHMAC.SHA256,
                                       CryptoUtils.makeHMACKey(prk));
    let T = "";
    let Tn = "";
    let iterations = Math.ceil(len/BLOCKSIZE);
    for (let i = 0; i < iterations; i++) {
      Tn = CryptoUtils.digestBytes(Tn + info + String.fromCharCode(i + 1), h);
      T += Tn;
    }
    return T.slice(0, len);
  },

  /**
   * PBKDF2 implementation in Javascript.
   *
   * The arguments to this function correspond to items in
   * PKCS #5, v2.0 pp. 9-10
   *
   * P: the passphrase, an octet string:              e.g., "secret phrase"
   * S: the salt, an octet string:                    e.g., "DNXPzPpiwn"
   * c: the number of iterations, a positive integer: e.g., 4096
   * dkLen: the length in octets of the destination
   *        key, a positive integer:                  e.g., 16
   *
   * The output is an octet string of length dkLen, which you
   * can encode as you wish.
   */
  pbkdf2Generate : function pbkdf2Generate(P, S, c, dkLen) {
    // We don't have a default in the algo itself, as NSS does.
    // Use the constant.
    if (!dkLen) {
      dkLen = SYNC_KEY_DECODED_LENGTH;
    }

    /* For HMAC-SHA-1 */
    const HLEN = 20;

    function F(S, c, i, h) {

      function XOR(a, b, isA) {
        if (a.length != b.length) {
          return false;
        }

        let val = [];
        for (let i = 0; i < a.length; i++) {
          if (isA) {
            val[i] = a[i] ^ b[i];
          } else {
            val[i] = a.charCodeAt(i) ^ b.charCodeAt(i);
          }
        }

        return val;
      }

      let ret;
      let U = [];

      /* Encode i into 4 octets: _INT */
      let I = [];
      I[0] = String.fromCharCode((i >> 24) & 0xff);
      I[1] = String.fromCharCode((i >> 16) & 0xff);
      I[2] = String.fromCharCode((i >> 8) & 0xff);
      I[3] = String.fromCharCode(i & 0xff);

      U[0] = CryptoUtils.digestBytes(S + I.join(''), h);
      for (let j = 1; j < c; j++) {
        U[j] = CryptoUtils.digestBytes(U[j - 1], h);
      }

      ret = U[0];
      for (j = 1; j < c; j++) {
        ret = CommonUtils.byteArrayToString(XOR(ret, U[j]));
      }

      return ret;
    }

    let l = Math.ceil(dkLen / HLEN);
    let r = dkLen - ((l - 1) * HLEN);

    // Reuse the key and the hasher. Remaking them 4096 times is 'spensive.
    let h = CryptoUtils.makeHMACHasher(Ci.nsICryptoHMAC.SHA1,
                                       CryptoUtils.makeHMACKey(P));

    T = [];
    for (let i = 0; i < l;) {
      T[i] = F(S, c, ++i, h);
    }

    let ret = "";
    for (i = 0; i < l-1;) {
      ret += T[i++];
    }
    ret += T[l - 1].substr(0, r);

    return ret;
  },

  deriveKeyFromPassphrase: function deriveKeyFromPassphrase(passphrase,
                                                            salt,
                                                            keyLength,
                                                            forceJS) {
    if (Svc.Crypto.deriveKeyFromPassphrase && !forceJS) {
      return Svc.Crypto.deriveKeyFromPassphrase(passphrase, salt, keyLength);
    }
    else {
      // Fall back to JS implementation.
      // 4096 is hardcoded in WeaveCrypto, so do so here.
      return CryptoUtils.pbkdf2Generate(passphrase, atob(salt), 4096,
                                        keyLength);
    }
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
  computeHTTPMACSHA1: function computeHTTPMACSHA1(identifier, key, method,
                                                  uri, extra) {
    let ts = (extra && extra.ts) ? extra.ts : Math.floor(Date.now() / 1000);
    let nonce_bytes = (extra && extra.nonce_bytes > 0) ? extra.nonce_bytes : 8;

    // We are allowed to use more than the Base64 alphabet if we want.
    let nonce = (extra && extra.nonce)
                ? extra.nonce
                : btoa(CryptoUtils.generateRandomBytes(nonce_bytes));

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

    let ext = (extra && extra.ext) ? extra.ext : "";

    let requestString = ts.toString(10) + "\n" +
                        nonce           + "\n" +
                        usedMethod      + "\n" +
                        uri.path        + "\n" +
                        host            + "\n" +
                        port            + "\n" +
                        ext             + "\n";

    let hasher = CryptoUtils.makeHMACHasher(Ci.nsICryptoHMAC.SHA1,
                                            CryptoUtils.makeHMACKey(key));
    let mac = CryptoUtils.digestBytes(requestString, hasher);

    function getHeader() {
      return CryptoUtils.getHTTPMACSHA1Header(this.identifier, this.ts,
                                              this.nonce, this.mac, this.ext);
    }

    return {
      identifier: identifier,
      key:        key,
      method:     usedMethod,
      hostname:   host,
      port:       port,
      mac:        mac,
      nonce:      nonce,
      ts:         ts,
      ext:        ext,
      getHeader:  getHeader
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
  getHTTPMACSHA1Header: function getHTTPMACSHA1Header(identifier, ts, nonce,
                                                      mac, ext) {
    let header ='MAC id="' + identifier + '", ' +
                'ts="'     + ts         + '", ' +
                'nonce="'  + nonce      + '", ' +
                'mac="'    + btoa(mac)  + '"';

    if (!ext) {
      return header;
    }

    return header += ', ext="' + ext +'"';
  },
};

XPCOMUtils.defineLazyGetter(CryptoUtils, "_utf8Converter", function() {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";

  return converter;
});

let Svc = {};

XPCOMUtils.defineLazyServiceGetter(Svc,
                                   "KeyFactory",
                                   "@mozilla.org/security/keyobjectfactory;1",
                                   "nsIKeyObjectFactory");

Svc.__defineGetter__("Crypto", function() {
  let ns = {};
  Cu.import("resource://services-crypto/WeaveCrypto.js", ns);

  let wc = new ns.WeaveCrypto();
  delete Svc.Crypto;
  return Svc.Crypto = wc;
});

Observers.add("xpcom-shutdown", function unloadServices() {
  Observers.remove("xpcom-shutdown", unloadServices);

  for (let k in Svc) {
    delete Svc[k];
  }
});
